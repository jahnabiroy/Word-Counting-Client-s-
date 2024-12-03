#include <iostream>
#include <pthread.h>
#include <unistd.h>
#include <string>
#include <queue>
#include <fstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sstream>
#include <signal.h>
#define PORT 8080
#define MAX_CLIENTS 10
#define WORDS_PER_PACKET 2

struct ServerStatus
{
    bool busy;
    int socket_id;
    time_t start_time;
    time_t last_concurrent_request;
};

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
ServerStatus server_status = {false, -1, 0, 0};

void handle_sigpipe(int sig)
{
    std::cerr << "Caught SIGPIPE signal " << sig << "\n";
}

void *handle_client(void *arg)
{
    int client_socket = *(int *)arg;
    delete (int *)arg;

    while (true)
    {
        char buffer[1024] = {0};
        int valread = read(client_socket, buffer, 1024);
        if (valread <= 0)
        {
            std::cerr << "Client disconnected or read error\n";
            close(client_socket);
            return nullptr;
        }
        std::string message(buffer, valread);

        pthread_mutex_lock(&mutex);
        std::cout << "Received message from client " << client_socket << ": " << message << std::endl;

        // if (message == "BUSY?\n") {
        //     std::string response = server_status.busy ? "BUSY\n" : "IDLE\n";
        //     send(client_socket, response.c_str(), response.size(), 0);
        //     pthread_mutex_unlock(&mutex);
        //     continue;
        // }

        if (server_status.busy)
        {
            std::string huh_message = "HUH!\n";
            send(client_socket, huh_message.c_str(), huh_message.size(), 0);
            pthread_cond_broadcast(&cond);
            pthread_mutex_unlock(&mutex);
            usleep(100000); // 100 ms
            continue;
        }
        server_status.busy = true;
        server_status.socket_id = client_socket;
        server_status.start_time = time(nullptr);
        pthread_mutex_unlock(&mutex);

        // Simulate request processing
        sleep(1);

        // Read from the memory-mapped file
        int fd = open("word.txt", O_RDONLY);
        if (fd == -1)
        {
            std::cerr << "Unable to open file\n";
            close(client_socket);
            return nullptr;
        }
        struct stat sb;
        fstat(fd, &sb);
        char *file_content = (char *)mmap(nullptr, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        close(fd);

        if (file_content == MAP_FAILED)
        {
            std::cerr << "Memory mapping failed\n";
            close(client_socket);
            return nullptr;
        }

        // Simulate sending data to the client
        std::string data(file_content, sb.st_size);
        munmap(file_content, sb.st_size);

        // Split data by commas
        std::istringstream ss(data);
        std::string word;
        std::vector<std::string> words;
        while (std::getline(ss, word, ','))
        {
            words.push_back(word);
        }

        size_t offset = 0;
        while (offset < words.size())
        {
            std::string packet;
            for (size_t i = 0; i < WORDS_PER_PACKET && offset < words.size(); ++i, ++offset)
            {
                packet += words[offset];
                packet += ",";
            }
            printf("Packet to Client %d: %s\n", client_socket, packet.c_str());
            if (send(client_socket, packet.c_str(), packet.size(), 0) == -1)
            {
                std::cerr << "Error sending data to client\n";
                break;
            }
            usleep(50000); // 50 ms
        }

        close(client_socket);

        pthread_mutex_lock(&mutex);
        server_status.busy = false;
        std::cout << "Done serving Client " << client_socket << "\n";
        pthread_cond_broadcast(&cond);
        pthread_mutex_unlock(&mutex);

        return nullptr;
    }
}

int main()
{
    // Set up the SIGPIPE signal handler
    struct sigaction sa;
    sa.sa_handler = handle_sigpipe;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGPIPE, &sa, NULL);

    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        std::cerr << "Socket failed\n";
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        std::cerr << "Bind failed\n";
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, MAX_CLIENTS) < 0)
    {
        std::cerr << "Listen failed\n";
        exit(EXIT_FAILURE);
    }

    pthread_t threads[MAX_CLIENTS];
    int i = 0;
    while (i < MAX_CLIENTS)
    {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
        {
            std::cerr << "Accept failed\n";
            exit(EXIT_FAILURE);
        }
        int *client_socket = new int(new_socket);
        pthread_create(&threads[i], nullptr, handle_client, client_socket);
        i++;
    }

    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        pthread_join(threads[i], nullptr);
    }

    return 0;
}