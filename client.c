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
#include <dirent.h>

#define LENGTH 2048 // Longueur max d'un message
#define SIZE 1024


int port;
char *ip;
// Variables globales
volatile sig_atomic_t flag = 0; // gère les signaux
int sockfd = 0;
char name[32];

// File management structure
struct sfile { char* ip; char* filename; } sfiles; 
typedef struct sfile sfile;

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

int isCommand(char* msg) { // Determines if the message contains a command
    if (msg[0] == '/') { return 1; }
    else { return 0; }
}

int listFile() { 
    struct dirent *dir;
    DIR *d = opendir("."); 
    if (d) {
      while ((dir = readdir(d)) != NULL) { printf("%s\n", dir->d_name); } // There are some files
      closedir(d);
    }
    return 0;
}

char** getCommand(char* msg) { // recupere la commande
    char * save = (char*)malloc(sizeof(char)*strlen(msg));
    strcpy(save,msg);
    static char *datas[2];
    for (int i=0;i<2;i++) { datas[i]=(char *)malloc(sizeof(char) * 200); } // Initialize the returned array

    char d[] = " ";
    char *p = strtok(msg, d);
    int i = 0;
    while((p != NULL) && (i<2)) { // Recover the two parts of the command
        strcpy(datas[i],p);
        p = strtok(NULL, d);
        i++;
    }
    free(save);
    return datas;
}

void send_file(FILE *fp, int sockfile) { 
  int n;
  char data[SIZE] = {0};
  int bytes_received;

  while((bytes_received=fread(data, sizeof(char), SIZE, fp)) != (unsigned long) NULL) { // There are data to send
    if (send(sockfile, data, bytes_received, 0) == -1) { perror("[-]Error in sending file"); exit(1);}
    bzero(data, SIZE);
  }

  close(sockfile);
}

void file() { 
  int stop = 0;
  strtok(sfiles.filename,"\n");  
  
  listFile();
  int filenoexists = 1;
  char* filename = malloc(sizeof(char)*SIZE);
  
  char* buffer = malloc(sizeof(char)*SIZE);

 while(filenoexists){

  printf("Écrivez le nom du fichier a envoyer:\n");
  fgets( filename, (sizeof(char*)*5), stdin );
  strtok(filename,"\0");
  strtok(filename,"\n");
  sfiles.filename = filename;
  
  struct dirent *dir;
  DIR *d = opendir("."); 
  if (d)  {
    while ((dir = readdir(d)) != NULL) { // There are some files
      if (strcmp(sfiles.filename,dir->d_name) == 0) { stop=1; filenoexists=0; }
    }
    closedir(d);
  }
  if (filenoexists){printf("Ce fichier n'existe pas.\n");}
 }


  if (stop) { // Filename exists
    int e;
    int sockfile;

    struct sockaddr_in server_addrfile;
    FILE *fp ;
    sockfile = socket(AF_INET, SOCK_STREAM, 0);
    server_addrfile.sin_family = AF_INET;
    server_addrfile.sin_port = htons(port+1);
    server_addrfile.sin_addr.s_addr = inet_addr(ip);
    
    
    e = connect(sockfile, (struct sockaddr*)&server_addrfile, sizeof(server_addrfile)); // Connection to the socket

   // while (e==-1) { e = connect(sockfile, (struct sockaddr*)&server_addrfile, sizeof(server_addrfile));}

    if( send(sockfile,sfiles.filename,SIZE,0) < 0) { perror("[-]Send failed"); exit(0); }
 
    fp = fopen(sfiles.filename, "r");
    if (fp == NULL) { perror("[-]Error in reading file"); exit(1);}
 
    send_file(fp, sockfile); // Send the file
    printf("\033[32;1;1m## File sent ##\033[0m\n\n");
 
    close(sockfile);
  } 
  else { printf("\033[32;1;1m## This file does not exist ##\033[0m\n\n"); }

  pthread_exit(0);
}


void executeCommand(char* content, char* ip, char* buffer, char* nameUser) {
    char * save = (char*)malloc(sizeof(char)*100);
    char ** command = getCommand(content);
    char * toCompare = command[0];
    char* name = command[1];
    strcpy(save,toCompare);
    strcat(save," ");
    strcat(save,name);
    strtok(toCompare,"\0");
    strtok(toCompare,"\n");
    if (strcmp(toCompare,"/list") == 0) { listFile(); } // List of personnal files
    else if (strcmp(toCompare,"/man") == 0) { // ask for manual
		// L'envoie au serveur
		send(sockfd, buffer, strlen(buffer), 0);
    }
    else if (strcmp(toCompare,"/mp") == 0) { // send mp
		// L'envoie au serveur
		send(sockfd, buffer, strlen(buffer), 0);
    }else if (strcmp(toCompare,"/send") == 0) { // send a file to the server
      send(sockfd, buffer, strlen(buffer), 0);
      pthread_t threadRFile; 
      pthread_create(&threadRFile, NULL, (void*)file, NULL); // Creates a thread that manages the reciving of a file
      pthread_join(threadRFile,NULL);
    }else if (strcmp(toCompare,"/dl") == 0) { // Download a file from the server
      send(sockfd, buffer, strlen(buffer), 0);
      
		
    }else{
		printf("Cette commande n'existe pas\n");
	}
    free(toCompare);
    free(save);
}

// Gère le ctrl+C
void catch_ctrl_c_and_exit(int sig) {
    flag = 1;
}

// envoie des msg au serveur
void send_msg_handler(char *argv[]) {
	char message[LENGTH] = {};
	char buffer[LENGTH + 32] = {};

	while(1) {

		// ecrire le message
		str_overwrite_stdout();
		fgets(message, LENGTH, stdin);
		str_trim_lf(message, LENGTH);
    int iscommand=isCommand(message);

		// sort de la boucle quand 'fin' est ecrit
		if (strcmp(message, "fin") == 0) {
			break;
    }else if(iscommand){
    // affiche le message
    sprintf(buffer, "%s: %s\n", name, message);
    executeCommand(message,argv[1], buffer, name);
		} else {
			// affiche le message
			sprintf(buffer, "%s: %s\n", name, message);
			// L'envoie au serveur
			send(sockfd, buffer, strlen(buffer), 0);
		}

		// remet tout a 0
		bzero(message, LENGTH);
		bzero(buffer, LENGTH + 32);
	}
	catch_ctrl_c_and_exit(2);
}

// récupère les messages du serveur
void recv_msg_handler() {
	char message[LENGTH] = {};
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

int main(int argc, char **argv){
	// si l'utilisateur n'a pas mis d'arguments
	if(argc != 2){
		printf("Usage: %s <port>\n", argv[0]);
		return EXIT_FAILURE;
	}

	// attribue le port et l'adresse ip
  ip=malloc(15);
	strcpy(ip,"127.0.0.1");
	port = atoi(argv[1]);

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
	printf("=== WELCOME TO THE CHATROOM ===\n");

	// crée le thread qui envoie les messages
	pthread_t send_msg_thread;
  	if(pthread_create(&send_msg_thread, NULL, (void *) send_msg_handler, argv) != 0){
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
