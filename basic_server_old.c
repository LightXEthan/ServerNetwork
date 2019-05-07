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

//FUNCTIONS
/*----------------------------------------------------------------------------------------*/
// Sends message to clients
int send_message(char *msg, int client_fd, int client_id) {
  char *buf = calloc(BUFFER_SIZE, sizeof(char));
  buf[0] = '\0';
  sprintf(buf, msg, client_id);
  int err = send(client_fd, buf, strlen(buf), 0);
  free(buf);

  ERR_CHECK_WRITE;

  return 1;
}

//MAIN
/*----------------------------------------------------------------------------------------*/
int main (int argc, char *argv[]) {

    if (argc < 2){
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

    //CREATE SOCKET, SET, BIND, LISTEN
    /*----------------------------------------------------------------------------------------*/
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


    //PIPE & SHARED MEMORY
    /*----------------------------------------------------------------------------------------*/
    // Setup Pipe for inter-process communication
    if (pipe(p1) < 0) {
      fprintf(stderr,"Could not pipe\n");
      exit(EXIT_FAILURE);
    }

    // Create shared memory to check if game has started
    void* shmem = mmap(NULL, 4, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0);
    memcpy(shmem, "GNS", 4); // Default value, game has not started

    //MAIN LOOP
    /*----------------------------------------------------------------------------------------*/
    while (true) {
        socklen_t client_len = sizeof(client);
        // Will block until a connection is made
        client_fd = accept(server_fd, (struct sockaddr *) &client, &client_len);

        if (client_fd < 0) {
          fprintf(stderr,"Could not establish new connection\n");
          exit(EXIT_FAILURE);
        }



        //FORK OUT CHILD PROCESSES
        /*----------------------------------------------------------------------------------------*/
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
          // Parent process, will loop back to accept() and wait for another connection
          close(client_fd);
          continue;
        }

        //VARIABLES
        /*----------------------------------------------------------------------------------------*/
        fd_set set;

        struct timeval timeout;
        FD_ZERO(&set);
        FD_SET(p1[0],&set);

        struct ClientState {
          int client_id;
          int nlives;
        } clientState;


        //START READING MESSAGE FROM CLIENTS
        /*----------------------------------------------------------------------------------------*/
        buf = calloc(BUFFER_SIZE, sizeof(char)); // Clear our buffer so we don't accidentally send/print garbage
        int rec = recv(client_fd, buf, BUFFER_SIZE, 0);    // Try to read from the incoming client
        ERR_CHECK_READ;


        //INIT & REJECT
        /*----------------------------------------------------------------------------------------*/
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


        //COUNTING PLAYERS & TIMEOUT & DECIDE START OR CANCEL
        /*----------------------------------------------------------------------------------------*/
        // Pipe to all processes that the game as started
        char inbuf[13];
        timeout.tv_sec = 10; // Timeout time
        timeout.tv_usec = 0;

        // Everyone send how many players they know to the pipe
        buf[0] = '\0';
        sprintf(buf, "%d",nplayers);
        write(p1[1], buf, sizeof(int));

        if (nplayers == 1) {
          // This process becomes the host
          host = true;

          // Get number of players
          int max = 0;
          memcpy(shmem, "GHS", 4); // Puts into shared memory that game has started
          printf("Game has started, players can no longer join\nCounting players...\n");

          // Counts number of players
          while (true) {
            int rv = select(p1[0]+1, &set, NULL, NULL, &timeout);
            if (rv == -1) {
              perror("Error with select\n");
            } else if (rv == 0) {//when timeout
              printf("Player count confirmed: %d\n", max);
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

          // Start the game, host sends number of players to players
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


        //SIGNAL START
        /*----------------------------------------------------------------------------------------*/
        buf[0] = '\0';
        sprintf(buf, "START,%d,%d\n",nplayers,clientState.nlives);

        err = send(client_fd, buf, strlen(buf), 0); // Send another thing
        ERR_CHECK_WRITE;


        //LOOP EACH GAME ROUND
        /*----------------------------------------------------------------------------------------*/
        while (true) {

            // Waits for move
            //printf("Waiting for input\n");
            //sleep(5); // TODO: implement time out time
            memset(buf, 0, BUFFER_SIZE);
            rec = recv(client_fd, buf, BUFFER_SIZE, 0); // See if we have a response
            ERR_CHECK_READ;

            // Watch-Dog, anti-cheat detection, checks that the player sent a vaild packet
            printf("%s\n", buf);
            if (strstr(buf, "MOV") == NULL) {  // Check if the message contained 'move'
                fprintf(stderr, "Unexpected message, terminating\n");
                free(buf);
                exit(EXIT_FAILURE); //Kick player instead Tier4
            }

            char s[2] = ",";
            char *tok = strtok(buf, s);
            int counter = 0;
            bool isCON = false;
            int number; // Stores the number selected by the player
            char action[5]; // Stores the action taken by the player
            while ( tok != NULL) {
              switch (counter) {
                case 0:
                  // Should be its own client id
                  if (clientState.client_id != atoi(tok)) {
                    // Kick for cheating
                    printf("Kicked for cheating\n");
                  }
                  break;

                case 1:
                  // Should be MOV
                  if (strcmp("MOV",tok) != 0) {
                    // Kick for cheating
                    printf("Kicked for cheating\n");
                  }
                  break;

                case 2:
                  // Should be a valid action
                  if (strcmp(tok,"EVEN")==0) {
                    sprintf(action,"EVEN");
                  } else if (strcmp(tok,"ODD")==0) {
                    sprintf(action,"ODD");
                  } else if (strcmp(tok,"DOUB")==0) {
                    sprintf(action,"DOUB");
                  } else if (strcmp(tok,"CON")==0) {
                    sprintf(action,"CON");
                    isCON = true;
                  } else {
                    // kick for cheating
                    printf("Kicked for cheating\n");
                  }
                  break;

                case 3:
                  // Should be a 2 digit int only iff move was CON
                  if (isCON) {
                    //tok[2] = '\0';
                    number = atoi(tok);
                    printf("Number: %d\n", number);
                    if (number < 2 || 12 < number) {
                      // Player entered invalid number
                      fprintf(stderr, "Invalid number.\n");
                      free(buf);
                      exit(EXIT_FAILURE); //Kick player instead Tier4
                    }
                  } else {
                    // kick for cheating
                    printf("Kicked for cheating\n");
                  }
                  break;

                default:
                  // Kick for cheating
                  printf("Kicked for cheating\n");
                  free(buf);
                  exit(EXIT_FAILURE); //Kick player instead Tier4

              }
              tok = strtok(NULL,s);
              counter++;
            }

            // We have confirmed here that the player has moved

            //HOST ROLL THE DICE
            /*----------------------------------------------------------------------------------------*/
            int dice[2];
            char dice1[3];
            char dice2[3];

            if (host) {
              srand(time(0));
              dice[0] = rand() % 6 + 1;
              dice[1] = rand() % 6 + 1;

              printf("Dice one roll: %d\n",dice[0]);
              printf("Dice two roll: %d\n",dice[1]);

              // Host pipes dice roll to other players
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


            //DECIDE PASS, FAIL, ELIM
            /*----------------------------------------------------------------------------------------*/
            // Calculate score using the players move
            char msg[8];
            memset(msg, 0, 8);
            if (strcmp(action,"DOUB")==0 && dice[0] == dice[1]) {
              // Doubles rolled and pass is sent
              sprintf(msg, "%s", "%d,PASS");

            } else if (strcmp(action,"EVEN")==0 && diceSum % 2 == 0 && dice[0] != dice[1]) {
              // Even rolled and pass is sent
              sprintf(msg, "%s", "%d,PASS");

            } else if (strcmp(action,"ODD")==0 && diceSum % 2 == 1 && diceSum > 5) {
              // Odd rolled above 5 and pass is sent
              sprintf(msg, "%s", "%d,PASS");

            } else if (strcmp(action,"CON")==0 && diceSum == number) {
              // Choice from the player, below gets the number
              sprintf(msg, "%s", "%d,PASS");

            } else if (clientState.nlives > 1) {
              // Sends fail but still in the game
              clientState.nlives--;
              sprintf(msg, "%s", "%d,FAIL");

            } else if (clientState.nlives <= 1) {
              // Eliminate player from game
              sprintf(msg, "%s", "%d,ELIM");

            }

            //HOST MANAGE GAME RESULT
            /*----------------------------------------------------------------------------------------*/
            // players send the result to the host
            write(p1[1], msg, 8);
            sleep(5); // Round time

            // Host collects info for each round and determine outcome
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

              //HOST BROADCAST NEW NO.PLAYERS
              /*----------------------------------------------------------------------------------------*/
              // Send new player count to each process
              //memcpy(players, &nplayers, sizeof(int)); // Updates the player count
              char np[8]; // Number of players in string, increase size of scaling
              sprintf(np, "%d", nplayers);
              for (int i = 0; i < playersAlive - 1; i++) {
                write(p1[1], np, 8);
              }

              printf("Sent new player count, %d\n", nplayers);

              //PLAYERS' UPDATE
              /*----------------------------------------------------------------------------------------*/
            } else {
              // Clients that are not the host
              sleep(4);
              printf("Reading\n");
              char np[8];
              read(p1[0], np, 8);
              nplayers = atoi(np);
              printf("Read: %d\n", nplayers);

            }

            //WIN CONDITIONS
            /*----------------------------------------------------------------------------------------*/
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
