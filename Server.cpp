
/*
Universidad del Valle de Guatemala
Sistemas operativos
Ing. Erick Pineda

Esteban del Valle 18221
Andrea Paniagua 18733
Mario Perdomo  18029

Proposito: Mantiene el server
para los clientes, incluyendo sincronizacion de mensajes y registro de usuarios
*/

#include <iostream>
#include <string>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unordered_map>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <fcntl.h>
#include <fstream>
#include <pthread.h>
#include <stdarg.h>
#include <sys/syscall.h>
#include "payload.pb.h"

#define MAX_newClient 100
#define BUFFER_SZ 4096

// The client contains the sockets, status, userName, ip_address
struct ChatClient
{
    int socketFd;
    std::string userName;
    char ipAddr[INET_ADDRSTRLEN];
    std::string status;
};



// All new clients
std::unordered_map<std::string, ChatClient *> clients_entering;

//Check Errors for network 
void errorMess(int socketFd, std::string messageError)
{	
    std::string messageSerialized;
    chat::Payload *message_error = new chat::Payload();
    message_error->set_sender("server");
    message_error->set_message(messageError);
	//As agreed in discord, if there's an error, will send to payload code 500
    message_error->set_code(500);

    message_error->SerializeToString(&messageSerialized);
    char buffer[messageSerialized.size() + 1];
    strcpy(buffer, messageSerialized.c_str());
    send(socketFd, buffer, sizeof buffer, 0);
}

