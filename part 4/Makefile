CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic -pthread
LDFLAGS = -pthread

all: build

build: client server

client: client.cpp
	$(CXX) $(CXXFLAGS) -o client client.cpp $(LDFLAGS)

server: server.cpp
	$(CXX) $(CXXFLAGS) -o server server.cpp $(LDFLAGS)

run-fifo:
	@echo "Running with FIFO scheduling..."
	@./server fifo &
	@sleep 1  
	@./client > fifo_output.txt
	@wait
	@grep "Average time per client:" fifo_output.txt | awk '{print $$5}'


run-rr:
	@echo "Running with Round Robin scheduling..."
	@./server fair &
	@sleep 1  
	@./client > rr_output.txt
	@wait
	@grep "Average time per client:" rr_output.txt | awk '{print $$5}'

run: run-fifo run-rr
	@fifo_time=$$(grep "Average time per client:" fifo_output.txt | awk '{print $$5}'); \
	rr_time=$$(grep "Average time per client:" rr_output.txt | awk '{print $$5}'); \
	echo "$$fifo_time, $$rr_time" > output.csv
	@echo "Results logged in output.csv"


plot: build 
	python3 plot.py

fairness: build
	python3 fair.py

clean:
	rm -f client server plot.png
	rm -f output_client_*.txt
	rm -f fifo_output.txt rr_output.txt output.csv fairness.txt
	killall server 2>/dev/null || true

.PHONY: all build run plot clean