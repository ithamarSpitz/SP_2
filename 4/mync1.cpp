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

struct sockaddr_in addr(string port, int* sockfd, bool is_server, bool is_udp){
    struct sockaddr_in server_addr;
    // Create socket
    if (*sockfd != -1 && (*sockfd = socket(AF_INET, is_udp? SOCK_DGRAM : SOCK_STREAM, 0)) < 0) {
        error("Socket creation failed");
    }
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = is_server ? INADDR_ANY : inet_addr("127.0.0.1"); // Assuming server is on the same machine
    server_addr.sin_port = htons(stoi(port));
    return server_addr;
}

//server

int createTCPServer(string port){
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

int createUDPServer(string port){
    int server_sockfd;
    struct sockaddr_in server_addr = addr(port, &server_sockfd, true, true);
    // Bind the server socket
    if (bind(server_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        error("Bind failed");
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
        string move_str = to_string(num) + "\n";
        if (write(pipe_stdin[1], move_str.c_str(), move_str.size()) < 0) {
            error("Pipe write failed");
        }    
}

void parentP(int pipe_stdin[2], int pipe_stdout[2], int server_send_sock, int server_recv_sock, bool print_out, bool udp_recv, string client_port){
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
            if(udp_recv){
                int p = -1;
                struct sockaddr_in client_addr = addr(client_port, &p, true, true);    
                n = recvfrom(server_recv_sock, &net_num, sizeof(net_num), 0, (struct sockaddr *)&client_addr, (socklen_t *)sizeof(client_addr));
            }else{
                n = recv(server_recv_sock, &net_num, sizeof(net_num), 0);
            }
            if (n > 0) {
                client2game(pipe_stdin, net_num);
            }
        }
        close(pipe_stdin[1]);
        close(pipe_stdout[0]);
        if(server_recv_sock != server_send_sock){
            close(server_recv_sock);
        }
        close(server_send_sock);
}

void server(string server_port, string command,  string client_port, bool print_out, bool udp_recv){
    pid_t pid;
    int server_recv_sock;
    int  server_send_sock = createTCPServer(server_port);
    int client_sockfd = getClientSock(server_send_sock, client_port);
    if (udp_recv){
        server_recv_sock = createUDPServer(server_port);
    }else{
        server_recv_sock = client_sockfd;
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
        parentP(pipe_stdin, pipe_stdout, server_send_sock, server_recv_sock, print_out, udp_recv, client_port);
    }
}

//client

void send2server(string buf, int sockfd, bool udp, string server_port){
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
    if(udp){
        int p = -1;
        cout << "server_port" << server_port << endl;
        struct sockaddr_in server_addr = addr(server_port, &p, true, true);        
        sendto(sockfd, &net_num, sizeof(net_num), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
        cout << "sent" << endl;
    }else{
        send(sockfd, &net_num, sizeof(net_num), 0);
    }
}

int createTCPClient(string server_port, string client_port){
    int sockfd;
    int p = -1;
    struct sockaddr_in server_addr = addr(server_port, &p, false, false);
    if(client_port != ""){
        struct sockaddr_in client_addr = addr(client_port,&sockfd , true, false);
        cout << "tries to bind " << client_port << endl;
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

int createUDPClient(string client_port, string server_port){
    int sockfd;
    if(client_port != ""){
        int p = -1;
        struct sockaddr_in client_addr = addr(client_port, &p, true, true);
        // if (bind(sockfd, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
        //     error("Bind failed");
        // }
    }
    return sockfd;
}


void sendNrecv(int sock_send, int sock_recv, bool udp_send, string server_port){
    char buffer[BUFFER_SIZE];
    while (true) {
        // Receive message from the server (game's output)
        ssize_t n;
        n = recv(sock_recv, buffer, sizeof(buffer) - 1, 0);
        if (n > 0) {
            buffer[n] = '\0';
            cout << buffer;
            string buf(buffer);
            cout << "rec '" << buf <<"'" <<endl;
            send2server(buf, sock_send, udp_send, server_port);
        } else if (n == 0) {
            cout << "Connection closed by server." << endl;
            break;
        } else {
            error("recv failed");
        }
    }
}

void client(string server_port, string client_port, bool udp_send){
    int sock_send;
    int sock_recv;
    sock_recv = createTCPClient(server_port, client_port);
    if (udp_send){
        sock_send = createUDPClient(client_port, server_port);
    }else{
        sock_send = sock_recv;
    }
    sendNrecv(sock_send, sock_recv, udp_send, server_port);
    if(sock_recv != sock_send){
        close(sock_send);
    }
    close(sock_recv);
}

int main(int argc, char* argv[]){
    bool is_server = false, print_out = true;
    bool udp_recv = false, udp_send = false;
    string server_port="8000", command, client_port = "9000";
    for (int i = 1; i < argc-1; i+=2){
        std::string cur_arg(argv[i]);
        std::string next_arg(argv[i+1]);
        if(cur_arg == "-e"){
            is_server = true;
            command = next_arg;
        }
        if(cur_arg == "-i" || cur_arg == "-b" ){
            if (next_arg.find("UDP") != string::npos) {
                udp_recv = true;
            }
            server_port = next_arg.substr(4);
        }
        if(cur_arg == "-b" || cur_arg == "-o"){
            if (next_arg.find("UDP") != string::npos) {
                udp_send = true;
            }
            print_out = false;
        }
        if(cur_arg == "-o"){
            if (next_arg.size() >= 4) {
                if(is_server){
                    client_port = next_arg.substr(next_arg.size()-4);
                }else{
                    server_port = next_arg.substr(next_arg.size()-4);
                }
            } else {
                error("cant read specific client port");
            }
        }
    }
    if(is_server){
        server(server_port, command, client_port, print_out, udp_recv);
    }else{
        client(server_port, client_port, udp_send);
    }
    return 0;
}