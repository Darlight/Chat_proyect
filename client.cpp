
/*
Universidad del Valle de Guatemala
Sistemas operativos
Ing. Erick Pineda

Esteban del Valle 18221
Andrea Paniagua 18733
Mario Perdomo  18029

client.c
Proposito: clientes que se unen al servidor, con opciones de interacciones entre otros clientes
*/

#include <iostream>
#include <string>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <fstream>
#include <vector>
#include <pthread.h>
#include "payload.pb.h"


int connected, waitingForServerResponse, waitingForInput;
std::string statusClient[3] = {"ACTIVO", "OCUPADO", "INACTIVO"};


// get sockaddr
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET)
	{
		return &(((struct sockaddr_in *)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

// Funcion de threads, su objetivo es siempre estar escuchando respuestas del servidor
void *listenToMessages(void *args)
{
	while (1)
	{
		// Estructura del mensaje
		char bufferMsg[8192];
		int *sockmsg = (int *)args;
		chat::Payload serverMsg;

		int bytesReceived = recv(*sockmsg, bufferMsg, 8192, 0);

		serverMsg.ParseFromString(bufferMsg);

		// Mensaje con codigo de error
		if (serverMsg.code() == 500)
		{
			printf("________________________________________________________\n");
			std::cout << "Error: "
			  << serverMsg.message()
			  << std::endl;
		}
		// Respuesta correcta del servidor
		else if (serverMsg.code() == 200 || serverMsg.flag() == chat::Payload_PayloadFlag_general_chat)
		{
			printf("________________________________________________________\n");
			std::cout << "Respuesta del servidor: \n"
			  << serverMsg.message()
			  << std::endl;
		}
		// En el posible caso que el servidor mande una respuesta no reconocida
		else
		{
			printf("El servidor no esta disponible, intentelo mas tarde\n");
			break;
		}

		waitingForServerResponse = 0;

		if (connected == 0)
		{
			pthread_exit(0);
		}
	}
}

// Menu de opciones
void print_menu(char *usrname)
{
	printf("|--------------------------------------------------------| \n");
	printf("Bienvenido al lobby, %s. \n", usrname);
	printf("1. Obtener lista de usuarios conectados. \n");
	printf("2. Desplegar informacion de un usuario. \n");
	printf("3. Cambiar status. \n");
	printf("4. Enviar un mensaje a todos los usuarios. \n");
	printf("5. Enviar mensaje directo. \n");
	printf("6. Salir. \n");
	printf("|--------------------------------------------------------| \n");
}


// Obtener opcion elegida por el cliente
int get_client_option()
{
	// Client options
	int client_opt;
	std::cin >> client_opt;

	while (std::cin.fail())
	{
		std::cout << "Por favor, ingrese una opcion valida: " << std::endl;
		std::cin.clear();
		std::cin.ignore(256, '\n');
		std::cin >> client_opt;
	}

	return client_opt;
}

int main(int argc, char *argv[])
{
	// Estructura de la coneccion
	int sockfd, numbytes;
	char buf[8192];
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];

	if (argc != 4)
	{
		fprintf(stderr, "Forma de uso: client <username> <server_ip> <server_port>\n");
		exit(1);
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(argv[2], argv[3], &hints, &servinfo)) != 0)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// Conectarse a la opcion que este disponible
	for (p = servinfo; p != NULL; p = p->ai_next)
	{
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
							 p->ai_protocol)) == -1)
		{
			perror("client: socket");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1)
		{
			perror("client: connect");
			close(sockfd);
			continue;
		}

		break;
	}
	// Indicar fallo al conectarse
	if (p == NULL)
	{
		fprintf(stderr, "failed to connect\n");
		return 2;
	}

	//Completar la coneccion
	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
			  s, sizeof s);
	printf("Conectado con %s\n", s);
	freeaddrinfo(servinfo);


	// Escribir el mensaje de registro
	char buffer[8192];
	std::string message_serialized;

	chat::Payload *firstMessage = new chat::Payload;

	firstMessage->set_sender(argv[1]);
	firstMessage->set_flag(chat::Payload_PayloadFlag_register_);
	firstMessage->set_ip(s);

	firstMessage->SerializeToString(&message_serialized);

	strcpy(buffer, message_serialized.c_str());
	send(sockfd, buffer, message_serialized.size() + 1, 0);

	// Obtener response de servidor
	recv(sockfd, buffer, 8192, 0);

	chat::Payload serverMessage;
	serverMessage.ParseFromString(buffer);

	// En caso de registro no exitoso
	if(serverMessage.code() == 500){
			std::cout << "Error: "
			  << serverMessage.message()
			  << std::endl;
			return 0;
	}

	// En caso de registro exitoso
	std::cout << "El servidor dice: "
			  << serverMessage.message()
			  << std::endl;	

	connected = 1;

	// despachar thread que escucha mensajes del server
	pthread_t thread_id;
	pthread_attr_t attrs;
	pthread_attr_init(&attrs);
	pthread_create(&thread_id, &attrs, listenToMessages, (void *)&sockfd);

	print_menu(argv[1]);
	int client_opt;

	// Loop para seguir preguntando opciones 
	while (true)
	{
		// Mantener un orden entre lo que se envia y se recibe
		while (waitingForServerResponse == 1){}

		printf("\nEscoja la opcion que desea:\n");
		client_opt = get_client_option();

		std::string msgSerialized;
		int bytesReceived, bytesSent;

		// Lista de usuarios conectados
		if (client_opt == 1)
		{
			chat::Payload *userList = new chat::Payload();
			userList->set_sender(argv[1]);
			userList->set_ip(s);
			userList->set_flag(chat::Payload_PayloadFlag_user_list);

			userList->SerializeToString(&msgSerialized);

			strcpy(buffer, msgSerialized.c_str());
			bytesSent = send(sockfd, buffer, msgSerialized.size() + 1, 0);
			waitingForServerResponse = 1;
		}
		// Informacion de un usuario en especifico
		else if (client_opt == 2)
		{
			std::string user_name;
			printf("Ingrese el nombre del usuario que quiere consultar: \n");
			std::cin >> user_name;

			chat::Payload *userInf = new chat::Payload();
			userInf->set_sender(argv[1]);
			userInf->set_flag(chat::Payload_PayloadFlag_user_info);
			userInf->set_extra(user_name);
			userInf->set_ip(s);

			userInf->SerializeToString(&msgSerialized);

			strcpy(buffer, msgSerialized.c_str());
			bytesSent = send(sockfd, buffer, msgSerialized.size() + 1, 0);
			waitingForServerResponse = 1;
		}
		// Cambiar estatus
		else if(client_opt == 3){
			printf("Ingrese su nuevo estado: \n");
			printf("1. ACTIVO\n");
			printf("2. OCUPADO\n");
			printf("3. INACTIVO\n");
			int option;
			std::cin >> option;
			std::string newStatus;
			if (option < 4){
				newStatus = statusClient[option-1];
			}
			else
			{
				printf("Estado invalido.\n");
				continue;
			}
			chat::Payload *userInf = new chat::Payload();
			userInf->set_sender(argv[1]);
			userInf->set_flag(chat::Payload_PayloadFlag_update_status);
			userInf->set_extra(newStatus);
			userInf->set_ip(s);

			userInf->SerializeToString(&msgSerialized);

			strcpy(buffer, msgSerialized.c_str());
			bytesSent = send(sockfd, buffer, msgSerialized.size() + 1, 0);
			waitingForServerResponse = 1;
		}
		// Enviar mensaje general
		else if (client_opt == 4)
		{
			waitingForInput = 1;
			printf("Ingrese un mensaje a enviar: \n");
			std::cin.ignore();
			std::string msg;
			std::getline(std::cin, msg);


			chat::Payload *clientMsg= new chat::Payload();

			clientMsg->set_sender(argv[1]);
			clientMsg->set_message(msg);
			clientMsg->set_flag(chat::Payload_PayloadFlag_general_chat);
			clientMsg->set_ip(s);
			clientMsg->SerializeToString(&msgSerialized);

			strcpy(buffer, msgSerialized.c_str());
			bytesSent = send(sockfd, buffer, msgSerialized.size() + 1, 0);
			waitingForServerResponse = 0;
			waitingForInput = 0;
		}

		// Enviar Mensaje privado
		else if (client_opt == 5)
		{
			printf("Ingrese el nombre de usuario al que quiere enviarle un mensaje: \n");
			std::cin.ignore();
			std::string user_name;
			std::getline(std::cin, user_name);

			printf("Ingrese un mensaje a enviar: \n");
			std::string msg;
			std::getline(std::cin, msg);

			chat::Payload *clientMsg= new chat::Payload();

			clientMsg->set_sender(argv[1]);
			clientMsg->set_message(msg);
			clientMsg->set_flag(chat::Payload_PayloadFlag_private_chat);
			clientMsg->set_extra(user_name);
			clientMsg->set_ip(s);
			clientMsg->SerializeToString(&msgSerialized);

			strcpy(buffer, msgSerialized.c_str());
			bytesSent = send(sockfd, buffer, msgSerialized.size() + 1, 0);
			waitingForServerResponse = 0;
		}
		
		else if (client_opt == 6)
		{
			int option;
			printf("Cerrar sesion? \n");
			printf("1. Si \n");
			printf("2. No \n");

			std::cin >> option;

			if (option == 1)
			{
				printf("Hasta la proxima!\n");
				break;
			}
		}
		else
		{
			std::cout << "Esa no es una opcion valida" << std::endl;
		}
		
		
	}

	// cerrar conexion
	pthread_cancel(thread_id);
	connected = 0;
	close(sockfd);

	return 0;
}
