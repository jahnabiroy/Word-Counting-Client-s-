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
#include <random>
#include <algorithm>
#include "json.hpp"

using json = nlohmann::json;

enum class Protocol
{
    SLOTTED_ALOHA,
    BEB,
    SENSING_BEB
};

class GrumpyClient
{
private:
    struct sockaddr_in serv_addr;
    std::map<std::string, int> word_frequency;
    json config;
    Protocol protocol;
    std::default_random_engine generator;
    int num_clients;

public:
    GrumpyClient(const std::string &config_file, Protocol p)
        : protocol(p)
    {
        std::ifstream f(config_file);
        config = json::parse(f);
        generator.seed(std::chrono::system_clock::now().time_since_epoch().count());
        num_clients = config["num_clients"].get<int>();
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

    void process_words(int sock)
    {
        int offset = 0;
        std::string message;
        char buffer[1024] = {0};
        int backoff_counter = 0;

        while (true)
        {
            bool success = false;
            while (!success)
            {
                switch (protocol)
                {
                case Protocol::SLOTTED_ALOHA:
                    success = slotted_aloha_send(sock, offset);
                    break;
                case Protocol::BEB:
                    success = beb_send(sock, offset, backoff_counter);
                    break;
                case Protocol::SENSING_BEB:
                    success = sensing_beb_send(sock, offset, backoff_counter);
                    break;
                }
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
                    word_frequency[word]++;
                    offset++;
                }
            }
        }
    }

    bool slotted_aloha_send(int sock, int offset)
    {
        int slot_duration = config["slot_duration"].get<int>();
        double prob = 1.0 / config["num_clients"].get<int>();

        std::uniform_real_distribution<double> distribution(0.0, 1.0);
        if (distribution(generator) < prob)
        {
            auto now = std::chrono::system_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
            int sleep_time = slot_duration - (ms.count() % slot_duration);
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));

            std::string message = std::to_string(offset) + "\n";
            send(sock, message.c_str(), message.length(), 0);

            char response[1024] = {0};
            read(sock, response, 1024);
            return (strcmp(response, "HUH!\n") != 0);
        }
        return false;
    }

    bool beb_send(int sock, int offset, int &backoff_counter)
    {
        int slot_duration = config["slot_duration"].get<int>();
        std::uniform_int_distribution<int> distribution(0, (1 << backoff_counter) - 1);
        int wait_time = distribution(generator) * slot_duration;
        std::this_thread::sleep_for(std::chrono::milliseconds(wait_time));

        std::string message = std::to_string(offset) + "\n";
        send(sock, message.c_str(), message.length(), 0);

        char response[1024] = {0};
        read(sock, response, 1024);
        if (strcmp(response, "HUH!\n") == 0)
        {
            backoff_counter = std::min(backoff_counter + 1, 10);
            return false;
        }
        backoff_counter = 0;
        return true;
    }

    bool sensing_beb_send(int sock, int offset, int &backoff_counter)
    {
        int slot_duration = config["slot_duration"].get<int>();
        while (true)
        {
            send(sock, "BUSY?\n", 6, 0);
            char response[1024] = {0};
            read(sock, response, 1024);
            if (strcmp(response, "IDLE\n") == 0)
            {
                std::string message = std::to_string(offset) + "\n";
                send(sock, message.c_str(), message.length(), 0);
                read(sock, response, 1024);
                if (strcmp(response, "HUH!\n") == 0)
                {
                    return beb_send(sock, offset, backoff_counter);
                }
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(slot_duration));
        }
    }

    void write_frequency()
    {
        std::string filename = "output_client_" + std::to_string(client_id) + ".txt";
        std::ofstream out(filename);

        for (const auto &pair : word_frequency)
        {
            out << pair.first << ", " << pair.second << "\n";
        }

        out.close();

        std::cout << "Client output written to " << filename << std::endl;
    }
    void run_client(int client_id)
    {
        auto start = std::chrono::high_resolution_clock::now();

        int sock = 0;
        if (!connect_to_server(sock))
        {
            return;
        }
    }
    void run()
    {
        auto start = std::chrono::high_resolution_clock::now();

        int sock = 0;
        if (!connect_to_server(sock))
        {
            return;
        }

        process_words(sock);
        close(sock);

        write_frequency();

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> diff = end - start;

        std::cout << "Client completed in " << diff.count() << " seconds" << std::endl;
    }
};

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <protocol>" << std::endl;
        return 1;
    }

    std::string protocol_str = argv[1];
    Protocol protocol;

    if (protocol_str == "aloha")
    {
        protocol = Protocol::SLOTTED_ALOHA;
    }
    else if (protocol_str == "beb")
    {
        protocol = Protocol::BEB;
    }
    else if (protocol_str == "sensing")
    {
        protocol = Protocol::SENSING_BEB;
    }
    else
    {
        std::cerr << "Invalid protocol. Use 'aloha', 'beb', or 'sensing'." << std::endl;
        return 1;
    }

    GrumpyClient client("config.json", protocol);
    client.run();
    return 0;
}