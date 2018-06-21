/**
 * FILE:	sds.c
 * AUTHOR:	Xhien Yi Tan
 * STUDENTID:	18249833
 * UNIT:	COMP2006
 * PURPOSE:	This file is responsible for running the program using process, especially Reader-Writer Problem
 */

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <semaphore.h>

#include "file.h"

#define READER 0
#define WRITER 1
#define B 20
#define D 100
#define MAX_PROCESS 7

//Shared variables
int *dataFromFile, *dataBuffer;
char *filename = "shared_data.txt", *outputFileName = "sim_out.txt";
int t1, t2, dataReadFilePosition = 0,  bufferIndex = 0, b = B, d = D;

sem_t *writer_sem, *reader_sem, *consume_sem, *condR_sem, *condW_sem, *output_sem;
int data_buffer[B]; 
int dataReadFromReader = 0, readersStopped = 0, initNumWriter = 0, initNumReader = 0, finish = 0, totalDataHasBeenRead = 0;

int i, pid, shm_DataFromFile,shm_DataBuffer;
int *ptr_readCount, *ptr_writeCount, *ptr_initNumReader, *ptr_initNumWriter, *ptr_t1, *ptr_t2;
int *ptr_done, *ptr_finish, *ptr_dataReadFilePosition, *ptr_totalDataHasBeenRead, *ptr_dataReadFromReader, *ptr_readerStopped;
void *ptr_DataFromFile, *ptr_DataBuffer;


/* Struct for process:
 * Counter - each process has this counter
 * id - id for each process
 */
typedef struct{
	int counter, id;
} Process;


/**
* PURPOSE : Consume data and increases each reader's counter
* IMPORTS : Reader process
* EXPORTS : None
*/ 
void *consumeData(void* readerProcess) {
	int i;
	//If no data has been read by writers or data buffer is empty, waits
	if ( *ptr_totalDataHasBeenRead == 0 )
	{
		/* While this reader process waits, unlocks other reader processes.
		 * At the same time wake up writers to read and write.
		 */
		sem_post(reader_sem);
		sem_post(writer_sem);
		sem_wait(condR_sem);

	} else {
	
		//From 0 to b - meaning from 0 to buffer size
		for (i = 0; i < *ptr_totalDataHasBeenRead; i++) {
			
			*ptr_dataReadFromReader = *ptr_dataReadFromReader + 1;

			//Increases the reader counter when it reads data
			if ( ( ( ( Process* )readerProcess )->counter ) < d ) {
				( ( ( Process* )readerProcess )->counter ) = ( ( ( Process* )readerProcess )->counter ) + 1;
			}
			
			
			//When it reads until the end of data buffer
			if ( (i+1) == b )
			{
				//Keep track number of waiting readers
				*ptr_readerStopped = *ptr_readerStopped + 1;

				//When all readers finished reading - wake up writers
				if ( *ptr_readerStopped == initNumReader )
				{
					//Reset the index of the data buffer to 0
					*ptr_dataReadFilePosition = 0;
					*ptr_readerStopped = 0;
					sem_post(condW_sem);					
				}
				
				// When there is one last writer and this reader has finished reading all the data in the shared_data
				if ( *ptr_writeCount == 0 && ( ( ( Process* )readerProcess )->counter ) == d )
				{
					//Set finish
					*ptr_finish = 1;
				} else {
					
					//Else wake up writers and wait
					sem_post(writer_sem);
					sem_post(consume_sem);
					sem_post(condW_sem);
					sem_wait(condR_sem);

				}
			}

		}
	}

	return NULL;
}


/**
* PURPOSE : Read data from buffer and print read data
* IMPORTS : Reader Process
* EXPORTS : None
*/ 
void *readFromBuffer(void *readerProcess) {

	//Reading data together - uncomment this if you want to print
	/*
	 *for (int i = 0; i < b; i++) {
	 *	printf("Reader ID %d priting data_buffer data - %d\n", ( ( ( Process* )readerProcess )->id ), ( ( int* )ptr_DataBuffer)[i]);
	 } */
	return NULL;
}

