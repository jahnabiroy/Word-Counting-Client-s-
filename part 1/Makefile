CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic
LDFLAGS = 

all: build

build: client server

client: client.cpp
	$(CXX) $(CXXFLAGS) -o client client.cpp $(LDFLAGS)

server: server.cpp
	$(CXX) $(CXXFLAGS) -o server server.cpp $(LDFLAGS)

run: client server
	./server & sleep 1 && ./client

plot:
	python3 plot_results.py

clean:
	rm -f client server plot.png output.txt
	killall server

.PHONY: all build run plot clean
