CC = gcc
CFLAGS = -g -Wall -pedantic -ansi 
OBJ = sds.o file.o 
EXEC = sds

$(EXEC) : $(OBJ)
	$(CC) -g -o sds -lm -lrt -pthread $(OBJ) 

file.o : file.c file.h 
	$(CC) $(CFLAGS) -c file.c  

sds.o : sds.c file.h 
	$(CC) -c sds.c 

clean :
	rm -f $(EXEC) $(OBJ)
