CC = gcc
CFLAGS = -Wall -c
SOURCES	= q2.c
OBJECTS = $(SOURCES:.c=.o)
EXECUTABLE = q2

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE) : $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ -lpthread

.c.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	-rm -f *.o q2
