#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 8080

int main() {
    int sock = 0, valread;
    struct sockaddr_in serv_addr;
    char buffer[1024] = {0};

    // Création du socket client
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }

    // Configuration de l'adresse du serveur
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convertir l'adresse IP du serveur en format binaire
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    // Connexion au serveur
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        return -1;
    }

    // Boucle pour envoyer et recevoir des messages
    while (1) {
        // Lecture du message du clavier
        printf("Enter message: ");
        fgets(buffer, 1024, stdin);

        // Envoi du message au serveur
        send(sock, buffer, strlen(buffer), 0);

        // Vérification si le message est "fin"
        if (strcmp(buffer, "fin\n") == 0) {
            break;
        }

        // Lecture du message en retour du serveur
        valread = read(sock, buffer, 1024);
        printf("Server: %s", buffer);

        // Effacement du buffer
        memset(buffer, 0, sizeof(buffer));
    }

    // Fermeture du socket client
    close(sock);

    return 0;
}
