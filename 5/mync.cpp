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
#include <sys/select.h>

#define SERVER_PORT 7000
#define BUFFER_SIZE 1024
using namespace std;

void error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

struct sockaddr_in addr(string port, int* sockfd, bool is_server, bool is_udp) {
    struct sockaddr_in server_addr;
    // Create socket
    if (*sockfd != -1 && (*sockfd = socket(AF_INET, is_udp ? SOCK_DGRAM : SOCK_STREAM, 0)) < 0) {
        error("Socket creation failed");
    }
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = is_server ? INADDR_ANY : inet_addr("127.0.0.1"); // Assuming server is on the same machine
    server_addr.sin_port = htons(stoi(port));
    return server_addr;
}

// Server

int createTCPServer(string port) {
    int server_sockfd;
    struct sockaddr_in server_addr = addr(port, &server_sockfd, true, false);
    // Set socket options to reuse the address
    int opt = 1;
    if (setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        error("setsockopt failed");
    }
    // Bind the server socket
    if (bind(server_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        error("Bind failed");
    }
    // Listen for incoming connections
    if (listen(server_sockfd, 5) < 0) {
        error("Listen failed");
    }
    return server_sockfd;
}

int createUDPServer(string port) {
    int server_sockfd;
    struct sockaddr_in server_addr = addr(port, &server_sockfd, true, true);
    // Bind the server socket
    if (bind(server_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        error("Bind failed");
    }
    return server_sockfd;
}

int getClientSock(int server_sockfd, string client_port) {
    int client_sockfd;
    struct sockaddr_in client_addr;
    socklen_t client_len;
    client_len = sizeof(client_addr);
    // Accept a connection
    if ((client_sockfd = accept(server_sockfd, (struct sockaddr *)&client_addr, &client_len)) < 0) {
        error("Accept failed");
    }
    // Convert the expected port to an integer
    int expected_port = (client_port.empty()) ? 0 : std::stoi(client_port);

    // Convert the received port to host byte order
    int received_port = ntohs(client_addr.sin_port);

    if (expected_port != 0 && received_port != expected_port) {
        std::string msg = "Received bad port: " + std::to_string(received_port);
        error(msg.c_str());
    }
    return client_sockfd;
}

void childP(int pipe_stdin[2], int pipe_stdout[2], string command) {
    // Child process
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

void game2client(char buffer[BUFFER_SIZE], int n, int client_sockfd, bool print_out) {
    buffer[n] = '\0';
    // Send the game's output to the other side of the connection
    if (send(client_sockfd, buffer, n, 0) < 0) {
        error("Socket send failed");
    }
    if (print_out) {
        cout << buffer << endl;
    }
}

void client2game(int pipe_stdin[2], int net_num) {
    int num = ntohl(net_num); // Convert from network byte order to host byte order
    string move_str = to_string(num) + "\n";
    if (write(pipe_stdin[1], move_str.c_str(), move_str.size()) < 0) {
        error("Pipe write failed");
    }
}

void handleTimeout(int signum) {
    error("Timeout occurred!");
}

void parentP(int pipe_stdin[2], int pipe_stdout[2], int parent_sock, int server_send_sock, int server_recv_sock, bool print_out, bool udp_recv, string client_port, int timeout) {
    // Parent process
    close(pipe_stdin[0]);
    close(pipe_stdout[1]);

    char buffer[BUFFER_SIZE];
    ssize_t n;
    while (true) {
        n = read(pipe_stdout[0], buffer, sizeof(buffer) - 1);
        if (n > 0) {
            game2client(buffer, n, server_send_sock, print_out);
        } else if (n == 0) {
            break;
        }

        // Receive from the socket (other side's input)
        int net_num;
        if (udp_recv) {
            int p = -1;
            struct sockaddr_in cliAddr;
            int len;
            // Set up the alarm and signal handler for timeout
            signal(SIGALRM, handleTimeout);
            alarm(timeout);

            n = recvfrom(server_recv_sock, &net_num, sizeof(net_num), 0, (struct sockaddr*)&cliAddr, (socklen_t*)&len);

            // Disable the alarm after recvfrom completes
            alarm(0);
            int num = ntohl(net_num); // Convert from network byte order to host byte order
            string move_str = to_string(num) + "\n";
            char *cstr = new char[move_str.size() + 1];
            strcpy(cstr, move_str.c_str());
            //tcp works, but udp doesnt get to receiving anything
        } else {
            n = recv(server_recv_sock, &net_num, sizeof(net_num), 0);
        }
        if (n > 0) {
            client2game(pipe_stdin, net_num);
        }
    }
    close(pipe_stdin[1]);
    close(pipe_stdout[0]);
    if (server_recv_sock != server_send_sock) {
        close(server_recv_sock);
    }
    close(server_send_sock);
    close(parent_sock);
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
        childP(pipe_stdin, pipe_stdout, command);
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
    cout << " mux" << endl;
    int server_sockfd = createTCPServer(port);
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

void server(string server_port, string command, string client_port, bool print_out, bool udp_recv, int timeout, bool io_mux) {
    if (io_mux) {
        server_tcp_mux(server_port, command);
    } else {
        pid_t pid;
        int server_recv_sock;
        int parent_sock = createTCPServer(server_port);
        int server_send_sock = getClientSock(parent_sock, client_port);
        if (udp_recv) {
            server_recv_sock = createUDPServer(server_port);
        } else {
            server_recv_sock = server_send_sock;
        }
        int pipe_stdin[2], pipe_stdout[2];
        if (pipe(pipe_stdin) == -1 || pipe(pipe_stdout) == -1) {
            error("Pipe failed");
        }
        if ((pid = fork()) == -1) {
            error("Fork failed");
        }
        if (pid == 0) {
            childP(pipe_stdin, pipe_stdout, command);
        } else {
            parentP(pipe_stdin, pipe_stdout, parent_sock, server_send_sock, server_recv_sock, print_out, udp_recv, client_port, timeout);
        }
    }
}

// Client

void send2server(string buf, int sockfd, bool udp, string server_port) {
    string sub1 = "D";
    string sub2 = "I";
    string user_input;
    if (buf.find(sub1) != string::npos || buf.find(sub2) != string::npos) {
        user_input = "-1";
    } else {
        // Send user input to the server (game's input)
        getline(cin, user_input);
    }
    int num = stoi(user_input);
    int net_num = htonl(num); // Convert to network byte order
    if (udp) {
        int p = -1;
        struct sockaddr_in server_addr = addr(server_port, &p, true, true);
        sendto(sockfd, &net_num, sizeof(net_num), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
    } else {
        send(sockfd, &net_num, sizeof(net_num), 0);
    }
}

int createTCPClient(string server_port, string client_port) {
    int sockfd;
    int p = -1;
    struct sockaddr_in server_addr = addr(server_port, &p, false, false);
    if (client_port != "") {
        struct sockaddr_in client_addr = addr(client_port, &sockfd, true, false);
        if (bind(sockfd, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
            error("Bind failed");
        }
    }

    // Set socket options to reuse the address
    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        error("setsockopt failed");
    }
    // Connect to server
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        error("Connect failed");
    }
    return sockfd;
}

int createUDPClient(string client_port) {
    int sockfd;
    if (client_port != "") {
        struct sockaddr_in client_addr = addr(client_port, &sockfd, true, true);
        if (bind(sockfd, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
            error("Bind failed");
        }
    }
    return sockfd;
}

void sendNrecv(int sock_send, int sock_recv, bool udp_send, string server_port) {
    char buffer[BUFFER_SIZE];
    while (true) {
        // Receive message from the server (game's output)
        ssize_t n;
        n = recv(sock_recv, buffer, sizeof(buffer) - 1, 0);
        if (n > 0) {
            buffer[n] = '\0';
            cout << buffer;
            string buf(buffer);
            send2server(buf, sock_send, udp_send, server_port);
        } else if (n == 0) {
            cout << "Connection closed by server." << endl;
            break;
        } else {
            error("recv failed");
        }
    }
}

void client(string server_port, string client_port, bool udp_send) {
    int sock_send;
    int sock_recv;
    sock_recv = createTCPClient(server_port, client_port);
    if (udp_send) {
        sock_send = createUDPClient("9005");
    } else {
        sock_send = sock_recv;
    }
    sendNrecv(sock_send, sock_recv, udp_send, server_port);
    if (sock_recv != sock_send) {
        close(sock_send);
    }
    close(sock_recv);
}

int main(int argc, char* argv[]) {
    bool is_server = false, print_out = true;
    bool udp_recv = false, udp_send = false, io_mux = false;
    string server_port = "8000", command, client_port = "9000";
    int timeout = 0;
    for (int i = 1; i < argc - 1; i += 2) {
        std::string cur_arg(argv[i]);
        std::string next_arg(argv[i + 1]);
        if (cur_arg == "-e") {
            is_server = true;
            command = next_arg;
        }
        if (cur_arg == "-i" || cur_arg == "-b") {
            if (next_arg.find("UDP") != string::npos) {
                udp_recv = true;
            }
            server_port = next_arg.substr(next_arg.size()-4);
        }
        if (cur_arg == "-b" || cur_arg == "-o") {
            if (next_arg.find("UDP") != string::npos) {
                udp_send = true;
            }
            if (next_arg.find("TCP") != string::npos) {
                server_port = next_arg.substr(next_arg.size()-4);
            }  

            print_out = false;
        }
        if (cur_arg == "-o") {
            if (next_arg.size() >= 4) {
                if (is_server) {
                    client_port = next_arg.substr(next_arg.size() - 4);
                } else {
                    server_port = next_arg.substr(next_arg.size() - 4);
                }
            } else {
                error("cant read specific client port");
            }
        }
        if (cur_arg == "-t") {
            timeout = stoi(next_arg);
        }
        if (next_arg.find("MUX") != string::npos) {
                io_mux = true;
        }
    }
    if (is_server) {
        server(server_port, command, client_port, print_out, udp_recv, timeout, io_mux);
    } else {
        client(server_port, client_port, udp_send);
    }
    return 0;
}
