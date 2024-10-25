# Lab 4.1 Condvar

本实验主要用于帮助大家熟悉一类典型的同步互斥原语——条件变量。后续的实验中，我们将大量使用条件变量及其变体。

## 1 实验介绍

本实验中我们将利用条件变量实现一个极简的 Redis（一种用于存储键值对的内存数据库）。

本实验创建了两类进程：`client` 与 `server`。`server` 是一个内存数据库，存储的键（key）为 [0, 499] 的数字，对应值（value）为一系列 email 地址。`client` 将产生大量的随机的键并向 `server` 请求这些键，`server` 则返回这些键对应的 email 地址。

具体流程如下：

* 主进程创建 1 个 `server` 进程，用于维护内存键值对数据库并响应 `client` 的请求。
* 主进程创建 32 个 `client` 进程，用于发起查询请求。
* 每个 `client` 进程将发起 2000 次请求，每次请求包含 1 个键。`client` 将请求的键 `key` 写入一个与 `server` 共享的buffer，并**通知**  `server` 有请求等待响应，后续 `server` 收到通知后，将读取 buffer 中 `client` 请求的 `key`，并将 `client` 请求的 `key` 所对应的 `value` 写入 `buffer` 中，写入完成后， `server` 会**通知** `client` 请求已完成，`client` 会读取 `server` 响应的 `value` 的内容，并将其写入日志文件。最终，我们的检查脚本会依次检查每个 `client` 输出的日志，来确认我们的 `server` 能够正确地响应 `client` 的请求。

## 2 条件变量

注意到上一节，我们两次提到了“通知”，分别是：

* `client` 通知 `server` 请求到达。
* `server` 通知 `client` 请求完成。

我们可以使用条件变量来实现这种通知机制。下面我们将以一个简单的程序来演示条件变量的使用。该程序主要实现了 `thread_join` 功能，使 `parent` 进程等待 `child` 进程。该程序同样需要 “通知” 机制，即 `child` 进程调用 `thread_exit` 通知 `parent` 进程其已经完成，结束 `parent` 中 `thread_join` 的等待。

```c
// 相关头文件请自行补全

int done = 0;
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t c = PTHREAD_COND_INITIAILIZER;

void thread_exit() {
  pthread_mutex_lock(&m); // Notes 2
  done = 1; // Notes 1
  pthread_cond_signal(&c); // Notes 2
  pthread_mutex_unlock(&m); // Notes 2
}

void *child(void *arg) {
  printf("child\n");
  thread_exit();
  return NULL;
}

void thread_join() {
  pthread_mutex_lock(&m); // Notes 2
  while (done == 0) // Notes 1 & 3
    pthread_cond_wait(&c, &m); // Notes 2
  pthread_mutex_unlock(&m);// Notes 2
}

int main(int argc, char *argv[]) {
  printf("parent: begin\n");
  pthread_t p;
  pthread_create(&p, NULL, child, NULL);
  thread_join();
  printf("parent: end\n");
  return 0;
}
```

**Notes**:

1. 我们使用一个共享变量 `done` 来构造“条件”。条件变量顾名思义，需要围绕某个条件进行。这里的条件是：
   * 当 `child` 进程未退出时，`done == 0`，此时 `parent` 休眠（SLEEP）。
   * 当其退出时 `done == 1`，并唤醒（通知）`parent`。

2. 我们注意到，`pthread_cond_wait` 与 `pthread_cond_signal` 是不对称的，前者的参数包含一个条件变量和一把锁，后者只需要一个条件变量。那么，为什么我们的 `wait` 操作需要一把锁呢？我们考虑一种 `wait` 不需要锁的**假想实现**:

   ```c
   void thread_exit() {
     done = 1;
     imaginary_cond_signal(&c);
   }
   
   void thread_join() {
     if (done == 0) {
       // A
       imaginary_cond_wait(&c);
     }
   }
   ```

   假设 `parent` 执行到 A 处，此时调度器调度 `child` 运行，`child` 调用 `thread_exit` 做了两件事：

   * 将 `done` 设为 1。
   * 调用 `imaginary_cond_signal(&c)` 尝试通知 `parent` 其已经退出。

   **条件变量的 `signal` 只会唤醒当前已经在 `wait` 的线程，任何在此 `signal` 之后的 `wait` 都不会察觉到此 `signal` 的发生。这点与 semaphore 很不一样。**可以通过以下案例验证：

   ```c
   int main() {
       pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
       pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
       printf("1\n");
       pthread_cond_signal(&cond);
       printf("2\n");
       pthread_mutex_lock(&mutex);
       printf("3\n");
       pthread_cond_wait(&cond, &mutex);
       printf("4\n");
       pthread_mutex_unlock(&mutex);
       printf("5\n");
   }
   ```

   在我们这里的执行顺序中，`parent` 尚未调用 `imaginary_cond_wait`，这意味着当调度器重新调度 `parent` 并执行 `imaginary_cond_wait` 时，其将永久休眠！

   因此，我们需要一把锁，锁住的是共享变量 `done`，或者说是条件变量的“条件”。也就是说，原版 `thread_join` 的等价代码是：

   ```c
   void thread_join() {
     pthread_mutex_lock(&m);
     while (done == 0) {
       // pthread_cond_wait(&c, &m); 相当于
       add_to_the_wait_queue_of(&c);
       pthread_mutex_unlock(&m);
       sleep();
       pthread_mutex_lock(&m);
     }
     pthread_mutex_unlock(&m);
   }
   ```

   这就避免了 “`parent` 还没有 `wait`，`child` 就已经提前 `signal`，最终 `parent` 一睡不醒” 的问题。

3. 为什么需要这样的循环：

   ```c
   while (done == 0) {
     pthread_cond_wait(&c, &m);
   }
   ```

   尽管在这个案例中，使用 `if` 代替 `while` 似乎不会有问题。这是因为，这个案例中只有 `child` 会写 `done`，只有 `parent` 会读 `done`。然而，在更一般的案例中，可能存在多个进程（线程）同时写 `done`，多个进程（线程）同时读 `done`。由于 pthread 的实现基于 Mesa 语义，进程被唤醒（`signal`）后不一定会立即执行，这意味着进程**被唤醒的时候**的假设（即这个例子里的 `done == 0`）到了进程**开始运行的时候**不一定成立了。因此，我们需要通过 `while` 循环，在进程开始运行时二次检查唤醒时的假设 `done == 0`，如不满足则应当继续休眠。

   当然了，在这个案例中，理论上唤醒时的假设 `done == 0` 在运行时仍将成立，因为 `done` 一共只有 `0` 和 `1` 两种状态，并且没有可能从 `0` 变为 `1`。[OSTEP](https://pages.cs.wisc.edu/~remzi/OSTEP/threads-cv.pdf) 第 30.2 节提供了一个唤醒时假设到了运行时的假设不一定成立的案例，大家可以去阅读了解。

## 任务

此实现不需要使用服务器，本地完成即可。

```
git clone https://github.com/Boreas618/Condvar.git
```

依照上述介绍与项目中的注释完成 `main.c` 的 todo 处内容。完成后 `make check` 验证程序正确性。

## 提交

**提交方式**：将实验报告提交到 elearning 上，格式为 `学号-lab4.1.pdf`。

**注意**：从`lab1`开始，用于评分的代码以实验报告提交时为准。如果需要使用新的代码版本，请重新提交实验报告。

**截止时间**：**11月1日23:59**。

