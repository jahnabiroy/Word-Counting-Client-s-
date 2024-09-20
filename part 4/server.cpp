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
    std::queue<int> request_queue;
    std::map<int, int> client_request_count;
    bool is_serving;
    pthread_t scheduler_thread;
    std::string scheduling_policy_given;

    struct ClientRequest
    {
        int client_socket;
        int offset;
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
        static Server instance("config.json", scheduling_policy);
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
        // while (true)
        // {
        int valread = read(client_socket, buffer, 1024);
        if (valread <= 0)
        {
            return;
        }

        int offset = std::stoi(buffer);
        printf("Received offset %d from client %d\n", offset, client_socket);

        pthread_mutex_lock(&words_mutex);
        if (offset >= (int)words.size())
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

        for (int i = 0; i < k && offset + i < (int)words.size(); i++)
        {
            response += words[offset + i] + ",";
            words_sent++;

            if (words_sent == p || i == k - 1 || offset + i == (int)words.size() - 1)
            {
                printf("Sending %s words at offset %d to client %d\n", response.c_str(), offset + i, client_socket);
                if (offset + i == (int)words.size() - 1 && !eofAdded)
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

        if (!eofAdded && offset + k >= (int)words.size())
        {
            response = "EOF\n";
            send(client_socket, response.c_str(), response.length(), 0);
        }

        if (!eofAdded)
        {
            add_to_queue(client_socket);
        }
        else
        {
            return;
        }
        // }
        // close(client_socket);
    }

    void add_to_queue(int client_socket)
    {
        pthread_mutex_lock(&queue_mutex);
        request_queue.push(client_socket);
        // client_request_count[client_socket]++;
        pthread_mutex_unlock(&queue_mutex);
    }

    static void *scheduler_thread_func(void *arg)
    {
        Server *server = static_cast<Server *>(arg);
        server->run_scheduler();
        return NULL;
    }

    void run_scheduler()
    {
        std::queue<int> fifo;
        while (true)
        {
            pthread_mutex_lock(&queue_mutex);
            if (!request_queue.empty())
            {
                int client_socket;
                if (scheduling_policy_given == "fair")
                {
                    client_socket = request_queue.front();
                    printf("Serving client %d\n", client_socket);
                    request_queue.pop();
                }
                else if (scheduling_policy_given == "fifo")
                {
                    if (fifo.empty())
                    {
                        while (!request_queue.empty())
                        {
                            fifo.push(request_queue.front());
                            printf("Adding client %d to fifo\n", request_queue.front());
                            request_queue.pop();
                        }
                    }

                    if (!fifo.empty())
                    {
                        client_socket = fifo.front();
                        fifo.pop();
                        printf("Adding client %d back to request queue\n", client_socket);
                        request_queue.push(client_socket);
                    }
                }
                pthread_mutex_unlock(&queue_mutex);

                is_serving = true;
                handle_client(client_socket);
                // client_request_count[client_socket]--;
                is_serving = false;

                pthread_mutex_lock(&queue_mutex);
                // if (client_request_count[client_socket] <= 0)
                // {
                //     client_request_count.erase(client_socket); // Remove the client if it's done
                // }
                pthread_mutex_unlock(&queue_mutex);
            }
            else
            {
                pthread_mutex_unlock(&queue_mutex);
                // printf("No clients in queue\n");
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

        pthread_create(&scheduler_thread, NULL, scheduler_thread_func, this);

        while (true)
        {
            int new_socket;
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
            {
                std::cerr << "Accept failed" << std::endl;
                continue;
            }

            add_to_queue(new_socket);
        }

        pthread_join(scheduler_thread, NULL);
        close(server_fd);
    }
};

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <scheduling_policy>" << std::endl;
        return 1;
    }

    std::string scheduling_policy = argv[1];
    Server *server = Server::get_instance(scheduling_policy);
    server->run();
    return 0;
}
