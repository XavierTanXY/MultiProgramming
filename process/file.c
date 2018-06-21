/**
 * FILE:	file.c
 * AUTHOR:	Xhien Yi Tan
 * STUDENTID:	18249833
 * UNIT:	COMP2006
 * PURPOSE:	This file incharge of writing and reading data from file
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

typedef struct{
	int counter, id;
} Process;



/**
* PURPOSE : Clear sim_out file everytime we run this program, 
*			so that the previous data is cleared before putting in new data.
* IMPORTS : Output file name
* EXPORTS : None
*/
void *clearContentOutputFile( char *outputfilename ) {

	FILE *f;
	f = fopen( outputfilename, "w" );

	if( f == NULL )
	{
		perror( outputfilename );
	} 
	fclose(f);

	return NULL;

}

/**
* PURPOSE : Read file and store the data into array
* IMPORTS : Input file name, the total size for shared_data
* EXPORTS : Data from file
*/
int* readFile( char *filename, int d )
{
	FILE *f;
	int check, done = 0,i = 0, num;
	int *dataArray;


	dataArray = ( int* )malloc( d * sizeof( int ) );
	f = fopen( filename, "r" );


	if( f == NULL )
	{
		perror( filename );
	} else {

		do {

			check = fscanf( f, "%d ", &num );


			if ( check == EOF ) {
				done = 1;
			} else if ( i == d ) {
				done = 1;
			} else {
				dataArray[i] = num;
				i++;
			}

		} while( done != 1);
	}

	fclose(f);

	return dataArray;
}


/**
* PURPOSE : Write data to file
* IMPORTS : Output file name, thread that wants tp input data, type - READER or WRITER
* EXPORTS : None
*/
void *writeToFile(char *filename, void* process, int type) {

		FILE *f;
		f = fopen( filename, "a" );

		if( f == NULL ) {
			perror( filename );
		} else {

			if ( type == READER ) {
				fprintf(f, "reader-%d has finished reading %d pieces of data from the data_buffer. \n", ( ( ( Process* )process )->id ),( ( ( Process* )process )->counter ) );
			} else {
				fprintf(f, "writer-%d has finished writing %d pieces of data to the data_buffer. \n", ( ( ( Process* )process )->id ),( ( ( Process* )process )->counter ) );
			}
			
		}

		fclose(f);
		return NULL;

}

