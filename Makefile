CXX = g++
CXXFLAGS = -std=c++11 -Wall -O2 -pthread

all: lab3

lab3: main.o scmp_queue.o mcmp_queue.o
	$(CXX) $(CXXFLAGS) -o lab3 main.o scmp_queue.o mcmp_queue.o

main.o: main.cpp
	$(CXX) $(CXXFLAGS) -c main.cpp

scmp_queue.o: scmp_queue.cpp
	$(CXX) $(CXXFLAGS) -c scmp_queue.cpp

mcmp_queue.o: mcmp_queue.cpp
	$(CXX) $(CXXFLAGS) -c mcmp_queue.cpp

clean:
	rm -f *.o lab3