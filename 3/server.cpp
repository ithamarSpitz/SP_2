//server
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
using namespace std;

void error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

struct sockaddr_in addr(string port, int* sockfd, bool is_server){
    struct sockaddr_in server_addr;
    // Create socket
    if ((*sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        error("Socket creation failed");
    }
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = is_server ? INADDR_ANY : inet_addr("127.0.0.1"); // Assuming server is on the same machine
    server_addr.sin_port = htons(stoi(port));
    return server_addr;
}

int createServer(){
    int server_sockfd;
    struct sockaddr_in server_addr = addr("7000", &server_sockfd, true);
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

int getClientSock(int server_sockfd){
    int client_sockfd;
    struct sockaddr_in client_addr;
    socklen_t client_len;
    client_len = sizeof(client_addr);
    // Accept a connection
    if ((client_sockfd = accept(server_sockfd, (struct sockaddr *)&client_addr, &client_len)) < 0) {
        error("Accept failed");
    }
    return client_sockfd;
}

void childP(int pipe_stdin[2],int pipe_stdout[2]){
        // Child process
        close(pipe_stdin[1]);
        close(pipe_stdout[0]);

        dup2(pipe_stdin[0], STDIN_FILENO);
        dup2(pipe_stdout[1], STDOUT_FILENO);

        close(pipe_stdin[0]);
        close(pipe_stdout[1]);

        execl("./ttt", "ttt", "123456789", (char *)NULL);
        error("Exec failed");
}

void game2client(char buffer[BUFFER_SIZE], int n, int client_sockfd){
        buffer[n] = '\0';
        std::cout << "Read from game's stdout:\n" << buffer << std::endl;
        // Send the game's output to the other side of the connection
        if (send(client_sockfd, buffer, n, 0) < 0) {
            error("Socket send failed");
        }
        std::cout << "Sent to client: " << buffer << std::endl;
}

void client2game(int pipe_stdin[2], int net_num){
    int num = ntohl(net_num); // Convert from network byte order to host byte order
        std::cout << "Received from client: " << num << std::endl;

        std::string move_str = std::to_string(num) + "\n";
        if (write(pipe_stdin[1], move_str.c_str(), move_str.size()) < 0) {
            error("Pipe write failed");
        }
        std::cout << "Sent to game's stdin: " << move_str << std::endl;
    
}

void parentP(int pipe_stdin[2], int pipe_stdout[2], int client_sockfd, int server_sockfd){
// Parent process
        close(pipe_stdin[0]);
        close(pipe_stdout[1]);

        char buffer[BUFFER_SIZE];
        ssize_t n;
        while (true) {
            n = read(pipe_stdout[0], buffer, sizeof(buffer) - 1);
            if (n > 0) {
                game2client(buffer, n, client_sockfd);
            } else if (n == 0) {
                break;
            }

            // Receive from the socket (other side's input)
            int net_num;
            n = recv(client_sockfd, &net_num, sizeof(net_num), 0);
            if (n > 0) {
                client2game(pipe_stdin, net_num);
            }
        }
        close(pipe_stdin[1]);
        close(pipe_stdout[0]);
        close(client_sockfd);
        close(server_sockfd);
}

void server(){
    pid_t pid;
    int server_sockfd = createServer();
    int client_sockfd = getClientSock(server_sockfd);
    int pipe_stdin[2], pipe_stdout[2];
    if (pipe(pipe_stdin) == -1 || pipe(pipe_stdout) == -1) {
        error("Pipe failed");
    }
    if ((pid = fork()) == -1) {
        error("Fork failed");
    }
    if (pid == 0) {
        childP(pipe_stdin, pipe_stdout);
    } else {
        parentP(pipe_stdin, pipe_stdout, client_sockfd, server_sockfd);
    }
}

int main() {
    server();
    return 0;
}