#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>

// nb d'user max et taille max des messages
#define MAX_CLIENTS 10
#define BUFFER_SZ 2048

// Valeurs globales

static _Atomic unsigned int cli_count = 0; // nb client
static int uid = 10; // client id

// type client
typedef struct{
	struct sockaddr_in address;
	int sockfd;
	int uid;
	char name[32];
} client_t;

client_t *clients[MAX_CLIENTS];

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

// Affichage, met un >
void str_overwrite_stdout() {
    printf("\r%s", "> ");
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

// Affichage de l'adresse ip
void print_client_addr(struct sockaddr_in addr){
    printf("%d.%d.%d.%d",
        addr.sin_addr.s_addr & 0xff,
        (addr.sin_addr.s_addr & 0xff00) >> 8,
        (addr.sin_addr.s_addr & 0xff0000) >> 16,
        (addr.sin_addr.s_addr & 0xff000000) >> 24);
}

// Ajout d'un client
void queue_add(client_t *cl){
	// verouille l'accès au tableau de client au cas ou plusieurs connections en meme temps
	pthread_mutex_lock(&clients_mutex);

	//parcours le tableau jusqu'a une case qui contient null et ajoute le client
	for(int i=0; i < MAX_CLIENTS; ++i){
		if(!clients[i]){
			clients[i] = cl;
			break;
		}
	}

	// dévérouille le tableau
	pthread_mutex_unlock(&clients_mutex);
}

// Retire le client du serveur avec son id
void queue_remove(int uid){
	pthread_mutex_lock(&clients_mutex);

	for(int i=0; i < MAX_CLIENTS; ++i){
		if(clients[i]){
			if(clients[i]->uid == uid){
				clients[i] = NULL;
				break;
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

// Envoie un message a tous les clients sauf a celui qui la envoyé
void send_message(char *s, int uid){
	pthread_mutex_lock(&clients_mutex);

	for(int i=0; i<MAX_CLIENTS; ++i){
		if(clients[i]){
			if(clients[i]->uid != uid){
				if(write(clients[i]->sockfd, s, strlen(s)) < 0){
					perror("ERROR: write to descriptor failed");
					break;
				}
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

// Envoie un message a tout les clients
void send_message_all(char *s){
    pthread_mutex_lock(&clients_mutex);

    for(int i=0; i<MAX_CLIENTS; ++i){
        if(clients[i]){
			if(write(clients[i]->sockfd, s, strlen(s)) < 0){
				perror("ERROR: write to descriptor failed");
				break;
			}
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

// Envoie un message a un seul client
void send_mp(char *s, int uid){
    pthread_mutex_lock(&clients_mutex);

    for(int i=0; i<MAX_CLIENTS; ++i){
        if(clients[i]){
            if(clients[i]->uid == uid){
                if(write(clients[i]->sockfd, s, strlen(s)) < 0){
                    perror("ERROR: write to descriptor failed");
                    break;
                }
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

void send_manuel(int uid){
	char* s = "\nTo send a private message : /mp @username\nTo logout : /end\nTo request the manual : /man\n";
	send_mp(s,uid);
}

void catch_ctrl_c_and_exit(int sig) {
	send_message_all("\nThe chat ended.\n");
	printf("\nThe chat ended.\n");
	exit(0);
}

// Gère le client connecté
void *handle_client(void *arg){
	char buff_out[BUFFER_SZ]; // Message a envoyer
	char name[32];
	int leave_flag = 0;

	cli_count++;
	client_t *cli = (client_t *)arg;

	// Reçoit le nom du client connecté
	if(recv(cli->sockfd, name, 32, 0) <= 0 || strlen(name) <  2 || strlen(name) >= 32-1){
		printf("Didn't enter the name.\n");
		leave_flag = 1;
	} else{
		// Accueille le client
		strcpy(cli->name, name);
		sprintf(buff_out, "%s has joined\n", cli->name);
		printf("%s", buff_out);
		send_message(buff_out, cli->uid);
	}

	bzero(buff_out, BUFFER_SZ);

	while(1){
		// sort de la boucle 
		if (leave_flag) {
			break;
		}

		// reçoit les messages du client
		int receive = recv(cli->sockfd, buff_out, BUFFER_SZ, 0);
		if (receive > 0){
			if(strlen(buff_out) > 0){
				// la où commence réelement le msg (+2 pour : et l'espace qui suivent le nom)
				int index = strlen(cli->name)+2;
				int len = strlen(buff_out);
				char msg[len-index+1];
				memcpy(msg, &buff_out[index], len - index);
   				msg[len - index] = '\0';
				if(msg[0] == '/') {
					str_trim_lf(msg, strlen(msg));
					if(strcmp(msg, "/man") == 0){
						send_manuel(cli->uid);
					}
				}else{
					// envoie le message aux autres clients
					send_message(buff_out, cli->uid);
					str_trim_lf(buff_out, strlen(buff_out));
					printf("%s\n", buff_out);
				}
				
			}
		// si le client se déconnecte
		} else if (receive == 0 || strcmp(buff_out, "exit") == 0){
			// informe les autres clients
			sprintf(buff_out, "%s has left\n", cli->name);
			printf("%s", buff_out);
			send_message(buff_out, cli->uid);
			leave_flag = 1;
		} else {
			printf("ERROR: -1\n");
			leave_flag = 1;
		}

		bzero(buff_out, BUFFER_SZ);
	}

 	// Enleve le client du serveur
	close(cli->sockfd);
	queue_remove(cli->uid);
	free(cli);
	cli_count--;
	pthread_detach(pthread_self());

	return NULL;
}

int main(int argc, char **argv){
	if(argc != 2){
		printf("Usage: %s <port>\n", argv[0]);
		return EXIT_FAILURE;
	}

	char *ip = "127.0.0.1";
	int port = atoi(argv[1]); // port a ecouter
	int option = 1;
	int listenfd = 0, connfd = 0;
	struct sockaddr_in serv_addr;
	struct sockaddr_in cli_addr;
	pthread_t tid;

	// reglages de la socket
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(ip);
	serv_addr.sin_port = htons(port);

 	// ignore le signal pipe
	signal(SIGPIPE, SIG_IGN);
	// pour gerer le ctrl+c
	signal(SIGINT, catch_ctrl_c_and_exit);

	// ajoute SO_REUSEADDR a la socket : reutilise le meme port et la meme adresse ip pour plusieurs socket
	if(setsockopt(listenfd, SOL_SOCKET,SO_REUSEADDR,(char*)&option,sizeof(option)) < 0){
		perror("ERROR: setsockopt failed");
   		return EXIT_FAILURE;
	}

	// associe la socket a l'adresse et port du serveur
	if(bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
		perror("ERROR: Socket binding failed");
		return EXIT_FAILURE;
	}

 	// met le socket en mode ecoute
	if (listen(listenfd, 10) < 0) {
		perror("ERROR: Socket listening failed");
		return EXIT_FAILURE;
	}

	printf("=== WELCOME TO THE CHATROOM ===\n");

	while(1){
		// Accetpe les demande de connexion des clients
		socklen_t clilen = sizeof(cli_addr);
		connfd = accept(listenfd, (struct sockaddr*)&cli_addr, &clilen);

		// vérifie si le nombre de clients max est atteint
		if((cli_count + 1) == MAX_CLIENTS){
			printf("Max clients reached. Rejected: ");
			print_client_addr(cli_addr);
			printf(":%d\n", cli_addr.sin_port);
			close(connfd);
			continue;
		}

		// instancie le type client
		client_t *cli = (client_t *)malloc(sizeof(client_t));
		cli->address = cli_addr;
		cli->sockfd = connfd;
		cli->uid = uid++;

		// ajoute le client
		queue_add(cli);
		pthread_create(&tid, NULL, &handle_client, (void*)cli);

		sleep(1);
	}

	return EXIT_SUCCESS;
}
