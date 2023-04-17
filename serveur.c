#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 8080
#define MAX_CLIENTS 2

int main() {
    int server_fd, new_socket, valread;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[1024] = {0};
    int client_sockets[MAX_CLIENTS] = {0};
    int num_clients = 0;

    // Création du socket serveur
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Configuration de l'adresse du serveur
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Attachement du socket à l'adresse du serveur
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Ecoute du socket pour des connexions entrantes
    if (listen(server_fd, 2) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Waiting for clients...\n");

    // Boucle pour gérer les clients
    while (1) {
        // Attente d'une connexion entrante
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept failed");
            exit(EXIT_FAILURE);
        }

        // Ajout du nouveau client à la liste des clients
        client_sockets[num_clients++] = new_socket;

        // Envoi d'un message de bienvenue au nouveau client
        char *welcome_message = "Welcome to the chat server!\n";
        send(new_socket, welcome_message, strlen(welcome_message), 0);

        printf("New client connected: %s\n", inet_ntoa(address.sin_addr));

        // Si le nombre de clients est atteint, on peut commencer à relayer les messages
        if (num_clients == MAX_CLIENTS) {
            // Boucle pour relayer les messages entre les clients
            while (1) {
                // Lecture du message du premier client
                valread = read(client_sockets[0], buffer, 1024);
                printf("Client 1: %s\n", buffer);

                // Vérification si le message est "fin"
                if (strcmp(buffer, "fin\n") == 0) {
                    break;
                }

                // Envoi du message du premier client au deuxième client
                send(client_sockets[1], buffer, strlen(buffer), 0);

                // Effacement du buffer
                memset(buffer, 0, sizeof(buffer));

                // Lecture du message du deuxième client
                valread = read(client_sockets[1], buffer, 1024);
                printf("Client 2: %s\n", buffer);

                // Vérification si le message est "fin"
                if (strcmp(buffer, "fin\n") == 0) {
                    break;
                }

                // Envoi du message
            send(client_sockets[0], buffer, strlen(buffer), 0);

            // Effacement du buffer
            memset(buffer, 0, sizeof(buffer));
        }

        // Fermeture des sockets clients
        close(client_sockets[0]);
        close(client_sockets[1]);
        // Réinitialisation de la liste des sockets clients et du nombre de clients
        num_clients = 0;
        memset(client_sockets, 0, sizeof(client_sockets));
        printf("Chat session ended.\n");
    }
}

return 0;
}