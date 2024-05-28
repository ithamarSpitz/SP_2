#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <arpa/inet.h>
#include <cstdlib>
#include <sys/wait.h>
#include <csignal>

#define BUFFER_SIZE 1024

void error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int main(int argc, char* argv[]){
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <port>\n";
        return 1;
    }
    int server_sockfd, client_sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    pid_t read_pid, write_pid;

    // Create server socket
    if ((server_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        error("Socket creation failed");
    }

    // Set socket options to reuse the address
    int opt = 1;
    if (setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        error("setsockopt failed");
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(std::stoi(argv[1]));

    // Bind the server socket
    if (bind(server_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        error("Bind failed");
    }

    // Listen for incoming connections
    if (listen(server_sockfd, 5) < 0) {
        error("Listen failed");
    }

    client_len = sizeof(client_addr);
    // Accept a connection
    if ((client_sockfd = accept(server_sockfd, (struct sockaddr *)&client_addr, &client_len)) < 0) {
        error("Accept failed");
    }

    int pipe_stdin[2], pipe_stdout[2];

    if (pipe(pipe_stdin) == -1 || pipe(pipe_stdout) == -1) {
        error("Pipe failed");
    }

    if ((read_pid = fork()) == -1) {
        error("Fork for reading failed");
    }

    if (read_pid == 0) {
        // Child process for reading from game's stdout
        close(pipe_stdin[1]);
        close(pipe_stdout[1]);

        char buffer[BUFFER_SIZE];
        ssize_t n;

        while (true) {
            // Read from the game's stdout
            n = read(pipe_stdout[0], buffer, sizeof(buffer) - 1);
            if (n > 0) {
                buffer[n] = '\0';
                // Send the game's output to the other side of the connection
                if (send(client_sockfd, buffer, n, 0) < 0) {
                    error("Socket send failed");
                }
            }
        }

        close(pipe_stdout[0]);
        close(client_sockfd);
        close(server_sockfd);
        exit(EXIT_SUCCESS);

    } else {
        if ((write_pid = fork()) == -1) {
            error("Fork for writing failed");
        }

        if (write_pid == 0) {
            // Child process for writing to game's stdin
            close(pipe_stdin[0]);
            close(pipe_stdout[0]);
            close(pipe_stdout[1]);

            char buffer[BUFFER_SIZE];
            ssize_t n;

            // while ((n = recv(client_sockfd, buffer, sizeof(buffer), 0)) > 0) {
            //     std::cout << "Received data from client: "<< buffer << std::endl;
            //     // Ensure the received data is null-terminated before printing
            //     buffer[n] = '\n';
            //     buffer[n+1] = '\0';
            //     std::cout << buffer << std::endl;

            //     // Write the received data to the game's stdin
            //     if (write(pipe_stdin[1], buffer, n) < 0) {
            //         error("Pipe write failed");
            //     }
            // }

            while (true) {
            // Read from the game's stdout
            n = recv(client_sockfd, buffer, sizeof(buffer), 0);
            if (n > 0) {
                buffer[n] = '\0';
                char buf[BUFFER_SIZE];
                memset( buffer, 0x00, BUFFER_SIZE );
                // buf[0]='2';
                // buf[1] = '\n';
                // buf[2] = '\0';
                // Send the gamememset( buffer, 0x00, BUFFER_SIZE );'s output to the other side of the connection
                    printf("Received data: %c%c\n", buffer[0], buffer[1]); // Print out received data

                send(pipe_stdin[1], buffer, 2,0);
                // if (write(pipe_stdin[1], buf, 2) < 0) {
                //     error("Pipe write failed");
                // }
            }
        }
            

            if (n < 0) {
                error("Socket receive failed");
            }

            close(pipe_stdin[1]);
            close(client_sockfd);
            close(server_sockfd);
            exit(EXIT_SUCCESS);

        } else {
            // Parent process
            close(pipe_stdin[0]);
            close(pipe_stdout[0]);

            dup2(pipe_stdin[1], STDIN_FILENO);
            dup2(pipe_stdout[1], STDOUT_FILENO);

            close(pipe_stdin[1]);
            close(pipe_stdout[1]);

            execl("./ttt","ttt", "123456789", (char *)NULL);
            error("Exec failed");
        }
    }

    close(client_sockfd);
    close(server_sockfd);

    waitpid(read_pid, NULL, 0);
    waitpid(write_pid, NULL, 0);

    return 0;
}