/// Multithreading, each thread will work for a client
void *ThreadWork(void *parameters)
{
    // New users at our server
    struct ChatClient newClient;
    struct ChatClient *clientParameters = (struct ChatClient *)parameters;
    int socketFd = clientParameters->socketFd;
    char buffer[BUFFER_SZ];
    std::string messageSerialized;
    chat::Payload messageReceived;
    while (1)
    {
        // Checks user reaction, if failed to responde then server will finish user's sessions
        if (recv(socketFd, buffer, BUFFER_SZ, 0) < 1)
        {
            if (recv(socketFd, buffer, BUFFER_SZ, 0) == 0)
            {
                // conection closed by client
                std::cout << "server: User:   "<< newClient.userName << "  has ended session. \n "<< std::endl;
            }
            break;
        }
        messageReceived.ParseFromString(buffer); // Change to String user message
        if (messageReceived.flag() == chat::Payload_PayloadFlag_register_) // Flag
        {
            std::cout << "Server: information found about: "<< messageReceived.sender()<< std::endl;
            if (clients_entering.count(messageReceived.sender()) > 0) //Check if there is an username, elsewhere error 
            {
                std::cout << "server: username already exists." << std::endl;
                errorMess(socketFd, "Try another username.");
                break;
            }
            chat::Payload *messageSend = new chat::Payload(); // Success message for client

            messageSend->set_sender("server");
            messageSend->set_message("Successfull register");
            messageSend->set_flag(chat::Payload_PayloadFlag_register_);
            messageSend->set_code(200);
            messageSend->SerializeToString(&messageSerialized);

            strcpy(buffer, messageSerialized.c_str());
            send(socketFd, buffer, messageSerialized.size() + 1, 0);
            std::cout << "server: user added with id: "<< socketFd<< std::endl; // server logging

            // saving clients information
            newClient.userName = messageReceived.sender();
            newClient.socketFd = socketFd;
            newClient.status = "active";
            strcpy(newClient.ipAddr, clientParameters->ipAddr);
            clients_entering[newClient.userName] = &newClient;
        }
        else if (messageReceived.flag() == chat::Payload_PayloadFlag_user_list) //List of all clientes connected 
        {
            std::cout << "server: user " << newClient.userName << " requested the list from " << clients_entering.size()<< " users. " << std::endl;
            chat::Payload *messageSend = new chat::Payload();//String with clients information
            std::string listFromClients = "";
            
            for (auto item = clients_entering.begin(); item != clients_entering.end(); ++item)
            {
                listFromClients = listFromClients + "Id: " + std::to_string(item->second->socketFd) + " Username: " + (item->first) + " Ip: " + (item->second->ipAddr) + " Status: " + (item->second->status) + "\n";
            }

            // Information sent to clients
            messageSend->set_sender("server");
            messageSend->set_message(listFromClients);
            messageSend->set_code(200);
            messageSend->set_flag(chat::Payload_PayloadFlag_user_list);
            messageSend->SerializeToString(&messageSerialized);
            strcpy(buffer, messageSerialized.c_str());
            send(socketFd, buffer, messageSerialized.size() + 1, 0);
        }
        else if (messageReceived.flag() == chat::Payload_PayloadFlag_user_info) // Information from a client
        {
            if (clients_entering.count(messageReceived.extra()) > 0) // See if client exists
            {
                std::cout << "server: the user " << newClient.userName<< " has requested information from " << messageReceived.extra()<< std::endl; // server Log

                // Field with client information
                chat::Payload *messageSend = new chat::Payload();
                struct ChatClient *reqClient = clients_entering[messageReceived.extra()];
                std::string mssToSend = "Id: " + std::to_string(reqClient->socketFd) + " Username: " + (reqClient->userName) + " Ip: " + (reqClient->ipAddr) + " Status: " + (reqClient->status) + "\n";

                messageSend->set_sender("Server");
                messageSend->set_message(mssToSend);
                messageSend->set_code(200);
                messageSend->set_flag(chat::Payload_PayloadFlag_user_info);

                messageSend->SerializeToString(&messageSerialized);
                strcpy(buffer, messageSerialized.c_str());
                send(socketFd, buffer, messageSerialized.size() + 1, 0);
            }
            else
            {
                std::cout << "server: no user name found" << std::endl;
                errorMess(socketFd, "No user name found");
            }
        }
        else if (messageReceived.flag() == chat::Payload_PayloadFlag_update_status) //Change state
        {
            std::cout << "server: user " << newClient.userName<< " the new state : "<< messageReceived.extra()<< std::endl;//Server Log

            newClient.status = messageReceived.extra();//Status update

            // If change works, then
            chat::Payload *messageSend = new chat::Payload();
            messageSend->set_sender("Server");
            messageSend->set_message("Status changed to " + messageReceived.extra());
            messageSend->set_code(200);
            messageSend->set_flag(chat::Payload_PayloadFlag_update_status);
            messageSend->SerializeToString(&messageSerialized);
            strcpy(buffer, messageSerialized.c_str());
            send(socketFd, buffer, messageSerialized.size() + 1, 0);
        }
        else if (messageReceived.flag() == chat::Payload_PayloadFlag_general_chat) // General Message
        {
            std::cout << "server: user " << newClient.userName<< " is trying to send a general chat:\n"<< messageReceived.message() << std::endl; // Log server            
            // Say if the message was delivered successfully
            chat::Payload *serverResponse = new chat::Payload();
            serverResponse->set_sender("Server");
            serverResponse->set_message("General Chat Succesfull");
            serverResponse->set_code(200);
            serverResponse->SerializeToString(&messageSerialized);
            strcpy(buffer, messageSerialized.c_str());
            send(socketFd, buffer, messageSerialized.size() + 1, 0);

            // Send general chat to everybody but not to the writer
            chat::Payload *genMessage = new chat::Payload();
            genMessage->set_sender("Server");
            genMessage->set_message("General chat from "+messageReceived.sender()+": "+messageReceived.message()+"\n");
            genMessage->set_code(200);
            genMessage->set_flag(chat::Payload_PayloadFlag_general_chat);
            genMessage->SerializeToString(&messageSerialized);
            strcpy(buffer, messageSerialized.c_str());
            for (auto item = clients_entering.begin(); item != clients_entering.end(); ++item)
            {
                if (item->first != newClient.userName)
                {
                    send(item->second->socketFd, buffer, messageSerialized.size() + 1, 0);
                }
            }
        }
        else if (messageReceived.flag() == chat::Payload_PayloadFlag_private_chat)// DM for an specific user
        {
            if (clients_entering.count(messageReceived.extra()) < 1)// check if receiver exists
            {
                std::cout << "server: user " << messageReceived.extra() << " is not connected" << std::endl;
                errorMess(socketFd, "User is not connected");
                continue;
            }
            std::cout << "server: user" << newClient.userName<< " is sending a direct message to" << messageReceived.extra()<< ". Message:  \n" <<  messageReceived.message()<< std::endl;// Log server       
            // Chat sent successfully
            chat::Payload *serverResponse = new chat::Payload();
            serverResponse->set_sender("Server");
            serverResponse->set_message("Message sent to "+messageReceived.extra());
            serverResponse->set_code(200);
            serverResponse->SerializeToString(&messageSerialized);
            strcpy(buffer, messageSerialized.c_str());
            send(socketFd, buffer, messageSerialized.size() + 1, 0);

            // Send Message to other user
            chat::Payload *privMessage = new chat::Payload();
            privMessage->set_sender(newClient.userName);
            privMessage->set_message("Private message from "+messageReceived.sender()+": "+messageReceived.message()+"\n");
            privMessage->set_code(200);
            privMessage->set_flag(chat::Payload_PayloadFlag_private_chat);
            privMessage->SerializeToString(&messageSerialized);
            int destSocket = clients_entering[messageReceived.extra()]->socketFd;
            strcpy(buffer, messageSerialized.c_str());
            send(destSocket, buffer, messageSerialized.size() + 1, 0);
        }
        // If an option does not exist
        else
        {
            errorMess(socketFd, "Option not available");
        }
        // Users connected:
        printf("\n");
        std::cout << std::endl << "Users connected: " << clients_entering.size() <<std::endl;
        printf("\n");
    }

    //If the client gets disconnected then its deleted from the list and also the socket
    clients_entering.erase(newClient.userName);
    close(socketFd);
    std::string userUser = newClient.userName;
    if (userUser.empty())
        userUser = "No client";
    // Log del server
    std::cout << "server: socket from " << userUser << " it is now closed."<< std::endl;
    pthread_exit(0);
}
int main(int argc, char *argv[])
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;
    if (argc != 2) //if there is no ip adress
    {
        fprintf(stderr, "Use: server <user port number>\n");
        return 1;
    }
    long port = strtol(argv[1], NULL, 10);// Server and create socket

    sockaddr_in server, connectionNow;
    socklen_t new_conn_size;
    int socket_F, connectionNew_F;
    char connectionNow_address[INET_ADDRSTRLEN];
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = INADDR_ANY;
    memset(server.sin_zero, 0, sizeof server.sin_zero);

    if ((socket_F = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        fprintf(stderr, "server: error creando socket.\n");
        return 1;
    }

    if (bind(socket_F, (struct sockaddr *)&server, sizeof(server)) == -1)
    {
        close(socket_F);
        fprintf(stderr, "server: error  IP:Port-socket\n");
        return 2;
    }

    // listen connections
    if (listen(socket_F, 5) == -1)
    {
        close(socket_F);
        fprintf(stderr, "server: error listen().\n");
        return 3;
    }
    printf("server: sound from port %ld\n", port);

    // Accept connections
    while (1)
    {
        new_conn_size = sizeof connectionNow;
        connectionNew_F = accept(socket_F, (struct sockaddr *)&connectionNow, &new_conn_size);
        if (connectionNew_F == -1)
        {
            perror("error accept()");
            continue;
        }

        // New Client
        struct ChatClient newClient;
        newClient.socketFd = connectionNew_F;
        inet_ntop(AF_INET, &(connectionNow.sin_addr), newClient.ipAddr, INET_ADDRSTRLEN);
        // Thread for new client
        pthread_t thread_id;
        pthread_attr_t attrs;
        pthread_attr_init(&attrs);
        pthread_create(&thread_id, &attrs, ThreadWork, (void *)&newClient);
    }

    google::protobuf::ShutdownProtobufLibrary();
    return 0;
}
