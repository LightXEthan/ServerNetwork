CFLAGS = -g -Wall
OUT_FILE_NAME_SERVER = basic_server 
SRCS_SERVER = basic_server.c

CC = gcc 

server: ${SRCS_SERVER} 
	${CC} ${CFLAGS} -o ${OUT_FILE_NAME_SERVER} ${SRCS_SERVER} 

clean:
	rm -f *.o *.out ${OUT_FILE_NAME_CLIENT} ${OUT_FILE_NAME_SERVER}


