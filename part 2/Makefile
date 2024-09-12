CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -pthread
# LDFLAGS = -lnlohmann_json

all: client server

client: client.cpp
	$(CXX) $(CXXFLAGS) -o client client.cpp $(LDFLAGS)

server: server.cpp
	$(CXX) $(CXXFLAGS) -o server server.cpp $(LDFLAGS)

build: client server

run: build
	./server config.json &
	sleep 1
	./client config.json $(shell jq -r .n config.json)

plot:
	python3 plot.py

clean:
	rm -f client server

.PHONY: all build run plot clean