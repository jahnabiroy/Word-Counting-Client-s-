#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <mutex>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class ConcurrentWordCountingServer {
private:
    int serverSocket;
    std::string serverIP;
    int serverPort;
    int k;
    int p;
    std::string filename;
    std::vector<std::string> words;
    std::mutex wordsMutex;

    void loadWords() {
        std::ifstream file(filename);
        std::string word;
        while (std::getline(file, word, ',')) {
            words.push_back(word);
        }
        words.push_back("EOF");
    }

    bool setupServer() {
        serverSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSocket == -1) {
            std::cerr << "Failed to create socket" << std::endl;
            return false;
        }

        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(serverPort);

        if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
            std::cerr << "Bind failed" << std::endl;
            return false;
        }

        if (listen(serverSocket, 32) < 0) {
            std::cerr << "Listen failed" << std::endl;
            return false;
        }

        return true;
    }

    void handleClient(int clientSocket) {
        char buffer[1024] = {0};
        while (true) {
            int bytesRead = recv(clientSocket, buffer, 1024, 0);
            if (bytesRead <= 0) {
                break;
            }

            int offset = std::stoi(buffer);
            std::unique_lock<std::mutex> lock(wordsMutex);
            if (offset >= words.size()) {
                send(clientSocket, "$$\n", 3, 0);
                break;
            }

            std::string response;
            for (int i = 0; i < k && offset + i < words.size(); i++) {
                response += words[offset + i] + "\n";
                if ((i + 1) % p == 0 || i == k - 1 || offset + i == words.size() - 1) {
                    send(clientSocket, response.c_str(), response.length(), 0);
                    response.clear();
                }
            }
            lock.unlock();
        }
        close(clientSocket);
    }

public:
    ConcurrentWordCountingServer(const std::string& configFile) {
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
        loadWords();
        if (!setupServer()) {
            return;
        }

        std::cout << "Server is listening on " << serverIP << ":" << serverPort << std::endl;

        while (true) {
            sockaddr_in clientAddr;
            socklen_t clientAddrLen = sizeof(clientAddr);
            int clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddr, &clientAddrLen);
            if (clientSocket < 0) {
                std::cerr << "Accept failed" << std::endl;
                continue;
            }

            std::cout << "New client connected" << std::endl;
            std::thread clientThread(&ConcurrentWordCountingServer::handleClient, this, clientSocket);
            clientThread.detach();
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
        return 1;
    }

    ConcurrentWordCountingServer server(argv[1]);
    server.run();

    return 0;
}