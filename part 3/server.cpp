#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>
#include "json.hpp"
#include <signal.h>

using json = nlohmann::json;

class GrumpyServer
{
private:
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    std::vector<std::string> words;
    json config;
    std::mutex words_mutex;
    std::atomic<bool> is_busy{false};
    std::chrono::time_point<std::chrono::steady_clock> current_request_start;
    std::chrono::time_point<std::chrono::steady_clock> last_collision_time;
    int num_clients;

public:
    GrumpyServer(const std::string &config_file)
    {
        std::ifstream f(config_file);
        config = json::parse(f);
        load_words();
        num_clients = config["num_clients"].get<int>();
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
        }
    }

    bool setup_server()
    {
        if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
        {
            std::cerr << "Socket failed" << std::endl;
            return false;
        }
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
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

    bool check_collision(const std::chrono::time_point<std::chrono::steady_clock> &request_time)
    {
        if (is_busy)
        {
            return true;
        }
        if (request_time <= last_collision_time)
        {
            return true;
        }
        return false;
    }

    void handle_client(int client_socket)
    {
        char buffer[1024] = {0};
        while (true)
        {
            int valread = read(client_socket, buffer, 1024);
            if (valread <= 0)
            {
                if (valread == 0)
                {
                    std::cout << "Client disconnected, socket: " << client_socket << std::endl;
                }
                else
                {
                    std::cerr << "Read error on socket " << client_socket << ": " << strerror(errno) << std::endl;
                }
                break;
            }

            std::string request(buffer);
            if (request == "BUSY?\n")
            {
                if (is_busy)
                {
                    if (send(client_socket, "BUSY\n", 5, 0) < 0)
                    {
                        std::cerr << "Send error on socket " << client_socket << ": " << strerror(errno) << std::endl;
                        break;
                    }
                }
                else
                {
                    if (send(client_socket, "IDLE\n", 5, 0) < 0)
                    {
                        std::cerr << "Send error on socket " << client_socket << ": " << strerror(errno) << std::endl;
                        break;
                    }
                }
                continue;
            }

            auto request_time = std::chrono::steady_clock::now();
            bool collision = check_collision(request_time);

            if (collision)
            {
                if (send(client_socket, "HUH!\n", 5, 0) < 0)
                {
                    std::cerr << "Send error on socket " << client_socket << ": " << strerror(errno) << std::endl;
                    break;
                }
                last_collision_time = request_time;
                continue;
            }

            is_busy = true;
            current_request_start = request_time;

            int offset = std::stoi(buffer);

            std::unique_lock<std::mutex> lock(words_mutex);
            if (offset >= (int)words.size())
            {
                if (send(client_socket, "$$\n", 3, 0) < 0)
                {
                    std::cerr << "Send error on socket " << client_socket << ": " << strerror(errno) << std::endl;
                }
                is_busy = false;
                break;
            }

            std::string response;
            int k = config["k"].get<int>();
            int p = config["p"].get<int>();
            int words_sent = 0;
            bool eofAdded = false;

            for (int i = 0; i < k && offset + i < (int)words.size(); i++)
            {
                response += words[offset + i] + ",";
                words_sent++;

                if (words_sent == p || i == k - 1 || offset + i == (int)words.size() - 1)
                {
                    if (offset + i == (int)words.size() - 1 && !eofAdded)
                    {
                        response += "EOF\n";
                        eofAdded = true;
                    }
                    response.pop_back(); // Remove the last comma
                    response += "\n";
                    if (send(client_socket, response.c_str(), response.length(), 0) < 0)
                    {
                        std::cerr << "Send error on socket " << client_socket << ": " << strerror(errno) << std::endl;
                        break;
                    }
                    response.clear();
                    words_sent = 0;
                }
            }
            lock.unlock();

            if (!eofAdded && offset + k >= (int)words.size())
            {
                response = "EOF\n";
                if (send(client_socket, response.c_str(), response.length(), 0) < 0)
                {
                    std::cerr << "Send error on socket " << client_socket << ": " << strerror(errno) << std::endl;
                    break;
                }
            }

            is_busy = false;
        }
        close(client_socket);
    }

    void run()
    {
        if (!setup_server())
        {
            return;
        }

        std::cout << "Grumpy Server is running..." << std::endl;

        while (true)
        {
            int new_socket;
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
            {
                std::cerr << "Accept failed" << std::endl;
                continue;
            }
            std::cout << "Accepted a new connection, socket: " << new_socket << std::endl;
            std::thread client_thread(&GrumpyServer::handle_client, this, new_socket);
            client_thread.detach();
        }

        close(server_fd);
    }
};

int main()
{
    signal(SIGPIPE, SIG_IGN);

    GrumpyServer server("config.json");
    server.run();
    return 0;
}