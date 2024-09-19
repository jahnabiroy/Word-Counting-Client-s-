#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class Server
{
private:
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    std::vector<std::string> words;
    json config;

public:
    Server(const std::string &config_file)
    {
        std::ifstream f(config_file);
        config = json::parse(f);
        load_words();
    }

    void load_words()
    {
        std::string filename = config["filename"].get<std::string>();
        std::ifstream file(filename);
        std::string word;
        if (!file.is_open())
        {
            std::cerr << "Failed to open file: " << filename << std::endl;
            return;
        }

        while (std::getline(file, word, ','))
        {
            words.push_back(word);
            printf("%s\n", word.c_str());
        }

        printf("Words loaded: %d\n", words.size());
    }

    bool setup_server()
    {
        if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
        {
            std::cerr << "Socket failed" << std::endl;
            return false;
        }

        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
        {
            std::cerr << "Setsockopt failed" << std::endl;
            return false;
        }

        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(config["server_port"]);

        if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
        {
            std::cerr << "Bind failed" << std::endl;
            return false;
        }

        if (listen(server_fd, 3) < 0)
        {
            std::cerr << "Listen failed" << std::endl;
            return false;
        }

        return true;
    }

    void handle_client()
    {
        char buffer[1024] = {0};
        while (true)
        {
            int valread = read(new_socket, buffer, 1024);
            if (valread <= 0)
            {
                break;
            }

            int offset = std::stoi(buffer);
            printf("Received Offset: %d\n", offset);

            if (offset >= words.size())
            {
                send(new_socket, "$$\n", 3, 0);
                break;
            }

            std::string response;
            int k = config["k"].get<int>();
            int p = config["p"].get<int>();
            printf("k: %d, p: %d\n", k, p);
            int words_sent = 0;
            bool eofAdded = false;

            for (int i = 0; i < k && offset + i < words.size(); i++)
            {
                printf("offset + i :%d\n", i + offset);
                response += words[offset + i] + ",";
                words_sent++;

                if (words_sent == p || i == k - 1 || offset + i == words.size() - 1)
                {
                    if (offset + i == words.size() - 1 && !eofAdded)
                    {
                        response += "EOF\n";
                        eofAdded = true;
                    }
                    response.pop_back(); // Remove the last comma
                    response += "\n";
                    send(new_socket, response.c_str(), response.length(), 0);
                    response.clear();
                    words_sent = 0;
                }
            }

            if (!eofAdded && offset + k >= words.size())
            {
                response = "EOF\n";
                send(new_socket, response.c_str(), response.length(), 0);
            }

            // printf("Sent: %d\n", words_sent);
        }
    }

    void run()
    {
        if (!setup_server())
        {
            return;
        }

        std::cout << "Server is running..." << std::endl;

        while (true)
        {
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
            {
                std::cerr << "Accept failed" << std::endl;
                continue;
            }
            handle_client();
            close(new_socket);
        }

        close(server_fd);
    }
};

int main()
{
    Server server("config.json");
    server.run();
    return 0;
}