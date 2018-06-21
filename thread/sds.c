/**
 * FILE:	sds.c
 * AUTHOR:	Xhien Yi Tan
 * STUDENTID:	18249833
 * UNIT:	COMP2006
 * PURPOSE:	This file is responsible for running the program using threads, especially Reader-Writer Problem
 */

#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include<semaphore.h>
#include<ctype.h>
#include "file.h"

#define READER 0
#define WRITER 1
#define B 20 
#define D 100

pthread_mutex_t read, wrt, consume, output;

int *dataFromFile;
char *filename = "shared_data.txt", *outputFileName = "sim_out.txt";
int t1, t2, readCount = 0, writeCount = 0, dataCount = 0, dataReadFilePosition = 0, bufferIndex = 0, b = B, d = D;
int data_buffer[B];
pthread_cond_t condW, condR;
int dataReadFromReader = 0, readersStopped = 0, iniNumWriter = 0, initNumReader = 0, finish = 0, totalDataHasBeenRead = 0;


/* Struct for thread:
 * Counter - each thread has this counter
 * id - id for each thread
 * thread - thread object itself
 */

typedef struct{
	int counter, id;
	pthread_t thread;
} Thread;

/**
* PURPOSE : Consume data and increases each reader's counter
* IMPORTS : Reader thread
* EXPORTS : None
*/ 
void *consumeData(void *thread) {

	int i;
	
	/* From i to where the writers stop reading */
	for (i = 0; i < totalDataHasBeenRead; i++) {

		dataReadFromReader++;


		if ( ( ( ( Thread* )thread )->counter ) < d ) {
			( ( ( Thread* )thread )->counter ) = ( ( ( Thread* )thread )->counter ) + 1;
		}

		/* If a reader has finished consume data until the end of data_buffer */
		if ((i+1) == b)
		{
			/* Keep track of how many readers are waiting */
			readersStopped++;

			/* If the number of readers waiting is the same as the initial readers, notify writers */
			if ( readersStopped == initNumReader )
			{

				dataReadFilePosition = 0;
				readersStopped = 0;

				pthread_cond_broadcast(&condW);
				pthread_mutex_unlock(&wrt);
				
			}
			
			/* If no writers left and this reader has finished reading all the data, then finish */
			if ( writeCount == 0 && ( ( ( Thread* )thread )->counter ) == d )
			{
				
				finish = 1;
			} else {
				
				/* Readers wait here */
				pthread_mutex_unlock(&wrt);
				pthread_cond_wait(&condR, &consume);
			}
		}
	}

	return NULL;
}


/**
* PURPOSE : Read data from buffer and print read data
* IMPORTS : None
* EXPORTS : None
*/ 
void *readFromBuffer() {

	int i;
	
	/* To enable printing data, uncomment this */
	
	/*	
	*for( i = 0; i < b; i++ ) {
	*	printf("data_buffer data - %d\n", data_buffer[i]);
	*}
	*/

	return NULL;
}


/**
* PURPOSE : Method for readers:
*			- To start reading from the buffer
*			- Sleeps after incrementing counter and reading
* IMPORTS : Reader thread
* EXPORTS : None
*/ 
void *reader(void *thread) {

	int done = 0;


	do {   


		pthread_mutex_lock(&read); 
		readCount = readCount + 1;
		
		if (readCount == 1)
		{	
			pthread_mutex_lock(&wrt);
		}

		pthread_mutex_unlock(&read);

		/* Read data together */
		readFromBuffer();
		sleep(t1);

		/* Consume data */
		pthread_mutex_lock(&consume);
		consumeData(thread);
		pthread_mutex_unlock(&consume);


		/* When the finish flag is set to 1 by writer and this reader has finished consuming,
		 * set done flag = 1 means done.
		 */
		if (finish == 1 && ( ( ( Thread* )thread )->counter ) == d )
		{
			done = 1;
		}

		
		pthread_mutex_lock(&read);

		readCount = readCount - 1;
		if ( readCount == 0 )
		{
		
			pthread_cond_broadcast(&condW);
			pthread_mutex_unlock(&wrt); 

		}
		
		pthread_mutex_unlock(&read); 
	

	} while( done != 1 );

	printf("Reader ID %d has finished reading %d pieces of data from the data_buffer \n", ( ( ( Thread* )thread )->id ),( ( ( Thread* )thread )->counter ) );



	pthread_mutex_lock(&output);
	writeToFile(outputFileName, thread, READER);
	pthread_mutex_unlock(&output);

	return NULL;
}

