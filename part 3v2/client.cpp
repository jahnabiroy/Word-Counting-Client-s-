#include <iostream>
#include <pthread.h>
#include <unistd.h>
#include <random>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstring>
#include <unordered_map>
#include <sstream> 
#include <sys/stat.h>
#include <vector>
#include <atomic>

#define PORT 8080
#define SERVER_IP "127.0.0.1"
#define BUFFER_SIZE 1024

std::atomic<int> completed_clients(0);
const int total_clients = 4;

void* binary_exponential_backoff(void* arg) {
    int client_id = *(int*)arg;
    delete (int*)arg;
    int backoff_attempts = 0;

    std::unordered_map<std::string, int> word_count;
    while (true) {
        // Establish TCP connection
        int sock = 0;
        struct sockaddr_in serv_addr;
        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            std::cerr << "Client " << client_id << ": Socket creation error\n";
            continue;
        }

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(PORT);

        if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
            std::cerr << "Client " << client_id << ": Invalid address/ Address not supported\n";
            close(sock);
            continue;
        }

        if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
            std::cerr << "Client " << client_id << ": Connection Failed\n";
            close(sock);
            continue;
        }

        // Send request
        std::string request = "GET /word.txt";
        send(sock, request.c_str(), request.size(), 0);

        // Receive data
        char buffer[BUFFER_SIZE] = {0};
        while (true) {
            int valread = read(sock, buffer, BUFFER_SIZE);
            if (valread <= 0) break;
            std::string data(buffer, valread);
            if (data == "HUH!") {
                // Binary Exponential Backoff
                int k = backoff_attempts++;
                int max_wait_time = (1 << k) - 1;
                int wait_time = (rand() % (max_wait_time + 1)) * 100; // T = 100 ms
                usleep(wait_time * 1000); // Convert to microseconds
                close(sock);
                break;
            }
            std::istringstream iss(data);
            std::string word;
            while (iss >> word) {
                if (word == "EOF") break;
                word_count[word]++;
            }
            if (word == "EOF") break;
        }

        close(sock);

        // Print word frequencies
        std::cout << "Client " << client_id << " word frequencies:\n";
        for (const auto& pair : word_count) {
            std::cout << pair.first << ": " << pair.second << "\n";
        }

        completed_clients++;
        return nullptr;
    }
}

void* slotted_aloha(void* arg) {
    int client_id = *(int*)arg;
    delete (int*)arg;
    double prob = 0.1;
    std::default_random_engine generator;
    std::bernoulli_distribution distribution(prob);

    std::unordered_map<std::string, int> word_count;
    while (true) {
        if (distribution(generator)) {
            // Establish TCP connection
            int sock = 0;
            struct sockaddr_in serv_addr;
            if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                std::cerr << "Client " << client_id << ": Socket creation error\n";
                continue;
            }

            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(PORT);

            if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
                std::cerr << "Client " << client_id << ": Invalid address/ Address not supported\n";
                close(sock);
                continue;
            }

            if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
                std::cerr << "Client " << client_id << ": Connection Failed\n";
                close(sock);
                continue;
            }

            // Send request
            std::string request = "GET /word.txt";
            send(sock, request.c_str(), request.size(), 0);

            // Receive data
            char buffer[BUFFER_SIZE] = {0};
            while (true) {
                int valread = read(sock, buffer, BUFFER_SIZE);
                if (valread <= 0) break;
                std::string data(buffer, valread);
                std::istringstream iss(data);
                std::string word;
                while (iss >> word) {
                    if (word == "EOF") break;
                    word_count[word]++;
                }
                if (word == "EOF") break;
            }

            close(sock);

            // Print word frequencies
            std::cout << "Client " << client_id << " word frequencies:\n";
            for (const auto& pair : word_count) {
                std::cout << pair.first << ": " << pair.second << "\n";
            }

            completed_clients++;
            return nullptr;
        }
        usleep(100000); // 100 ms
    }
}

int main() {
    pthread_t threads[total_clients];
    for (int i = 0; i < total_clients; ++i) {
        int* client_id = new int(i);
        // pthread_create(&threads[i], nullptr, slotted_aloha, client_id);
        // pthread_create(&threads[i], nullptr, sensing_with_beb, client_id);
        pthread_create(&threads[i], nullptr, binary_exponential_backoff, client_id);
        usleep(100000); // 100 ms
    }

    for (int i = 0; i < total_clients; ++i) {
        pthread_join(threads[i], nullptr);
    }

    while (completed_clients.load() < total_clients) {
        usleep(100000); // Wait for 100 ms before checking again
    }

    std::cout << "All clients have received the complete file.\n";
    return 0;
}
