#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <fcntl.h>

void runCommand(const std::string& command) {
    char* args[3];
    args[0] = const_cast<char*>("/bin/sh");
    args[1] = const_cast<char*>("-c");
    args[2] = const_cast<char*>(command.c_str());
    args[3] = nullptr;
    execvp(args[0], args);
    perror("execvp");
}

void handleClient(int client_socket, const std::string& command, bool redirect_both) {
    int pipe_stdin[2];
    int pipe_stdout[2];

    if (pipe(pipe_stdin) == -1 || pipe(pipe_stdout) == -1) {
        perror("pipe");
        close(client_socket);
        exit(1);
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        close(client_socket);
        exit(1);
    }

    if (pid == 0) {  // Child process
        close(pipe_stdin[1]);
        close(pipe_stdout[0]);
        dup2(pipe_stdin[0], STDIN_FILENO);
        dup2(pipe_stdout[1], STDOUT_FILENO);
        if (redirect_both) {
            dup2(pipe_stdout[1], STDERR_FILENO);
        }
        runCommand(command);
        close(pipe_stdin[0]);
        close(pipe_stdout[1]);
        exit(0);
    } else {  // Parent process
        close(pipe_stdin[0]);
        close(pipe_stdout[1]);

        char buffer[1024];
        fd_set read_fds;
        int max_fd = std::max(client_socket, pipe_stdout[0]);

        while (true) {
            FD_ZERO(&read_fds);
            FD_SET(client_socket, &read_fds);
            FD_SET(pipe_stdout[0], &read_fds);

            int activity = select(max_fd + 1, &read_fds, nullptr, nullptr, nullptr);
            if (activity < 0) {
                perror("select");
                break;
            }

            if (FD_ISSET(client_socket, &read_fds)) {
                ssize_t bytes_read = read(client_socket, buffer, sizeof(buffer));
                if (bytes_read <= 0) {
                    break;
                }
                write(pipe_stdin[1], buffer, bytes_read);
            }

            if (FD_ISSET(pipe_stdout[0], &read_fds)) {
                ssize_t bytes_read = read(pipe_stdout[0], buffer, sizeof(buffer));
                if (bytes_read <= 0) {
                    break;
                }
                write(client_socket, buffer, bytes_read);
            }
        }

        close(pipe_stdin[1]);
        close(pipe_stdout[0]);
        close(client_socket);
        waitpid(pid, nullptr, 0);
    }
}

void tcpServer(int port, const std::string& command, bool redirect_both) {
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("socket");
        exit(1);
    }

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        close(server_socket);
        exit(1);
    }

    if (listen(server_socket, 3) == -1) {
        perror("listen");
        close(server_socket);
        exit(1);
    }

    while (true) {
        int client_socket = accept(server_socket, nullptr, nullptr);
        if (client_socket == -1) {
            perror("accept");
            continue;
        }
        handleClient(client_socket, command, redirect_both);
    }

    close(server_socket);
}

void tcpClient(const std::string& host, int port, const std::string& command, bool redirect_both) {
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("socket");
        exit(1);
    }

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host.c_str(), &server_addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(client_socket);
        exit(1);
    }

    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("connect");
        close(client_socket);
        exit(1);
    }

    handleClient(client_socket, command, redirect_both);
    close(client_socket);
}

int main(int argc, char* argv[]) {
    std::string command;
    std::string input;
    std::string output;
    std::string both;

    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "-e") {
            if (i + 1 < argc) {
                command = argv[i + 1];
                ++i;
            }
        } else if (std::string(argv[i]) == "-i") {
            if (i + 1 < argc) {
                input = argv[i + 1];
                ++i;
            }
        } else if (std::string(argv[i]) == "-o") {
            if (i + 1 < argc) {
                output = argv[i + 1];
                ++i;
            }
        } else if (std::string(argv[i]) == "-b") {
            if (i + 1 < argc) {
                both = argv[i + 1];
                ++i;
            }
        }
    }

    if (!command.empty()) {
        if (!input.empty() && input.substr(0, 4) == "TCPS") {
            int port = std::stoi(input.substr(4));
            tcpServer(port, command, false);
        } else if (!input.empty() && input.substr(0, 4) == "TCPC") {
            std::vector<std::string> parts;
            size_t pos = input.find(',');
            std::string host = input.substr(4, pos - 4);
            int port = std::stoi(input.substr(pos + 1));
            tcpClient(host, port, command, false);
        } else if (!output.empty() && output.substr(0, 4) == "TCPC") {
            std::vector<std::string> parts;
            size_t pos = output.find(',');
            std::string host = output.substr(4, pos - 4);
            int port = std::stoi(output.substr(pos + 1));
            tcpClient(host, port, command, true);
        } else if (!both.empty() && both.substr(0, 4) == "TCPS") {
            int port = std::stoi(both.substr(4));
            tcpServer(port, command, true);
        }
    } else {
        std::string line;
        while (std::getline(std::cin, line)) {
            std::cout << line << std::endl;
        }
    }

    return 0;
}
