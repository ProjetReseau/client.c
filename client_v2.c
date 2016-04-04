#include<stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include <unistd.h>
#include<string.h>
#include<strings.h>
#include<netdb.h>
#include"protocole.h"
#include "file.h"
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

#ifndef EWOULDBLOCK
	#define EWOULDBLOCK EAGAIN
#endif



static char pseudo[TAILLE_PSEUDO];
 pthread_t th;

fifo *recu,*envoi;

int saisir_texte(char *chaine, int longueur);
void send_file(int sock);
void copy_file (const char *source, const char *destination);

void * connexion(void* socK){
  int sock=*((int*)socK);
  trame trame_read;
  trame trame_write;
  trame trame1;
  char pseudo_dist[TAILLE_PSEUDO];
  char datas[TAILLE_MAX_MESSAGE+32];
  ssize_t result_read;
  useconds_t timeToSleep=100;
  int nchar=0;
  char buffer[2*TAILLE_MAX_MESSAGE];
  FILE *dest=NULL;
  int ok=0;
  int taille_re=0;
  int taille_w=0;
  int ok_close=0;
  struct stat fic;
  int taille_w2=0;

  dest=fopen("./res","w");

  if (dest==NULL){
	printf("Erreur open\n");
	exit(EXIT_FAILURE);
  }

//  char pseudo[TAILLE_PSEUDO];

/*  printf("Choisir un pseudo: ");
  saisir_texte(pseudo,TAILLE_PSEUDO);
*/
  //Dis "bonjour !" en envoyant son pseudo
  bzero(trame1.message,TAILLE_MAX_MESSAGE);
  strcpy(trame1.message,pseudo);
  trame1.type_message=hello;
  trame1.taille=strlen(trame1.message);
  fcntl(sock,F_SETFL,fcntl(sock,F_GETFL)|O_NONBLOCK);
  strcpy(buffer,tr_to_str(trame1));
  write(sock,buffer,sizeof(buffer));



  while(1){
    bzero(trame_read.message,TAILLE_MAX_MESSAGE);
    timeToSleep=100;
    
    //Phase lecture
    errno=0;
    if((nchar=read(sock,buffer,sizeof(buffer)))==0){
      printf("Connexion interrompue\n");
      close(sock);
      exit(EXIT_FAILURE);
    }
    result_read=errno;

    str_to_tr(buffer,&trame_read);

    if ((result_read != EWOULDBLOCK)&&(result_read != EAGAIN)){
      bzero(datas,sizeof(datas));
      timeToSleep=1;
      printf("Type reçu: %d\n", trame_read.type_message);
      if (trame_read.type_message==hello){
	strcpy(pseudo_dist,trame_read.message);
	/*s*/printf(/*datas,*/"%s vient de se connecter \n",pseudo_dist);
	
      }else if(trame_read.type_message==quit){
	printf("Fermeture de connexion (en toute tranquillité)\n");
	write(sock,(void *)&trame_read,trame_read.taille);
	close(sock);
	exit(EXIT_FAILURE);
      }
	else if (trame_read.type_message==fileTransfert){
		printf("Reception d'un file ....\n");
		lstat("./res",&fic);
		if (!ok){
			taille_re=trame_read.taille;
			printf("Le fichier a recevoir taille: %d\n",taille_re);
			ok=1;
		}
	//	receive_file(sock, trame_read);
		else {
			taille_w=fwrite(trame_read.message,sizeof(char),(nchar-sizeof(trame_read.type_message)-sizeof(int)),dest);
			printf("J'ai ecris %d dans dest\n", taille_w);
			taille_w2+=taille_w;
		}
		lstat("./res",&fic);
		printf("Taille actuelle du fichier dest: %d\n", taille_w2);
		if (taille_re==taille_w2 && !ok_close){
			fclose(dest);
			printf("Fichier dest ferme\n");
			ok_close=1;
		}
     	}
	else { 
		printf("Reception d'un message texte\n");
	/*s*/printf(/*datas,*/"[%s] %s\n",pseudo_dist,trame_read.message);	//pour une utilisation future
	     if (!strcmp("send/",trame_read.message)){
		printf("Send FILE OK\n");
		send_file(sock);
	     }
      	}
      //enfiler_fifo(recu, datas);
    }
    
     bzero(buffer,TAILLE_MAX_MESSAGE);
    //Phase écriture
    if(!(estVide_fifo(envoi))){

      bzero(trame_write.message,TAILLE_MAX_MESSAGE);
      timeToSleep=1;
      defiler_fifo(envoi,trame_write.message);
      //saisir_texte(trame_write.message,TAILLE_MAX_MESSAGE);
     
      if(0==strcmp("QUIT",trame_write.message)){
	trame_write.type_message=quit;
/*      else
	trame_write.type_message=texte;*/
	}
	else {
	trame_write.type_message=texte; }
	trame_write.taille=strlen(trame_write.message);
	printf("Taille message: %d\n", trame_write.taille);
	printf("Type message envoyé: %d\n", trame_write.type_message);
	strcpy(buffer,tr_to_str(trame_write));
	write(sock,buffer,sizeof(buffer));
    }
    
    usleep(timeToSleep);
    
  }
} 



