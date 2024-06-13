CC = g++
CFLAGS = -std=c++11 -fprofile-arcs -ftest-coverage
LIBS = -lpthread
OBJECTS = mync.o ttt.o

all: mync ttt

mync: mync.o
	$(CC) $(CFLAGS) -o mync mync.o $(LIBS)

ttt: ttt.o
	$(CC) $(CFLAGS) -o ttt ttt.o $(LIBS)

mync.o: mync.cpp
	$(CC) $(CFLAGS) -c mync.cpp

ttt.o: ttt.cpp
	$(CC) $(CFLAGS) -c ttt.cpp

clean:
	rm -f mync ttt *.o *.gcno *.gcda *.gcov

coverage: all
	gcov $(OBJECTS:.o=.cpp)
