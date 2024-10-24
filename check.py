import re

with open("db.txt") as f:
    emails = f.readlines()

incorrect = 0
num_records = 32 * 2000
num_threads = 32
for i in range(num_threads):
    with open(f"out{i // 10}{i % 10}.txt") as f:
        records = f.readlines()
        for record in records:
            number = re.findall(r'\d+', record)
            email = re.findall(r'\S+@\S+', record)
            if emails[int(number[0])].strip() != email[0].strip():
                incorrect += 1
            num_records += 1
            
GREEN = '\033[92m'
RED = '\033[91m'
RESET = '\033[0m'

if incorrect == 0:
    print(f"{GREEN}PASS{RESET}")
else:
    print(f"{RED}FAILED{RESET}")
            