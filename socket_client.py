"""
This is a simple example of a client program written in Python.
Again, this is a very basic example to complement the 'basic_server.c' example.


When testing, start by initiating a connection with the server by sending the "init" message outlined in
the specification document. Then, wait for the server to send you a message saying the game has begun.

Once this message has been read, plan out a couple of turns on paper and hard-code these messages to
and from the server (i.e. play a few rounds of the 'dice game' where you know what the right and wrong
dice rolls are). You will be able to edit this trivially later on; it is often easier to debug the code
if you know exactly what your expected values are.

From this, you should be able to bootstrap message-parsing to and from the server whilst making it easy to debug.
Then, start to add functions in the server code that actually 'run' the game in the background.
"""

'''
THINGS TO DO:
@ adding timeout, deal with unexpected message sent by server
@ think of how can the client cheat on server
@ reconnect after some time period when the request has been cancelled?
@ terminate when server shut down/reset e.g. keyboard interrupt
@ things to do when the waiting after move getting too long

'''

import socket
from time import sleep
# Create a TCP/IP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

# Connect the socket to the port where the server is listening
server_address = ('localhost', 4444)
print ('connecting to %s port %s' % server_address)
sock.connect(server_address)

# client status
client_id = 0
# make it -1 for now, update when get the start message from the server
nlives = -1

# Sends init to server connect to a game
message = 'INIT'.encode()
sock.sendall(message)

try:
    while True:

        exit = False

        # Look for the response
        amount_received = 0
        amount_expected = len(message)

        while amount_received < amount_expected:
            data = sock.recv(1024)
            amount_received += len(data)
            server_msg = data.decode()
            print("Recieved: " + server_msg)

            if "WELCOME" in server_msg:
                client_id = server_msg[-3:] # Gets the client_id
                print("My client id is: " + client_id)

            elif "START" in server_msg:
                print("The games has started!")

                gameInfo = server_msg.split(",")
                nlives = int(gameInfo[2])
                print("Players Left: " + gameInfo[1])
                print("Lives   Left: " + gameInfo[2])

                # first move
                move = str(input("Your move: "))
                # sendall():continues to send data until either all data has been sent or an error occurs. None is returned on success.
                # TODO: check exception??
                sock.sendall("{0},MOV,{1},0".format(client_id,move).encode()) # Client has ID 231

            elif "PASS" in server_msg:
                # make move
                print('''
 ____   __   ____  ____
/ ___) / _\ (  __)(  __)
\___ \/    \ ) _)  ) _)
(____/\_/\_/(__)  (____)
                ''')
                print("Lives Left: " + str(nlives))
                move = str(input("Your move: "))
                sock.sendall("{0},MOV,{1},0".format(client_id,move).encode())

            elif "FAIL" in server_msg:
                nlives -= 1
                print('''
  _  _   _   _  _
 | \| | /_\ | || |
 | .` |/ _ \| __ |  _   _
 |_|\_/_/ \_\_||_| (_) (_)
                        ''')
                print("Lives Left: " + str(nlives))
                move = str(input("Your move: "))
                sock.sendall("{0},MOV,{1},0".format(client_id,move).encode())

            elif "ELIM" in server_msg:
                print("You lost, good game.")
                # ascii art credit: http://patorjk.com/software/taag/#p=display&f=Graffiti&t=Type%20Something%20
                print('''
                      ______      _____ _
                     |  ____/\   |_   _| |
                     | |__ /  \    | | | |
                     |  __/ /\ \   | | | |
                     | | / ____ \ _| |_| |____
                     |_|/_/ ___\_\_____|______|
                     \ \   / / __ \| |  | |
                      \ \_/ / |  | | |  | |
                       \   /| |  | | |  | |
                        | | | |__| | |__| |
                       _|_|_ \____/ \____/ _  __
                      / ____| |  | |/ ____| |/ /
                     | (___ | |  | | |    | ' /
                      \___ \| |  | | |    |  <
                      ____) | |__| | |____| . \\
                     |_____/ \____/ \_____|_|\_\\

                               ''')
                exit = True
                break

            elif "VICT" in server_msg:
                # ascii art credit: https://www.asciiart.eu/computers/smileys
                print('''
                         __          __  _____   _   _        _____
                         \ \        / / |_   _| | \ | |     .'     '.
                          \ \  /\  / /    | |   |  \| |    /  o   o  \
                           \ \/  \/ /     | |   | . ` |   |           |
                            \  /\  /     _| |_  | |\  |   |  \     /  |
                             \/  \/     |_____| |_| \_|    \  '---'  /
                                                            '._____.'
                                    ''')

                exit = True
                break

            elif "REJECT" in server_msg:
                print("Request was rejected due to delayed connection")
                exit = True
                break

            elif "CANCEL" in server_msg:
                print("Not enough player, game cannot start")
                # attempt to connect again??

            else:
                print ( 'received "%s"' % server_msg)
                sleep(5) # Sleep time in seconds

        if exit:
            break
finally:
    print ('closing socket')
    sock.close()
