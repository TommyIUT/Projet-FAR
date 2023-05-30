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
#include <dirent.h>

// nb d'user max et taille max des messages
#define MAX_CLIENTS 4000
#define BUFFER_SZ 3000
#define CHANNEL_SZ 11

#define TAILLE_MAX_FICHIER 1024
#define PORT_RECEIVE port + 1

// Valeurs globales
int dSF;											   // socket receive file
static _Atomic unsigned int cli_count = 0;		
static _Atomic unsigned int channel_count = 0;		   // nb client connecté
static _Atomic unsigned int cli_restant = MAX_CLIENTS; // nb client restant
static int uid = 10;								   // client id

// type client
typedef struct
{
	struct sockaddr_in address;
	int sockfd;
	int uid;
	char name[32];
	int channel_co;
} client_t;

typedef struct
{
	int uid_channel;
	char name[32];
	int cli_co; // combien de user co 
} channel;

client_t *clients[MAX_CLIENTS];
//ca marche pas
channel* liste_channel[CHANNEL_SZ];


pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

// Affichage, met un >
void str_overwrite_stdout()
{
	printf("\n%s", "> ");
	// vide le tampon de sortie
	fflush(stdout);
}

// supprimer le charactère '\n'
void str_trim_lf(char *arr, int length)
{
	int i;
	for (i = 0; i < length; i++)
	{ // dernier caractère
		if (arr[i] == '\n')
		{
			arr[i] = '\0';
			break;
		}
	}
}

// Affichage de l'adresse ip
void print_client_addr(struct sockaddr_in addr)
{
	printf("%d.%d.%d.%d",
		   addr.sin_addr.s_addr & 0xff,
		   (addr.sin_addr.s_addr & 0xff00) >> 8,
		   (addr.sin_addr.s_addr & 0xff0000) >> 16,
		   (addr.sin_addr.s_addr & 0xff000000) >> 24);
}

// Ajout d'un client
void queue_add(client_t *cl)
{
	// verouille l'accès au tableau de client au cas ou plusieurs connections en meme temps
	pthread_mutex_lock(&clients_mutex);

	// parcours le tableau jusqu'a une case qui contient null et ajoute le client
	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		if (!clients[i])
		{
			clients[i] = cl;
			clients[i]->uid = i;
			break;
		}
	}

	// dévérouille le tableau
	pthread_mutex_unlock(&clients_mutex);
}

