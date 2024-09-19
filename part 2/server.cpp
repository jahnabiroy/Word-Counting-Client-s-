#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include "json.hpp"
#include <cstring>
#include <cerrno>
// #include <thread>
#include <pthread.h>
#include <mutex>

using json = nlohmann::json;

class Server
{
private:
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    std::vector<std::string> words;
    json config;
    // std::mutex words_mutex;
    pthread_mutex_t words_mutex;

public:
    Server(const std::string &config_file)
    {
        std::ifstream f(config_file);
        config = json::parse(f);
        load_words();
        pthread_mutex_init(&words_mutex, NULL);
    }

    ~Server()
    {
        pthread_mutex_destroy(&words_mutex);
    }

    // Singleton pattern to access the Server instance from static methods
    static Server *get_instance()
    {
        static Server instance("config.json");
        return &instance;
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

    static void *handle_client_thread(void *arg)
    {
        int client_socket = *((int *)arg);
        delete (int *)arg;

        Server *server = Server::get_instance();
        server->handle_client(client_socket);

        return NULL;
    }

    void handle_client(int client_socket)
    {
        char buffer[1024] = {0};
        while (true)
        {
            int valread = read(client_socket, buffer, 1024);
            if (valread <= 0)
            {
                break;
            }

            int offset = std::stoi(buffer);

            // std::unique_lock<std::mutex> lock(words_mutex);
            pthread_mutex_lock(&words_mutex);
            if (offset >= (int)words.size())
            {
                send(client_socket, "$$\n", 3, 0);
                pthread_mutex_unlock(&words_mutex);
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
                    send(client_socket, response.c_str(), response.length(), 0);
                    response.clear();
                    words_sent = 0;
                }
            }
            // lock.unlock();
            pthread_mutex_unlock(&words_mutex);

            if (!eofAdded && offset + k >= (int)words.size())
            {
                response = "EOF\n";
                send(client_socket, response.c_str(), response.length(), 0);
            }
        }
        close(client_socket);
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
            int new_socket;
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
            {
                std::cerr << "Accept failed" << std::endl;
                continue;
            }
            // std::thread client_thread(&Server::handle_client, this, new_socket);
            // client_thread.detach();
            int *client_socket = new int(new_socket);
            pthread_t thread_id;
            int rc = pthread_create(&thread_id, NULL, handle_client_thread, (void *)client_socket);
            if (rc)
            {
                std::cerr << "Error creating thread: " << rc << std::endl;
                delete client_socket;
                continue;
            }
            pthread_detach(thread_id);
        }

        close(server_fd);
    }
};

int main()
{
    // Server server("config.json");
    // server.run();
    Server *server = Server::get_instance();
    server->run();
    return 0;
}