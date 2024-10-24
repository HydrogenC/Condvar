check:
	gcc -o main main.c
	./main
	python3 check.py

clean:
	rm out*.txt main