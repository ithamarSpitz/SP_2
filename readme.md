
```markdown
# MyNetCat Project

This project implements a custom version of the netcat tool (`mync`) that can redirect input and output for a specified program through various types of sockets. Additionally, it includes a simple tic-tac-toe player (`ttt`) that demonstrates the usage of `mync`.

## Assignment Description

The project is based on the following assignment:

Netcat - a tool that allows text redirection to different types of sockets.
It's recommended to learn how to use netcat by doing `man nc`.

Note - there are several popular versions of netcat, and not all support all different options, particularly the `-e` parameter which we will use in this assignment. If your version does not include the parameter, you can read explanations about it online.

### Step 1 - The World's Worst Tic-Tac-Toe Player (10 points)

We are trying to restore the honor of humanity by creating the world's worst tic-tac-toe player. The program will receive a 9-digit strategy number and play tic-tac-toe accordingly.

### Step 2 - MyNetCat - Stage 1 (10 points)

Write the `mync` program. The program will accept a parameter with `-e` to specify a program to run. For example, `mync -e date` will run the `date` program.

### Step 3 - MyNetCat - Stage 2 (20 points)

Enhance the `mync` program to support the following parameters:
- `-i` for input redirection
- `-o` for output redirection
- `-b` for both input and output redirection

### Step 4 - MyNetCat - Stage 3 (10 points)

Enhance the `mync` program to support the following parameters:
- `UDPS<PORT>` - Start UDP server on port <PORT>
- `UDPC<IP, PORT>` - Start a UDP client that connects to IP/hostname on PORT.

### Step 5 - MyNetCat - Stage 4 (30 points)

Enhance the `mync` program to support the following parameters:
- `TCPMUXS<PORT>` - Start a TCP server that supports IO MUX on PORT.

### Step 6 - Using Unix Domain Sockets for Communication (30 points)

Enhance the `mync` program to support Unix domain sockets with the following parameters:
- `UDSSD<path>` - Unix domain sockets server (datagram) on path
- `UDSCD<path>` - Unix domain sockets client (datagram) on path
- `UDSSS<path>` - Unix domain sockets server (stream) on path
- `UDSCS<path>` - Unix domain sockets client (stream) on path

## Main Files

### ttt.cpp

This file contains the code for the world's worst tic-tac-toe player. The program receives a 9-digit strategy number and plays tic-tac-toe based on the provided strategy. It chooses moves in a fixed priority order and outputs the result of each move.

### mync.cpp

This file implements the custom netcat tool, `mync`, which redirects input and output for a specified program through various types of sockets (TCP and UDP). It supports different modes for input and output redirection and can be used to create TCP and UDP servers and clients.

### mync+unix.cpp

This file extends the functionality of `mync` to support Unix domain sockets. It allows communication using Unix domain sockets in addition to TCP and UDP sockets.

## Additional Notes

- It is recommended to use `getopt(3)` for parameter handling.
- It is recommended to use `alarm(2)` for implementing timeout functionality.
- The project includes a recursive Makefile for building all stages.
- Code coverage reports should be included for all implemented stages.

# TCP:---------------------------------------------------------------

## To get input from client to server and print output to server:
```sh
terminal server: ./mync -e "ttt 123456789" -i TCPS4450
terminal client: nc localhost 4450
```

## To get input from client to server and print output to client:
```sh
terminal server: ./mync -e "ttt 123456789" -o TCPS4450
terminal client: nc localhost 4450
```

## To get input from client to server and print output to server & client:
```sh
terminal server: ./mync -e "ttt 123456789" -b TCPS4450
terminal client: nc localhost 4450
```

## To get input from client to server at port (E.g. 4050) and print output to client at another port (E.g. 4455):
```sh
terminal server: ./mync -e "ttt 123456789" -i TCPS4050 -o TCPClocalhost,4455
terminal client input: nc localhost 4050
terminal client output: nc localhost 4455
```

## To get input from client to server at port (E.g. 4050) and print output to client at another port (E.g. 4455):
```sh
terminal server: ./mync -e "ttt 123456789" -i TCPS4050 -o TCPC4455
terminal client input: nc localhost 4050
terminal client output: nc localhost 4455
```

# ---------------------------------------------------------------------------------------

# UDP:---------------------------------------------------------------------------------------

## To get input from client to server and print output to server:
```sh
terminal server: ./mync -e "ttt 123456789" -i UDPS4050 -t 10
terminal client: nc localhost 4050
```

## To get input from client to server at port (E.g. 4050) and print output to client at another port (E.g. 4455):
```sh
terminal server: ./mync -e "ttt 123456789" -i TCPS4050 -o UDPS4455 -t 100
terminal client input: nc localhost 4050
terminal client output: nc localhost 4455
```

# ---------------------------------------------------------------------------------------

# UNIX:---------------------------------------------------------------------------------------

## To get input from client to server and print output to server:
```sh
terminal server: ./mync UDSSS /tmp/unix_socket_path -e "ttt 123456789"
terminal client: ./mync UDSCS /tmp/unix_socket_path
```

# ---------------------------------------------------------------------------------------
```