void connectTO(char *adresse, int port){

 int sock;
 struct sockaddr_in extremite_locale, extremite_distante;
 socklen_t length = sizeof(struct sockaddr_in);
 struct hostent *hote_distant;

 sock=socket(AF_INET, SOCK_STREAM, 0);

 if (sock==-1){
   perror ("Erreur appel sock ");
   exit(1);
 }
 
 extremite_locale.sin_family=AF_INET;
 extremite_locale.sin_addr.s_addr=inet_addr("172.28.1.16");//htonl(INADDR_ANY);
 extremite_locale.sin_port=0;
 
 if (bind(sock, (struct sockaddr *) &extremite_locale, sizeof(extremite_locale))==-1){
   perror("Erreur appel bind ");
   exit(1);
 }
 
   
   if ((hote_distant=gethostbyname(adresse))==(struct hostent *)NULL){
	fprintf(stderr, "chat:unknown host: %s\n", adresse);
	exit(EXIT_FAILURE);
   }
   
   bzero((char *)&extremite_distante, sizeof(extremite_distante));
   extremite_distante.sin_family=AF_INET;  
   (void) bcopy ((char *)hote_distant->h_addr, (char *) &extremite_distante.sin_addr,hote_distant->h_length);

   extremite_distante.sin_port=htons(port);

   if (connect(sock, (struct sockaddr *)&extremite_distante,sizeof(extremite_distante))==0){// On essaye de se connecter à un client distant
    pthread_create(&th,NULL,connexion,(void*) &sock);

    }
   else printf("Echec de connexion a %s %d\n", inet_ntoa(extremite_distante.sin_addr),ntohs(extremite_distante.sin_port));

sleep(1);   

}

void * waitConnectFROM(){

 int sock;
 struct sockaddr_in extremite_locale, extremite_distante;
 socklen_t length = sizeof(struct sockaddr_in);
// struct hostent *hote_distant;


 sock=socket(AF_INET, SOCK_STREAM, 0);
 if (sock==-1){
   perror ("Erreur appel sock ");
   exit(1);
 }

 extremite_locale.sin_family=AF_INET;
 extremite_locale.sin_addr.s_addr=inet_addr("172.28.1.16");//htonl(INADDR_ANY);
 extremite_locale.sin_port=0; 

 if (bind(sock, (struct sockaddr *) &extremite_locale, sizeof(extremite_locale))==-1){
   perror("Erreur appel bind ");
   exit(1);
 }

 if (getsockname(sock,(struct sockaddr *) &extremite_locale, &length)<0){
   perror("Erreur appel getsockname ");
   exit(1);
 }

 printf("\nOuverture d'une socket (n°%i) sur le port %i on mode connecté\n", sock, ntohs(extremite_locale.sin_port));
 printf("extremite locale :\n sin_family = %d\n sin_addr.s_addr = %s\n sin_port = %d\n\n", extremite_locale.sin_family, inet_ntoa(extremite_locale.sin_addr), ntohs(extremite_locale.sin_port));

  printf("En attente de connexion.........\n"); 

 if (listen(sock, 2)<0){
     perror("Erreur appel listen ");
     exit(1);
 }

 while(1){
   int ear=accept(sock, (struct sockaddr *) &extremite_distante, &length);
   if (ear == -1){
     perror("Erreur appel accept ");
     exit(1);
   }
   printf("\nConnection établie\n\n");
   
   printf("Connection sur la socket ayant le fd %d\nextremite distante\n sin_family : %d\n sin_addr.s_addr = %s\n sin_port = %d\n\n", ear, extremite_distante.sin_family, inet_ntoa(extremite_distante.sin_addr), ntohs(extremite_distante.sin_port));
   
   pthread_create(&th,NULL,connexion,(void*) &ear);
   

   }

}