/**
* PURPOSE : Read data from shared_data and put it into buffer
* IMPORTS : Writer thread
* EXPORTS : None
*/ 
void *readFromSharedData(void* thread) {

	int done, i;

	
	do {
		dataReadFilePosition++;
		
		/* If all data from shared_data has been read, set flag to finish */
	  	if ( totalDataHasBeenRead == d ) {

	  
	  		done = 1;
	  		finish = 1;

	  	/* If data_buffer is full, wait for readers to read and consume */
	  	} else if ( dataReadFilePosition > b ) {

	  		
		  	while( dataReadFilePosition > b ) {

					/* Signal all readers to read */
				for (i = 0; i < initNumReader; i++) {
			  		pthread_cond_signal(&condR);
			  	}

		  		if ( finish == 1 ) {
		  			done = 1;
		  			break;
		  		} else {
		  			
					/* If only one reader left. wake it up */
		  			if (readCount == 0) {
		  				pthread_mutex_unlock(&read);
		  			} else {
		  				/* Writers wait here */
		  				pthread_cond_wait(&condW, &wrt);
		  			}
			  			
		  		}
		  	}

	  	} else {
	  
	  		totalDataHasBeenRead++;
	  		( ( ( Thread* )thread )->counter ) = ( ( ( Thread* )thread )->counter ) + 1;
	  		data_buffer[dataReadFilePosition - 1] = dataFromFile[dataReadFilePosition - 1];
	  	}
	  	
  	} while( done != 1);



  return NULL;
}

/**
* PURPOSE : Method for writers:
*			- To start reading from the shared_data and put to buffer
*			- Sleeps after incrementing counter and writing
* IMPORTS : Writer thread
* EXPORTS : None
*/ 
void * writer(void *thread)
{
	int i,j;

	pthread_mutex_lock(&wrt);

	writeCount = writeCount + 1;
		

	readFromSharedData(thread);
	sleep(t2);


	pthread_mutex_unlock(&wrt);

	/* When one of the writers come out from its CS.
	 * means that writers has finish writing to buffer.
	 * So that change the finish flag to 1 - means finished.
	 */
	finish = 1;

	for (i = 0; i < initNumReader; i++) {
		pthread_cond_signal(&condR);
	}


	writeCount = writeCount - 1;

	printf("writer-%d has finished writing %d pieces of data to the data_buffer \n", ( ( ( Thread* )thread )->id ), ( ( ( Thread* )thread )->counter ));

	pthread_mutex_lock(&output);
	writeToFile(outputFileName, thread, WRITER);
	pthread_mutex_unlock(&output);


	return NULL;
}
 

/**
* PURPOSE : Method that kick starts the program 
* IMPORTS : Number of readers, number of writers, t1, t2
* EXPORTS : None
*/ 
int main(int argc, char *argv[])
{
	int i;

	initNumReader = atoi( argv[1] );
	iniNumWriter = atoi( argv[2] );

	t1 = atoi( argv[3] );
	t2 = atoi( argv[4] );

	if( t1 < 0 || t2 < 0 || initNumReader <= 0 || iniNumWriter <= 0 ) {
		printf("Invalid Inputs - Inputs can't be negative and Readers - Writers can't be zero\n");
	} else {

		/* Initialise mutexes and conditional variables */
		pthread_t Readers_thr[initNumReader-1],Writer_thr[iniNumWriter-1];
		pthread_mutex_init(&read, NULL);
		pthread_mutex_init(&wrt, NULL);
		pthread_mutex_init(&output, NULL);
		pthread_mutex_init(&consume, NULL);
		pthread_cond_init(&condW, NULL);
		pthread_cond_init(&condR, NULL);
		Thread *writerThread,*readerThread;

		/* Clean output file before writing */
		clearContentOutputFile(outputFileName);

		/* Read file */
		dataFromFile = readFile(filename,d);
	
		/* Create Writers and Readers */
		for(i=0;i<iniNumWriter;i++)
		{
			writerThread = ( Thread* )malloc( sizeof( Thread ) );
			writerThread->id = i;
			writerThread->counter = 0;
			pthread_create(&Writer_thr[i],NULL,writer,(void *)writerThread);
		}

		for(i=0;i<initNumReader;i++)
		{
			readerThread = ( Thread* )malloc( sizeof( Thread ) );
			readerThread->id = i;
			readerThread->counter = 0;
			pthread_create(&Readers_thr[i],NULL,reader,(void *)readerThread);
		}

		for(i=0;i<iniNumWriter;i++)
		{
			pthread_join(Writer_thr[i],NULL);
		}

		for(i=0;i<initNumReader;i++)
		{
			pthread_join(Readers_thr[i],NULL);
		}
	
		/* Clean up resources */
		pthread_mutex_destroy(&read);
		pthread_mutex_destroy(&wrt);
		pthread_mutex_destroy(&output);
		pthread_mutex_destroy(&consume);
		free(readerThread);
		free(writerThread);
	}



	return 0;
}