/**
* PURPOSE : Method for readers:
*			- To start reading from the buffer
*			- Sleeps after incrementing counter and reading
* IMPORTS : Reader process
* EXPORTS : None
*/ 
void* reader(void *readerProcess) {

	int done = 0, i;

	do {

		//Lock CS for updating read count
		sem_wait( reader_sem );
		*ptr_readCount = *ptr_readCount + 1;

		if ( *ptr_readCount == 1) {
		    sem_wait( writer_sem );
		}

		sem_post( reader_sem );
		

		//Read data from buffer together
		readFromBuffer(readerProcess);
		sleep( *ptr_t1 );

		//Lock CS for consuming data - consume one after one 
		sem_wait( consume_sem );
		consumeData(readerProcess);
		sem_post( consume_sem );


		/* When the finish flag is set to 1 by writer and this reader has finished consuming,
		 * set done flag = 1 means done.
		 */
		if (*ptr_finish == 1 && ( ( ( Process* )readerProcess )->counter ) == d ) {
			done = 1;
		}

		//Lock CS for decrement read count
		sem_wait( reader_sem );

		*ptr_readCount = *ptr_readCount - 1;

		if ( *ptr_readCount == 0) {
			//Wake up remaining writers
			sem_post( condW_sem );
		    	sem_post( writer_sem );
		}

		sem_post( reader_sem );
	} while( done != 1 );

	printf("Reader ID %d has finished reading %d pieces of data from the data_buffer \n", ( ( ( Process* )readerProcess )->id ),( ( ( Process* )readerProcess )->counter ) );


	//Lock CS for writing data to shared output file
	sem_wait( output_sem );
	writeToFile(outputFileName, readerProcess, READER);
	sem_post( output_sem );




	return NULL;
}

/**
* PURPOSE : Read data from shared_data and put it into buffer
* IMPORTS : Writer process
* EXPORTS : None
*/ 
void *readFromSharedData(void *writerProcess) {

	int done = 0;

	do {

		//Increments when data has been read by writers
		*ptr_dataReadFilePosition = *ptr_dataReadFilePosition + 1;
			
		//This is where EOF happens
		if ( *ptr_totalDataHasBeenRead == d ) {

	  		//Set flags done and fnish to 1
	  		done = 1;
	  		*ptr_finish = 1;

	  	//If the buffer is full	
	  	} else if ( *ptr_dataReadFilePosition > b ) {	

	  		//While the buffer is full, writer waits
			while( *ptr_dataReadFilePosition > b ) {
		  		
		  		//If fnished the stops this process
		  		if ( *ptr_finish == 1 )
		  		{
		  			done = 1;
		  			break;
		  		}
	
	
		  		/* Wake up other writers and readers that are waiting,
				 * then waits since buffer is full.
		  		 */
				sem_post(writer_sem);
				sem_post(condR_sem);
	  			sem_wait(condW_sem);
	
	  		}

	  	} else {
	  
	  		//Put data into buffer from shared_data and increases the counter for this writer
	  		*ptr_totalDataHasBeenRead = *ptr_totalDataHasBeenRead + 1;
	  		( ( ( Process* )writerProcess )->counter ) = ( ( ( Process* )writerProcess )->counter ) + 1;
	  		( ( int* )ptr_DataBuffer)[*ptr_dataReadFilePosition - 1 ] = ( ( int* )ptr_DataFromFile)[*ptr_dataReadFilePosition - 1];
	  	}
	  	
	} while( done != 1);



	return NULL;
}

/**
* PURPOSE : Method for writers:
*			- To start reading from the shared_data and put to buffer
*			- Sleeps after incrementing counter and writing
* IMPORTS : Writer process
* EXPORTS : None
*/ 
void* writer(void *writerProcess) {

	int i;
	//Lock critical section for writer 
	sem_wait( writer_sem );

	*ptr_writeCount = *ptr_writeCount + 1;

	//Read from shared_data and put into buffer
	readFromSharedData(writerProcess);
	sleep(*ptr_t2);

	//Unlock critical section for writer
	sem_post( writer_sem );

	/* When one of the writers finish reading, 
	 * means that data_buffer has been finished reading.
	 * Set finish flag = 1, let all others processes knows.
	 */
	*ptr_finish = 1;

	*ptr_writeCount = *ptr_writeCount - 1;

	printf("writer-%d has finished writing %d pieces of data to the data_buffer \n", ( ( ( Process* )writerProcess )->id ), ( ( ( Process* )writerProcess )->counter ));

	//Another CS for writing the result to a shared output file
	sem_wait( output_sem );
	writeToFile(outputFileName, writerProcess, WRITER);
	sem_post( output_sem );

	//If there is any writers left and all data has been written, wake up writers
	if ( *ptr_writeCount != 0 && *ptr_finish == 1 ) {
		for( i = 0; i < *ptr_writeCount; i++ ) {
			sem_post(condW_sem);
		}
		
	}
	return NULL;
}



