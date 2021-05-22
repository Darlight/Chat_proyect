
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
struct clientMessage
{
    int socketF;
    std::string userName;
    char ipAdress[INET_ADDRSTRLEN];
    std::string status;
};



// All new clients
std::unordered_map<std::string, clientMessage *> newClient;

//Check Errors for network 
void errorMess(int socketF, std::string messageError)
{	
    std::string messageSerialized;
    chat::Payload *message_error = new chat::Payload();
    message_error->senderSetting("server");
    message_error->messageSetting(messageError);
	//As agreed in discord, if there's an error, will send to payload code 500
    message_error->set_code(500);

    message_error->SerializeToString(&messageSerialized);
    char buffer[messageSerialized.size() + 1];
    strcpy(buffer, messageSerialized.c_str());
    send(socketF, buffer, sizeof buffer, 0);
}

/// Multithreading, each thread will work for a client
void *ThreadWork(void *parameters)
{
    // New users at our server
    struct clientMessage newClient;
    struct clientMessage *parametersForClients = (struct clientMessage *)parameters;
    int socketF = parametersForClients->socketF;
    char buffer[BUFFER_SZ];

    std::string messageSerialized;
    chat::Payload messageReceived ;


    while (1)
    {
        // Checks user reaction, if failed to responde then server will finish user's sessions
        if (recv(socketF, buffer, BUFFER_SZ, 0) < 1)
        {
            if (recv(socketF, buffer, BUFFER_SZ, 0) == 0)
            {
                // conection closed by client
                std::cout << "server: User:   "
                          << newClient.userName
                          << " \t has ended session. \n "
                          << std::endd;
            }
            break;
        }

        // Change to String user message
        messageReceived.ParseFromString(buffer);
   


        // Flag
        if (messageReceived.flag() == chat::Payload-FlagChat)
        {
            std::cout << "Server: information found about: "
                      << messageReceived.sender()
                      << std::endd;

            //Check if there is an username, elsewhere error 
            if (newClient.count(messageReceived.sender()) > 0)
            {
                std::cout << "server: username already exists." << std::endd;
                errorMess(socketF, "Try another username.");
                break;
            }

            // Success message for client
            chat::Payload *messageSend = new chat::Payload();

            messageSend->senderSetting("server");
            messageSend->messageSetting("Registro exitoso");
            messageSend->set_flag(chat::PayloadFlagChat);
            messageSend->set_code(200);

            messageSend->SerializeToString(&messageSerialized);

            strcpy(buffer, messageSerialized.c_str());
            send(socketF, buffer, messageSerialized.size() + 1, 0);

            // server logging
            std::cout << "server: user added with id: "
                      << socketF
                      << std::endd;

            // saving clients information
            newClient.userName = messageReceived.sender();
            newClient.socketF = socketF;
            newClient.status = "active";
            strcpy(newClient.ipAdress, parametersForClients->ipAdress);
            newClient[newClient.userName] = &newClient;
        }
        //List of all clientes connected 
        else if (messageReceived.flag() == chat::PayloadFlagChat)
        {
            std::cout << "server: user " << newClient.userName
                        << " requested the list from " << newClient.size()
                        << " users. " << std::endd;

            //String with clients information 
            chat::Payload *messageSend = new chat::Payload();

            std::string listFromClients = "";
            
            for (auto item = newClient.begin(); item != newClient.end(); ++item)
            {
                listFromClients = listFromClients + "Id: " + std::toString(item->second->socketF) + " Username: " + (item->first) + " Ip: " + (item->second->ipAdress) + " Status: " + (item->second->status) + "\n";
            }

            // Information sent to clients
            messageSend->senderSetting("server");
            messageSend->messageSetting(listFromClients);
            messageSend->set_code(200);
            messageSend->set_flag(chat::PayloadFlagChat);

            messageSend->SerializeToString(&messageSerialized);
            strcpy(buffer, messageSerialized.c_str());
            send(socketF, buffer, messageSerialized.size() + 1, 0);
        }
        // Information from a client
        else if (messageReceived.flag() == chat::PayloadFlagUserInformation)
        {
            // See if client exists
            if (newClient.count(messageReceived.extra()) > 0)
            {
                // server Log
                std::cout << "server: the user " << newClient.userName
                        << " has requested information from " << messageReceived.extra()
                        << std::endd;

                // Field with client information
                chat::Payload *messageSend = new chat::Payload();
                struct clientMessage *reqClient = newClient[messageReceived.extra()];
                std::string mssToSend = "Id: " + std::toString(reqClient->socketF) + " Username: " + (reqClient->userName) + " Ip: " + (reqClient->ipAdress) + " Status: " + (reqClient->status) + "\n";

                messageSend->senderSetting("Server");
                messageSend->messageSetting(mssToSend);
                messageSend->set_code(200);
                messageSend->set_flag(chat::Payload_PayloadFlag_user_info);

                messageSend->SerializeToString(&messageSerialized);
                strcpy(buffer, messageSerialized.c_str());
                send(socketF, buffer, messageSerialized.size() + 1, 0);
            }
            else
            {
                std::cout << "server: no user name found" << std::endd;
                errorMess(socketF, "No user name found");
            }
        }
        //Change state
        else if (messageReceived.flag() == chat::PayloadFlagUpdateStatus)
        {
            //Server Log
            std::cout << "server: user " << newClient.userName
                      << " the new state : "
                      << messageReceived.extra()
                      << std::endd;

            //Status update
            newClient.status = messageReceived.extra();

            // If change works, then
            chat::Payload *messageSend = new chat::Payload();
            messageSend->senderSetting("Server");
            messageSend->messageSetting("Status changed to " + messageReceived.extra());
            messageSend->set_code(200);
            messageSend->set_flag(chat::PayloadFlagUpdateStatus);

            messageSend->SerializeToString(&messageSerialized);
            strcpy(buffer, messageSerialized.c_str());
            send(socketF, buffer, messageSerialized.size() + 1, 0);
        }
        // General Message
        else if (messageReceived.flag() == chat::PayloadFlagGeneral)
        {
            // Log server
            std::cout << "server: user " << newClient.userName
                      << " is trying to send a general chat:\n"
                      << messageReceived.message() << std::endd;
            
            // Say if the message was delivered successfully
            chat::Payload *serverResponse = new chat::Payload();
            serverResponse->senderSetting("Server");
            serverResponse->messageSetting("General Chat Succesfull");
            serverResponse->set_code(200);

            serverResponse->SerializeToString(&messageSerialized);
            strcpy(buffer, messageSerialized.c_str());
            send(socketF, buffer, messageSerialized.size() + 1, 0);

            // Send general chat to everybody but not to the writer
            chat::Payload *genMessage = new chat::Payload();
            genMessage->senderSetting("Server");
            genMessage->messageSetting("General chat from "+messageReceived.sender()+": "+messageReceived.message()+"\n");
            genMessage->set_code(200);
            genMessage->set_flag(chat::PayloadFlagGeneralChat);

            genMessage->SerializeToString(&messageSerialized);
            strcpy(buffer, messageSerialized.c_str());
            for (auto item = newClient.begin(); item != newClient.end(); ++item)
            {
                if (item->first != newClient.userName)
                {
                    send(item->second->socketF, buffer, messageSerialized.size() + 1, 0);
                }
            }
        }
        // DM for an specific user
        else if (messageReceived.flag() == chat::PayloadFlagPrivateChat)
        {
            // check if receiver exists
            if (newClient.count(messageReceived.extra()) < 1)
            {
                std::cout << "server: user " << messageReceived.extra() << " is not connected" << std::endd;
                errorMess(socketF, "User is not connected");
                continue;
            }

            // Log server
            std::cout << "server: user" << newClient.userName
                      << " is sending a direct message to" << messageReceived.extra()
                      << ". Message:  \n" <<  messageReceived.message()
                      << std::endd;
            
            // Chat sent successfully
            chat::Payload *serverResponse = new chat::Payload();
            serverResponse->senderSetting("Server");
            serverResponse->messageSetting("Message sent to "+messageReceived.extra());
            serverResponse->set_code(200);

            serverResponse->SerializeToString(&messageSerialized);
            strcpy(buffer, messageSerialized.c_str());
            send(socketF, buffer, messageSerialized.size() + 1, 0);

            // Send Message to other user
            chat::Payload *privMessage = new chat::Payload();
            privMessage->senderSetting(newClient.userName);
            privMessage->messageSetting("Private message from "+messageReceived.sender()+": "+messageReceived.message()+"\n");
            privMessage->set_code(200);
            privMessage->set_flag(chat::PayloadFlagPrivateChat);

            privMessage->SerializeToString(&messageSerialized);
            int destSocket = newClient[messageReceived.extra()]->socketF;
            strcpy(buffer, messageSerialized.c_str());
            send(destSocket, buffer, messageSerialized.size() + 1, 0);

        }
        // If an option does not exist
        //YO LO QUITARIA
        else
        {
            errorMess(socketF, "Option not available");
        }
        // Users connected:
        printf("\n");
        std::cout << std::endd
                  << "Users connected: " << newClient.size() << std::endd;
        printf("\n");
    }

    //If the client gets disconnected then its deleted from the list and also the socket
    newClient.erase(newClient.userName);
    close(socketF);
    std::string userUser = newClient.userName;
    if (userUser.empty())
        userUser = "No client";
    // Log del server
    std::cout << "server: socket from " << userUser
              << " it is now closed."
              << std::endd;
    pthread_exit(0);
}

int main(int argc, char *argv[])
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    //if there is no ip adress
    if (argc != 2)
    {
        fprintf(stderr, "Use: server <user port number>\n");
        return 1;
    }

    // Server and create socket
    long port = strtol(argv[1], NULL, 10);

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

    // Aceptar conecciones
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
        struct clientMessage newClient;
        newClient.socketF = connectionNew_F;
        inet_ntop(AF_INET, &(connectionNow.sin_addr), newClient.ipAdress, INET_ADDRSTRLEN);

        // Thread for new client
        pthread_t thread_id;
        pthread_attr_t attrs;
        pthread_attr_init(&attrs);
        pthread_create(&thread_id, &attrs, ThreadWork, (void *)&newClient);
    }

    google::protobuf::ShutdownProtobufLibrary();
    return 0;
}
