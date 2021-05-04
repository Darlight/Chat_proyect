
/*
Universidad del Valle de Guatemala
Sistemas operativos
Ing. Erick Pineda
client.c
Proposito: clientes que se unen al servidor, con opciones de interacciones entre otros clientes
*/

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <string.h>



