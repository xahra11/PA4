CC = gcc
CFLAGS = -Wall -Wextra -Werror -g
LDFLAGS = 
OBJ = nimd.o message.o handlers.o send.o
TEST_OBJ = tests.o

all: nimd_server tests

nimd_server: $(OBJ)
	$(CC) $(CFLAGS) -o nimd_server $(OBJ) $(LDFLAGS)

tests: $(TEST_OBJ)
	$(CC) $(CFLAGS) -o tests $(TEST_OBJ) $(LDFLAGS)

nimd.o: nimd.c message.h handlers.h send.h
message.o: message.c message.h
handlers.o: handlers.c handlers.h message.h send.h
send.o: send.c send.h handlers.h
tests.o: tests.c

clean:
	rm -f *.o nimd_server tests

.PHONY: all clean
