#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define LENGTH 2048 // Longueur max d'un message


// Variables globales
volatile sig_atomic_t flag = 0; // gère les signaux
int sockfd = 0;
char name[32];
char id[32];

// Affichage, met un >
void str_overwrite_stdout() {
  printf("%s", "> ");
  // vide le tampon de sortie
  fflush(stdout);
}

// supprimer le charactère '\n'
void str_trim_lf (char* arr, int length) {
  int i;
  for (i = 0; i < length; i++) { // dernier charactère
    if (arr[i] == '\n') {
      arr[i] = '\0';
      break;
    }
  }
}

//----------------------------------------------------------------------

// Gère le ctrl+C
void catch_ctrl_c_and_exit(int sig) {
    flag = 1;
}

//----------------------------------------------------------------------

// envoie des msg au serveur
void send_msg_handler() {
	char message[LENGTH] = {};
	char buffer[LENGTH + 32] = {};

	while(1) {

		// ecrire le message
		str_overwrite_stdout();
		fgets(message, LENGTH, stdin);
		str_trim_lf(message, LENGTH);

		// sort de la boucle quand 'fin' est ecrit
		if (strcmp(message, "fin") == 0) {
			break;
		} else {
			// affiche le message
			sprintf(buffer, "%s~ %s\n", name, message);
			// L'envoie au serveur
			send(sockfd, buffer, strlen(buffer), 0);
		}

		// remet tout a 0
		bzero(message, LENGTH);
		bzero(buffer, LENGTH + 32);
	}
	catch_ctrl_c_and_exit(2);
}

//----------------------------------------------------------------------

// récupère les messages du serveur
void recv_msg_handler() {
	char message[LENGTH] = {};
	char buffer[LENGTH + 32] = {};
	int index;
	while (1) {
		// recoit les données de la socket et les stocke dans message
		int receive = recv(sockfd, message, LENGTH, 0);
		if (receive > 0) {
			// affiche le message
			printf("%s", message);
			str_overwrite_stdout();	  
		} else if (receive == 0) {
				break;
		} else {
			// -1
		}
		// vide message
		memset(message, 0, sizeof(message));
	}
}

// -------------------------------------------------------------------------------

int main(int argc, char **argv){

	// si l'utilisateur n'a pas mis d'arguments
	if(argc != 2){
		printf("Usage: %s <port>\n", argv[0]);
		return EXIT_FAILURE;
	}

	// attribue le port et l'adresse ip
	char *ip = "127.0.0.1";
	int port = atoi(argv[1]);

	// indique avec quelle fonction gerer le signal
	signal(SIGINT, catch_ctrl_c_and_exit);

	// demande le nom du client
	printf("Please enter your name: ");
	fgets(name, 32, stdin);
	str_trim_lf(name, strlen(name));

	// erreur si le nom est trop court
	if (strlen(name) > 32 || strlen(name) < 2){
		printf("Name must be less than 30 and more than 2 characters.\n");
		return EXIT_FAILURE;
	}

	struct sockaddr_in server_addr;

	// reglages socket
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(ip);
	server_addr.sin_port = htons(port);


 	 // Connexion au serveur
  	int err = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
 	 if (err == -1) {
		printf("ERROR: connect\n");
		return EXIT_FAILURE;
	}

	// Envoie le nom du client
	send(sockfd, name, 32, 0);

	// Message qui indique qu'on est bien connectés
	printf("\e[1;31m" "=== WELCOME TO THE CHATROOM ===\n" "\e[0m");

	// crée le thread qui envoie les messages
	pthread_t send_msg_thread;
  	if(pthread_create(&send_msg_thread, NULL, (void *) send_msg_handler, NULL) != 0){
		printf("ERROR: pthread\n");
    return EXIT_FAILURE;
	}

	// crée un autre thread qui lit les messages en continu
	pthread_t recv_msg_thread;
  	if(pthread_create(&recv_msg_thread, NULL, (void *) recv_msg_handler, NULL) != 0){
		printf("ERROR: pthread\n");
		return EXIT_FAILURE;
	}

	// boucle infinie jusqu'a un ctrlc+c
	while (1){
		if(flag){
			printf("\nAu revoir\n");
			break;
   		}
	}

	// ferme la socket
	close(sockfd);
	return EXIT_SUCCESS;
}
