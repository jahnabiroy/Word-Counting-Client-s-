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
#define PORT 8080
#define MAX_CLIENTS 10
#define WORDS_PER_PACKET 1

struct ServerStatus {
    bool busy;
    int socket_id;
    time_t start_time;
    time_t last_concurrent_request;
};

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
ServerStatus server_status = {false, -1, 0, 0};

void* handle_client(void* arg) {
    int client_socket = *(int*)arg;
    delete (int*)arg;

    while (true) {
        pthread_mutex_lock(&mutex);
        if (server_status.busy) {
            // Send "HUH!" message to client
            std::string huh_message = "HUH!";
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
        if (fd == -1) {
            std::cerr << "Unable to open file\n";
            close(client_socket);
            return nullptr;
        }
        struct stat sb;
        fstat(fd, &sb);
        char* file_content = (char*)mmap(nullptr, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        close(fd);

        if (file_content == MAP_FAILED) {
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
        while (std::getline(ss, word, ',')) {
            words.push_back(word);
        }

        // Send data in packets
        size_t offset = 0;
        while (offset < words.size()) {
            std::string packet;
            for (size_t i = 0; i < WORDS_PER_PACKET && offset < words.size(); ++i, ++offset) {
                if (i > 0) packet += ",";
                packet += words[offset];
            }
            send(client_socket, packet.c_str(), packet.size(), 0);
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

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        std::cerr << "Socket failed\n";
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Bind failed\n";
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, MAX_CLIENTS) < 0) {
        std::cerr << "Listen failed\n";
        exit(EXIT_FAILURE);
    }

    pthread_t threads[MAX_CLIENTS];
    int i = 0;
    while (i < MAX_CLIENTS) {
        if ((new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
            std::cerr << "Accept failed\n";
            exit(EXIT_FAILURE);
        }
        int* client_socket = new int(new_socket);
        pthread_create(&threads[i], nullptr, handle_client, client_socket);
        i++;
    }

    for (int i = 0; i < MAX_CLIENTS; ++i) {
        pthread_join(threads[i], nullptr);
    }

    return 0;
}
