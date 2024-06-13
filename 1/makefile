CC = g++
CFLAGS = -std=c++11 -fprofile-arcs -ftest-coverage
LIBS = -lpthread
OBJECTS = ttt.o

export PATH := $(PATH)

all: ttt

ttt: ttt.o
	$(CC) $(CFLAGS) -o ttt ttt.o $(LIBS)

ttt.o: ttt.cpp
	$(CC) $(CFLAGS) -c ttt.cpp

clean:
	rm -f ttt *.o *.gcno *.gcda *.gcov

coverage: all
	gcov $(OBJECTS:.o=.cpp)
