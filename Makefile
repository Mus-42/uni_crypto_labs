CC:=gcc
CC_FLAGS:=-g -march=native -std=gnu11 -I labs/common/include 
# CC_FLAGS:=-O2 -march=native -std=gnu11 -I labs/common/include 

.PHONY: lab1
lab1: bin/lab1
	./bin/lab1

bin/lab1: labs/lab1/main.c
	${CC} ${CC_FLAGS} labs/lab1/main.c -o bin/lab1

.PHONY: lab2
lab2: bin/lab2
	./bin/lab2

bin/lab2: labs/lab2/main.c labs/common/random.c
	${CC} ${CC_FLAGS} labs/lab2/main.c labs/common/random.c -o bin/lab2

.PHONY: lab3
lab3: bin/lab3
	./bin/lab3

bin/lab3: labs/lab3/main.c labs/common/random.c
	${CC} ${CC_FLAGS} -I labs/lab3 labs/lab3/main.c labs/common/random.c -o bin/lab3

.PHONY: lab4
lab4: bin/lab4
	./bin/lab4

bin/lab4: labs/lab4/main.c labs/common/random.c labs/lab3/main.c
	${CC} ${CC_FLAGS} -I labs/lab3 -D LAB3_NOMAIN  labs/lab3/main.c labs/lab4/main.c labs/common/random.c -ltommath -o bin/lab4

.PHONY: lab5
lab5: bin/lab5
	./bin/lab5

bin/lab5: labs/lab5/main.c labs/common/random.c
	${CC} ${CC_FLAGS} labs/lab5/main.c labs/common/random.c -o bin/lab5
