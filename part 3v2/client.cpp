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
#include <fstream>
#include <signal.h>

#define PORT 8080
#define SERVER_IP "127.0.0.1"
#define BUFFER_SIZE 1024

std::atomic<int> completed_clients(0);
const int total_clients = 2;
int slot_time_ms = 100;

void dump_word_frequencies(int client_id, const std::unordered_map<std::string, int>& word_count) {
    std::ofstream outfile("output_client" + std::to_string(client_id) + ".txt");
    if (outfile.is_open()) {
        // outfile << "Client " << client_id << " word frequencies:\n";
        for (const auto& pair : word_count) {
            outfile << pair.first << ": " << pair.second << "\n";
        }
        outfile.close();
    } else {
        std::cerr << "Unable to open file for client " << client_id << "\n";
    }
}

void handle_sigpipe(int sig) {
    std::cerr << "Caught SIGPIPE signal " << sig << "\n";
}

void* binary_exponential_backoff(void* arg) {
    int client_id = *(int*)arg;
    delete (int*)arg;

    std::unordered_map<std::string, int> word_count;
    const int max_backoff_attempts = 10; // Set a limit for backoff attempts

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
        if (send(sock, request.c_str(), request.size(), 0) == -1) {
            std::cerr << "Client " << client_id << ": Error sending request\n";
            close(sock);
            continue;
        }

        // Receive data
        char buffer[BUFFER_SIZE] = {0};
        int backoff_attempts_beb = 0; // Reset backoff attempts for each new connection
        while (true) {
            int valread = read(sock, buffer, BUFFER_SIZE);
            if (valread <= 0) break;
            std::string data(buffer, valread);
            std::istringstream iss(data);
            std::string word;
            std::vector<std::string> words;
            while (std::getline(iss, word, ',')) {
                words.push_back(word);
            }
            if (word == "EOF") break;
            else if (word.substr(0, 4) == "HUH!") {
                backoff_attempts_beb++;
                backoff_attempts_beb = std::min(backoff_attempts_beb, max_backoff_attempts);
                int max_wait_time = ((1 << backoff_attempts_beb) - 1) * slot_time_ms;
                int wait_time = rand() % (max_wait_time + 1);
                usleep(wait_time * 10);
                continue;
            }
            for (const auto& word : words) {
                std::cout << "Client " << client_id << ": " << word << std::endl;
                word_count[word]++;
            }
        }

        close(sock);

        // Print word frequencies
        dump_word_frequencies(client_id, word_count);

        completed_clients++;
        return nullptr;
    }
}

void* slotted_aloha(void* arg) {
    int client_id = *(int*)arg;
    delete (int*)arg;
    double prob = (double)1/(double)total_clients;
    std::default_random_engine generator;
    std::bernoulli_distribution distribution(prob);

    std::unordered_map<std::string, int> word_count;
    while (completed_clients.load() < total_clients) { // Check if all clients are completed
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
            if (send(sock, request.c_str(), request.size(), 0) == -1) {
                std::cerr << "Client " << client_id << ": Error sending request\n";
                close(sock);
                continue;
            }

            // Receive data
            char buffer[BUFFER_SIZE] = {0};
            while (true) {
                int valread = read(sock, buffer, BUFFER_SIZE);
                if (valread <= 0) break;
                std::string data(buffer, valread);
                std::istringstream iss(data);
                std::string word;
                std::vector<std::string> words;
                while (std::getline(iss, word, ',')) {
                    words.push_back(word);
                }
                if (word == "EOF") break;
                else if(word.substr(0, 4) == "HUH!") {
                    int wait_time = slot_time_ms;
                    usleep(wait_time);
                    continue;
                }
                for (const auto& word : words) {
                    std::cout << "Client " << client_id << ": " << word << std::endl;
                    word_count[word]++;
                }
            }

            close(sock);

            // Print word frequencies
            dump_word_frequencies(client_id, word_count);

            completed_clients++;
            return nullptr;
        }
        usleep(10); // 100 ms
    }
    return nullptr;
}

void* sensing_with_beb(void* arg) {
    int client_id = *(int*)arg;
    delete (int*)arg;

    while (true) {
        int sock = 0;
        struct sockaddr_in serv_addr;
        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            std::cerr << "Socket creation error\n";
            continue;
        }

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(PORT);

        if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
            std::cerr << "Invalid address/ Address not supported\n";
            close(sock);
            continue;
        }

        if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
            std::cerr << "Connection Failed\n";
            close(sock);
            usleep(10); // Wait for 100 ms before retrying
            continue;
        }

        int backoff_time = 1;
        while (true) {
            std::string busy_check = "BUSY?\n";
            if (send(sock, busy_check.c_str(), busy_check.size(), 0) == -1) {
                std::cerr << "Client " << client_id << ": Error sending busy check\n";
                close(sock);
                break; // Exit the inner loop and retry connection
            }

            char buffer[1024] = {0};
            int valread = read(sock, buffer, 1024);
            if (valread <= 0) {
                std::cerr << "Client " << client_id << ": Error reading response\n";
                close(sock);
                break; // Exit the inner loop and retry connection
            }
            std::string response(buffer, valread);
            std::cout << "Client " << client_id << " received: " << response << "\n";
            if (response == "IDLE\n") {
                std::string data_request = "DATA_REQUEST\n";
                if (send(sock, data_request.c_str(), data_request.size(), 0) == -1) {
                    std::cerr << "Client " << client_id << ": Error sending data request\n";
                    close(sock);
                    break; // Exit the inner loop and retry connection
                }

                // Read server response
                valread = read(sock, buffer, 1024);
                if (valread <= 0) {
                    std::cerr << "Client " << client_id << ": Error reading data response\n";
                    close(sock);
                    break; // Exit the inner loop and retry connection
                }
                response = std::string(buffer, valread);

                if (response == "HUH!\n") {
                    // Revert to BEB
                    usleep(backoff_time * 10);
                    backoff_time = std::min(backoff_time * 2, 10); // Exponential backoff with a max limit
                    continue;
                }

                // Process the data received from the server
                std::cout << "Client " << client_id << " received: " << response << "\n";
                break; // Exit the inner loop after successful data reception
            } else {
                usleep(10); // Wait for 100 ms before asking again
            }
        }

        close(sock);
        completed_clients++;
        if (completed_clients.load() >= total_clients) {
            break; // Exit the outer loop if all clients are completed
        }
    }

    return nullptr;
}

int main() {
    // Set up the SIGPIPE signal handler
    struct sigaction sa;
    sa.sa_handler = handle_sigpipe;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGPIPE, &sa, NULL);

    pthread_t threads[total_clients];
    for (int i = 0; i < total_clients; ++i) {
        int* client_id = new int(i);
        pthread_create(&threads[i], nullptr, slotted_aloha, client_id);
        // pthread_create(&threads[i], nullptr, sensing_with_beb, client_id);
        // pthread_create(&threads[i], nullptr, binary_exponential_backoff, client_id);
        usleep(10); // 100 ms
    }

    for (int i = 0; i < total_clients; ++i) {
        pthread_join(threads[i], nullptr);
    }

    while (completed_clients.load() < total_clients) {
        usleep(10); // Wait for 100 ms before checking again
    }

    std::cout << "All clients have received the complete file.\n";
    return 0;
}