/**
* PURPOSE : Create readers and writers
* IMPORTS : The sum of readers and writers
* EXPORTS : None
*/ 
void createProcessors(int nprocesses)
{
	pid_t pid;
	int i;
	Process *writerProcess,*readerProcess;

	int parentId = getpid();

	//Create Maximum number of processes
	for (i = 0; i < MAX_PROCESS; ++i)
	{
		fork();
	}

	int childPid = getpid() - parentId;

	//Create writers
	if(childPid > 0 && childPid <= *ptr_initNumWriter) {
		writerProcess = ( Process* )malloc( sizeof( Process ) );
		writerProcess->id = childPid;
		writerProcess->counter = 0;
		writer(writerProcess);
	}

	//Create ID for readers
	int readerLeft = 0;
	if ( *ptr_initNumWriter == *ptr_initNumReader ) {
		readerLeft = *ptr_initNumWriter + *ptr_initNumReader;
	} else  {
		readerLeft = ( *ptr_initNumWriter ) + *ptr_initNumReader;
	}

	//Create readers
	if(childPid > *ptr_initNumWriter && childPid <= readerLeft) {
		readerProcess = ( Process* )malloc( sizeof( Process ) );
		readerProcess->id = childPid - *ptr_initNumWriter;
		readerProcess->counter = 0;
		reader(readerProcess);
	}


}


