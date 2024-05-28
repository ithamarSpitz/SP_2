#include <iostream>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace std;

void error(const char* msg) {
    perror(msg);
    exit(1);
}

void startTCPServer(int port, int& client_socket) {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        error("socket failed");
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        error("setsockopt");
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        error("bind failed");
    }

    if (listen(server_fd, 3) < 0) {
        error("listen");
    }

    cout << "Listening on port " << port << "..." << endl;

    if ((client_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
        error("accept");
    }

    close(server_fd);
}

void startTCPClient(const string& hostname, int port, int& server_socket) {
    struct sockaddr_in serv_addr;
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        error("Socket creation error");
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, hostname.c_str(), &serv_addr.sin_addr) <= 0) {
        error("Invalid address/ Address not supported");
    }

    if (connect(server_socket, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        error("Connection Failed");
    }
}

void handleIO(int input_socket, int output_socket) {
    fd_set read_fds;
    char buffer[1024];
    int max_sd = max(input_socket, output_socket);
    while (true) {
        FD_ZERO(&read_fds);
        if (input_socket != -1) FD_SET(input_socket, &read_fds);
        if (output_socket != -1) FD_SET(output_socket, &read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        FD_SET(STDOUT_FILENO, &read_fds);
        
        int activity = select(max_sd + 1, &read_fds, NULL, NULL, NULL);
        if (activity < 0 && errno != EINTR) {
            error("select error");
        }

        if (input_socket != -1 && FD_ISSET(input_socket, &read_fds)) {
            int bytes_read = read(input_socket, buffer, sizeof(buffer));
            if (bytes_read <= 0) {
                if (bytes_read == 0) cerr << "Connection closed by peer" << endl;
                else perror("read");
                break;
            }
            buffer[bytes_read] = '\0';
            write(STDIN_FILENO, buffer, bytes_read);
        }

        if (output_socket != -1 && FD_ISSET(STDIN_FILENO, &read_fds)) {
            int bytes_read = read(STDIN_FILENO, buffer, sizeof(buffer));
            if (bytes_read <= 0) {
                if (bytes_read == 0) cerr << "EOF on stdin" << endl;
                else perror("read");
                break;
            }
            buffer[bytes_read] = '\0';
            send(output_socket, buffer, bytes_read, 0);
        }
    }
}

int main(int argc, char* argv[]) {
    bool execute_command = false;
    string command;
    int input_socket = -1, output_socket = -1;

    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "-e") {
            if (i + 1 < argc) {
                command = argv[++i];
                execute_command = true;
            } else {
                cerr << "Error: Missing command after -e" << endl;
                return 1;
            }
        } else if (arg.substr(0, 6) == "-iTCPS") {
            int port = stoi(arg.substr(6));
            startTCPServer(port, input_socket);
        } else if (arg.substr(0, 6) == "-oTCPC") {
            size_t comma_pos = arg.find(',');
            string hostname = arg.substr(6, comma_pos - 6);
            int port = stoi(arg.substr(comma_pos + 1));
            startTCPClient(hostname, port, output_socket);
        } else if (arg.substr(0, 6) == "-bTCPS") {
            int port = stoi(arg.substr(6));
            startTCPServer(port, input_socket);
            output_socket = input_socket;
        } else {
            cerr << "Error: Unknown argument " << arg.substr(0, 5) << endl;
            return 1;
        }
    }

    if (execute_command) {
        if (input_socket != -1) {
            dup2(input_socket, STDIN_FILENO);
        }
        if (output_socket != -1) {
            dup2(output_socket, STDOUT_FILENO);
            dup2(output_socket, STDERR_FILENO);
        }
        int result = system(command.c_str());
        if (input_socket != -1) close(input_socket);
        if (output_socket != -1 && output_socket != input_socket) close(output_socket);
        return result;
    } else {
        handleIO(input_socket, output_socket);
    }

    return 0;
}
