#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>

#define BUFFER_SIZE 1024

void handle_error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

// Function to create a TCP server
int create_tcp_server(int port) {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;

    // Create socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        handle_error("socket failed");
    }

    // Attach socket to the port
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        handle_error("setsockopt failed");
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    // Bind the socket to the network address and port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        handle_error("bind failed");
    }

    // Listen for incoming connections
    if (listen(server_fd, 3) < 0) {
        handle_error("listen failed");
    }

    return server_fd;
}

// Function to create a TCP client and connect to a server
int create_tcp_client(const char *hostname, int port) {
    int client_fd;
    struct sockaddr_in serv_addr;

    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        handle_error("Socket creation error");
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, hostname, &serv_addr.sin_addr) <= 0) {
        handle_error("Invalid address/ Address not supported");
    }

    if (connect(client_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        handle_error("Connection failed");
    }

    return client_fd;
}

void run_ttt(int input_fd, int output_fd, char *strategy) {
    // Redirect stdin and stdout
    if (dup2(input_fd, STDIN_FILENO) == -1) {
        handle_error("dup2 input_fd");
    }
    if (dup2(output_fd, STDOUT_FILENO) == -1) {
        handle_error("dup2 output_fd");
    }

    // Execute the ttt program
    execlp("./ttt", "ttt", strategy, (char *)NULL);
    handle_error("execlp ttt failed");
}


