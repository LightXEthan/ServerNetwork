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
#define PIPE_BUFF_SIZE 14

#define ERR_CHECK_WRITE if (err < 0){fprintf(stderr,"Client write failed\n");exit(EXIT_FAILURE);}
#define ERR_CHECK_READ if (rec < 0){fprintf(stderr,"Client read failed\n");exit(EXIT_FAILURE);}
#define MIN_PLAYERS 4

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

/*----------------------------------------------------------------------------------------*/
// Anti-Cheat function
void watch_dog(char* buf, int client_id, int *number, char (*action)[]) {
  if (strstr(buf, "MOV") == NULL) {  // Check if the message contained 'move'
      fprintf(stderr, "Unexpected message, terminating\n");
      free(buf);
      exit(EXIT_FAILURE); //Kick player instead Tier4
  }

  char s[2] = ",";
  char *tok = strtok(buf, s);
  int counter = 0;
  bool isCON = false;
  //char action[5]; // Stores the action taken by the player
  // Scans the whole packet
  while ( tok != NULL) {
    switch (counter) {
      case 0:
        // Should be its own client id
        if (client_id != atoi(tok)) {
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
          sprintf(*action,"EVEN");
        } else if (strcmp(tok,"ODD")==0) {
          sprintf(*action,"ODD");
        } else if (strcmp(tok,"DOUB")==0) {
          sprintf(*action,"DOUB");
        } else if (strcmp(tok,"CON")==0) {
          sprintf(*action,"CON");
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
          *number = atoi(tok);
          printf("Number: %d\n", *number);
          if (*number < 2 || 12 < *number) {
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
    int pid, hid;
    int *nplayers;
    bool host = false;
    int p[2];

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
    if (pipe(p) < 0) {
      fprintf(stderr,"Could not pipe\n");
      exit(EXIT_FAILURE);
    }

    ////QUESTION: also do the clean up after we are done with shared memory, unmapping using munmap()

    // Create shared memory to check if game has started
    // Default value - GNS game has not started
    void* shmem_start = mmap(NULL, 4, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0);
    //check if mmap failed
    if(shmem_start == MAP_FAILED){fprintf(stderr,"Failed to create shared memory for start status.\n"); exit(EXIT_FAILURE);}
    memcpy(shmem_start, "GNS", 4); 


    // Create shared memory that stored number of players
    int* shmem_players =  (int*) mmap(NULL, 4, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0);
    if(shmem_players == MAP_FAILED){fprintf(stderr,"Failed to create shared memory for number of players\n"); exit(EXIT_FAILURE);}
    //interpret shmem_players as an integer and make nplayer_ptr points at where this shmem points at
    *shmem_players = 0;  nplayers = shmem_players;
    

    //CREATE A HOST SERVER
    /*----------------------------------------------------------------------------------------*/
    hid = fork();

    if (hid < 0) {
      fprintf(stderr,"Could not fork\n");
      exit(EXIT_FAILURE);
    } else if (hid == 0) {
      // Child process, will become host
      close(server_fd);
      host = true;
    }
    // Parent process will continue


    //MAIN LOOP
    /*----------------------------------------------------------------------------------------*/
    while (!host) {
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
        int round = 0;
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
        if (strstr(buf, "INIT") && strstr(shmem_start, "GNS")) {
            int client_id = 230 + *nplayers; // client id is dependent on number of players
            printf("INIT received, sending welcome.\n");

            memset(buf, 0, BUFFER_SIZE);
            sprintf(buf, "WELCOME,%d",client_id); // Gives client_id to the clients
            err = send(client_fd, buf, strlen(buf), 0);
            ERR_CHECK_WRITE;
            *nplayers += 1;

            //the first player entered the lobby signal the host to start counting down
            if(client_id == 230){
              buf[0] = '\0';
              sprintf(buf, "first join");
              write(p[1], buf, PIPE_BUFF_SIZE);
            }

            // Creates clientStates
            clientState.client_id = client_id;
            clientState.nlives = 2;

        } else {
            // Rejects when game has already started
            err = send(client_fd, "REJECT", 6, 0);
            ERR_CHECK_WRITE;
            exit(EXIT_FAILURE);
        }

        //SIGNAL START
        /*----------------------------------------------------------------------------------------*/
        sleep(12);

        if(strstr(shmem_start, "GHS")){
          memset(buf, 0, BUFFER_SIZE);
          sprintf(buf, "START,%d,%d\n", *nplayers, clientState.nlives); 
          send_message(buf, client_fd, clientState.client_id); 
        }
        else{
          memset(buf, 0, BUFFER_SIZE);
          sprintf(buf, "CANCEL"); 
          send_message(buf, client_fd, clientState.client_id); 
          exit(EXIT_FAILURE);
        }

        //LOOP EACH GAME ROUND
        /*----------------------------------------------------------------------------------------*/
        while (true) {
            round++;
            //CLIENT READS THE DICE FROM HOST
            /*----------------------------------------------------------------------------------------*/
            int dice[2];
            char dice1[3];
            char dice2[3];

            read(p[0],dice1,PIPE_BUFF_SIZE);
            dice[0] = atoi(dice1);
            sleep(0.1);
            read(p[0],dice2,PIPE_BUFF_SIZE);
            dice[1] = atoi(dice2);
            printf("Client Dice read\n");

            int diceSum = dice[0] + dice[1];
            sleep(1);
            // Waits for move from player
            memset(buf, 0, BUFFER_SIZE);
            rec = recv(client_fd, buf, BUFFER_SIZE, 0); // See if we have a response
            ERR_CHECK_READ;

            // Watch-Dog, anti-cheat detection, checks that the player sent a vaild packet
            printf("%s\n", buf);
            int number; // Stores the number selected by the player
            char action[5]; // Stores the action taken by the player
            watch_dog(buf, clientState.client_id, &number, &action);

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

            // players send the result to the host
            write(p[1], msg, PIPE_BUFF_SIZE);

            //PLAYERS' UPDATE
            /*----------------------------------------------------------------------------------------*/
            sleep(2);
            printf("Updating new nplayers\n");

            //WIN CONDITIONS
            /*----------------------------------------------------------------------------------------*/
            if (*nplayers == 0) {
              // Everyone wins if all get eliminated
              sprintf(msg, "%s", "%d,VICT");
              //printf("msg: %s\n",msg);
              send_message(msg, client_fd, clientState.client_id);
              free(buf);
              exit(EXIT_SUCCESS);

            } else if (*nplayers == 1 && (strstr(msg, "PASS") || strstr(msg, "FAIL"))) {
              // If one person alive then they get vict
              printf("Victory Last alive. %d\n", *nplayers);
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
    // End of while Loop
    // HOST CODE
    /*----------------------------------------------------------------------------------------*/
    if (host) {
      fd_set set;

      struct timeval timeout;
      FD_ZERO(&set);
      FD_SET(p[0],&set);
      timeout.tv_sec = 10; // Timeout time
      timeout.tv_usec = 0;
      char inbuf[13];

      printf("###Host: player can join at any time...\n");

      // Counts number of players
      while (true) {

        int rv = select(p[0]+1, &set, NULL, NULL, 0);
        if (rv == -1) {
          perror("Error with select\n");
        } 
        else {
          read(p[0],inbuf,PIPE_BUFF_SIZE);

          if (strcmp(inbuf, "first join") == 0) {
            printf("###Host: lobby open, start counting down...\n");

            sleep(10);

            if(*nplayers == 1 || *nplayers > MIN_PLAYERS){
              memcpy(shmem_start, "GHS", 4); 
            }
          }
        }
      sleep(3);

      // Roll the dice
      /*----------------------------------------------------------------------------------------*/
      while (true) {
        int dice[2];
        char dice1[3];
        char dice2[3];

        srand(time(0));
        dice[0] = rand() % 6 + 1;
        dice[1] = rand() % 6 + 1;

        printf("###Host: dice one roll=  %d\n",dice[0]);
        printf("###Host: dice two roll=  %d\n",dice[1]);

        // Host pipes dice roll to other players
        // Each child gets the dice
        for (int i = 0; i < *nplayers; i++) {
          sprintf(dice1, "%d", dice[0]);
          write(p[1],dice1,PIPE_BUFF_SIZE);
          sleep(0.1);
          sprintf(dice2, "%d", dice[1]);
          write(p[1],dice2,PIPE_BUFF_SIZE);
        }
        sleep(1);

        //HOST MANAGE GAME RESULT
        /*----------------------------------------------------------------------------------------*/
        // Pipe Children -> Host, if they passed or died
        // Host collects info for each round and determine outcome
        int elims = 0;
        int fails = 0;
        int pass = 0;
        int playersAlive = 0;
        char rmsg[8];
        fd_set set2;
        FD_ZERO(&set2);
        FD_SET(p[0],&set2);

        // Count number of each result
        timeout.tv_usec = 0;
        timeout.tv_sec = 10; // Timeout time

        printf("###Host: Waiting for player actions.\n");
        while (true) {
          int rv = select(p[0]+1, &set2, NULL, NULL, &timeout);
          if (rv == -1) {
            perror("###Host: Error with select\n");
          } else if (rv == 0) {
            printf("###Host: player result sending timeout.\n");
            break; // After timeout

          } else {
            memset(rmsg, 0, 8);
            rmsg[8] = '\0';
            read(p[0], rmsg, PIPE_BUFF_SIZE);
            printf("###Host: result from player=  %s\n", rmsg);

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
        *nplayers = elims + pass + fails;
        playersAlive = *nplayers;
        printf("###Host: elim=%d, pass=%d, fails=%d\n", elims, pass, fails);

        // If everyone is elim, then everyone gets vict
        if (elims == *nplayers) {
          *nplayers = 0; // 0 when all players die, so everyone wins

        } else {
          *nplayers -= elims;
        }

        //HOST BROADCAST NEW NO.PLAYERS
        /*----------------------------------------------------------------------------------------*/
        if (*nplayers <= 1) {
          // End game
          // Kill the parent?
          printf("###Host: game over.");
          free(buf);
          exit(EXIT_SUCCESS);
        }

      }
    }
}
}