/**
* PURPOSE : Method that kick starts the program 
* IMPORTS : Number of readers, number of writers, t1, t2
* EXPORTS : None
*/ 
int main ( int argc, char *argv[] )
{
	int i, j;

	//Set initial numbers for both readers and writers
	initNumReader = atoi( argv[1] );
	initNumWriter = atoi( argv[2] );

	//Convert arguments to both t1 and t2
	t1 = atoi( argv[3] );
	t2 = atoi( argv[4] );


	if( t1 < 0 || t2 < 0 || initNumReader <= 0 || initNumWriter <= 0 ) {
		printf("Invalid Inputs - Inputs can't be negative and Readers - Writers can't be zero\n");
	} else {

		//Clear output file before doing anything else
		clearContentOutputFile(outputFileName);

		//Read data from the file
		dataFromFile = readFile(filename,d);


		//Create shared memory data
		shm_DataFromFile = shm_open( "dataFromFile", O_CREAT | O_RDWR, 0666 );
		ftruncate( shm_DataFromFile, sizeof( dataFromFile ) );
		ptr_DataFromFile = mmap( NULL, sizeof( dataFromFile ), PROT_READ | PROT_WRITE, MAP_SHARED, shm_DataFromFile, 0 );

		shm_DataBuffer = shm_open( "data_Buffer", O_CREAT | O_RDWR, 0666 );
		ftruncate( shm_DataBuffer, sizeof( dataBuffer ) );
		ptr_DataBuffer = mmap( NULL, sizeof( dataBuffer ), PROT_READ | PROT_WRITE, MAP_SHARED, shm_DataBuffer, 0 );


		//Create shared memory data
		ptr_t1 = mmap(NULL, sizeof *ptr_t1, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 0, 0);
		ptr_t2 = mmap(NULL, sizeof *ptr_t2, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 0, 0);
		ptr_initNumReader = mmap(NULL, sizeof *ptr_initNumReader, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 0, 0);
		ptr_initNumWriter = mmap(NULL, sizeof *ptr_initNumWriter, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 0, 0);
		ptr_readCount = mmap(NULL, sizeof *ptr_readCount, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 0, 0);
		ptr_writeCount = mmap(NULL, sizeof *ptr_writeCount, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 0, 0);
		ptr_done = mmap(NULL, sizeof *ptr_done, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 0, 0);
		ptr_finish = mmap(NULL, sizeof *ptr_finish, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 0, 0);
		ptr_dataReadFilePosition = mmap(NULL, sizeof *ptr_dataReadFilePosition, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 0, 0);
		ptr_dataReadFromReader = mmap(NULL, sizeof *ptr_dataReadFromReader, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 0, 0);
		ptr_totalDataHasBeenRead = mmap(NULL, sizeof *ptr_totalDataHasBeenRead, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 0, 0);
		ptr_readerStopped = mmap(NULL, sizeof *ptr_readerStopped, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 0, 0);

		//Initialise shared memory data
		*ptr_t1 = t1;
		*ptr_t2 = t2;
		*ptr_initNumReader = initNumReader;
		*ptr_initNumWriter = initNumWriter;
		*ptr_readCount = 0;
		*ptr_writeCount = 0;
		*ptr_done = 0;
		*ptr_finish = 0;
		*ptr_dataReadFilePosition = 0;
		*ptr_dataReadFromReader = 0;
		*ptr_totalDataHasBeenRead = 0;
		*ptr_readerStopped = 0;

		sem_unlink("/writer");
		sem_unlink("/reader");
		sem_unlink("/consume");
		sem_unlink("/condR");
		sem_unlink("/condW");
		sem_unlink("/output");

		//Check for errors
		if ( ( condW_sem = sem_open("/condW", O_CREAT|O_EXCL, 0, 1) ) == SEM_FAILED )
		{
			perror("sem open");
			exit(EXIT_FAILURE);
		}

		if ( ( condR_sem = sem_open("/condR", O_CREAT|O_EXCL, 0, 1) ) == SEM_FAILED )
		{
			perror("sem open");
			exit(EXIT_FAILURE);
		}
		if ( ( consume_sem = sem_open("/consume", O_CREAT|O_EXCL, 0, 1) ) == SEM_FAILED )
		{
			perror("sem open");
			exit(EXIT_FAILURE);
		}
		if ( ( writer_sem = sem_open("/writer", O_CREAT|O_EXCL, 0, 1) ) == SEM_FAILED )
		{
			perror("sem open");
			exit(EXIT_FAILURE);
		}

		if ( ( reader_sem = sem_open("/reader", O_CREAT|O_EXCL, 0, 1) ) == SEM_FAILED )
		{
			perror("sem open");
			exit(EXIT_FAILURE);
		}

		if ( ( output_sem = sem_open("/output", O_CREAT|O_EXCL, 0, 1) ) == SEM_FAILED )
		{
			perror("sem open");
			exit(EXIT_FAILURE);
		}

		//Transfer file data to shared memory
		for (i = 0; i < d; i++) {
			( ( int* )ptr_DataFromFile)[i] = dataFromFile[i];
			( (int* )ptr_DataBuffer)[i] = 0;
		}

		//Create readers and writers
		createProcessors(initNumReader + initNumWriter);

		//Free processors 
		for (j = 0; j < (initNumReader + initNumWriter) * 2; j++) {
			wait(NULL);
		}

		//Clean up resources 
		munmap( ptr_DataFromFile, sizeof( dataFromFile ) );
		munmap( ptr_DataBuffer, sizeof( dataBuffer ) );
		munmap( ptr_t1, sizeof( *ptr_t1 ) );
		munmap( ptr_t2, sizeof( *ptr_t2 ) );
		munmap( ptr_initNumReader, sizeof( *ptr_initNumReader ) );
		munmap( ptr_initNumWriter, sizeof( *ptr_initNumWriter ) );
		munmap( ptr_readCount, sizeof( *ptr_readCount ) );
		munmap( ptr_writeCount, sizeof( *ptr_writeCount ) );
		munmap( ptr_done, sizeof( *ptr_done ) );
		munmap( ptr_finish, sizeof( *ptr_finish ) );
		munmap( ptr_dataReadFilePosition, sizeof( *ptr_dataReadFilePosition ) );
		munmap( ptr_dataReadFromReader, sizeof( *ptr_dataReadFromReader ) );
		munmap( ptr_totalDataHasBeenRead, sizeof( *ptr_totalDataHasBeenRead ) );
		munmap( ptr_readerStopped, sizeof( *ptr_readerStopped ) );

		close( shm_DataFromFile );
		close( shm_DataBuffer );
	}
	

	return 0;

}
