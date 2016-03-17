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

#ifndef EWOULDBLOCK
	#define EWOULDBLOCK EAGAIN
#endif



static char pseudo[TAILLE_PSEUDO];
 pthread_t th;

fifo *recu,*envoi;

int saisir_texte(char *chaine, int longueur);


void * connexion(void* socK){
  int sock=*((int*)socK);
  trame trame_read;
  trame trame_write;
  trame trame1;
  int ENVOYE=0;
  char pseudo_dist[TAILLE_PSEUDO];
  char datas[TAILLE_MAX_MESSAGE+32];
  ssize_t result_read;
  useconds_t timeToSleep=100;

  int file=open("/promo2018/echarrie/Images/396.bmp", O_RDONLY);
  int file2=open("./logoinsa.bmp", O_WRONLY | O_CREAT | O_TRUNC);
  int nchar;
  char buffer[TAILLE_MAX_MESSAGE];

//  char pseudo[TAILLE_PSEUDO];

/*  printf("Choisir un pseudo: ");
  saisir_texte(pseudo,TAILLE_PSEUDO);
*/
  //Dis "bonjour !" en envoyant son pseudo
  bzero(trame1.message,TAILLE_MAX_MESSAGE);
  strcpy(trame1.message,pseudo);
  trame1.type_message=hello;
  trame1.taille=sizeof(trame1);
  fcntl(sock,F_SETFL,fcntl(sock,F_GETFL)|O_NONBLOCK);
  write(sock,(void *)&trame1,trame1.taille);



  while(1){
    bzero(trame_read.message,TAILLE_MAX_MESSAGE);
    timeToSleep=100;
    
    //Phase lecture
    errno=0;
    if(0==read(sock,(void *)&trame_read,sizeof(trame_read))){
      printf("Connexion interrompue\n");
      close(sock);
      return;
    }
    result_read=errno;
    

    if ((result_read != EWOULDBLOCK)&&(result_read != EAGAIN)){
      bzero(datas,sizeof(datas));
      timeToSleep=1;
      if (trame_read.type_message==hello){
	strcpy(pseudo_dist,trame_read.message);
	/*s*/printf(/*datas,*/"%s vient de se connecter \n",pseudo_dist);
	
      }else if(trame_read.type_message==quit){
	printf("Fermeture de connexion (en toute tranquillité)\n");
	write(sock,(void *)&trame_read,trame_read.taille);
	close(sock);
	return;
      }else if(trame_read.type_message==fileTransfert){

	write(file2, trame_read.message, sizeof(trame_read.message));

      }else
	/*s*/printf(/*datas,*/"[%s] %s\n",pseudo_dist,trame_read.message);	//pour une utilisation future
      
      //enfiler_fifo(recu, datas);
    }
    
    //Phase écriture
    if(!(estVide_fifo(envoi))){

      bzero(trame_write.message,TAILLE_MAX_MESSAGE);
      timeToSleep=1;
      defiler_fifo(envoi,trame_write.message);
      //saisir_texte(trame_write.message,TAILLE_MAX_MESSAGE);
      
      

      if(0==strcmp("QUIT",trame_write.message))
	trame_write.type_message=quit;
      else if (ENVOYE==0){
        while((nchar=read(file, buffer, sizeof(buffer)))){
  	  printf("Je suis en train d'envoyer le fichier ...\n");
	  strcpy(trame_write.message, buffer);
	  trame_write.type_message=fileTransfert;
	  trame_write.taille=sizeof(trame_write);
	  write(sock, (void*)&trame_write, trame_write.taille);
        }
      ENVOYE=1;
      printf("Fichier envoyé !\n");}
      else trame_write.type_message=texte;
       
      trame_write.taille=sizeof(trame_write);
      write(sock,(void *)&trame_write,trame_write.taille);
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
 extremite_locale.sin_addr.s_addr=htonl(INADDR_ANY);
 extremite_locale.sin_port=0;
 
 if (bind(sock, (struct sockaddr *) &extremite_locale, sizeof(extremite_locale))==-1){
   perror("Erreur appel bind ");
   exit(1);
 }
 
   
   if ((hote_distant=gethostbyname(adresse))==(struct hostent *)NULL){
	fprintf(stderr, "chat:unknown host: %s\n", *adresse);
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
 struct hostent *hote_distant;


 sock=socket(AF_INET, SOCK_STREAM, 0);
 if (sock==-1){
   perror ("Erreur appel sock ");
   exit(1);
 }

 extremite_locale.sin_family=AF_INET;
 extremite_locale.sin_addr.s_addr=htonl(INADDR_ANY);
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




