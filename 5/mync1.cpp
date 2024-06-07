#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <cstdlib>
#include <string>
#include <sys/wait.h>
#include <ctime>
#include <signal.h>
#include <vector>
#include <fcntl.h>

#define BUFFER_SIZE 1024
using namespace std;

void error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

struct sockaddr_in create_addr(const string &port, bool is_server) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = is_server ? INADDR_ANY : inet_addr("127.0.0.1");
    addr.sin_port = htons(stoi(port));
    return addr;
}

int create_server_socket(const string &port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        error("Socket creation failed");
    }
    struct sockaddr_in addr = create_addr(port, true);

    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        error("setsockopt failed");
    }

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        error("Bind failed");
    }

    if (listen(sockfd, 5) < 0) {
        error("Listen failed");
    }
    return sockfd;
}

void child_process(int pipe_stdin[2], int pipe_stdout[2], const string &command) {
    close(pipe_stdin[1]);
    close(pipe_stdout[0]);

    dup2(pipe_stdin[0], STDIN_FILENO);
    dup2(pipe_stdout[1], STDOUT_FILENO);

    close(pipe_stdin[0]);
    close(pipe_stdout[1]);

    int spacePos = command.find(' ');
    if (spacePos != string::npos) {
        string firstPart = command.substr(0, spacePos);
        string secondPart = command.substr(spacePos + 1);

        execl(firstPart.c_str(), firstPart.c_str(), secondPart.c_str(), (char *)NULL);
        error("Exec failed");
    } else {
        error("wrong command");
    }
}

void handle_client(int client_sockfd, const string &command) {
    pid_t pid;
    int pipe_stdin[2], pipe_stdout[2];
    if (pipe(pipe_stdin) == -1 || pipe(pipe_stdout) == -1) {
        error("Pipe failed");
    }
    if ((pid = fork()) == -1) {
        error("Fork failed");
    }
    if (pid == 0) {
        child_process(pipe_stdin, pipe_stdout, command);
    } else {
        close(pipe_stdin[0]);
        close(pipe_stdout[1]);

        char buffer[BUFFER_SIZE];
        ssize_t n;

        while (true) {
            n = read(pipe_stdout[0], buffer, sizeof(buffer) - 1);
            if (n > 0) {
                buffer[n] = '\0';
                if (send(client_sockfd, buffer, n, 0) < 0) {
                    error("Socket send failed");
                }
            } else if (n == 0) {
                break;
            }

            n = recv(client_sockfd, buffer, sizeof(buffer) - 1, 0);
            if (n > 0) {
                buffer[n] = '\0';
                if (write(pipe_stdin[1], buffer, n) < 0) {
                    error("Pipe write failed");
                }
            }
        }

        close(pipe_stdin[1]);
        close(pipe_stdout[0]);
        close(client_sockfd);
    }
}

void server_tcp_mux(const string &port, const string &command) {
    int server_sockfd = create_server_socket(port);
    vector<int> client_sockets;
    fd_set readfds;

    while (true) {
        FD_ZERO(&readfds);
        FD_SET(server_sockfd, &readfds);
        int max_sd = server_sockfd;

        for (int client_sock : client_sockets) {
            FD_SET(client_sock, &readfds);
            if (client_sock > max_sd) {
                max_sd = client_sock;
            }
        }

        int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0 && errno != EINTR) {
            error("select error");
        }

        if (FD_ISSET(server_sockfd, &readfds)) {
            int client_sockfd;
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            if ((client_sockfd = accept(server_sockfd, (struct sockaddr *)&client_addr, &client_len)) < 0) {
                error("accept error");
            }

            client_sockets.push_back(client_sockfd);
            if (fork() == 0) {
                handle_client(client_sockfd, command);
                exit(0);
            }
        }

        for (auto it = client_sockets.begin(); it != client_sockets.end();) {
            if (FD_ISSET(*it, &readfds)) {
                char buffer[BUFFER_SIZE];
                int valread = read(*it, buffer, sizeof(buffer));
                if (valread == 0) {
                    close(*it);
                    it = client_sockets.erase(it);
                } else {
                    ++it;
                }
            } else {
                ++it;
            }
        }
    }
}

int main(int argc, char* argv[]) {
    bool is_server = false;
    string server_port = "7000", command;

    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "-e" && i + 1 < argc) {
            is_server = true;
            command = argv[++i];
        } else if (arg.substr(0, 7) == "TCPMUXS") {
            server_port = arg.substr(7);
            cout << server_port << endl;

        }

        
    }

    if (is_server) {
        server_tcp_mux(server_port, command);
    } else {
        cout << "Client mode not implemented for TCPMUXS" << endl;
    }

    return 0;
}
