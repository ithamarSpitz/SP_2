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
    if (*sockfd != -1 && (*sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        error("Socket creation failed");
    }
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = is_server ? INADDR_ANY : inet_addr("127.0.0.1"); // Assuming server is on the same machine
    server_addr.sin_port = htons(stoi(port));
    return server_addr;
}

//server

int createServer(string port){
    int server_sockfd;
    struct sockaddr_in server_addr = addr(port, &server_sockfd, true);
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

int getClientSock(int server_sockfd, string client_port){
    int client_sockfd;
    struct sockaddr_in client_addr;
    socklen_t client_len;
    client_len = sizeof(client_addr);
    // Accept a connection
    if ((client_sockfd = accept(server_sockfd, (struct sockaddr *)&client_addr, &client_len)) < 0) {
        error("Accept failed");
    }
    int recieved_port = static_cast<int>(client_addr.sin_port);
    if( client_port != "" && recieved_port != stoi(client_port)){
        error("received bad port");
    }

    return client_sockfd;
}

void childP(int pipe_stdin[2],int pipe_stdout[2], string comand){
        // Child process
        close(pipe_stdin[1]);
        close(pipe_stdout[0]);

        dup2(pipe_stdin[0], STDIN_FILENO);
        dup2(pipe_stdout[1], STDOUT_FILENO);

        close(pipe_stdin[0]);
        close(pipe_stdout[1]);

        int spacePos = comand.find(' ');
        if (spacePos != string::npos) {
            string firstPart = comand.substr(0, spacePos);
            string secondPart = comand.substr(spacePos + 1);
            
            execl(firstPart.c_str(), firstPart.c_str(), secondPart.c_str(), (char *)NULL);
            error("Exec failed");
        }else{
            error("wrong command");
        }    
}

void game2client(char buffer[BUFFER_SIZE], int n, int client_sockfd, bool print_out){
        buffer[n] = '\0';
        // Send the game's output to the other side of the connection
        if (send(client_sockfd, buffer, n, 0) < 0) {
            error("Socket send failed");
        }
        if (print_out){
            cout << buffer << endl;
        }
}

void client2game(int pipe_stdin[2], int net_num){
    int num = ntohl(net_num); // Convert from network byte order to host byte order
        //cout << "Received from client: " << num << endl;

        string move_str = to_string(num) + "\n";
        if (write(pipe_stdin[1], move_str.c_str(), move_str.size()) < 0) {
            error("Pipe write failed");
        }
        //cout << "Sent to game's stdin: " << move_str << endl;
    
}

void parentP(int pipe_stdin[2], int pipe_stdout[2], int client_sockfd, int server_sockfd, bool print_out){
// Parent process
        close(pipe_stdin[0]);
        close(pipe_stdout[1]);

        char buffer[BUFFER_SIZE];
        ssize_t n;
        while (true) {
            n = read(pipe_stdout[0], buffer, sizeof(buffer) - 1);
            if (n > 0) {
                game2client(buffer, n, client_sockfd, print_out);
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

void server(string server_port, string command,  string client_port, bool print_out){
    pid_t pid;
    int server_sockfd = createServer(server_port);
    int client_sockfd = getClientSock(server_sockfd, client_port);
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
        parentP(pipe_stdin, pipe_stdout, client_sockfd, server_sockfd, print_out);
    }
}

//client

void send2server(string buf, int sockfd){
    string sub1 = "D";
    string sub2 = "I";
    string user_input;
    if (buf.find(sub1) != string::npos || buf.find(sub2) != string::npos){
        user_input = "-1";
    }else{
        // Send user input to the server (game's input)
        getline(cin, user_input);
    }
    int num = stoi(user_input);
    int net_num = htonl(num); // Convert to network byte order
    send(sockfd, &net_num, sizeof(net_num), 0);
}

int createClient(string server_port, string client_port){
    int sockfd;
    struct sockaddr_in server_addr = addr(server_port, &sockfd, false);
    if (client_port != "") {
        struct sockaddr_in client_addr = addr(client_port, &sockfd, true);
        if (bind(sockfd, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
            error("Bind failed");
        }
    }else{
        bool unbound = true;
        int port = 8000;
            int p = -1;
        struct sockaddr_in client_addr = addr(to_string(port), &sockfd, true);
        while(unbound){
            client_addr = addr(to_string(port), &p, false);
            if (bind(sockfd, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
                port++;
                sleep(1);
            }else{
               unbound = false; 
            }
        }
    }
    // Connect to server
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        error("Connect failed");
    }
    return sockfd;
}

void sendNrecv(int sockfd){
    char buffer[BUFFER_SIZE];
    while (true) {
        // Receive message from the server (game's output)
        ssize_t n = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
        if (n > 0) {
            buffer[n] = '\0';
            cout << buffer;
            string buf(buffer);
            send2server(buf, sockfd);
        } else if (n == 0) {
            cout << "Connection closed by server." << endl;
            break;
        } else {
            error("recv failed");
        }
    }
}

void client(string server_port, string client_port){
    int sockfd = createClient(server_port, client_port);
    char buffer[BUFFER_SIZE];
    sendNrecv(sockfd);
    close(sockfd);
}

int main(int argc, char* argv[]){
    bool is_server = false, print_out = true;
    string server_port, command, client_port = "";
    for (int i = 1; i < argc-1; i+=2){
        std::string cur_arg(argv[i]);
        std::string next_arg(argv[i+1]);
        if(cur_arg == "-e"){
            is_server = true;
            command = next_arg;
        }
        if(cur_arg == "-i" || cur_arg == "-b" ){
            server_port = next_arg.substr(4);
        }
        if(cur_arg == "-b" || cur_arg == "-o"){
            print_out = false;
        }
        if(cur_arg == "-o"){
            if (next_arg.size() >= 4) {
                client_port = next_arg.substr(next_arg.size()-4);
            } else {
                error("cant read specific client port");
            }
        }
    }
    if(is_server){
        server(server_port, command, client_port, print_out);
    }else{
        client(server_port, client_port);
    }
    return 0;
}