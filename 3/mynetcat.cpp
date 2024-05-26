#include <iostream>
#include <string>
#include <thread>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <csignal>
#include <sys/wait.h>

struct sockaddr_in addr(int port){
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    return server_addr;
}

// Function to handle TCP server input
void tcp_server_input(int port, int &client_sock) {
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr = addr(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_sock, 1);

    std::cout << "Listening for connections on port " << port << std::endl;
    client_sock = accept(server_sock, nullptr, nullptr);
    if (client_sock < 0) {
        std::cerr << "Failed to accept connection." << std::endl;
        exit(1);
    }
    close(server_sock);
}

// Function to handle TCP client output
void tcp_client_output(const std::string& hostname, int port, int &client_sock) {
    struct hostent *host = gethostbyname(hostname.c_str());
    if (!host) {
        std::cerr << "Failed to resolve hostname." << std::endl;
        exit(1);
    }

    client_sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr = addr(port);
    memcpy(&server_addr.sin_addr.s_addr, host->h_addr, host->h_length);

    if (connect(client_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Failed to connect to server." << std::endl;
        exit(1);
    }
}

// Function to handle TCP server for both input and output
void tcp_server_both(int port, int &client_sock) {
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr = addr(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_sock, 1);

    std::cout << "Listening for connections on port " << port << std::endl;
    client_sock = accept(server_sock, nullptr, nullptr);
    if (client_sock < 0) {
        std::cerr << "Failed to accept connection." << std::endl;
        exit(1);
    }
    close(server_sock);
}

// Function to handle TCP client for both input and output
void tcp_client_both(const std::string& hostname, int port, int &client_sock) {
    struct hostent *host = gethostbyname(hostname.c_str());
    if (!host) {
        std::cerr << "Failed to resolve hostname." << std::endl;
        exit(1);
    }

    client_sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr = addr(port);
    memcpy(&server_addr.sin_addr.s_addr, host->h_addr, host->h_length);

    if (connect(client_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Failed to connect to server." << std::endl;
        exit(1);
    }
}

// Function to redirect input from a socket to the child process
void redirect_input(int input_sock, int child_stdin) {
    char buffer[1024];
    while (true) {
        ssize_t bytes_read = read(input_sock, buffer, sizeof(buffer));
        if (bytes_read <= 0) {
            std::cerr << "Connection closed or error." << std::endl;
            break;
        }
        ssize_t bytes_written = write(child_stdin, buffer, bytes_read);
        if (bytes_written <= 0) {
            std::cerr << "Failed to write to child stdin." << std::endl;
            break;
        }
    }
    close(input_sock);
    exit(1);
}

// Function to redirect output from the child process to a socket
void redirect_output(int output_sock, int child_stdout) {
    char buffer[1024];
    while (true) {
        ssize_t bytes_read = read(child_stdout, buffer, sizeof(buffer));
        if (bytes_read <= 0) {
            std::cerr << "Failed to read from child stdout or child closed stdout." << std::endl;
            break;
        }
        ssize_t bytes_written = write(output_sock, buffer, bytes_read);
        if (bytes_written <= 0) {
            std::cerr << "Failed to write to socket." << std::endl;
            break;
        }
    }
    close(output_sock);
    exit(1);
}

// Function to handle both input and output redirection using a single socket
void redirect_both_io(int sock, int child_stdin, int child_stdout) {
    std::thread input_thread(redirect_input, sock, child_stdin);
    std::thread output_thread(redirect_output, sock, child_stdout);

    input_thread.join();
    output_thread.join();

    close(sock);
    exit(1);
}

int main(int argc, char* argv[]) {
    if (argc < 5) {
        std::cerr << "Usage: mync -e <command> (-i <input> | -o <output> | -b <both>)" << std::endl;
        return 1;
    }

    std::string command, input_param, output_param, both_param;
    int sock = -1;
    int pipe_stdin[2], pipe_stdout[2];

    if (pipe(pipe_stdin) == -1 || pipe(pipe_stdout) == -1) {
        std::cerr << "Failed to create pipes." << std::endl;
        return 1;
    }

    pid_t pid = fork();
    if (pid == -1) {
        std::cerr << "Failed to fork process." << std::endl;
        return 1;
    } else if (pid == 0) {
        // Child process
        close(pipe_stdin[1]);  // Close write end of stdin pipe
        close(pipe_stdout[0]); // Close read end of stdout pipe

        dup2(pipe_stdin[0], STDIN_FILENO);  // Redirect stdin to read end of stdin pipe
        dup2(pipe_stdout[1], STDOUT_FILENO); // Redirect stdout to write end of stdout pipe
        dup2(pipe_stdout[1], STDERR_FILENO); // Redirect stderr to write end of stdout pipe

        execl("/bin/sh", "sh", "-c", command.c_str(), nullptr);
        _exit(127); // Exec failed
    } else {
        // Parent process
        close(pipe_stdin[0]);  // Close read end of stdin pipe
        close(pipe_stdout[1]); // Close write end of stdout pipe

        for (int i = 1; i < argc; ++i) {
            if (strcmp(argv[i], "-e") == 0 && i + 1 < argc) {
                command = argv[++i];
            } else if (strcmp(argv[i], "-i") == 0 && i + 1 < argc) {
                input_param = argv[++i];
            } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
                output_param = argv[++i];
            } else if (strcmp(argv[i], "-b") == 0 && i + 1 < argc) {
                both_param = argv[++i];
            }
        }

        if (!both_param.empty()) {
            if (both_param.substr(0, 4) == "TCPS") {
                int port = std::stoi(both_param.substr(4));
                std::thread both_thread(tcp_server_both, port, std::ref(sock));
                both_thread.join();
            } else if (both_param.substr(0, 4) == "TCPC") {
                auto pos = both_param.find(',');
                std::string hostname = both_param.substr(4, pos - 4);
                int port = std::stoi(both_param.substr(pos + 1));
                std::thread both_thread(tcp_client_both, hostname, port, std::ref(sock));
                both_thread.join();
            }
            if (sock != -1) {
                std::thread io_thread(redirect_both_io, sock, pipe_stdin[1], pipe_stdout[0]);
                io_thread.join();
            }
        } else if (!input_param.empty()) {
            if (input_param.substr(0, 4) == "TCPS") {
                int port = std::stoi(input_param.substr(4));
                std::thread input_thread(tcp_server_input, port, std::ref(sock));
                input_thread.join();
            } else if (input_param.substr(0, 4) == "TCPC") {
                auto pos = input_param.find(',');
                std::string hostname = input_param.substr(4, pos - 4);
                int port = std::stoi(input_param.substr(pos + 1));
                std::thread input_thread(tcp_client_output, hostname, port, std::ref(sock));
                input_thread.join();
            }
            if (sock != -1) {
                std::thread io_thread(redirect_input, sock, pipe_stdin[1]);
                io_thread.join();
            }
        } else if (!output_param.empty()) {
            if (output_param.substr(0, 4) == "TCPS") {
                int port = std::stoi(output_param.substr(4));
                std::thread output_thread(tcp_server_input, port, std::ref(sock));
                output_thread.join();
            } else if (output_param.substr(0, 4) == "TCPC") {
                auto pos = output_param.find(',');
                std::string hostname = output_param.substr(4, pos - 4);
                int port = std::stoi(output_param.substr(pos + 1));
                std::thread output_thread(tcp_client_output, hostname, port, std::ref(sock));
                output_thread.join();
            }
            if (sock != -1) {
                std::thread io_thread(redirect_output, sock, pipe_stdout[0]);
                io_thread.join();
            }
        }

        int status;
        waitpid(pid, &status, 0);
    }

    return 0;
}
