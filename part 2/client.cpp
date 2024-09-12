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
#include <thread>
#include <vector>
#include <mutex>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class WordCountingClient
{
private:
    std::string serverIP;
    int serverPort;
    int k;
    int p;
    std::string filename;
    std::map<std::string, int> wordFrequency;
    std::mutex wordFrequencyMutex;

    bool connectToServer(int &sock)
    {
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == -1)
        {
            std::cerr << "Failed to create socket" << std::endl;
            return false;
        }

        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(serverPort);
        if (inet_pton(AF_INET, serverIP.c_str(), &serverAddr.sin_addr) <= 0)
        {
            std::cerr << "Invalid address/ Address not supported" << std::endl;
            return false;
        }

        if (connect(sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
        {
            std::cerr << "Connection Failed" << std::endl;
            return false;
        }

        return true;
    }

    void countWords(const std::string &words)
    {
        std::istringstream iss(words);
        std::string word;
        std::unique_lock<std::mutex> lock(wordFrequencyMutex);
        while (iss >> word)
        {
            if (word == "EOF")
            {
                break;
            }
            wordFrequency[word]++;
        }
    }

    void runClient()
    {
        int sock;
        if (!connectToServer(sock))
        {
            return;
        }

        int offset = 0;
        while (true)
        {
            std::string request = std::to_string(offset) + "\n";
            send(sock, request.c_str(), request.length(), 0);

            char buffer[1024] = {0};
            int bytesRead = recv(sock, buffer, 1024, 0);
            if (bytesRead <= 0)
            {
                break;
            }

            std::string response(buffer);
            if (response == "$$\n")
            {
                break;
            }

            countWords(response);
            offset += k;
        }

        close(sock);
    }

public:
    WordCountingClient(const std::string &configFile)
    {
        std::ifstream file(configFile);
        json config;
        file >> config;

        serverIP = config["server_ip"];
        serverPort = config["server_port"];
        k = config["k"];
        p = config["p"];
        filename = config["filename"];
    }

    void run(int numClients)
    {
        auto start = std::chrono::high_resolution_clock::now();

        std::vector<std::thread> clientThreads;
        for (int i = 0; i < numClients; ++i)
        {
            clientThreads.emplace_back(&WordCountingClient::runClient, this);
        }

        for (auto &thread : clientThreads)
        {
            thread.join();
        }

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;

        std::cout << "Average completion time per client: " << elapsed.count() / numClients << " seconds" << std::endl;
    }

    void printWordFrequency()
    {
        for (const auto &pair : wordFrequency)
        {
            std::cout << pair.first << ", " << pair.second << std::endl;
        }
    }
};

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        std::cerr << "Usage: " << argv[0] << " <config_file> <num_clients>" << std::endl;
        return 1;
    }

    int numClients = std::stoi(argv[2]);
    WordCountingClient client(argv[1]);
    client.run(numClients);
    client.printWordFrequency();

    return 0;
}