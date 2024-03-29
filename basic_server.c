/**
* CITS3002 Computer Networks Project
* Written by Ethan Chin 22248878 and Daphne Yu 22531975
*
**/

/**
 * @brief RNG Battle Royal server using socket programming
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
#include <ctype.h>
#include <fcntl.h>

#define BUFFER_SIZE 1024
#define PIPE_BUFF_SIZE 14
#define MIN_PLAYERS 4

#define ERR_CHECK_WRITE if (err < 0){fprintf(stderr,"Client write failed\n");exit(EXIT_FAILURE);}
#define ERR_CHECK_READ if (rec < 0){fprintf(stderr,"Client read failed\n");exit(EXIT_FAILURE);}


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
// Closes the socket and exits the process
void close_socket(int client_fd) {
  if (close(client_fd) != 0){
      fprintf(stderr,"Socket close unsuccessfully.\n");
  }
  exit(EXIT_SUCCESS);
}

/*----------------------------------------------------------------------------------------*/
// Anti-Cheat function, returns 0 if invalid packet, 1 otherwise
int watch_dog(char* buf, int client_id, int *number, char (*action)[]) {
  if (strstr(buf, "MOV") == NULL) {  // Check if the message contained 'move'
      fprintf(stderr, "Unexpected message, terminating\n");
      return 0;
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
          return 0;
        }
        break;

      case 1:
        // Should be MOV
        if (strcmp("MOV",tok) != 0) {
          // Kick for cheating
          printf("Kicked for cheating\n");
          return 0;
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
          return 0;
        }
        break;

      case 3:
        // Should be a 2 digit int only iff move was CON
        if (isCON) {
          //tok[2] = '\0';
          *number = atoi(tok);
          printf("Number: %d\n", *number);
          if (*number < 1 || 6 < *number) {
            // Player entered invalid number
            fprintf(stderr, "Invalid number.\n");
            return 0;
          }
        } else {
          // kick for cheating
          printf("Kicked for cheating\n");
          return 0;
        }
        break;

      default:
        // Kick for cheating
        printf("Kicked for cheating\n");
        return 0;

    }
    tok = strtok(NULL,s);
    counter++;
  }
  if (counter == 2) {
    printf("Kicked for cheating\n");
    return 0;
  }
  return 1;
}

//upon success creation return server file descriptor, otherwise, -1
int initiate_sock(int port){
  int server_fd, opt_val, err;
  struct sockaddr_in server;

  server_fd = socket(AF_INET, SOCK_STREAM, 0);

  if (server_fd < 0){
    return -1;
  }

  server.sin_family = AF_INET;
  server.sin_port = htons(port);
  server.sin_addr.s_addr = htonl(INADDR_ANY);

  opt_val = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof opt_val);

  err = bind(server_fd, (struct sockaddr *) &server, sizeof(server));
  if (err < 0){
    fprintf(stderr,"Could not bind socket\n");
    return -1;
  }

  err = listen(server_fd, 128);
  if (err < 0){
    fprintf(stderr,"Could not listen on socket\n");
    return -1;
  }

  printf("Server is listening on %d\n", port);
  return server_fd;
}

//return -1 if no move after timeout, otherwise 0
bool wait_move(int client_fd, char *buf){
	struct timeval mvtout;
    mvtout.tv_sec = 10; //wait move response for 10 sec
    mvtout.tv_usec = 0;

    //char *buf = calloc(BUFFER_SIZE, sizeof(char));
  	memset(buf, 0, BUFFER_SIZE);

    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &mvtout, sizeof(struct timeval));
    int rec = recv(client_fd, buf, BUFFER_SIZE, 0); // See if we have a response

    if( rec < 0){
      fprintf(stderr,"No move response from the client. LIVE - 1\n");
      return false;
  	}
  	return true;
}

