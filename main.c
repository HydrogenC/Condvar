#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <unistd.h>

#define BUFFER_SIZE 6
#define NUM_KEYS 500
#define NUM_THREADS 32
#define REQUESTS_PER_THREAD 2000

typedef struct
{
    int key;
    int status;
    char value[100];
    pthread_cond_t cond;
} request_t;

request_t buffer[BUFFER_SIZE];
pthread_mutex_t buffer_mutex;

void *client(void *arg)
{
    int thread_id = *(int *)arg;
    free(arg);

    /**
     * Open the file to store the request traces of the current thread.
     * The trace will later be checked by a script to verify that the
     * requests are correctly served.
     */
    char out_name[10] = "out__.txt";
    out_name[3] = '0' + thread_id / 10;
    out_name[4] = '0' + thread_id % 10;
    FILE *res = fopen(out_name, "w");

    size_t request_count = 0;
    while (request_count < REQUESTS_PER_THREAD)
    {
        /**
         * @todo Acquire a slot in the shared buffer to issue requests.
         * A slot with `status == 0` indicates that the client can place
         * a new request in that slot. Once the request has been set up,
         * the client changes its status to `1`, signaling that the request
         * is pending and should be processed by the server. The client then
         * waits for the condition variable to be signaled, indicating the
         * server has processed the request. After receiving the response,
         * the client saves the result and reverts the slot's status back to `0`,
         * making it available for future requests.
         *
         * The client saves the result into out_name by the following format:
         *
         * [key_1] email_addr_1
         * [key_2] email_addr_2
         * [key_3] email_addr_3
         * ...
         */

        exit(1); // Edit this.
    }

    pthread_exit(NULL);
}

void *server(void *arg)
{
    char **email_data = (char **)malloc(sizeof(char *) * NUM_KEYS);

    /**
     *  Load the database into memory.
     */
    FILE *db_file = fopen("db.txt", "r");
    size_t len = 0;
    for (size_t i = 0; i < NUM_KEYS; i++)
        if (getline(&email_data[i], &len, db_file) <= 0)
            return NULL;

    /**
     * Serve the requests from the clients
     */
    size_t request_count = 0;
    while (request_count <= NUM_THREADS * REQUESTS_PER_THREAD)
    {
        /**
         * @todo The server acquires the shared buffer using a mutex, then
         * scans the buffer to process outstanding requests. For requests
         * where the status is 1 (indicating the client has provided a valid
         * key), the server retrieves the value associated with the key,
         * updates the request's value field, and changes the status to 2.
         * A status of 2 indicates that the request has been processed and
         * the client can now read the value. After updating the request,
         * the server signals the condition variable associated with that
         * request to notify the client.
         */

        exit(1); // Edit this.
    }

    for (size_t i = 0; i < NUM_KEYS; i++)
        free(email_data[i]);
    free(email_data);

    pthread_exit(NULL);
}

int main()
{
    pthread_t clients[NUM_THREADS];
    pthread_t server_thread;

    for (size_t i = 0; i < BUFFER_SIZE; i++)
    {
        request_t *r = &buffer[i];
        pthread_cond_init(&r->cond, NULL);
        r->status = 0;
    }

    pthread_mutex_init(&buffer_mutex, NULL);

    pthread_create(&server_thread, NULL, server, NULL);

    for (int i = 0; i < NUM_THREADS; i++)
    {
        int *thread_id = malloc(sizeof(int));
        *thread_id = i;
        pthread_create(&clients[i], NULL, client, thread_id);
    }

    for (int i = 0; i < NUM_THREADS; i++)
        pthread_join(clients[i], NULL);

    pthread_mutex_destroy(&buffer_mutex);
    for (size_t i = 0; i < BUFFER_SIZE; i++)
        pthread_cond_destroy(&buffer[i].cond);
    return 0;
}