// Retire le client du serveur avec son id
void queue_remove(int uid)
{
	pthread_mutex_lock(&clients_mutex);

	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		if (clients[i])
		{
			if (clients[i]->uid == uid)
			{
				clients[i] = NULL;
				break;
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

// Envoie un message à tous les clients sauf à celui qui la envoyé
void send_message(char *s, client_t* cli)
{
	pthread_mutex_lock(&clients_mutex);

	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		if (clients[i])
		{
			if (clients[i]->uid != cli->uid && clients[i]->channel_co == cli->channel_co )
			{
				if (write(clients[i]->sockfd, s, strlen(s)) < 0)
				{
					perror("ERROR: write to descriptor failed");
					break;
				}
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

// Envoie un message a tout les clients
void send_message_all(char *s)
{
	pthread_mutex_lock(&clients_mutex);

	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		if (clients[i])
		{
			if (write(clients[i]->sockfd, s, strlen(s)) < 0)
			{
				perror("ERROR: write to descriptor failed");
				break;
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

// Envoie un message a un seul client
void send_mp(char *s, int uid)
{
	pthread_mutex_lock(&clients_mutex);

	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		if (clients[i])
		{
			if (clients[i]->uid == uid)
			{
				if (write(clients[i]->sockfd, s, strlen(s)) < 0)
				{
					perror("ERROR: write to descriptor failed");
					break;
				}
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

// envoie le manuel à l'utilisateur
// mod
void send_manuel(int uid)
{
	char *s = "\e[1;34m"
			  "\nTo send a private message : /mp username message\nTo logout : /end\nTo request the manual : /man\nTo receive a file : /rf id_file\nTo request the list of the files on the server : /li\nTo create a channel : /cc name_channel\nTo request the list of the channels : /lc\nTo join a channel : /jc id_channel\nTo leave a channel : /dc\n"
			  "\e[0m";
	send_mp(s, uid);
}
// fin mod
// gère les messages privés
void mp_handler(char *s, int uid)
{
	char buff_out[BUFFER_SZ]; // Message a envoyer
	char *id_receive = malloc(sizeof(char) * (strlen(s) + 1));
	char *message = malloc(sizeof(char) * (strlen(s) + 1));

	// Récupère l'utilisateur destinataire
	int i = 4; // commencer après "/mp "
	int j = 0;
	while (s[i] != ' ' && s[i] != '\0')
	{
		id_receive[j] = s[i];
		i++;
		j++;
	}
	id_receive[j] = '\0'; // ajouter le caractère de fin de chaîne

	// Récupère le message
	i++; // sauter l'espace
	j = 0;
	while (s[i] != '\0')
	{
		message[j] = s[i];
		i++;
		j++;
	}
	message[j] = '\0'; // ajouter le caractère de fin de chaîne

	// recupere le pseudo de celui qui envoit
	char *id_send = malloc(sizeof(char) * (strlen(s) + 1));
	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		if (clients[i])
		{
			if (clients[i]->uid == uid)
			{
				strcpy(id_send, clients[i]->name);
			}
		}
	}

	// recupere l'uid du receveur'
	int uid_receive = -1;
	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		if (clients[i])
		{
			if (strcmp(clients[i]->name, id_receive) == 0)
			{
				uid_receive = i;
			}
		}
	}

	if (uid_receive == -1)
	{
		printf("Le pseudo du destinataire n'existe pas.\n");
		sprintf(buff_out, "Le pseudo du destinataire n'existe pas.\n");
		send_mp(buff_out, uid);
	}
	else
	{
		printf("%s -> %s : %s\n", id_send, id_receive, message);
		// envoie le mp mod
		sprintf(buff_out, "\e[1;33m %s vous chuchote : %s\e[0m \n", id_send, message);
		send_mp(buff_out, uid_receive);
	}
	free(id_receive);
	free(message);
}

void catch_ctrl_c_and_exit(int sig)
{
	send_message_all("\nThe chat ended.\n");
	printf("\nThe chat ended.\n");
	exit(0);
}




/* Renvois la liste des fichiers dans /clientfiles  */
char **listFile(int *tailleListe)
{
	DIR *d;
	struct dirent *dir;
	d = opendir("./serverfiles");
	if (d)
	{
		int i = 0;
		char **fileList = NULL;
		while ((dir = readdir(d)) != NULL)
		{
			if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0)
			{
				fileList = realloc(fileList, (i + 1) * sizeof(char *));
				fileList[i] = malloc(strlen(dir->d_name) + 1);
				strcpy(fileList[i], dir->d_name);
				// pos mais marche pas
				i++;
			}
		}
		closedir(d);
		fileList = realloc(fileList, (i + 1) * sizeof(char *)); // ajouter un pointeur NULL à la fin du tableau
		fileList[i] = NULL;
		*tailleListe = i;
		return fileList;
	}
	return NULL;
}

void afficher_channel_handler(int uid)
{
	char msg[100];
    send_mp("Voici la liste des channels:\n", uid);
	if (liste_channel[0] == NULL) {
            send_mp("\e[1;31m Il n'y a pas de channels pour l'instant \e[0m\n", uid);
        }
    for (int i = 0; i < CHANNEL_SZ; i++)
    {
		
        if (liste_channel[i] != NULL) {
			sprintf(msg,"%s , id : %d",liste_channel[i]->name,i);
            send_mp(msg, uid);
			send_mp("\n",uid);
        }
		
    }
}


void create_channel_handler(char *s, int uid) {
    char buff_out[BUFFER_SZ]; // Message à envoyer
    char channel_name[32];    // Nom du canal extrait de la commande
    int leave_flag = 0;

    strcpy(channel_name, s + 4); // Extrait le nom du canal à partir de la position 4

    // Vérifie si le nom du canal est vide ou invalide
    if (strlen(channel_name) < 2 || strlen(channel_name) >= 32 - 1) {
        printf("Invalid channel name.\n");
        sprintf(buff_out, "\e[32mInvalid channel name\e[0m\n");
        send_mp(buff_out, uid);
        leave_flag = 1;
    }

    if (!leave_flag) {
        // Vérifie si le nom du canal est déjà utilisé
        int duplicate = 0;
        for (int i = 0; i < CHANNEL_SZ; ++i) {
            if (liste_channel[i]) {
                if (strcmp(liste_channel[i]->name, channel_name) == 0) {
                    duplicate = 1;
                    break;
                }
            }
        }

        // Si duplicate est vrai, le nom du canal est déjà utilisé
        if (duplicate) {
            printf("Channel name %s is already used.\n", channel_name);
            sprintf(buff_out, "\e[32mChannel name %s is already used\e[0m\n", channel_name);
            send_mp(buff_out, uid);
            leave_flag = 1;
        } else {
            // Crée le nouveau canal
            channel *new_channel = (channel *)malloc(sizeof(channel));

            strcpy(new_channel->name, channel_name);


            // Ajoute le canal à la liste des canaux
            int index = -1;
            for (int i = 0; i < CHANNEL_SZ; ++i) {
                if (!liste_channel[i]) {
                    index = i;
                    break;
                }
            }

            if (index != -1) {
				 // Utilise l'index comme ID du canal
                new_channel->uid_channel = index;
                liste_channel[index] = new_channel;
            } else {
                printf("No available slots for new channels.\n");
                sprintf(buff_out, "\e[32mNo available slots for new channels\e[0m\n");
                send_mp(buff_out, uid);
                leave_flag = 1;
            }

            if (!leave_flag) {
                // Envoie un message de confirmation au client
                sprintf(buff_out, "\e[32mChannel %s has been created.\e[0m\n", channel_name);
                send_mp(buff_out, uid);
            }
        }
    }

    bzero(buff_out, BUFFER_SZ);
}

channel* find_the_channel(int id_channel, int uid) {
    for (int i = 0; i < CHANNEL_SZ; i++) {
        if (i == id_channel) {
            return liste_channel[i];
        }
    }
    
    send_mp("Nous ne trouvons pas votre channel", uid);
    return NULL;
}

char find_the_channel_name(int id_channel) {
    for (int i = 0; i < CHANNEL_SZ; i++) {
        if (i == id_channel) {
            return liste_channel[i]->name;
        }
    }
    
    send_mp("Nous ne trouvons pas votre channel", uid);
    return NULL;
}


void join_channel(int uid, channel* Channel, client_t* cli) {
	char msg[100];
	cli->channel_co = Channel->uid_channel;
	sprintf(msg,"   \e[1;31m=== WELCOME TO %s ===\e[0m\n", Channel->name);
    send_mp(msg, uid);
	Channel->cli_co++;
}

void leave_channel(client_t* cli, channel* Channel) {
	char msg[100];
	sprintf(msg,"\e[1;30myou are leaving channel %s..\n \e[0m",Channel->name);
	cli->channel_co =11;
	send_mp(msg,cli->uid);
	send_mp("\n",cli->uid);
	send_mp("\e[1;31m"
		   "=== WELCOME TO THE CHATROOM ===\n"
		   "\e[0m",cli->uid);
}




void function_handler(char *s, int uid, client_t *cli)
{
	if (strcmp(s, "/man") == 0)
	{
		send_manuel(uid);
	}
	if (s[1] == 'm' && s[2] == 'p' && s[3] == ' ')
	{
		mp_handler(s, uid);
	}
	if (s[1] == 'c' && s[2] == 'c' && s[3] == ' ')
	{
		create_channel_handler(s, uid);
	}
	if (s[1] == 'l' && s[2] == 'c')
	{
		afficher_channel_handler(uid);
	}
	if (s[1] == 'd' && s[2] == 'c')
	{
		for(int i=0;i<CHANNEL_SZ;i++){
			if(i==cli->channel_co){
				leave_channel(cli,liste_channel[i]);
			}
		}
	}
	if (s[1] == 'j' && s[2] == 'c' && s[3]==' ')
		{
			int channel_id = atoi(&s[4]);
			// // 	// Effectuez les actions requises avec l'identifiant du fichier
			printf("Client selected channel ID: %d\n", channel_id);

			channel* channel_voulu = find_the_channel(channel_id,uid);
			printf("Client selected channel name: %s\n", channel_voulu->name);
			join_channel(uid,channel_voulu, cli);
		}

	// POUR LES FICHIERS --------------------------------------------------

	if ((s[1] == 'r' && s[2] == 'f' && s[3]==' ') || (s[1] == 'l' && s[2] == 'i'))
	{
		int nbFichier = 0;
		char id_fichier[32];
		char chaine[100];
		char nom_fichier[200];
		char **fileList = listFile(&nbFichier);
		int index;
		// POUR LES RECEVOIR  --------------------------------------------------
		if (s[1] == 'r' && s[2] == 'f')
		{
			int file_id = atoi(&s[4]);
			// // 	// Effectuez les actions requises avec l'identifiant du fichier
			printf("Client selected file ID: %d\n", file_id);

		}
		// POUR LES LISTER --------------------------------------------------
		if (s[1] == 'l' && s[2] == 'i')
		{

			send(uid, &nbFichier, sizeof(nbFichier), 0);
			sprintf(chaine, "Nombre de fichier : %d\n", nbFichier);
			printf("%s\n", chaine);
			send_mp(chaine, uid);
			send_mp("liste des fichiers :\n", uid);
			for (int i = 0; i < nbFichier; i++)
			{
				sprintf(nom_fichier, "%s id: %d\n", fileList[i], i);
				send_mp(nom_fichier, uid);
			}
		}
	}
}

	
// Gère le client connecté
void *handle_client(void *arg)
{
	char buff_out[BUFFER_SZ]; // Message a envoyer
	char name[32];
	int leave_flag = 0;

	cli_count++;
	cli_restant--;
	client_t *cli = (client_t *)arg;

	// Reçoit le nom du client connecté
	if (recv(cli->sockfd, name, 32, 0) <= 0 || strlen(name) < 2 || strlen(name) >= 32 - 1)
	{
		printf("Didn't enter the name.\n");
		leave_flag = 1;
	}
	else
	{
		// vérifie si le pseudo a un espace
		int ouiespace = 0;
		for (int i = 0; name[i] != '\0'; i++)
		{
			if (name[i] == ' ')
			{
				ouiespace = 1;
			}
		}

		// vérifie si le pseudo est unique
		int doublon = 1;
		for (int i = 0; i < MAX_CLIENTS; ++i)
		{
			if (clients[i])
			{
				if (strcmp(clients[i]->name, name) == 0)
				{
					doublon = 0;
				}
			}
		}

		// si doublon == 0 le client a un nom pas unique => ca dégage
		if (doublon == 0)
		{

			printf(" Name %s is already used\n", name);
			sprintf(buff_out, "\e[32m Name %s is already used\e[0m\n", name);
			send_mp(buff_out, cli->uid);
			leave_flag = 1;
			// si ouiespace == 1 le client a un espace dans son nom on accepte pas
		}
		else if (ouiespace == 1)
		{
			printf("Le pseudo contient des espaces\n");
			sprintf(buff_out, "Pas d'espace dans le pseudo svp\n");
			send_mp(buff_out, cli->uid);
			leave_flag = 1;
		}
		else
		{
			// il est co au channel serveur au debut
			cli->channel_co =11;
			// Accueille le client
			strcpy(cli->name, name);
			sprintf(buff_out, "\e[32m%s\e[0m has joined\n", cli->name);
			printf("%s\n", buff_out);
			printf("%d places restantes.\n", cli_restant);
			send_message(buff_out, cli);
			// envoyer au client qui vient de se connecter 5/10 par ex, le nb de gens connectés
			sprintf(buff_out, "\e[1;35m Utilisateurs connectés : %d/%d \e[0m\n", cli_count, MAX_CLIENTS);
			send_mp(buff_out, cli->uid);
			send_manuel(cli->uid);
		}
	}

	bzero(buff_out, BUFFER_SZ);

	while (1)
	{
		// sort de la boucle
		if (leave_flag)
		{
			break;
		}

		// reçoit les messages du client

		int receive = recv(cli->sockfd, buff_out, BUFFER_SZ, 0);
		if (receive > 0)
		{
			if (strlen(buff_out) > 0)
			{
				int index = strlen(cli->name) + 2;
				char *name = malloc(index + 1); // Alloue de la mémoire pour le tableau name
				memcpy(name, buff_out, index);
				name[index] = '\0';

				char name_colored[index + 15]; // +15 pour la taille de la séquence ANSI de couleur
				strcpy(name_colored, "\033[1;32m");
				strcat(name_colored, name);

				int len = strlen(buff_out);
				char msg[len - index + 1];
				memcpy(msg, &buff_out[index], len - index);
				msg[len - index+1] = '\0';
				

				char buff_out_modified[len + index + 30]; // + index + 15 pour la taille du nom en rouge
				strcpy(buff_out_modified, name_colored);
				strcat(buff_out_modified, "in ");
				if(cli->channel_co==11) {
					strcat(buff_out_modified, "serveur : \033[0m");
				} else {
					for(int i=0;i<CHANNEL_SZ-1;i++) {
						if(i==cli->channel_co){
							strcat(buff_out_modified,liste_channel[i]->name);
							strcat(buff_out_modified, ": \033[0m");
						}
					}
				}
				strcat(buff_out_modified, "\033[0m");
				strcat(buff_out_modified, msg);

				if (msg[0] == '/')
				{
					str_trim_lf(msg, strlen(msg));
					function_handler(msg,cli->uid, cli);
				}
				else
				{
					// envoie le message aux autres clients
					send_message(buff_out_modified, cli);
					str_trim_lf(buff_out_modified, strlen(buff_out_modified));
					printf("%s\n", buff_out_modified);
				}

				// Libère la mémoire allouée pour name
				free(name);
			}
			// si le client se déconnecte
		}
		else if (receive == 0 || strcmp(buff_out, "exit") == 0)
		{
			// informe les autres clients
			sprintf(buff_out, "%s has left\n", cli->name);
			printf("%s", buff_out);
			send_message(buff_out, cli);
			leave_flag = 1;
		}
		else
		{
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
	cli_restant++;
	pthread_detach(pthread_self());

	return NULL;
}

// ---------------------------------------------------------------

int main(int argc, char **argv)
{
	if (argc != 2)
	{
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

	dSF = socket(PF_INET, SOCK_STREAM, 0);

	struct sockaddr_in adF;
	adF.sin_family = AF_INET;
	adF.sin_addr.s_addr = INADDR_ANY;
	adF.sin_port = htons(PORT_RECEIVE);

	/* Nommage de la socket reception fichier du serveur */
	if (bind(dSF, (struct sockaddr *)&adF, sizeof(adF)) == -1)
	{
		perror("Erreur dans le bind serveur de la socket reception file");
		exit(1);
	}
	listen(dSF, MAX_CLIENTS);

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
	if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (char *)&option, sizeof(option)) < 0)
	{
		perror("ERROR: setsockopt failed");
		return EXIT_FAILURE;
	}

	// associe la socket a l'adresse et port du serveur
	if (bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		perror("ERROR: Socket binding failed");
		return EXIT_FAILURE;
	}

	// met le socket en mode ecoute
	if (listen(listenfd, 10) < 0)
	{
		perror("ERROR: Socket listening failed");
		return EXIT_FAILURE;
	}
	channel *new_channel = (channel *)malloc(sizeof(channel));
	strcpy(new_channel->name,"serveur");
	liste_channel[CHANNEL_SZ] = new_channel;

	printf("\e[1;31m=== WELCOME TO THE CHATROOM ===\e[0m\n");
	
	while (1)
	{
		// Accetpe les demande de connexion des clients
		socklen_t clilen = sizeof(cli_addr);
		connfd = accept(listenfd, (struct sockaddr *)&cli_addr, &clilen);

		// vérifie si le nombre de clients max est atteint
		if ((cli_count + 1) == MAX_CLIENTS)
		{
			printf("\e[1;31m"
				   "Max clients reached. Rejected: "
				   "\e[0m");
			print_client_addr(cli_addr);
			printf(":%d\n", cli_addr.sin_port);
			close(connfd);
			continue;
		}

		// instancie le type client
		client_t *cli = (client_t *)malloc(sizeof(client_t));
		cli->address = cli_addr;
		cli->sockfd = connfd;

		// ajoute le client
		queue_add(cli);
		pthread_create(&tid, NULL, &handle_client, (void *)cli);

		sleep(1);
	}

	return EXIT_SUCCESS;
}
