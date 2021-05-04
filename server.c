
/*
Universidad del Valle de Guatemala
Sistemas operativos
Ing. Erick Pineda

Esteban del Valle 
MArio Perdomo 

Server.c
Proposito: Mantiene el servidor
para los clientes, incluyendo sincronizacion de mensajes y registro de usuarios
*/


//Libraries
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <string.h>

//TO run this code, we'll be using the makefile syntax, in order to compile both the server and
// the client

// Maximum clients, and can be modify
#define MAX_CLIENTS 100
#define BUFFER_SZ 2048

static _Atomic unsigned int cli_count = 0;
static int uid = 10;

/* Client structure */
// contains address which is the ip
// uid is the identity of each cleint
// name of the client

// we need to add username for the client and also the port
typedef struct{
	struct sockaddr_in address;
	int sockfd;
	int uid;
	char name[32];
} client_t;
// list of structure of clients
client_t *clients[MAX_CLIENTS];
// THis mutex will allow the program to not have interception between threads
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void str_overwrite_stdout() {
    printf("\r%s", "> ");
    fflush(stdout);
}

