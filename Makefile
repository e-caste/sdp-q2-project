CC = gcc
CFLAGS = -Wall -c
OBJECTS = main.o readGraph.o buildLabels.o solveQuery.o utility.o
EXECUTABLE = main

all: $(EXECUTABLE)

$(EXECUTABLE): main.o readGraph.o buildLabels.o solveQuery.o utility.o
	$(CC) $(OBJECTS) -o $@ -lpthread

main.o: main.c
	$(CC) $(CFLAGS) main.c -o main.o

readGraph.o: readGraph.c readGraph.h
	$(CC) $(CFLAGS) readGraph.c -o readGraph.o

buildLabels.o: buildLabels.c buildLabels.h
	$(CC) $(CFLAGS) buildLabels.c -o buildLabels.o

solveQuery.o: solveQuery.c solveQuery.h
	$(CC) $(CFLAGS) solveQuery.c -o solveQuery.o

utility.o: utility.c utility.h
	$(CC) $(CFLAGS) utility.c -o utility.o

clean:
	-rm -f *.o main res_query_prova.txt