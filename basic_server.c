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
#include <sys/mman.h>
#include <netinet/in.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>

#define BUFFER_SIZE 1024

#define ERR_CHECK_WRITE if (err < 0){fprintf(stderr,"Client write failed\n");exit(EXIT_FAILURE);}
#define ERR_CHECK_READ if (rec < 0){fprintf(stderr,"Client read failed\n");exit(EXIT_FAILURE);}
#define MIN_PLAYERS 2

// Sends message to clients
int send_message(char *msg, int client_fd, int client_id) {
  char *buf = calloc(BUFFER_SIZE, sizeof(char));
  buf[0] = '\0';
  sprintf(buf, msg, client_id);
  int err = send(client_fd, buf, strlen(buf), 0);
  free(buf);

  ///replace with ERR_CHECK_WRITE??
  if (err < 0) {
    fprintf(stderr,"Client write failed\n");
    exit(EXIT_FAILURE);
  }
  return 1;
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
    int nplayers = 0;
    bool host = false;
    int p1[2];

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

    // Setup Pipe for inter-process communication
    if (pipe(p1) < 0) {
      fprintf(stderr,"Could not pipe\n");
      exit(EXIT_FAILURE);
    }

    // Create shared memory to check if game has started
    void* shmem = mmap(NULL, 4, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0);
    memcpy(shmem, "GNS", 4); // Default value, game has not started

    while (true) {
        socklen_t client_len = sizeof(client);
        // Will block until a connection is made
        client_fd = accept(server_fd, (struct sockaddr *) &client, &client_len);

        if (client_fd < 0) {
            fprintf(stderr,"Could not establish new connection\n");
            exit(EXIT_FAILURE);
        }

        // Create host process
        //hid = fork();


        // Create child process
        pid = fork();
        nplayers++;
        if (pid < 0) {
          fprintf(stderr,"Could not fork\n");
          exit(EXIT_FAILURE);
        }

        if (pid == 0) {
          // Child process/Client process
          close(server_fd);

        } else {
          // Parent process, will loop back and wait for another connection
          close(client_fd);
          continue;
        }

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
        fd_set set;

        struct timeval timeout;
        FD_ZERO(&set);
        FD_SET(p1[0],&set);

        struct ClientState {
          int client_id;
          int nlives;
        } clientState;

        buf = calloc(BUFFER_SIZE, sizeof(char)); // Clear our buffer so we don't accidentally send/print garbage
        int rec = recv(client_fd, buf, BUFFER_SIZE, 0);    // Try to read from the incoming client
        ERR_CHECK_READ;

        // Receives client request to enter game
        if (strstr(buf, "INIT") && strstr(shmem, "GNS")) {
            int client_id = 230 + nplayers; // client id is dependent on number of players
            printf("INIT received, sending welcome.\n");

            memset(buf, 0, BUFFER_SIZE);
            sprintf(buf, "WELCOME,%d",client_id); // Gives client_id to the clients
            err = send(client_fd, buf, strlen(buf), 0);
            ERR_CHECK_WRITE;

            // Creates clientStates
            clientState.client_id = client_id;
            clientState.nlives = 2;

        } else {
            // Rejects when game has already started
            err = send(client_fd, "REJECT", 6, 0);
            ERR_CHECK_WRITE;
            exit(EXIT_FAILURE);
        }



        // If not enough players in lobby, cancel game, Tier4

        // Signals the start of the game
        // Pipe to all processes that the game as started
        char inbuf[13];
        timeout.tv_sec = 5; // Timeout time
        timeout.tv_usec = 0;

        // Everyone says how many players they know
        buf[0] = '\0';
        sprintf(buf, "%d",nplayers);
        write(p1[1], buf, sizeof(int));

        if (nplayers == 1) {
          // This process becomes the host
          host = true;

          // Get number of players
          int max = 0;
          printf("Game start initiated, counting players...\n");

          // Counts number of players
          while (true) {
            int rv = select(p1[0]+1, &set, NULL, NULL, &timeout);
            if (rv == -1) {
              perror("Error with select\n");
            } else if (rv == 0) {
              // After timeout
              printf("Player count confirmed: %d\n", max);
              memcpy(shmem, "GHS", 4); // Puts into shared memory that game has started
              break;
            } else {
              printf("Player Joined.\n");
              read(p1[0],inbuf,13);
              // Gets the highest number which is the player count
              if (atoi(inbuf) > max) {
                max = atoi(inbuf);
              }
            }
          }

          nplayers = max;
          // Check min number of players
          //printf("Game start!\n");

          // Start the game, send info to players, Each child gets the number of players
          buf[0] = '\0';
          sprintf(buf, "%d",nplayers);

          for (int i = 0; i < nplayers - 1; i++) {
            write(p1[1], buf, sizeof(int));
          }

        } else {
          sleep(1);
          read(p1[0],inbuf,sizeof(int));
          nplayers = atoi(inbuf);
        }
        //printf("Number of players: %d\n",nplayers);

        buf[0] = '\0';
        sprintf(buf, "START,%d,%d\n",nplayers,clientState.nlives);

        err = send(client_fd, buf, strlen(buf), 0); // Send another thing
        ERR_CHECK_WRITE;

        // Loops each game round
        while (true) {

            // Waits for move
            //printf("Waiting for input\n");
            //sleep(5); // TODO: implement time out time
            memset(buf, 0, BUFFER_SIZE);
            rec = recv(client_fd, buf, BUFFER_SIZE, 0); // See if we have a response
            ERR_CHECK_READ;

            printf("%s\n", buf);
            if (strstr(buf, "MOV") == NULL) {  // Check if the message contained 'move'
                fprintf(stderr, "Unexpected message, terminating\n");
                free(buf);
                exit(EXIT_FAILURE); //Kick player instead Tier4
            }

            // We have confirmed here that the player has moved

            // Host process will roll dice
            int dice[2];
            char dice1[3];
            char dice2[3];

            if (host) {
              srand(time(0));
              dice[0] = rand() % 6 + 1;
              dice[1] = rand() % 6 + 1;

              printf("Dice one roll: %d\n",dice[0]);
              printf("Dice two roll: %d\n",dice[1]);

              // Pipe Host -> Children
              // Each child gets the dice
              for (int i = 0; i < nplayers - 1; i++) {
                sprintf(dice1, "%d", dice[0]);
                write(p1[1],dice1,3);
                sleep(0.1);
                sprintf(dice2, "%d", dice[1]);
                write(p1[1],dice2,3);
              }
              sleep(2);

            } else {
              read(p1[0],dice1,3);
              dice[0] = atoi(dice1);
              sleep(0.1);
              read(p1[0],dice2,3);
              dice[1] = atoi(dice2);
            }

            int diceSum = dice[0] + dice[1];

            // Calculate score using the players move
            char msg[8];
            memset(msg, 0, 8);

            if (strstr(buf, "DOUB") && dice[0] == dice[1]) {
              // Doubles rolled and pass is sent
              sprintf(msg, "%s", "%d,PASS");

            } else if (strstr(buf, "EVEN") && diceSum % 2 == 0 && dice[0] != dice[1]) {
              // Even rolled and pass is sent
              sprintf(msg, "%s", "%d,PASS");

            } else if (strstr(buf, "ODD") && diceSum % 2 == 1 && diceSum > 5) {
              // Odd rolled above 5 and pass is sent
              sprintf(msg, "%s", "%d,PASS");

            } else if (strstr(buf, "CON")) {
              // Choice from the player, below gets the number
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
              if (number == 0) {
                // No number was selected
                fprintf(stderr, "Player did not specify number.\n");
                free(buf);
                exit(EXIT_FAILURE); //Kick player instead Tier4
              }
              printf("Number guessed: %d\n", number);

              if (diceSum == number) {
                sprintf(msg, "%s", "%d,PASS");
              }

            } else if (clientState.nlives > 1) {
              // Sends fail but still in the game
              clientState.nlives--;
              sprintf(msg, "%s", "%d,FAIL");

            } else if (clientState.nlives <= 1) {
              // Eliminate player from game
              sprintf(msg, "%s", "%d,ELIM");

            }

            // Send the result to the host
            write(p1[1], msg, 9);
            sleep(5); // Round time

            // Pipe Children -> Host, if they passed or died
            if (host) {
              // Receive msg
              int elims = 0;
              int fails = 0;
              int pass = 0;
              int playersAlive = 0;
              char rmsg[8];
              fd_set set2;
              FD_ZERO(&set2);
              FD_SET(p1[0],&set2);

              // Count number of each result
              timeout.tv_usec = 0;
              timeout.tv_sec = 0; // Timeout time

              while (true) {
                int rv = select(p1[0]+1, &set2, NULL, NULL, &timeout);
                if (rv == -1) {
                  perror("Error with select\n");
                } else if (rv == 0) {
                  break; // After timeout

                } else {
                  memset(rmsg, 0, 8);
                  rmsg[8] = '\0';
                  int a = read(p1[0], rmsg, 8);
                  printf("Read: %s, %d\n", rmsg, a);

                  // Count number of outcomes
                  if (strstr(rmsg, "ELIM")) {
                    elims++;
                  } else if (strstr(rmsg, "PASS")) {
                    pass++;
                  } else if (strstr(rmsg, "FAIL")) {
                    fails++;
                  }
                }
              }

              // Finds new number of playersAlive
              nplayers = elims + pass + fails;
              playersAlive = nplayers;
              printf("N: %d, %d, %d\n", elims, pass, fails);

              // If everyone is elim, then everyone gets vict
              if (elims == nplayers) {
                nplayers = 0; // 0 when all players die, so everyone wins

              } else {
                nplayers -= elims;
              }

              // Send new player count to each process
              //memcpy(players, &nplayers, sizeof(int)); // Updates the player count
              char np[5]; // Number of players in string, increase size of scaling
              sprintf(np, "%d", nplayers);
              for (int i = 0; i < playersAlive - 1; i++) {
                write(p1[1], np, 5);
              }

              printf("Sent new player count, %d\n", nplayers);

            } else {
              // Clients that are not the host
              sleep(2);
              printf("Reading\n");
              char np[5];
              read(p1[0], np, 5);
              nplayers = atoi(np);
              printf("Read: %d\n", nplayers);

            }

            if (nplayers == 0) {
              // Everyone wins if all get eliminated
              sprintf(msg, "%s", "%d,VICT");
              //printf("msg: %s\n",msg);
              send_message(msg, client_fd, clientState.client_id);
              free(buf);
              exit(EXIT_SUCCESS);

            } else if (nplayers == 1 && (strstr(msg, "PASS") || strstr(msg, "FAIL"))) {
              // If one person alive then they get vict
              printf("Victory Last alive. %d\n", nplayers);
              sprintf(msg, "%s", "%d,VICT");
              //printf("msg: %s\n",msg);
              send_message(msg, client_fd, clientState.client_id);
              free(buf);
              exit(EXIT_SUCCESS);
            }

            printf("msg: %s\n",msg);
            send_message(msg, client_fd, clientState.client_id);


        }
        free(buf);
    }
}
