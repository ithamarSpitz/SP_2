CC = g++
CFLAGS = -std=c++11 -fprofile-arcs -ftest-coverage
LIBS = -lpthread
OBJECTS = mync.o ttt.o server.o client.o

export PATH := $(PATH)

all: mync ttt server client

mync: mync.o
	$(CC) $(CFLAGS) -o mync mync.o $(LIBS)

ttt: ttt.o
	$(CC) $(CFLAGS) -o ttt ttt.o $(LIBS)

server: server.o
	$(CC) $(CFLAGS) -o server server.o $(LIBS)

client: client.o
	$(CC) $(CFLAGS) -o client client.o $(LIBS)

mync.o: mync.cpp
	$(CC) $(CFLAGS) -c mync.cpp

ttt.o: ttt.cpp
	$(CC) $(CFLAGS) -c ttt.cpp

server.o: server.cpp
	$(CC) $(CFLAGS) -c server.cpp

client.o: client.cpp
	$(CC) $(CFLAGS) -c client.cpp

clean:
	rm -f mync ttt server client *.o *.gcno *.gcda *.gcov

coverage: all
	gcov $(OBJECTS:.o=.cpp)
