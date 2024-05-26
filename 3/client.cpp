// client.cpp
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <cstdlib>

#define SERVER_PORT 7000
#define BUFFER_SIZE 1024

void error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int main() {
    int sockfd;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];

    // Create socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        error("Socket creation failed");
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // Assuming server is on the same machine
    server_addr.sin_port = htons(SERVER_PORT);

    // Connect to server
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        error("Connect failed");
    }

    while (true) {
        // Receive message from the server (game's output)
        ssize_t n = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
        if (n > 0) {
            buffer[n] = '\0';
            std::cout << "Game says: " << buffer << std::endl;

            // Send user input to the server (game's input)
            std::string user_input;
            std::cout << "Your move: ";
            std::getline(std::cin, user_input);
            send(sockfd, user_input.c_str(), user_input.length(), 0);
            std::cout << "sent to server!\n";

        } else if (n == 0) {
            std::cout << "Connection closed by server." << std::endl;
            break;
        } else {
            error("recv failed");
        }
    }

    close(sockfd);
    return 0;
}
