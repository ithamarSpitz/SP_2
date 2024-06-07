// client.cpp
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <cstdlib>
#include <string> 


#define BUFFER_SIZE 1024
using namespace std;

void error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

struct sockaddr_in addr(string port, int* sockfd){
    struct sockaddr_in server_addr;
    // Create socket
    if ((*sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        error("Socket creation failed");
    }
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // Assuming server is on the same machine
    server_addr.sin_port = htons(stoi(port));
    return server_addr;
}

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

int createClient(string port){
    int sockfd;
    struct sockaddr_in server_addr = addr(port, &sockfd);
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

void client(string port){
    int sockfd = createClient(port);
    char buffer[BUFFER_SIZE];
    sendNrecv(sockfd);
    close(sockfd);
}

int main(int argc, char* argv[]){
    string port = argv[1];
    client(port);
    return 0;
}