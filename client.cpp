#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <chrono>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class Client {
private:
    int sock = 0;
    struct sockaddr_in serv_addr;
    std::map<std::string, int> word_frequency;
    json config;

public:
    Client(const std::string& config_file) {
        std::ifstream f(config_file);
        config = json::parse(f);
    }

    bool connect_to_server() {
        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            std::cout << "Socket creation error" << std::endl;
            return false;
        }

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(config["server_port"]);

        if (inet_pton(AF_INET, config["server_ip"].get<std::string>().c_str(), &serv_addr.sin_addr) <= 0) {
            std::cout << "Invalid address/ Address not supported" << std::endl;
            return false;
        }

        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            std::cout << "Connection Failed" << std::endl;
            return false;
        }

        return true;
    }

    void process_words() {
        int offset = 0;
        std::string message;
        char buffer[1024] = {0};

        while (true) {
            message = std::to_string(offset) + "\n";
            send(sock, message.c_str(), message.length(), 0);
            memset(buffer, 0, sizeof(buffer));
            int valread = read(sock, buffer, 1024);
            
            if (strcmp(buffer, "$$\n") == 0) {
                break;
            }

            std::istringstream iss(buffer);
            std::string word;
            while (std::getline(iss, word)) {
                if (word == "EOF") {
                    return;
                }
                word_frequency[word]++;
                offset++;
            }
        }
    }

    void print_frequency() {
        for (const auto& pair : word_frequency) {
            std::cout << pair.first << ", " << pair.second << std::endl;
        }
    }

    void run() {
        auto start = std::chrono::high_resolution_clock::now();

        if (!connect_to_server()) {
            return;
        }

        process_words();
        print_frequency();

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> diff = end - start;
        std::cout << "Time taken: " << diff.count() << " seconds" << std::endl;

        close(sock);
    }
};

int main() {
    Client client("config.json");
    client.run();
    return 0;
}
