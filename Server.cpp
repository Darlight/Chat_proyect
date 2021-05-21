
/*
Universidad del Valle de Guatemala
Sistemas operativos
Ing. Erick Pineda

Esteban del Valle 18221
Andrea Paniagua 18733
Mario Perdomo  18029

Proposito: Mantiene el servidor
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

#define MAX_new_clients 100
#define BUFFER_SZ 4096

// Structure of the client
// Being the sockets, status, username and finally, ip_address
//MOdificable, podemos cambiar el nombre de estructura y posicion
struct ChatClient
{
    int socketFd;
    std::string username;
    char ipAddr[INET_ADDRSTRLEN];
    std::string status;
};



// List of new_clients
std::unordered_map<std::string, ChatClient *> new_clients;

// Error method checking for problems with network
void SendErrorResponse(int socketFd, std::string errorMsg)
{	
    std::string msgSerialized;
    chat::Payload *errorMessage = new chat::Payload();
    errorMessage->set_sender("server");
    errorMessage->set_message(errorMsg);
	//As agreed in discord, if there's an error, will send to payload code 500
    errorMessage->set_code(500);

    errorMessage->SerializeToString(&msgSerialized);
    char buffer[msgSerialized.size() + 1];
    strcpy(buffer, msgSerialized.c_str());
    send(socketFd, buffer, sizeof buffer, 0);
}

/// Multithreading for the clients, each thread will take care of a user
// in the chatroom
void *ThreadWork(void *params)
{
    // New users at our server
    struct ChatClient newClient;
    struct ChatClient *clientParameters = (struct ChatClient *)params;
    int socketFd = clientParameters->socketFd;
    char buffer[BUFFER_SZ];

    std::string msgSerialized;
    chat::Payload messageReceived ;


    while (1)
    {
        // THis check if there's a reaction for a user, if failed to responde
		// server will terminate user's session
        if (recv(socketFd, buffer, BUFFER_SZ, 0) < 1)
        {
            if (recv(socketFd, buffer, BUFFER_SZ, 0) == 0)
            {
                // cliente cerro conexion
                std::cout << "Servidor: User:   "
                          << newClient.username
                          << " \t has ended session. \n "
                          << std::endl;
            }
            break;
        }

        // Parsing the user's message
        messageReceived.ParseFromString(buffer);
   


        // Register Flag
        if (messageReceived.flag() == chat::Payload_PayloadFlag_register_)
        {
            std::cout << "Servidor: se recibió informacion de: "
                      << messageReceived.sender()
                      << std::endl;

            // Revisar si nombre de usuario ya existe, si ya existe, se devuelver un error.
            if (new_clients.count(messageReceived.sender()) > 0)
            {
                std::cout << "Servidor: el nombre de usuario ya existe." << std::endl;
                SendErrorResponse(socketFd, "El nombre de usuario ya existe.");
                break;
            }

            // Indicarle al cliente que se registro correctamente
            chat::Payload *messageSend = new chat::Payload();

            messageSend->set_sender("server");
            messageSend->set_message("Registro exitoso");
            messageSend->set_flag(chat::Payload_PayloadFlag_register_);
            messageSend->set_code(200);

            messageSend->SerializeToString(&msgSerialized);

            strcpy(buffer, msgSerialized.c_str());
            send(socketFd, buffer, msgSerialized.size() + 1, 0);

            // Log del server
            std::cout << "Servidor:  usuario agregado exitosamente con id: "
                      << socketFd
                      << std::endl;

            // Guardar informacion de nuevo cliente
            newClient.username = messageReceived.sender();
            newClient.socketFd = socketFd;
            newClient.status = "ACTIVO";
            strcpy(newClient.ipAddr, clientParameters->ipAddr);
            new_clients[newClient.username] = &newClient;
        }
        // Servicio que manda la lista de todos los usuarios conectados
        else if (messageReceived.flag() == chat::Payload_PayloadFlag_user_list)
        {
            std::cout << "Servidor: el usuario " << newClient.username
                        << " ha solicitado lista de los " << new_clients.size()
                        << " usuario/s. " << std::endl;

            // Se concatena en un string toda la informacion de los clientes
            chat::Payload *messageSend = new chat::Payload();

            std::string list_to_sent = "";
            
            for (auto item = new_clients.begin(); item != new_clients.end(); ++item)
            {
                list_to_sent = list_to_sent + "Id: " + std::to_string(item->second->socketFd) + " Username: " + (item->first) + " Ip: " + (item->second->ipAddr) + " Status: " + (item->second->status) + "\n";
            }

            // Envio de informacion a los clientes
            messageSend->set_sender("server");
            messageSend->set_message(list_to_sent);
            messageSend->set_code(200);
            messageSend->set_flag(chat::Payload_PayloadFlag_user_list);

            messageSend->SerializeToString(&msgSerialized);
            strcpy(buffer, msgSerialized.c_str());
            send(socketFd, buffer, msgSerialized.size() + 1, 0);
        }
        // Servicio que brinda informacion de un cliente en especifico
        else if (messageReceived.flag() == chat::Payload_PayloadFlag_user_info)
        {
            // Verificar que el cliente especificado exista
            if (new_clients.count(messageReceived.extra()) > 0)
            {
                // Log del servidor
                std::cout << "Servidor: el usuario " << newClient.username
                        << " ha solicitado la informacion del usuario " << messageReceived.extra()
                        << std::endl;

                // Se llena con la informacion del cliente especificado
                chat::Payload *messageSend = new chat::Payload();
                struct ChatClient *reqClient = new_clients[messageReceived.extra()];
                std::string mssToSend = "Id: " + std::to_string(reqClient->socketFd) + " Username: " + (reqClient->username) + " Ip: " + (reqClient->ipAddr) + " Status: " + (reqClient->status) + "\n";

                messageSend->set_sender("Server");
                messageSend->set_message(mssToSend);
                messageSend->set_code(200);
                messageSend->set_flag(chat::Payload_PayloadFlag_user_info);

                messageSend->SerializeToString(&msgSerialized);
                strcpy(buffer, msgSerialized.c_str());
                send(socketFd, buffer, msgSerialized.size() + 1, 0);
            }
            else
            {
                std::cout << "Servidor: No existe el nombre de usuario solicitado" << std::endl;
                SendErrorResponse(socketFd, "No existe el nombre de usuario especificado.");
            }
        }
        // Servicio que cambia el estado
        else if (messageReceived.flag() == chat::Payload_PayloadFlag_update_status)
        {
            //Log del server
            std::cout << "Servidor: usuario " << newClient.username
                      << " ha solicitado un nuevo estado: "
                      << messageReceived.extra()
                      << std::endl;

            // Actualizar estado de cliente
            newClient.status = messageReceived.extra();

            // Indicar que el cambio fue exitoso
            chat::Payload *messageSend = new chat::Payload();
            messageSend->set_sender("Server");
            messageSend->set_message("Se ha actualizado el status correctamente a " + messageReceived.extra());
            messageSend->set_code(200);
            messageSend->set_flag(chat::Payload_PayloadFlag_update_status);

            messageSend->SerializeToString(&msgSerialized);
            strcpy(buffer, msgSerialized.c_str());
            send(socketFd, buffer, msgSerialized.size() + 1, 0);
        }
        // Servicio que manda un mensaje general
        else if (messageReceived.flag() == chat::Payload_PayloadFlag_general_chat)
        {
            // Log del server
            std::cout << "Servidor: el usuario " << newClient.username
                      << " desea enviar un mensaje general:\n"
                      << messageReceived.message() << std::endl;
            
            // Indicarle al remitente que el mensaje se mando exitosamente
            chat::Payload *serverResponse = new chat::Payload();
            serverResponse->set_sender("Server");
            serverResponse->set_message("Mensaje general mandado correctamente");
            serverResponse->set_code(200);

            serverResponse->SerializeToString(&msgSerialized);
            strcpy(buffer, msgSerialized.c_str());
            send(socketFd, buffer, msgSerialized.size() + 1, 0);

            // Mandar el mensaje general a todos los clientes conectados menos al remitente
            chat::Payload *genMessage = new chat::Payload();
            genMessage->set_sender("Server");
            genMessage->set_message("Mensaje general de "+messageReceived.sender()+": "+messageReceived.message()+"\n");
            genMessage->set_code(200);
            genMessage->set_flag(chat::Payload_PayloadFlag_general_chat);

            genMessage->SerializeToString(&msgSerialized);
            strcpy(buffer, msgSerialized.c_str());
            for (auto item = new_clients.begin(); item != new_clients.end(); ++item)
            {
                if (item->first != newClient.username)
                {
                    send(item->second->socketFd, buffer, msgSerialized.size() + 1, 0);
                }
            }
        }
        // Servicio que manda un mensaje directo a un usuario especificado
        else if (messageReceived.flag() == chat::Payload_PayloadFlag_private_chat)
        {
            // Validar que usuario destinatario exista y este conectado
            if (new_clients.count(messageReceived.extra()) < 1)
            {
                std::cout << "Servidor: el usuario " << messageReceived.extra() << " no existe o no está conectado" << std::endl;
                SendErrorResponse(socketFd, "Destinario no existe o no esta conectado");
                continue;
            }

            // Log del server
            std::cout << "Servidor: el usuario " << newClient.username
                      << " desea enviar un mensaje privado a " << messageReceived.extra()
                      << ". El mensaje es: \n" <<  messageReceived.message()
                      << std::endl;
            
            // Indicarle al remitente que el mensaje se mando exitosamente
            chat::Payload *serverResponse = new chat::Payload();
            serverResponse->set_sender("Server");
            serverResponse->set_message("Mensaje privado mandado correctamente a "+messageReceived.extra());
            serverResponse->set_code(200);

            serverResponse->SerializeToString(&msgSerialized);
            strcpy(buffer, msgSerialized.c_str());
            send(socketFd, buffer, msgSerialized.size() + 1, 0);

            // Enviar mensaje a destinatario
            chat::Payload *privMessage = new chat::Payload();
            privMessage->set_sender(newClient.username);
            privMessage->set_message("Mensaje privado de "+messageReceived.sender()+": "+messageReceived.message()+"\n");
            privMessage->set_code(200);
            privMessage->set_flag(chat::Payload_PayloadFlag_private_chat);

            privMessage->SerializeToString(&msgSerialized);
            int destSocket = new_clients[messageReceived.extra()]->socketFd;
            strcpy(buffer, msgSerialized.c_str());
            send(destSocket, buffer, msgSerialized.size() + 1, 0);

        }
        // En el hipotetico caso que se mande una opcion que no existe
        else
        {
            SendErrorResponse(socketFd, "Opcion indicada no existe.");
        }
        // Se indica siempre cuantos usuarios estan conectados
        printf("________________________________________________________\n");
        std::cout << std::endl
                  << "Usuarios actuales: " << new_clients.size() << std::endl;
        printf("________________________________________________________\n");
    }

    // Cuando el cliente se desconecta se elimina de la lista y se cierra su socket
    new_clients.erase(newClient.username);
    close(socketFd);
    std::string thisUser = newClient.username;
    if (thisUser.empty())
        thisUser = "No cliente";
    // Log del server
    std::cout << "Servidor: socket de " << thisUser
              << " cerrado."
              << std::endl;
    pthread_exit(0);
}

int main(int argc, char *argv[])
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    //Cuando no se indica el puerto del server
    if (argc != 2)
    {
        fprintf(stderr, "Uso: server <puertodelservidor>\n");
        return 1;
    }

    // Manejo general del server y creacion del socket
    long port = strtol(argv[1], NULL, 10);

    sockaddr_in server, incoming_conn;
    socklen_t new_conn_size;
    int socket_fd, new_conn_fd;
    char incoming_conn_addr[INET_ADDRSTRLEN];

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = INADDR_ANY;
    memset(server.sin_zero, 0, sizeof server.sin_zero);

    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        fprintf(stderr, "Servidor: error creando socket.\n");
        return 1;
    }

    if (bind(socket_fd, (struct sockaddr *)&server, sizeof(server)) == -1)
    {
        close(socket_fd);
        fprintf(stderr, "Servidor: error bindeando IP:Puerto a socket.\n");
        return 2;
    }

    // escuchar conexiones
    if (listen(socket_fd, 5) == -1)
    {
        close(socket_fd);
        fprintf(stderr, "Servidor: error en listen().\n");
        return 3;
    }
    printf("Servidor: escuchando en puerto %ld\n", port);

    // Aceptar conecciones
    while (1)
    {
        new_conn_size = sizeof incoming_conn;
        new_conn_fd = accept(socket_fd, (struct sockaddr *)&incoming_conn, &new_conn_size);
        if (new_conn_fd == -1)
        {
            perror("error en accept()");
            continue;
        }

        // Aceptar nuevo cliente
        struct ChatClient newClient;
        newClient.socketFd = new_conn_fd;
        inet_ntop(AF_INET, &(incoming_conn.sin_addr), newClient.ipAddr, INET_ADDRSTRLEN);

        // Thread para le nuevo cliente
        pthread_t thread_id;
        pthread_attr_t attrs;
        pthread_attr_init(&attrs);
        pthread_create(&thread_id, &attrs, ThreadWork, (void *)&newClient);
    }

    google::protobuf::ShutdownProtobufLibrary();
    return 0;
}
