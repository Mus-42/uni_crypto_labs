CC=gcc -std=c11 -I labs/common/include

.PHONY: lab1
lab1: bin/lab1
	./bin/lab1

bin/lab1: labs/lab1/main.c
	${CC} labs/lab1/main.c -o bin/lab1

.PHONY: lab2
lab2: bin/lab2
	./bin/lab2

bin/lab2: labs/lab2/main.c
	${CC} labs/lab2/main.c labs/common/random.c  -o bin/lab2
