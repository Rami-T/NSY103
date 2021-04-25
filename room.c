/*
* This program is written by Rami TOFEILY as a NSY103 Linux project fall-2020/2021
* It is a simple chat application which is consisted of a common chatroom
* where all connected users can talk in public
* similar to the good old IRC networks channels
*************************
* this is the room Program
*************************
*/
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>

#define MAX_IN_CONNECTIONS 100
#define BFR_LNGTH 2032
#define NICKNAME_LONGEUR 16

// variables
static _Atomic unsigned int clientsCounter = 0;
static int uniqueId = 10;
char chatLinePrefix[5] = "#-> ";
char welcomeMsg[64] = "~#~CNAM Chat Server Started Successfully~#~";

// client structure
typedef struct
{
	struct sockaddr_in address;
	int clientSocket;
	int uniqueId;
	char nickname[NICKNAME_LONGEUR];
} clientStructure;

// array to hold clients
clientStructure *clients[MAX_IN_CONNECTIONS];

// Mutex to safely add and remove clients from array
pthread_mutex_t clientsMutex = PTHREAD_MUTEX_INITIALIZER;

// prepend chat line with a prefix
void prepandChatLine()
{
	printf("\r%s ", chatLinePrefix);
	fflush(stdout);
}

// replace newline characters in text with null
void replaceNewlineWithNull(char *arr, int longueur)
{
	int ndx;
	for (ndx = 0; ndx < longueur; ndx++)
	{
		if (arr[ndx] == '\n')
		{
			arr[ndx] = '\0';
			break;
		}
	}
}
// print client address
void printClientAddress(struct sockaddr_in addr)
{
	printf("%d.%d.%d.%d",
		   addr.sin_addr.s_addr & 0xff,
		   (addr.sin_addr.s_addr & 0xff00) >> 8,
		   (addr.sin_addr.s_addr & 0xff0000) >> 16,
		   (addr.sin_addr.s_addr & 0xff000000) >> 24);
}

// Add clients to array
void threadSafeAdd(clientStructure *cl)
{
	pthread_mutex_lock(&clientsMutex);

	for (int i = 0; i < MAX_IN_CONNECTIONS; ++i)
	{
		if (!clients[i])
		{
			clients[i] = cl;
			break;
		}
	}

	pthread_mutex_unlock(&clientsMutex);
}

// Remove clients from array
void threadSafeRemove(int uniqueId)
{
	pthread_mutex_lock(&clientsMutex);

	for (int i = 0; i < MAX_IN_CONNECTIONS; ++i)
	{
		if (clients[i])
		{
			if (clients[i]->uniqueId == uniqueId)
			{
				clients[i] = NULL;
				break;
			}
		}
	}

	pthread_mutex_unlock(&clientsMutex);
}

// Send message to all clients except sender
void sendMassMessage(char *s, int uniqueId)
{
	pthread_mutex_lock(&clientsMutex);

	for (int i = 0; i < MAX_IN_CONNECTIONS; ++i)
	{
		if (clients[i])
		{
			if (clients[i]->uniqueId != uniqueId)
			{
				if (write(clients[i]->clientSocket, s, strlen(s)) < 0)
				{
					perror("ERROR: write to socket's descriptor failed");
					break;
				}
			}
		}
	}

	pthread_mutex_unlock(&clientsMutex);
}

// Handle client communications
void *handle_client(void *arg)
{
	char buff_out[BFR_LNGTH];
	char nickname[NICKNAME_LONGEUR];
	int quitFlag = 0;

	clientsCounter++;
	clientStructure *cli = (clientStructure *)arg;

	// nickname
	if (recv(cli->clientSocket, nickname, NICKNAME_LONGEUR, 0) <= 0 || strlen(nickname) < 2 || strlen(nickname) >= NICKNAME_LONGEUR - 1)
	{
		printf("Didn't enter the nickname.\n");
		quitFlag = 1;
	}
	else
	{
		strcpy(cli->nickname, nickname);
		sprintf(buff_out, "%s has joined\n", cli->nickname);
		printf("%s", buff_out);
		sendMassMessage(buff_out, cli->uniqueId);
	}

	bzero(buff_out, BFR_LNGTH);

	while (1)
	{
		if (quitFlag)
		{
			break;
		}

		int receive = recv(cli->clientSocket, buff_out, BFR_LNGTH, 0);
		if (receive > 0)
		{
			if (strlen(buff_out) > 0)
			{
				sendMassMessage(buff_out, cli->uniqueId);

				replaceNewlineWithNull(buff_out, strlen(buff_out));
				printf("%s -> %s\n", buff_out, cli->nickname);
			}
		}
		else if (receive == 0 || strcmp(buff_out, "exit") == 0)
		{
			sprintf(buff_out, "%s has left\n", cli->nickname);
			printf("%s", buff_out);
			sendMassMessage(buff_out, cli->uniqueId);
			quitFlag = 1;
		}
		else
		{
			printf("ERROR: -1\n");
			quitFlag = 1;
		}

		bzero(buff_out, BFR_LNGTH);
	}

	// Delete client from array and detach thread
	close(cli->clientSocket);
	threadSafeRemove(cli->uniqueId);
	free(cli);
	clientsCounter--;
	pthread_detach(pthread_self());

	return NULL;
}

int main(int argc, char **argv)
{
	if (argc != 3)
	{
		printf("Syntax: %s <IP> <port>\n", argv[0]);
		return EXIT_FAILURE;
	}

	char *ip = argv[1];
	int port = atoi(argv[2]);
	int option = 1;
	int listenServiceSocket = 0, clientSocket = 0;
	struct sockaddr_in serv_addr;
	struct sockaddr_in cli_addr;
	pthread_t tid;

	listenServiceSocket = socket(AF_INET, SOCK_STREAM, 0);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(ip);
	serv_addr.sin_port = htons(port);

	// Ignore pipe signals
	signal(SIGPIPE, SIG_IGN);

	if (setsockopt(listenServiceSocket, SOL_SOCKET, (SO_REUSEPORT | SO_REUSEADDR), (char *)&option, sizeof(option)) < 0)
	{
		perror("ERROR: failed setting socket options");
		return EXIT_FAILURE;
	}

	// Bind listenServiceSocket
	if (bind(listenServiceSocket, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		perror("ERROR: failed binding Socket");
		return EXIT_FAILURE;
	}

	// Listen listenServiceSocket
	if (listen(listenServiceSocket, 10) < 0)
	{
		perror("ERROR: failed listening Socket");
		return EXIT_FAILURE;
	}

	printf("%s\n",welcomeMsg);

	while (1)
	{
		socklen_t clilen = sizeof(cli_addr);
		clientSocket = accept(listenServiceSocket, (struct sockaddr *)&cli_addr, &clilen);

		// Check if max clients is reached
		if ((clientsCounter + 1) == MAX_IN_CONNECTIONS)
		{
			printf("Max clients reached. Rejected: ");
			printClientAddress(cli_addr);
			printf(":%d\n", cli_addr.sin_port);
			close(clientSocket);
			continue;
		}

		clientStructure *cli = (clientStructure *)malloc(sizeof(clientStructure));
		cli->address = cli_addr;
		cli->clientSocket = clientSocket;
		cli->uniqueId = uniqueId++;

		// Add client to the array and fork thread
		threadSafeAdd(cli);
		pthread_create(&tid, NULL, &handle_client, (void *)cli);

		// Reduce CPU usage by sleeping
		sleep(1);
	}

	return EXIT_SUCCESS;
}
