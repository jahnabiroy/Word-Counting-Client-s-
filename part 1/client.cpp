#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class WordCountingClient {
private:
    int sock;
    std::string serverIP;
    int serverPort;
    int k;
    int p;
    std::string filename;
    std::map<std::string, int> wordFrequency;

    bool connectToServer() {
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == -1) {
            std::cerr << "Failed to create socket" << std::endl;
            return false;
        }

        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(serverPort);
        if (inet_pton(AF_INET, serverIP.c_str(), &serverAddr.sin_addr) <= 0) {
            std::cerr << "Invalid address/ Address not supported" << std::endl;
            return false;
        }

        if (connect(sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
            std::cerr << "Connection Failed" << std::endl;
            return false;
        }

        return true;
    }

    void countWords(const std::string& words) {
        std::istringstream iss(words);
        std::string word;
        while (iss >> word) {
            if (word == "EOF") {
                break;
            }
            wordFrequency[word]++;
        }
    }

    void printWordFrequency() {
        for (const auto& pair : wordFrequency) {
            std::cout << pair.first << ", " << pair.second << std::endl;
        }
    }

public:
    WordCountingClient(const std::string& configFile) {
        std::ifstream file(configFile);
        json config;
        file >> config;

        serverIP = config["server_ip"];
        serverPort = config["server_port"];
        k = config["k"];
        p = config["p"];
        filename = config["filename"];
    }

    void run() {
        if (!connectToServer()) {
            return;
        }

        int offset = 0;
        while (true) {
            std::string request = std::to_string(offset) + "\n";
            send(sock, request.c_str(), request.length(), 0);

            char buffer[1024] = {0};
            int bytesRead = recv(sock, buffer, 1024, 0);
            if (bytesRead <= 0) {
                break;
            }

            std::string response(buffer);
            if (response == "$$\n") {
                break;
            }

            countWords(response);
            offset += k;
        }

        close(sock);
        printWordFrequency();
    }
};

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
        return 1;
    }

    WordCountingClient client(argv[1]);
    client.run();

    return 0;
}