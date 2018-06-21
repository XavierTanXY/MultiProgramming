CC = gcc
CFLAGS = -g -Wall -pedantic -ansi 
OBJ = sds.o file.o 
EXEC = sds

$(EXEC) : $(OBJ)
	$(CC) -g -o sds -lm -pthread $(OBJ) 

file.o : file.c file.h 
	$(CC) $(CFLAGS) -c file.c  

ass.o : sds.c file.h 
	$(CC) -c sds.c 

clean :
	rm -f $(EXEC) $(OBJ)
