#include <sys/shm.h>
#include <sys/msg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "msg.h"    /* For the message struct */

/* The size of the shared memory segment */
#define SHARED_MEMORY_CHUNK_SIZE 1000

/* The ids for the shared memory segment and the message queue */
int shmid, msqid;

/* The pointer to the shared memory */
void* sharedMemPtr;

/**
 * Sets up the shared memory segment and message queue
 * @param shmid - the id of the allocated shared memory
 * @param msqid - the id of the allocated message queue
 */
void init(int& shmid, int& msqid, void*& sharedMemPtr)
{
	// NOTE: keyfile.txt manually created
	printf("\nInitializing Sender..\n");

	// Assign key
	key_t key = ftok("keyfile.txt", 'a');

	if (key < 0)
	{
		perror("ftok");
		exit(-1);
	}

	// Get the id of the shared memory segment.
	shmid = shmget(key, SHARED_MEMORY_CHUNK_SIZE, 0644 | IPC_CREAT);

	if (shmid < 0)
	{
		perror("shmget");
		exit(-1);
	}

	// Attach to the shared memory
	sharedMemPtr = shmat(shmid, (void *)0, 0);

	if (sharedMemPtr == (void *) -1)
	{
		perror("shmat");
		exit(-1);
	}

	// Attach to the message queue
	msqid = msgget(key, 0666 | IPC_CREAT);

	if (msqid < 0)
	{
		perror("msgget");
		exit(-1);
	}

	printf("\nSender initializing complete..\n");
}

/**
 * Performs the cleanup functions
 * @param sharedMemPtr - the pointer to the shared memory
 * @param shmid - the id of the shared memory segment
 * @param msqid - the id of the message queue
 */
void cleanUp(const int& shmid, const int& msqid, void* sharedMemPtr)
{
	// Detach from shared memory
	if (shmdt(sharedMemPtr) < 0)
	{
		perror("shmdt");
		exit(-1);
	}

	printf("\nDetached from shared memory..\n");
}

/**
 * The main send function
 * @param fileName - the name of the file
 * @return - the number of bytes sent
 */
unsigned long sendFile(const char* fileName)
{

	/* A buffer to store message we will send to the receiver. */
	message sndMsg;

	/* A buffer to store message received from the receiver. */
	ackMessage rcvMsg;

	/* The number of bytes sent */
	unsigned long numBytesSent = 0;

	/* Open the file */
	FILE* fp =  fopen(fileName, "r");

	/* Was the file open? */
	if(!fp)
	{
		perror("fopen");
		exit(-1);
	}

	/* Read the whole file */
	while(!feof(fp))
	{
		/* Read at most SHARED_MEMORY_CHUNK_SIZE from the file and
 		 * store them in shared memory.  fread() will return how many bytes it has
		 * actually read. This is important; the last chunk read may be less than
		 * SHARED_MEMORY_CHUNK_SIZE.
 		 */
		if((sndMsg.size = fread(sharedMemPtr, sizeof(char), SHARED_MEMORY_CHUNK_SIZE, fp)) < 0)
		{
			perror("fread");
			exit(-1);
		}

		// Count the number of bytes sent.
		numBytesSent += sndMsg.size;

		// Send a message to the receiver telling him that the data is ready to be read
		sndMsg.mtype = SENDER_DATA_TYPE;

		printf("\nSending message..\n");

		if (msgsnd(msqid, &sndMsg, sizeof(message) - sizeof(long), 0) < 0)
		{
			perror("msgsnd");
			exit(-1);
		}

		printf("...complete!\n");

		// Wait until the receiver has finished saving a chunk of memory.
		do
		{
			printf("Receiving message..\n");
			msgrcv(msqid, &rcvMsg, sizeof(ackMessage) - sizeof(long), RECV_DONE_TYPE, 0);
		}
		while(rcvMsg.mtype != RECV_DONE_TYPE);

		printf("... complete!\n");
	}

	// Send message of size 0 to receiver
	sndMsg.size = 0;

	if (msgsnd(msqid, &sndMsg, sizeof(message) - sizeof(long), 0) < 0)
	{
		perror("msgsnd");
		exit(-1);
	}

	/* Close the file */
	fclose(fp);
	printf("File closed.\n");

	return numBytesSent;
}

/**
 * Used to send the name of the file to the receiver
 * @param fileName - the name of the file to send
 */
void sendFileName(const char* fileName)
{
	/* Get the length of the file name */
	int fileNameSize = strlen(fileName);

	// Make sure the file name does not exceed
	// If exceeds, then terminate with an error.
	if (fileNameSize > (sizeof(fileNameMsg) - sizeof(long)))
	{
		perror("The name of the file to send is too long!\n");
		exit(-1);
	}

	// Instance of the struct representing the message containing the name of the file.
	fileNameMsg msg;

	// Set the message type FILE_NAME_TRANSFER_TYPE
	msg.mtype = FILE_NAME_TRANSFER_TYPE;

	// Set the file name in the message
	strncpy(msg.fileName, fileName, fileNameSize+1);

	// Send the message using msgsnd
	if (msgsnd(msqid, &msg, sizeof(msg) - sizeof(long), 0) < 0)
	{
		perror("msgsnd");
		exit(-1);
	}
}

int main(int argc, char** argv)
{

	/* Check the command line arguments */
	if(argc < 2)
	{
		fprintf(stderr, "USAGE: %s <FILE NAME>\n", argv[0]);
		exit(-1);
	}

	/* Connect to shared memory and the message queue */
	init(shmid, msqid, sharedMemPtr);

	/* Send the name of the file */
        sendFileName(argv[1]);

	/* Send the file */
	fprintf(stderr, "The number of bytes sent is %lu\n", sendFile(argv[1]));

	/* Cleanup */
	cleanUp(shmid, msqid, sharedMemPtr);

	return 0;
}
