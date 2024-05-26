#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <arpa/inet.h>
#include <cstdlib>
#include <sys/wait.h>

#define SERVER_PORT 7000
#define BUFFER_SIZE 1024

void error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int main() {
    int server_sockfd, client_sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    pid_t pid;

    // Create server socket
    if ((server_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        error("Socket creation failed");
    }
    std::cout << "Server socket created successfully" << std::endl;

    // Set socket options to reuse the address
    int opt = 1;
    if (setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        error("setsockopt failed");
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);

    // Bind the server socket
    if (bind(server_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        error("Bind failed");
    }
    std::cout << "Server socket bind successful" << std::endl;

    // Listen for incoming connections
    if (listen(server_sockfd, 5) < 0) {
        error("Listen failed");
    }
    std::cout << "Server listening on port " << SERVER_PORT << std::endl;

    client_len = sizeof(client_addr);
    // Accept a connection
    if ((client_sockfd = accept(server_sockfd, (struct sockaddr *)&client_addr, &client_len)) < 0) {
        error("Accept failed");
    }
    std::cout << "Client connection accepted" << std::endl;

    int pipe_stdin[2], pipe_stdout[2];

    if (pipe(pipe_stdin) == -1 || pipe(pipe_stdout) == -1) {
        error("Pipe failed");
    }
    std::cout << "Pipes created successfully" << std::endl;

    if ((pid = fork()) == -1) {
        error("Fork failed");
    }

    if (pid == 0) {
        // Child process
        std::cout << "In child process" << std::endl;
        close(pipe_stdin[1]);
        close(pipe_stdout[0]);

        dup2(pipe_stdin[0], STDIN_FILENO);
        dup2(pipe_stdout[1], STDOUT_FILENO);

        close(pipe_stdin[0]);
        close(pipe_stdout[1]);

        execl("./ttt", "ttt", "123456789", (char *)NULL);
        error("Exec failed");
    } else {
        // Parent process
        std::cout << "In parent process" << std::endl;
        close(pipe_stdin[0]);
        close(pipe_stdout[1]);

        char buffer[BUFFER_SIZE];
        ssize_t n;

        while (true) {
            // Read from the game's stdout
            std::cout << "Reading from game's stdout" << std::endl;
            n = read(pipe_stdout[0], buffer, sizeof(buffer) - 1);
            if (n > 0) {
                buffer[n] = '\0';
                std::cout << "Read from game's stdout: " << buffer << std::endl;
                // Send the game's output to the other side of the connection
                if (send(client_sockfd, buffer, n, 0) < 0) {
                    error("Socket send failed");
                }
                std::cout << "Sent to client: " << buffer << std::endl;
            } else if (n == 0) {
                // Game process terminated
                std::cout << "Game process terminated" << std::endl;
                break;
            }

            // Receive from the socket (other side's input)
            std::cout << "Waiting to receive from client" << std::endl;
            n = recv(client_sockfd, buffer, sizeof(buffer) - 1, 0);
            if (n > 0) {
                buffer[n] = '\0';
                std::cout << "Received from client: " << buffer << std::endl;
                // Send the input from the socket to the game's stdin
                if (write(pipe_stdin[1], buffer, n) < 0) {
                    error("Pipe write failed");
                }
                std::cout << "Sent to game's stdin: " << buffer << std::endl;
            }
        }

        close(pipe_stdin[1]);
        close(pipe_stdout[0]);
        close(client_sockfd);
        close(server_sockfd);
    }

    return 0;
}
