CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic -pthread
LDFLAGS = -pthread

all: build

build: client server

client: client.cpp
	$(CXX) $(CXXFLAGS) -o client client.cpp $(LDFLAGS)

server: server.cpp
	$(CXX) $(CXXFLAGS) -o server server.cpp $(LDFLAGS)

run: client server
	./server & sleep 1 && ./client

run-fifo:
	./server fifo > log.txt 2>&1 & sleep 1 && ./client

run-rr:
	./server fair > log.txt 2>&1 & sleep 1 && ./client

plot:
	python3 plot.py

clean:
	rm -f client server plot.png
	rm -f output_client_*.txt
	killall server 2>/dev/null || true

.PHONY: all build run plot clean