//MAIN
/*----------------------------------------------------------------------------------------*/
int main (int argc, char *argv[]) {

    if (argc < 2){
      fprintf(stderr,"Usage: %s [port]\n",argv[0]);
      exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);
    int server_fd, client_fd, err;
    struct sockaddr_in client;
    char *buf;
    int pid, hid;
    int nplayers = 0;
    bool host = false;
    int p1[2], p2[2];

    //CREATE SOCKET
    /*----------------------------------------------------------------------------------------*/

    server_fd = initiate_sock(port);
    if (server_fd < 0){
      fprintf(stderr,"Could not create socket\n");
      exit(EXIT_FAILURE);
    }

    //PIPE & SHARED MEMORY
    /*----------------------------------------------------------------------------------------*/
    // Setup Pipe for inter-process communication
    // Pipe child to host
    if (pipe(p1) < 0) {
      fprintf(stderr,"Could not pipe\n");
      exit(EXIT_FAILURE);
    }

    // Pipe host to child
    if (pipe(p2) < 0) {
      fprintf(stderr,"Could not pipe\n");
      exit(EXIT_FAILURE);
    }

    // Create shared memory to check if game has started
    void* shmem = mmap(NULL, 4, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0);
    memcpy(shmem, "GNS", 4); // Default value, game has not started

    // Create host process
    hid = fork();

    if (hid < 0) {
      fprintf(stderr,"Could not fork\n");
      exit(EXIT_FAILURE);
    } else if (hid == 0) {
      // Child process, will become host
      close(server_fd);
      host = true;
      close(p1[1]);
      close(p2[0]);
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

        //GAME VARIABLES
        /*----------------------------------------------------------------------------------------*/
        int round = 0;
        struct ClientState {
          int client_id;
          int nlives;
        } clientState;


        //START READING MESSAGE FROM CLIENTS
        /*----------------------------------------------------------------------------------------*/
        buf = calloc(BUFFER_SIZE, sizeof(char)); // Clear our buffer so we don't accidentally send/print garbage
        int rec = recv(client_fd, buf, BUFFER_SIZE, 0);    // Read from the incoming client
        ERR_CHECK_READ;

        //INIT & REJECT
        /*----------------------------------------------------------------------------------------*/
        // Receives client request to enter game
        if (strstr(buf, "INIT") && strstr(shmem, "GNS")) {
            int client_id = 230 + nplayers; // client id is dependent on number of players
            printf("INIT received, sending welcome.\n");

            memset(buf, 0, BUFFER_SIZE);
            sprintf(buf, "WELCOME,%d",client_id); // Gives client_id to the clients
            send_message(buf, client_fd, client_id);

            // Creates clientStates
            clientState.client_id = client_id;
            clientState.nlives = 3;

        } else {
            // Rejects when game has already started
            err = send(client_fd, "REJECT", 6, 0);
            ERR_CHECK_WRITE;
            free(buf);
            exit(EXIT_FAILURE);
        }

        //COUNTING PLAYERS & TIMEOUT & DECIDE START OR CANCEL
        /*----------------------------------------------------------------------------------------*/
        // Pipe to all processes that the game has started

        close(p1[0]);
        close(p2[1]);

        // Everyone send how many players they know to the pipe
        char players[PIPE_BUFF_SIZE];
        memset(players, 0, PIPE_BUFF_SIZE);
        sprintf(players, "%d",nplayers);
        write(p1[1], players, PIPE_BUFF_SIZE);

        //read in finalised nplayers
        char inbuf[PIPE_BUFF_SIZE];
        memset(inbuf, 0, PIPE_BUFF_SIZE);
        read(p2[0],inbuf, PIPE_BUFF_SIZE);
        nplayers = atoi(inbuf);
        //printf("Read from host: %d\n", nplayers);

        //single player mode: given 3 lives, win if survive 5 rounds
        bool singlemode = false;
        if(nplayers == 1){
          singlemode = true;
          clientState.nlives = 3;
        }
        else if(nplayers < MIN_PLAYERS){
          err = send(client_fd, "CANCEL", 6, 0);
          ERR_CHECK_WRITE;
          printf("Gameover,cleaning memory......\n");
          free(buf);
          close_socket(client_fd);
          exit(EXIT_SUCCESS);
        }

        // Send start to players
        memset(buf, 0, BUFFER_SIZE);
        sprintf(buf, "START,%d,%d\n",nplayers,clientState.nlives);
        send_message(buf, client_fd, clientState.client_id);

        //LOOP EACH GAME ROUND
        /*----------------------------------------------------------------------------------------*/
        while (true) {
            round++;

            //ROLLS THE DICE
            /*----------------------------------------------------------------------------------------*/
            int dice[2];
            srand(time(0));
            dice[0] = rand() % 6 + 1;
            dice[1] = rand() % 6 + 1;
            //printf("Dice rolled: %d, %d\n", dice[0], dice[1]);
            int diceSum = dice[0] + dice[1];

            bool moved = wait_move(client_fd, buf);

            int number; // Stores the number selected by the player
            char action[5]; // Stores the action taken by the player
            memset(action, 0, 5);

            if (moved) {
              // Watch-Dog, anti-cheat detection, checks that the player sent a vaild packet
              int wd = watch_dog(buf, clientState.client_id, &number, &action);
              if (wd == 0) {
                free(buf);
                close_socket(client_fd);
              }
            }

            //DECIDE PASS, FAIL, ELIM
            /*----------------------------------------------------------------------------------------*/
            // Calculate score using the players move
            char msg[PIPE_BUFF_SIZE];
            memset(msg, 0, PIPE_BUFF_SIZE);
            if (strcmp(action,"DOUB")==0 && dice[0] == dice[1]) {
              // Doubles rolled and pass is sent
              sprintf(msg, "%s", "%d,PASS");

            } else if (strcmp(action,"EVEN")==0 && diceSum % 2 == 0 && dice[0] != dice[1]) {
              // Even rolled and pass is sent
              sprintf(msg, "%s", "%d,PASS");

            } else if (strcmp(action,"ODD")==0 && diceSum % 2 == 1 && diceSum > 5) {
              // Odd rolled above 5 and pass is sent
              sprintf(msg, "%s", "%d,PASS");

            } else if (strcmp(action,"CON")==0 && (dice[0] == number || dice[1] == number)) {
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
            write(p1[1], msg, PIPE_BUFF_SIZE);

            //PLAYERS' UPDATE
            /*----------------------------------------------------------------------------------------*/
            char np[PIPE_BUFF_SIZE];
            memset(np, 0, PIPE_BUFF_SIZE);
            read(p2[0], np, PIPE_BUFF_SIZE);
            nplayers = atoi(np);
            //printf("Client nplayers Read: %d\n", nplayers);

            // Display purposes only
            printf("Dice rolled: %d, %d\n", dice[0], dice[1]);

            //WIN CONDITIONS
            /*----------------------------------------------------------------------------------------*/
            char winner[BUFFER_SIZE];
            bool gameover = false;

            if (!singlemode && nplayers == 0) {
              // Everyone wins if all get eliminated
              sprintf(msg, "%s", "%d,VICT");

              sprintf(winner, "%d", clientState.client_id);
              write(p1[1], winner, PIPE_BUFF_SIZE);

              gameover = true;

            } else if (nplayers == 0){
              sprintf(msg, "%s", "%d,ELIM");
              gameover = true;

            } else if (!singlemode && nplayers == 1 && (strstr(msg, "PASS") || strstr(msg, "FAIL"))) {
              // If one person alive then they get vict
              printf("Victory Last alive. %d\n", nplayers);
              sprintf(msg, "%s", "%d,VICT");

              sprintf(winner, "%d", clientState.client_id);
              write(p1[1], winner, PIPE_BUFF_SIZE);

              gameover = true;

            } else if (singlemode && nplayers == 1 && (strstr(msg, "PASS") || strstr(msg, "FAIL")) && round == 5) {
              // single player mode: if in round 5 after action the player's life still >= 0, the player win
              printf("Champion! Survived 5 rounds!");
              sprintf(msg, "%s", "%d,VICT");
              gameover = true;

            } else if (strstr(msg, "ELIM")) {
              sprintf(msg, "%s", "%d,ELIM");
              gameover = true;
            }

            //printf("msg: %s\n",msg);
            // Sends the outcome to the player
            send_message(msg, client_fd, clientState.client_id);

            if(gameover) break;//if game over, go to clean memory part


        }//end of game handling while

        printf("Gameover,cleaning memory......\n");
        free(buf);
        close_socket(client_fd);
    }
    // End of while Loop


    // HOST CODE
    /*----------------------------------------------------------------------------------------*/
    while (host) {
      printf("Host is setting up game.\n");
      fd_set set;

      struct timeval timeout;
      FD_ZERO(&set);
      FD_SET(p1[0],&set);
      timeout.tv_sec = 10; // Timeout time
      timeout.tv_usec = 0;
      char inbuf[PIPE_BUFF_SIZE];
      memset(inbuf, 0, PIPE_BUFF_SIZE);
      bool singlemode = false;

      memcpy(shmem, "GNS", 4);
      printf("Lobby open...\n");

      // Counts number of players
      nplayers = 0;
      while (true) {
        int rv = select(p1[0]+1, &set, NULL, NULL, &timeout);
        if (rv == -1) {
          perror("Error with select\n");
        } else if (rv == 0) {//when timeout
          memcpy(shmem, "GHS", 4); // Puts into shared memory that game has started
          printf("Players can no longer join.\nNumber of players: %d\n", nplayers);
          break;

        } else {
          read(p1[0],inbuf,PIPE_BUFF_SIZE);
          printf("Player Joined.\n");
          // Gets the highest number which is the player count
          nplayers++;
        }
      }

      // Start the game, host sends number of players to players
      char buff[PIPE_BUFF_SIZE];
      memset(buff, 0, PIPE_BUFF_SIZE);
      sprintf(buff, "%d",nplayers);

      // Sends number of players to the child processes
      for (int i = 0; i < nplayers; i++) {
        write(p2[1], buff, PIPE_BUFF_SIZE);
      }
      printf("Host sent players: %d\n", nplayers);

      // Single player mode when there is one person
      if (nplayers == 1){
        singlemode = true;
      }
      else if(nplayers < MIN_PLAYERS){
        printf("Host:game has been cancelled. New game start in 10 seconds.\n");
        sleep(10);
        continue;
      }

      sleep(1);

      int round = 0;
      // Roll the dice
      /*----------------------------------------------------------------------------------------*/
      while (true) {
        round++;

        //HOST MANAGE GAME RESULT
        /*----------------------------------------------------------------------------------------*/
        // Pipe Children -> Host, if they passed or died
        // Host collects info for each round and determine outcome
        int elims = 0;
        int fails = 0;
        int pass = 0;
        int playersAlive = 0;
        char rmsg[PIPE_BUFF_SIZE];
        memset(rmsg, 0, PIPE_BUFF_SIZE);
        FD_ZERO(&set);
        FD_SET(p1[0],&set);

        // Count number of each result
        timeout.tv_usec = 0;
        timeout.tv_sec = 11; // Timeout time

        printf("Host: Waiting for player actions...\n");
        while (true) {
          int rv = select(p1[0]+1, &set, NULL, NULL, &timeout);
          if (rv == -1) {
            perror("Host: Error with select\n");
          } else if (rv == 0) {
            printf("Host: Round Ended.\n");
            break; // After timeout

          } else {
            read(p1[0], rmsg, PIPE_BUFF_SIZE);
            //printf("Host Read: %s\n", rmsg);

            // Count number of outcomes
            if (strstr(rmsg, "ELIM")) {
              elims++;
            } else if (strstr(rmsg, "PASS")) {
              pass++;
            } else if (strstr(rmsg, "FAIL")) {
              fails++;
            }

            if (singlemode) {
              sleep(1); break;
            }
          }
        }

        // Finds new number of playersAlive
        nplayers = elims + pass + fails;
        playersAlive = nplayers;
        //printf("Host N: %d, %d, %d\n", elims, pass, fails);

        bool edge = false;
        // If everyone is elim, then everyone gets vict
        if (elims == nplayers) {
          edge = true;
          nplayers = 0; // 0 when all players die, so everyone wins

        } else {
          nplayers -= elims;
        }

        //HOST BROADCAST NEW NO.PLAYERS
        /*----------------------------------------------------------------------------------------*/
        // Send new player count to each process
        char np[PIPE_BUFF_SIZE];
        memset(np, 0, PIPE_BUFF_SIZE);
        sprintf(np, "%d", nplayers);
        for (int i = 0; i < playersAlive; i++) {
          write(p2[1], np, PIPE_BUFF_SIZE);
        }

        printf("Round ended. Players alive: %d\n", nplayers);

        sleep(2);

        if (nplayers <= 1 && !singlemode) {
          // End game

          //read the final game result from the pipe p1
          printf("\t#######################################\n");
          printf("\t##     Game ended after %2d rounds    ##\n",round);
          round = 0;
          if(edge){//when more than one player died at the same time
            for (int i = 0; i < elims; i++){
              read(p1[0], rmsg, PIPE_BUFF_SIZE);
              printf("\t##\t\t%swins!\t     ##\n", rmsg);
            }
          }
          else{//one player survive at the end
              read(p1[0], rmsg, PIPE_BUFF_SIZE);
              printf("\t##\t\t%swins!\t     ##\n", rmsg);
          }

          printf("\t##  Next game starts in 10 seconds   ##\n");
          printf("\t#######################################\n");
          sleep(10);
          break;
        } else if (singlemode && nplayers == 0) {
          // End game
          printf("########### Game ended after %d rounds ##########\n", round);
          printf("######## Next game starts in 10 seconds ########\n");
          round = 0;
          sleep(10);
          break;
        }

      }//end of game playing while
    }//end of host while
}
