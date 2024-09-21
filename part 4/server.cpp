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
#include <pthread.h>
#include <queue>
#include <map>
#include <chrono>
#include <climits>

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
    pthread_mutex_t words_mutex;
    pthread_mutex_t queue_mutex;
    std::queue<std::pair<int, int>> request_queue; // pair of <client_socket, offset>
    std::map<int, std::queue<int>> client_queues;  // For fair scheduling
    bool is_serving;
    pthread_t scheduler_thread;
    std::string scheduling_policy_given;

    struct ClientRequest
    {
        int client_socket;
        int offset = 0;
    };

public:
    Server(const std::string &config_file, const std::string &scheduling_policy)
        : is_serving(false), scheduling_policy_given(scheduling_policy)
    {
        std::ifstream f(config_file);
        config = json::parse(f);
        load_words();
        pthread_mutex_init(&words_mutex, NULL);
        pthread_mutex_init(&queue_mutex, NULL);
        scheduling_policy_given = scheduling_policy;
    }

    ~Server()
    {
        pthread_mutex_destroy(&words_mutex);
        pthread_mutex_destroy(&queue_mutex);
    }

    static Server *get_instance(const std::string &scheduling_policy)
    {
        static Server instance("config_4.json", scheduling_policy);
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
        ClientRequest *request = static_cast<ClientRequest *>(arg);
        Server *server = Server::get_instance("");
        server->handle_client(request->client_socket);
        delete request;
        return NULL;
    }

    void handle_client(int client_socket)
    {
        char buffer[1024] = {0};
        int valread = read(client_socket, buffer, 1024);
        if (valread <= 0)
        {
            return;
        }

        int offset_received = std::stoi(buffer);

        pthread_mutex_lock(&words_mutex);
        if (offset_received >= (int)words.size())
        {
            send(client_socket, "$$\n", 3, 0);
            pthread_mutex_unlock(&words_mutex);
            return;
        }

        std::string response;
        int k = config["k"].get<int>();
        int p = config["p"].get<int>();
        int words_sent = 0;
        bool eofAdded = false;

        for (int i = 0; i < k && offset_received + i < (int)words.size(); i++)
        {
            response += words[offset_received + i] + ",";
            words_sent++;

            if (words_sent == p || i == k - 1 || offset_received + i == (int)words.size() - 1)
            {
                if (offset_received + i == (int)words.size() - 1 && !eofAdded)
                {
                    response += "EOF\n";
                    eofAdded = true;
                }
                response.pop_back();
                response += "\n";
                send(client_socket, response.c_str(), response.length(), 0);
                response.clear();
                words_sent = 0;
            }
        }
        pthread_mutex_unlock(&words_mutex);

        if (!eofAdded && offset_received + k < (int)words.size())
        {
            add_to_queue(client_socket, offset_received + k);
        }
        else if (!eofAdded)
        {
            response = "EOF\n";
            send(client_socket, response.c_str(), response.length(), 0);
        }
    }

    void add_to_queue(int client_socket, int offset)
    {
        pthread_mutex_lock(&queue_mutex);
        if (scheduling_policy_given == "fifo")
        {
            request_queue.push({client_socket, offset});
        }
        else if (scheduling_policy_given == "fair")
        {
            client_queues[client_socket].push(offset);
            if (client_queues[client_socket].size() == 1)
            {
                request_queue.push({client_socket, offset});
            }
        }
        pthread_mutex_unlock(&queue_mutex);
    }

    void run_scheduler()
    {
        while (true)
        {
            pthread_mutex_lock(&queue_mutex);
            if (!request_queue.empty())
            {
                auto [client_socket, offset] = request_queue.front();
                request_queue.pop();

                if (scheduling_policy_given == "fair")
                {
                    client_queues[client_socket].pop();
                }

                pthread_mutex_unlock(&queue_mutex);

                // // printf("Serving client %d at offset %d (%s Scheduling)\n",
                //        client_socket, offset, scheduling_policy_given.c_str());

                is_serving = true;
                handle_client(client_socket);
                is_serving = false;

                if (scheduling_policy_given == "fair")
                {
                    pthread_mutex_lock(&queue_mutex);
                    if (!client_queues[client_socket].empty())
                    {
                        int next_offset = client_queues[client_socket].front();
                        request_queue.push({client_socket, next_offset});
                    }
                    else
                    {
                        client_queues.erase(client_socket);
                    }
                    pthread_mutex_unlock(&queue_mutex);
                }
            }
            else
            {
                pthread_mutex_unlock(&queue_mutex);
                usleep(1000);
            }
        }
    }

    void run()
    {
        if (!setup_server())
        {
            return;
        }

        std::cout << "Server is running with " << scheduling_policy_given << " scheduling..." << std::endl;

        pthread_create(&scheduler_thread, NULL, [](void *arg) -> void *
                       {
            static_cast<Server*>(arg)->run_scheduler();
            return NULL; }, this);

        while (true)
        {
            int new_socket;
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
            {
                std::cerr << "Accept failed" << std::endl;
                continue;
            }

            add_to_queue(new_socket, 0);
        }

        pthread_join(scheduler_thread, NULL);
        close(server_fd);
    }
};

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <scheduling_policy_given>" << std::endl;
        return 1;
    }

    std::string scheduling_policy = argv[1];
    if (scheduling_policy != "fifo" && scheduling_policy != "fair")
    {
        std::cerr << "Invalid scheduling policy. Use 'fifo' or 'fair'." << std::endl;
        return 1;
    }

    Server server("config_4.json", scheduling_policy);
    server.run();
    return 0;
}