int main(int argc, char *argv[]) {
    int opt;
    int server_mode = 0, client_mode = 0, both_mode = 0;
    int input_port = 0, output_port = 0;
    char output_host[BUFFER_SIZE];
    char *exec_command = nullptr;
    char *strategy = argv[3];
    strategy[9] = '\0';

    while ((opt = getopt(argc, argv, "e:i:o:b:")) != -1) {
        switch (opt) {
            case 'e':
                exec_command = optarg;
                break;
            case 'i':
                server_mode = 1;
                input_port = atoi(optarg + 4); // Extract port from TCPS#####
                break;
            case 'o':
                client_mode = 1;
                output_port = atoi(optarg + 4); // Extract port from TCPC#####
                strcpy(output_host, "127.0.0.1");
                break;
            case 'b':
                both_mode = 1;
                input_port = atoi(optarg + 4); // Extract port from TCPS#####
                strcpy(output_host, "127.0.0.1");
                output_port = input_port;
                break;
            default:
                fprintf(stderr, "Usage: %s -e <command> [-i TCPSport] [-o TCPChost,port] [-b TCPSport]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (!exec_command) {
        fprintf(stderr, "Execution command is required\n");
        exit(EXIT_FAILURE);
    }

    int server_fd = -1, client_fd = -1, new_socket = -1;
    pid_t pid;

    if (server_mode || both_mode) {
        server_fd = create_tcp_server(input_port);

        // Wait for a client to connect
        if ((new_socket = accept(server_fd, NULL, NULL)) < 0) {
            handle_error("accept failed");
        }
    }

    if (client_mode || both_mode) {
        client_fd = create_tcp_client(output_host, output_port);
    }

    if ((pid = fork()) == 0) {
        // Child process
        if (server_mode && !both_mode) {
            run_ttt(new_socket, STDOUT_FILENO, strategy);
        } else if (client_mode && !both_mode) {
            run_ttt(client_fd, client_fd, strategy);
        }else if (both_mode) {
            run_ttt(new_socket, STDOUT_FILENO, strategy);
        } else {
            // Default execution without redirection
            execlp("./ttt", "ttt", strategy, (char *)NULL);
            handle_error("execlp ttt failed");
        }
    } else if (pid > 0) {
        // Parent process
        if (both_mode) {
            char buffer[BUFFER_SIZE];
            ssize_t nread;
            int flag = 0;
            // Parent process: read from ttt's stdout and send to client
            while ((nread = read(new_socket, buffer, BUFFER_SIZE)) > 0 && flag != 1) {
                printf("%c\n",buffer[nread - 1]);
                if (buffer[nread - 1] == '!' || buffer[nread - 1] == 'W' || buffer[nread - 1] == '.') {
                    flag = 1;
                }
                
                if (write(client_fd, buffer, nread) != nread) {
                    handle_error("write to client failed");
                }
            }

            if (nread < 0) {
                handle_error("read from server failed");
            }
        }
        
        // Parent process: Wait for child to complete
        int status;
        waitpid(pid, &status, 0);

        // Close sockets
        if (server_fd != -1) close(server_fd);
        if (client_fd != -1) close(client_fd);
        if (new_socket != -1) close(new_socket);

        // Check child exit status
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            fprintf(stderr, "Child process exited with status %d\n", WEXITSTATUS(status));
        }
    } else {
        handle_error("fork failed");
    }

    return 0;
}


/*----------------------------------------------ORIGINAL DOWN--------------------------------------------------------*/

// #include <iostream>
// #include <string>
// #include <cstring>
// #include <unistd.h>
// #include <netinet/in.h>
// #include <sys/types.h>
// #include <sys/socket.h>
// #include <arpa/inet.h>
// #include <signal.h>
// #include <errno.h>
// #include <sys/wait.h>

// #define BUFFER_SIZE 1024

// void handle_error(const char *msg) {
//     perror(msg);
//     exit(EXIT_FAILURE);
// }

// // Function to create a TCP server
// int create_tcp_server(int port) {
//     int server_fd;
//     struct sockaddr_in address;
//     int opt = 1;

//     // Create socket file descriptor
//     if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
//         handle_error("socket failed");
//     }

//     // Attach socket to the port
//     if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
//         handle_error("setsockopt failed");
//     }
//     address.sin_family = AF_INET;
//     address.sin_addr.s_addr = INADDR_ANY;
//     address.sin_port = htons(port);

//     // Bind the socket to the network address and port
//     if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
//         handle_error("bind failed");
//     }

//     // Listen for incoming connections
//     if (listen(server_fd, 3) < 0) {
//         handle_error("listen failed");
//     }

//     return server_fd;
// }

// // Function to create a TCP client and connect to a server
// int create_tcp_client(const char *hostname, int port) {
//     int client_fd;
//     struct sockaddr_in serv_addr;

//     if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
//         handle_error("Socket creation error");
//     }

//     serv_addr.sin_family = AF_INET;
//     serv_addr.sin_port = htons(port);

//     // Convert IPv4 and IPv6 addresses from text to binary form
//     if (inet_pton(AF_INET, hostname, &serv_addr.sin_addr) <= 0) {
//         handle_error("Invalid address/ Address not supported");
//     }

//     if (connect(client_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
//         handle_error("Connection failed");
//     }

//     return client_fd;
// }

// void run_ttt(int input_fd, int output_fd, char *strategy) {
//     // Redirect stdin and stdout
//     if (dup2(input_fd, STDIN_FILENO) == -1) {
//         handle_error("dup2 input_fd");
//     }
//     if (dup2(output_fd, STDOUT_FILENO) == -1) {
//         handle_error("dup2 output_fd");
//     }

//     // Execute the ttt program
//     execlp("./ttt", "ttt", strategy,(char *)NULL);
//     handle_error("execlp ttt failed");
// }


// int main(int argc, char *argv[]) {
//     int opt;
//     int server_mode = 0, server_o_mode =0, client_mode = 0, both_mode = 0;
//     int input_port = 0, output_port = 0;
//     char output_host[BUFFER_SIZE];
//     char *exec_command = nullptr;
//     char *strategy = argv[3];
//     strategy[9] = '\0';

//     while ((opt = getopt(argc, argv, "e:i:o:b:")) != -1) {
//         switch (opt) {
//             case 'e':
//                 exec_command = optarg;
//                 break;
//             case 'i':
//                 server_mode = 1;
//                 input_port = atoi(optarg + 4); // Extract port from TCPS#####
//                 break;
//             case 'o':
//                 server_o_mode = 1;
//                 // sscanf(optarg, "TCPClocalhost,%d", &output_port); // Extract port
//                 input_port = atoi(optarg + 4); // Extract port from TCPC#####
//                 strcpy(output_host, "127.0.0.1");
//                 break;
//             case 'b':
//                 both_mode = 1;
//                 input_port = atoi(optarg + 4); // Extract port from TCPS#####
//                 break;
//             default:
//                 fprintf(stderr, "Usage: %s -e <command> [-i TCPSport] [-o TCPChost,port] [-b TCPSport]\n", argv[0]);
//                 exit(EXIT_FAILURE);
//         }
//     }

//     if (!exec_command) {
//         fprintf(stderr, "Execution command is required\n");
//         exit(EXIT_FAILURE);
//     }

//     int server_fd = -1, client_fd = -1, new_socket = -1;
//     pid_t pid;

//     if (server_mode || both_mode || server_o_mode) {
//         server_fd = create_tcp_server(input_port);

//         // Wait for a client to connect
//         if ((new_socket = accept(server_fd, NULL, NULL)) < 0) {
//             handle_error("accept failed");
//         }
//     }

//     if (client_mode) {
//         client_fd = create_tcp_client(output_host, output_port);
//     }

//     if ((pid = fork()) == 0) {
//         // Child process
//         if (server_mode && !both_mode) {
//             run_ttt(new_socket, STDOUT_FILENO, strategy);
//         } else if (server_o_mode && !both_mode) {
//             run_ttt(new_socket, new_socket, strategy);
//         } else if (both_mode) {
//             run_ttt(new_socket, new_socket, strategy);
//         } else {
//             // Default execution without redirection
//             execlp("./ttt", "ttt", "strategy", (char *)NULL);
//             handle_error("execlp ttt failed");
//         }
//     } else if (pid > 0) {
//         // Parent process: Wait for child to complete
//         int status;
//         waitpid(pid, &status, 0);

//         // Close sockets
//         if (server_fd != -1) close(server_fd);
//         if (client_fd != -1) close(client_fd);
//         if (new_socket != -1) close(new_socket);

//         // Check child exit status
//         if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
//             fprintf(stderr, "Child process exited with status %d\n", WEXITSTATUS(status));
//         }
//     } else {
//         handle_error("fork failed");
//     }

//     return 0;
// }
