/*
* This program is written by Rami TOFEILY as a NSY103 Linux project fall-2020/2021
* It is a simple chat application which is consisted of a common chatroom
* where all connected users can talk in public
* similar to the good old IRC networks channels
*************************
* this is the user Program
*************************
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define LNGTH 2032
#define NICKNAME_LONGEUR 16

// variables
char exitCommand[6] = "/exit";
char chatLinePrefix[5] = "#-> ";
char welcomeMsg[64] = "~#~Bienvenue dans le plus adorable chat du CNAM~#~";
volatile sig_atomic_t quitFlag = 0;
int chatClientSocket = 0;
char nickname[NICKNAME_LONGEUR];

// prepends chat line with a prefix
void prepandChatLine()
{
	printf("%s", chatLinePrefix);
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

// sets the quit flag to 1 so that we can exist the while true loop 
void initiateTermination(int sig)
{
	quitFlag = 1;
}

// a handler method to handle sending messages
void sendMessageHandler()
{
	char message[LNGTH] = {};
	char bfr[LNGTH + NICKNAME_LONGEUR + 2] = {};

	while (1)
	{
		prepandChatLine();
		fgets(message, LNGTH, stdin);
		replaceNewlineWithNull(message, LNGTH);

		if (strcmp(message, exitCommand) == 0)
		{
			break;
		} 
		else
		{
			sprintf(bfr, "%s: %s\n", nickname, message);
			send(chatClientSocket, bfr, strlen(bfr), 0);
		}

		bzero(message, LNGTH);
		bzero(bfr, LNGTH + NICKNAME_LONGEUR);
	}
	initiateTermination(2);
}

// a handler method to handle receiving messages
void receiveMessageHandler()
{
	char message[LNGTH] = {};
	while (1)
	{
		int receive = recv(chatClientSocket, message, LNGTH, 0);
		if (receive > 0)
		{
			printf("%s", message);
			prepandChatLine();
		}
		else if (receive == 0)
		{
			break;
		}
		memset(message, 0, sizeof(message));
	}
}

int main(int argc, char **argv)
{
	// check passed arguments
	if (argc != 3)
	{
		printf("Syntax: %s <port>\n", argv[0]);
		return EXIT_FAILURE;
	}
	char *ip = argv[1];
	int port = atoi(argv[2]);

	// change default SIGINT handler to print See U later on CTRL+C
	signal(SIGINT, initiateTermination);

	printf("#-> pick a name: ");
	fgets(nickname, NICKNAME_LONGEUR, stdin);

	replaceNewlineWithNull(nickname, strlen(nickname));

	if (strlen(nickname) < 2)
	{
		printf("nickname must be more than 2 and limited to 15 characters.\n");
		return EXIT_FAILURE;
	}

	struct sockaddr_in serverSocketAddress;

	chatClientSocket = socket(AF_INET, SOCK_STREAM, 0);
	serverSocketAddress.sin_family = AF_INET;
	serverSocketAddress.sin_addr.s_addr = inet_addr(ip);
	serverSocketAddress.sin_port = htons(port);

	int err = connect(chatClientSocket, (struct sockaddr *)&serverSocketAddress, sizeof(serverSocketAddress));
	if (err == -1)
	{
		printf("ERROR: connect\n");
		return EXIT_FAILURE;
	}

	// Send nickname
	send(chatClientSocket, nickname, NICKNAME_LONGEUR, 0);

	printf("%s\n",welcomeMsg);

	pthread_t send_msg_thread;
	if (pthread_create(&send_msg_thread, NULL, (void *)sendMessageHandler, NULL) != 0)
	{
		printf("ERROR: pthread\n");
		return EXIT_FAILURE;
	}

	pthread_t recv_msg_thread;
	if (pthread_create(&recv_msg_thread, NULL, (void *)receiveMessageHandler, NULL) != 0)
	{
		printf("ERROR: pthread\n");
		return EXIT_FAILURE;
	}

	// keep checking if quit flag has became 1 in order to quit with a msg
	while (1)
	{
		if (quitFlag)
		{
			printf("\nSee U later\n");
			break;
		}
	}

	// after while true loop stops, close socket and exit program successfully
	close(chatClientSocket);
	return EXIT_SUCCESS;
}
