"""
CITS3002 Computer Networks Project
Written by Ethan Chin 22248878 and Daphne Yu 22531975

@brief: socket client for basic_server.c
		extended version of the given example code
"""


import socket,sched,time
from time import sleep
# Create a TCP/IP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

# Connect the socket to the port where the server is listening
server_address = ('localhost', 4444)
print ('connecting to %s port %s' % server_address)
sock.connect(server_address)

client_id = 0
nlives = -1		# update upon get the start message from the server

# Sends init to server connect to a game
message = 'INIT'.encode()
sock.sendall(message)


try:
    while True:

        exit = False

        # Look for the response
        amount_received = 0
        amount_expected = len(message)
        gameInfo = ["",""]

        while amount_received < amount_expected:

            try:
            	data = sock.recv(1024)
            except ConnectionResetError:
            	print("Lost connection with server.")
            	exit = True

            if data == 0:
              print("Lost connection with server")
              exit = True

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

                sock.sendall("{0},MOV,{1}".format(client_id,move).encode()) 

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
                sock.sendall("{0},MOV,{1}".format(client_id,move).encode())

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
                sock.sendall("{0},MOV,{1}".format(client_id,move).encode())

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

            elif "VICT" in server_msg or gameInfo[1] == 1:
                # ascii art credit: https://www.asciiart.eu/computers/smileys
                print('''
__          __  _____   _   _      _____
\ \        / / |_   _| | \ | |   .'     '.
 \ \  /\  / /    | |   |  \| |  /  o   o  \\
  \ \/  \/ /     | |   | . ` | |           |
   \  /\  /     _| |_  | |\  | |  \     /  |
    \/  \/     |_____| |_| \_|  \  '---'  /
                                 '._____.'
                                ''')

                exit = True
                break

            elif "REJECT" in server_msg:
                print("Request was rejected due to delayed connection.")
                exit = True
                break

            elif "CANCEL" in server_msg:
                print("Not enough player, game cannot start.")
                exit = True
                break

            else:
                print ( 'received "%s"' % server_msg)
                print("Unexpected message.")
                exit = True
                break

        if exit:
            break
finally:
    print ('closing socket.')
    sock.close()