int main(int argc, char ** argv){

 recu=creer_fifo();
 envoi=creer_fifo();
 char datas[TAILLE_MAX_MESSAGE];
 int agc;
 
  
 printf("Choisir un pseudo: ");
 saisir_texte(pseudo,TAILLE_PSEUDO);


   for(agc=2;agc<argc;agc+=2)
      connectTO(argv[(agc-1)], atoi(argv[agc]));


 pthread_create(&th,NULL,waitConnectFROM,NULL);
 
 while(1){
   
   saisir_texte(datas,sizeof(datas));
   enfiler_fifo(envoi, datas);
}
 
 
 
 return EXIT_SUCCESS;
}





int saisir_texte(char *chaine, int longueur){

  char *entre=NULL;

  if (fgets(chaine,longueur,stdin)!=NULL){
 
    entre=strchr(chaine,'\n');
    if (entre!=NULL){
	*entre='\0';
    }
    return 1;
  }
  else return 0;
}


void send_file(int sock){
	
	FILE * file;
	file=fopen("/promo2018/dgeveaux/Documents/ariane5.mp4", "r");
	char buffer[2*TAILLE_MAX_MESSAGE];
	trame trame_write;
	struct stat fichier;
	int nchar=0;
	int nwrite=0;
	int size_tot=0;
	int data_send=0;

	if (file==NULL){
		printf("Erreur fopen\n");
		exit(EXIT_FAILURE);
	}

	lstat("/promo2018/dgeveaux/Documents/ariane5.mp4",&fichier); 

	printf("We are in send_file ;) \n");

	trame_write.type_message=fileTransfert;
	trame_write.taille=fichier.st_size;
	strcpy(buffer,tr_to_str(trame_write));
	write(sock,buffer,sizeof(buffer));

	printf("Je lui envoi la taille du fichier: %d\n", trame_write.taille);

	bzero(trame_write.message,TAILLE_MAX_MESSAGE);
	bzero(buffer,2*TAILLE_MAX_MESSAGE);

	while ((nchar=fread(trame_write.message,sizeof(char),TAILLE_MAX_MESSAGE,file))){
	//	strncpy(trame_write.message,buffer,nchar);
		printf("J'ai lu: %d caractère \n", nchar);	
		trame_write.type_message=fileTransfert;
		trame_write.taille=nchar;
		strcpy(buffer,tr_to_str(trame_write));
		size_tot=trame_write.taille+sizeof(trame_write.taille)+sizeof(trame_write.type_message);
		printf("Type envoyé: %d\n", trame_write.type_message);
		printf("Je dois envoyer %li \n", (trame_write.taille+sizeof(trame_write.taille)+sizeof(trame_write.type_message)));
		nwrite=write(sock,buffer,size_tot);
		if (nwrite==-1){
			perror("Erreur write: ");
		}
		if (nwrite!=size_tot){
			fseek(file,-nchar,SEEK_CUR);
			printf("Erreur, on renvoit le paquet \n");
		}
		else data_send+=nchar;
		printf("J'ai écrit %d\n", nwrite);
		printf("Taille totale envoyee: %d\n", data_send);
		printf("Message en cours ....\n");
		sleep(0.1);
		bzero(buffer,TAILLE_MAX_MESSAGE);
		bzero(trame_write.message,TAILLE_MAX_MESSAGE);	
	}
	printf("Fichier envoyé\n");	
	fclose(file);

}
