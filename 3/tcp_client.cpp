#include <iostream>
#include <string>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <cstring>

void tcp_client(const std::string& hostname, int port) {
    struct hostent *host = gethostbyname(hostname.c_str());
    if (!host) {
        std::cerr << "Failed to resolve hostname." << std::endl;
        return;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    memcpy(&server_addr.sin_addr.s_addr, host->h_addr, host->h_length);

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Failed to connect to server." << std::endl;
        return;
    }

    char buffer[1024];
    std::string input;
    while (true) {
        std::cout << "> ";
        std::getline(std::cin, input);
        if (input == "exit") break;
        write(sock, input.c_str(), input.size());
        ssize_t bytes_read = read(sock, buffer, sizeof(buffer));
        if (bytes_read <= 0) break;
        buffer[bytes_read] = '\0';
        std::cout << buffer << std::endl;
    }

    close(sock);
}

int main() {
    std::string hostname = "localhost";
    int port = 4051;
    tcp_client(hostname, port);
    return 0;
}
