
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

#define BUFFER_SZ 4096

int online, serverResponse, inputMessage;
std::string statusClient[] = {"ACTIVE", "BUSY", "INACTIVE"};


// get sockaddr 
void *get_server_address(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET)
	{
		return &(((struct sockaddr_in *)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

// listen to server
void *listenToMessages(void *args)
{
	while (1)
	{
		char bufferMsg[BUFFER_SZ];
		int *sockmsg = (int *)args;
		chat::Payload serverMsg;

		int bytesReceived = recv(*sockmsg, bufferMsg, BUFFER_SZ, 0);

		serverMsg.ParseFromString(bufferMsg);

		//Error
		if (serverMsg.code() == 500)
		{
			printf("\n");
			std::cout << "Error: "
			  << serverMsg.message()
			  << std::endl;
		}
		else if (serverMsg.code() == 200 || serverMsg.flag() == chat::Payload_PayloadFlag_general_chat)
		{
			printf("\n");
			std::cout << "server answers: \n"
			  << serverMsg.message()
			  << std::endl;
		}
		else
		{
			printf("server does not recognize the answer\n");
			break;
		}

		serverResponse = 0;

		if (online == 0)
		{
			pthread_exit(0);
		}
	}
}
void print_menu(char *nameCLient)
{
	printf("\n");
	printf("Welcome to Chatroom, %s! \n\n", nameCLient);
	printf("1. List of users online. \n");
	printf("2. Specific information from an user \n");
	printf("3. Change Status\n");
	printf("4. Global message\n");
	printf("5. Private message to an user\n");
	printf("6. Exit. \n");
	printf("\n");
}

int get_client_option()
{
	// Client options
	int client_opt;
	std::cin >> client_opt;

	while (std::cin.fail())
	{
		std::cout << "Select a valid option: " << std::endl;
		std::cin.clear();
		std::cin.ignore(256, '\n');
		std::cin >> client_opt;
	}

	return client_opt;
}
int main(int argc, char *argv[])
{
	// Connection Structure
	int sockfd, numbytes;
	char buf[BUFFER_SZ];
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];

	if (argc != 4)
	{
		fprintf(stderr, "Registration: client <username> <server_ip> <server_port>\n\n Try again... \n");
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
			perror("client: connection");
			close(sockfd);
			continue;
		}

		break;
	}
	//conecction error
	if (p == NULL)
	{
		fprintf(stderr, "Connection Failed\n");
		return 2;
	}

	//Finish Connection
	inet_ntop(p->ai_family, get_server_address((struct sockaddr *)p->ai_addr),
			  s, sizeof s);
	printf("Connected to %s\n", s);
	freeaddrinfo(servinfo);


	// REgustration Message
	char buffer[BUFFER_SZ];
	std::string message_serialized;

	chat::Payload *firstMessage = new chat::Payload;

	firstMessage->set_sender(argv[1]);
	firstMessage->set_flag(chat::Payload_PayloadFlag_register_);
	firstMessage->set_ip(s);

	firstMessage->SerializeToString(&message_serialized);

	strcpy(buffer, message_serialized.c_str());
	send(sockfd, buffer, message_serialized.size() + 1, 0);

	// Server Answer
	recv(sockfd, buffer, BUFFER_SZ, 0);

	chat::Payload serverMessage;
	serverMessage.ParseFromString(buffer);

	// If the register is incorrect
	if(serverMessage.code() == 500){
			std::cout << "Error: "
			  << serverMessage.message()
			  << std::endl;
			return 0;
	}
	std::cout << "Server answers: "
			  << serverMessage.message()
			  << std::endl;	

	online = 1;

	// end listener thread
	pthread_t thread_id;
	pthread_attr_t attrs;
	pthread_attr_init(&attrs);
	pthread_create(&thread_id, &attrs, listenToMessages, (void *)&sockfd);

	print_menu(argv[1]);
	int client_opt;
	while (true)
	{
		while (serverResponse == 1){}

		printf("\nChoose an option:\n");
		client_opt = get_client_option();

		std::string msgSerialized;
		int bytesReceived, bytesSent;

		// Connected Users
		if (client_opt == 1)
		{
			chat::Payload *userList = new chat::Payload();
			userList->set_sender(argv[1]);
			userList->set_ip(s);
			userList->set_flag(chat::Payload_PayloadFlag_user_list);

			userList->SerializeToString(&msgSerialized);

			strcpy(buffer, msgSerialized.c_str());
			bytesSent = send(sockfd, buffer, msgSerialized.size() + 1, 0);
			serverResponse = 1;
		}
		// Information of an specific user
		else if (client_opt == 2)
		{
			std::string user_name;
			printf("Write the user name: \n");
			std::cin >> user_name;

			chat::Payload *userInf = new chat::Payload();
			userInf->set_sender(argv[1]);
			userInf->set_flag(chat::Payload_PayloadFlag_user_info);
			userInf->set_extra(user_name);
			userInf->set_ip(s);

			userInf->SerializeToString(&msgSerialized);

			strcpy(buffer, msgSerialized.c_str());
			bytesSent = send(sockfd, buffer, msgSerialized.size() + 1, 0);
			serverResponse = 1;
		}
		// Change Status
		else if(client_opt == 3){
			printf("New State: \n");
			printf("1. ACTIVE\n");
			printf("2. BUSY\n");
			printf("3. INACTIVE\n");
			int option;
			std::cin >> option;
			std::string newStatus;
			if (option < 4){
				newStatus = statusClient[option-1];
			}
			else
			{
				printf("Not Valid.\n");
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
			serverResponse = 1;
		}
		// Global Message
		else if (client_opt == 4)
		{
			inputMessage = 1;
			printf("Global Message: \n");
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
			serverResponse = 0;
			inputMessage = 0;
		}

		// Send DM
		else if (client_opt == 5)
		{
			printf("Destination User Name: \n");
			std::cin.ignore();
			std::string user_name;
			std::getline(std::cin, user_name);

			printf("Message: \n");
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
			serverResponse = 0;
		}
		
		else if (client_opt == 6)
		{
			int option;
			printf("What you want to do? \n");
			printf("1. Exit \n");
			printf("2. Stay \n");

			std::cin >> option;

			if (option == 1)
			{
				printf("Disconnected!\n");
				break;
			}
		}
		else
		{
			std::cout << "Not Valid" << std::endl;
		}
		
		
	}

	//Close
	pthread_cancel(thread_id);
	online = 0;
	close(sockfd);

	return 0;
}