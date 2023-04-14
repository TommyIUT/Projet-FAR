#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // Pour la fonction close()

int main(int argc, char *argv[]) {
  
  printf("Début programme\n");

  // Création de la socket
  int dS = socket(AF_INET, SOCK_STREAM, 0);
  if (dS == -1) {
    perror("Erreur création socket");
    exit(EXIT_FAILURE);
  }
  printf("Socket créée\n");

  // Préparation de l'adresse du serveur
  struct sockaddr_in ad;
  memset(&ad, 0, sizeof(ad)); // Initialisation à 0 pour éviter des erreurs de données résiduelles
  ad.sin_family = AF_INET;
  ad.sin_addr.s_addr = INADDR_ANY; // Le serveur accepte les connexions sur toutes ses interfaces réseau
  ad.sin_port = htons(atoi(argv[1])); // Le port d'écoute est spécifié en argument

  // Association de l'adresse au socket
  if (bind(dS, (struct sockaddr*)&ad, sizeof(ad)) == -1) {
    perror("Erreur nommage socket");
    close(dS);
    exit(EXIT_FAILURE);
  }
  printf("Socket nommée\n");

  // Attente de connexions entrantes
  if (listen(dS, 7) == -1) { // backlog = 7 (nombre de connexions en attente maximales)
    perror("Erreur mise en écoute");
    close(dS);
    exit(EXIT_FAILURE);
  }
  printf("Mode écoute\n");

  // Acceptation d'une connexion entrante
  struct sockaddr_in aC ;
  socklen_t lg = sizeof(struct sockaddr_in) ;
  int dSC = accept(dS, (struct sockaddr*) &aC, &lg) ;
  if (dSC == -1) {
    perror("Erreur acceptation connexion");
    close(dS);
    exit(EXIT_FAILURE);
  }
  printf("Client connecté\n");

  // Réception d'un message du client
  char msg[20] ;
  ssize_t n = recv(dSC, msg, sizeof(msg)-1, 0) ; // -1 pour réserver un octet pour le caractère nul final
  if (n == -1) {
    perror("Erreur réception message");
    close(dSC);
    close(dS);
    exit(EXIT_FAILURE);
  } else if (n == 0) {
    printf("Le client a fermé la connexion\n");
    close(dSC);
    close(dS);
    exit(EXIT_SUCCESS);
  }
  msg[n] = '\0'; // Ajout du caractère nul final pour terminer la chaîne de caractères
  printf("Message reçu : %s\n", msg) ;
  
  // Envoi d'une réponse au client
  int r = 10 ;
  if (send(dSC, &r, sizeof(int), 0) == -1) {
    perror("Erreur envoi réponse");
    close(dSC);
    close(dS);
    exit
