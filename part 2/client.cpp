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
#include "json.hpp"
#include <pthread.h>
#include <vector>

using json = nlohmann::json;

class Client
{
private:
    struct sockaddr_in serv_addr;
    std::vector<std::map<std::string, int>> word_frequencies;
    json config;
    pthread_mutex_t word_frequencies_mutex; // Changed from std::mutex to pthread_mutex_t
    std::vector<double> client_times;

public:
    Client(const std::string &config_file)
    {
        std::ifstream f(config_file);
        config = json::parse(f);
        int num_clients = config["num_clients"].get<int>();
        word_frequencies.resize(num_clients);
        client_times.resize(num_clients);
        pthread_mutex_init(&word_frequencies_mutex, NULL); // Initialize pthread mutex
    }

    ~Client()
    {
        pthread_mutex_destroy(&word_frequencies_mutex); // Destroy the mutex in destructor
    }

    bool connect_to_server(int &sock)
    {
        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            std::cerr << "Socket creation error" << std::endl;
            return false;
        }

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(config["server_port"]);

        if (inet_pton(AF_INET, config["server_ip"].get<std::string>().c_str(), &serv_addr.sin_addr) <= 0)
        {
            std::cerr << "Invalid address/ Address not supported" << std::endl;
            return false;
        }

        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        {
            std::cerr << "Connection Failed" << std::endl;
            return false;
        }

        return true;
    }

    void process_words(int sock, int client_id)
    {
        int offset = 0;
        std::string message;
        char buffer[1024] = {0};
        int req_words = config["k"].get<int>();
        int words_received = 0;

        while (true)
        {
            if (offset == 0)
            {
                message = std::to_string(offset) + "\n";
                send(sock, message.c_str(), message.length(), 0);
            }
            if (words_received >= req_words)
            {
                message = std::to_string(offset) + "\n";
                send(sock, message.c_str(), message.length(), 0);
                words_received = 0;
            }
            memset(buffer, 0, sizeof(buffer));
            int valread = read(sock, buffer, 1024);

            if (strcmp(buffer, "$$\n") == 0 || valread <= 0)
            {
                break;
            }

            std::istringstream iss(buffer);
            std::string line;
            while (std::getline(iss, line))
            {
                std::istringstream line_stream(line);
                std::string word;
                while (std::getline(line_stream, word, ','))
                {
                    if (word == "EOF")
                    {
                        return;
                    }
                    words_received++;

                    // Lock the mutex before updating the shared word frequencies
                    pthread_mutex_lock(&word_frequencies_mutex);

                    word_frequencies[client_id][word]++;
                    offset++;

                    // Unlock the mutex after updating shared data
                    pthread_mutex_unlock(&word_frequencies_mutex);
                }
            }
        }
    }

    void write_frequency(int client_id)
    {
        std::string filename = "output_client_" + std::to_string(client_id) + ".txt";
        std::ofstream out(filename);

        for (const auto &pair : word_frequencies[client_id])
        {
            out << pair.first << ", " << pair.second << "\n";
        }

        out.close();

        std::cout << "Client " << client_id << " output written to " << filename << std::endl;
    }

    void run_client(int client_id)
    {
        auto start = std::chrono::high_resolution_clock::now();

        int sock = 0;
        if (!connect_to_server(sock))
        {
            return;
        }

        process_words(sock, client_id);
        close(sock);

        write_frequency(client_id);

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> diff = end - start;
        client_times[client_id] = diff.count();

        std::cout << "Client " << client_id << " completed in " << diff.count() << " seconds" << std::endl;
    }

    static void *run_client_thread(void *arg)
    {
        ThreadArgs *args = static_cast<ThreadArgs *>(arg);
        args->client->run_client(args->client_id);
        delete args;
        return NULL;
    }

    void run()
    {
        auto start = std::chrono::high_resolution_clock::now();

        int num_clients = config["num_clients"].get<int>();
        std::vector<pthread_t> client_threads(num_clients);

        for (int i = 0; i < num_clients; ++i)
        {
            ThreadArgs *args = new ThreadArgs{this, i};
            int rc = pthread_create(&client_threads[i], NULL, run_client_thread, args);
            if (rc)
            {
                std::cerr << "Error creating thread: " << rc << std::endl;
                return;
            }
        }

        for (int i = 0; i < num_clients; ++i)
        {
            pthread_join(client_threads[i], NULL);
        }

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> total_time = end - start;

        std::cout << "All clients completed." << std::endl;
        std::cout << "Total time taken: " << total_time.count() << " seconds" << std::endl;

        double avg_time = 0;
        for (int i = 0; i < num_clients; ++i)
        {
            avg_time += client_times[i];
        }
        avg_time /= num_clients;

        std::cout << "Average time per client: " << avg_time << " seconds" << std::endl;
    }

private:
    struct ThreadArgs
    {
        Client *client;
        int client_id;
    };
};

int main()
{
    Client client("config.json");
    client.run();
    return 0;
}
