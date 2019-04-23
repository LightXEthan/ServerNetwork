/**
* Based on code found at https://github.com/mafintosh/echo-servers.c (Copyright (c) 2014 Mathias Buus)
* Copyright 2019 Nicholas Pritchard, Ryan Bunney
**/

/**
 * @brief A simple example of a network server written in C implementing a basic echo
 *
 * This is a good starting point for observing C-based network code but is by no means complete.
 * We encourage you to use this as a starting point for your project if you're not sure where to start.
 * Food for thought:
 *   - Can we wrap the action of sending ALL of out data and receiving ALL of the data?
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <ctype.h>

#define BUFFER_SIZE 1024

#define ERR_CHECK_WRITE if (err < 0){fprintf(stderr,"Client write failed\n");exit(EXIT_FAILURE);}
#define ERR_CHECK_READ if (read < 0){fprintf(stderr,"Client read failed\n");exit(EXIT_FAILURE);}

// Sends message to clients
int send_message(char *msg, char *buf, int client_fd, int client_id) {
  buf[0] = '\0';
  sprintf(buf, msg, client_id);
  int err = send(client_fd, buf, strlen(buf), 0);
  if (err < 0) {
    fprintf(stderr,"Client write failed\n");
    exit(EXIT_FAILURE);
  }
  return 1;
}

void doprocessing (int sock) {
  int n;
  char buffer[256];
  bzero(buffer,256);
  n = read(sock,buffer,255);

  if (n < 0) {
    perror("ERROR reading from socket");
    exit(EXIT_FAILURE);
  }

  printf("Here is the message: %s\n", buffer);
  n = write(sock,"I got your message",18);

  if (n < 0) {
    perror("ERROR writing to socket");
    exit(EXIT_FAILURE);
  }
}

int main (int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr,"Usage: %s [port]\n",argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);

    int server_fd, client_fd, err, opt_val;
    struct sockaddr_in server, client;
    char *buf;
    int pid;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0){
        fprintf(stderr,"Could not create socket\n");
        exit(EXIT_FAILURE);
    }

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = htonl(INADDR_ANY);

    opt_val = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof opt_val);

    err = bind(server_fd, (struct sockaddr *) &server, sizeof(server));
    if (err < 0){
        fprintf(stderr,"Could not bind socket\n");
        exit(EXIT_FAILURE);
    }

    err = listen(server_fd, 128);
    if (err < 0){
        fprintf(stderr,"Could not listen on socket\n");
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on %d\n", port);

    while (true) {
        socklen_t client_len = sizeof(client);
        // Will block until a connection is made
        client_fd = accept(server_fd, (struct sockaddr *) &client, &client_len);

        if (client_fd < 0) {
            fprintf(stderr,"Could not establish new connection\n");
            exit(EXIT_FAILURE);
        }

        /*
        // Create child process
        pid = fork();

        if (pid < 0) {
          perror("ERROR on fork\n");
          exit(1);
        }

        if (pid == 0) {
          // Client process
          printf("PID 0: %d", pid);
          close(server_fd);
          //doprocessing(client_fd);
        } else {
          printf("PID !0: %d", pid);
          close(client_fd);
        }
        */

        /**
        The following while loop contains some basic code that sends messages back and forth
        between a client (e.g. the socket_client.py client).

        The majority of the server 'action' will happen in this part of the code (unless you decide
        to change this dramatically, which is allowed). The following function names/definitions will
        hopefully provide inspiration for how you can start to build up the functionality of the server.

        parse_message(char *){...} :
            * This would be a good 'general' function that reads a message from a client and then
            determines what the required response is; is the client connecting, making a move, etc.
            * It may be useful having an enum that is used to track what type of client message is received
            (e.g. CONNECT/MOVE etc.)

        send_message() {...}:
            * This would send responses based on what the client has sent through, or if the server needs
            to send all clients messages

        play_game_round() {...}: Implements the functionality for a round of the game
            * 'Roll' the dice (using a random number generator) and then check if the move made by the user
            is correct
            * update game state depending on success or failure.

        setup_game/teardown_game() {} :
            * this will set up the initial state of the game (number of rounds, players
            etc.)/ print out final game results and cancel socket connections.

        Accepting multiple connections (we recommend not starting this until after implementing some
        of the basic message parsing/game playing):
            * Whilst in a while loop
                - Accept a new connection
                - Create a child process
                - In the child process (which is associated with client), perform game_playing functionality
                (or read the messages)
        **/

        // Game States, put this in a function later
        bool gameStarted = false;
        int nplayers = 0;
        int playersAlive = 0;

        struct ClientState {
          int client_id;
          int nlives;
        } clientState;

        buf = calloc(BUFFER_SIZE, sizeof(char)); // Clear our buffer so we don't accidentally send/print garbage
        int read = recv(client_fd, buf, BUFFER_SIZE, 0);    // Try to read from the incoming client
        ERR_CHECK_READ;
        // Receives client request to enter game
        if (strstr(buf, "INIT")) {
            // Fork somewhere here for multiplayer
            int client_id = 231; // value of single player
            printf("INIT received, sending welcome.\n");
            buf[0] = '\0';
            sprintf(buf, "WELCOME,%d",client_id); // Gives client_id to the clients
            err = send(client_fd, buf, strlen(buf), 0);
            nplayers++;
            playersAlive++;
            // Creates clientStates
            clientState.client_id = client_id;
            clientState.nlives = 3;
        } else {
            // Rejects when client connection is rejected,
            // like when game already stared
            fprintf(stderr,"Client rejected\n");
            buf[0] = '\0';
            sprintf(buf, "REJECT");
            err = send(client_fd, buf, strlen(buf), 0);
        }

        sleep(5); // Lobby wait time, 5 seconds

        // If not enough players in lobby, cancel game, Tier4

        // Signals the start of the game
        buf[0] = '\0';
        sprintf(buf, "START,%d,%d\n",nplayers,clientState.nlives);

        err = send(client_fd, buf, strlen(buf), 0); // Send another thing
        ERR_CHECK_WRITE;

        // Loops each game round
        while (true) {

            // Waits for move
            printf("Waiting for input\n");
            sleep(5); // TODO: implement time out time
            // Clear buffer?
            read = recv(client_fd, buf, BUFFER_SIZE, 0); // See if we have a response
            ERR_CHECK_READ;

            printf("%s\n", buf);
            if (strstr(buf, "MOV") == NULL) {  // Check if the message contained 'move'
                fprintf(stderr, "Unexpected message, terminating\n");
                exit(EXIT_FAILURE); //Kick player instead Tier4
            }

            // We have confirmed here that the player has moved
            // Rolls the dice
            srand(time(0)); //time(0)
            int dice[2]; //TODO: memory?
            dice[0] = 2;//rand() % 6 + 1;
            dice[1] = 2;//rand() % 6 + 1;
            int diceSum = dice[0] + dice[1];
            printf("Dice one roll: %d\n",dice[0]);
            printf("Dice two roll: %d\n",dice[1]);

            // Calculate score using the players move
            if (strstr(buf, "DOUB") && dice[0] == dice[1]) {
              // Doubles rolled and pass is sent
              send_message("%d,PASS", buf, client_fd, clientState.client_id);

            } else if (strstr(buf, "EVEN") && diceSum % 2 == 0) {
              // Even rolled and pass is sent
              send_message("%d,PASS", buf, client_fd, clientState.client_id);

            } else if (strstr(buf, "ODD") && diceSum % 2 == 1 && diceSum > 5) {
              // Odd rolled above 5 and pass is sent
              send_message("%d,PASS", buf, client_fd, clientState.client_id);

            } else if (strstr(buf, "CON")) {
              // Choice from the player
              printf("%s\n",buf);
              char s[2] = ",";
              char *token = strtok(buf, s);
              while ( token != NULL && strcmp(token,"CON") != 0) {
                token = strtok(NULL,s);
              }
              token = strtok(NULL,s);

              if (isdigit(token[1])) {
                token[2] = '\0';
              } else {
                token[1] = '\0';
              }
              int number = atoi(token);
              printf("Number guessed: %d\n", number);

              if (diceSum == number) {
                send_message("%d,PASS", buf, client_fd, clientState.client_id);
              }
            } else {
              send_message("%d,FAIL", buf, client_fd, clientState.client_id);
              clientState.nlives--;
            }

            // We need to check if the player has no more lives
            if (clientState.nlives <= 0) {
              // Eliminate player from game
              send_message("%d,ELIM", buf, client_fd, clientState.client_id);
              playersAlive--;
            } else if (clientState.nlives > 0 && playersAlive == 1) {
              // Check if no other players alive, win condition

            }

        }
        printf("%d,%d,%d",gameStarted,nplayers,playersAlive);
        free(buf);
    }
}
