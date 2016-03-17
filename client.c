#include<stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<string.h>
#include<strings.h>
#include<netdb.h>
#include"protocole.h"


int saisir_texte(char *chaine, int longueur);
void init_trame(trame *trame_init);

int connexion(int sock,char *pseudo){
  pid_t pid;
  trame trame_read;
  trame trame_write;
  trame trame1;
//  char pseudo[TAILLE_PSEUDO];

/*  printf("Choisir un pseudo: ");
  saisir_texte(pseudo,TAILLE_PSEUDO);
*/
  init_trame(&trame1);
  sprintf(trame1.pseudo_envoyeur,"%s",pseudo);
  trame1.type_message.hello=1;
  bzero(trame1.message,TAILLE_MAX_MESSAGE);
  trame1.taille=sizeof(trame1);
  write(sock,(void *)&trame1,trame1.taille);

  pid=fork();
   switch (pid){
	case -1:
		perror("Erreur fork");
		exit(EXIT_FAILURE);
		break;
	case 0:
		while(1){
		  bzero(trame_read.message,TAILLE_MAX_MESSAGE);
		  init_trame(&trame_read);
		  if (read(sock,(void *)&trame_read,sizeof(trame_read))>0){
		    if (trame_read.type_message.hello==1){
			printf("%s vient de se connecter \n",trame_read.pseudo_envoyeur);
		    }
		    printf("[%s] %s\n",trame_read.pseudo_envoyeur,trame_read.message);
		  } else return 2;
		}
		break;
	default:
		while (1){
		  bzero(trame_write.message,TAILLE_MAX_MESSAGE);
		  bzero(trame_write.pseudo_envoyeur,TAILLE_MAX_MESSAGE);
		  init_trame(&trame_write);
		  saisir_texte(trame_write.message,TAILLE_MAX_MESSAGE);
		  trame_write.type_message.texte=1;
		  sprintf(trame_write.pseudo_envoyeur,"%s",pseudo);
		  trame_write.taille=sizeof(trame_write);
		  write(sock,(void *)&trame_write,trame_write.taille);
		}
   } 

}


int main(int argc, char ** argv){

 int sock;
 struct sockaddr_in extremite_locale, extremite_distante;
 socklen_t length = sizeof(struct sockaddr_in);
 struct hostent *hote_distant;
 char pseudo[20];
 
 printf("Choisir un pseudo: ");
 saisir_texte(pseudo,TAILLE_PSEUDO);

 sock=socket(AF_INET, SOCK_STREAM, 0);
 if (sock==-1){
   perror ("Erreur appel sock ");
   return EXIT_FAILURE;
 }

 extremite_locale.sin_family=AF_INET;
 extremite_locale.sin_addr.s_addr=inet_addr("127.0.0.1");
 extremite_locale.sin_port=0; 

 if (bind(sock, (struct sockaddr *) &extremite_locale, sizeof(extremite_locale))==-1){
   perror("Erreur appel bind ");
   return EXIT_FAILURE;
 }

 if (getsockname(sock,(struct sockaddr *) &extremite_locale, &length)<0){
   perror("Erreur appel getsockname ");
   return EXIT_FAILURE;
 }

 printf("\nOuverture d'un socket (n°%i) sur le port %i on mode connecté\n", sock, ntohs(extremite_locale.sin_port));
 printf("extremite locale :\n sin_family = %d\n sin_addr.s_addr = %s\n sin_port = %d\n\n", extremite_locale.sin_family, inet_ntoa(extremite_locale.sin_addr), ntohs(extremite_locale.sin_port));

 if (argc==3){
   
   if ((hote_distant=gethostbyname(argv[1]))==(struct hostent *)NULL){
	fprintf(stderr, "%s:unknown host: %s\n", argv[0], argv[1]);
	exit(EXIT_FAILURE);
   }

   bzero((char *)&extremite_distante, sizeof(extremite_distante));
   extremite_distante.sin_family=AF_INET;  
   (void) bcopy ((char *)hote_distant->h_addr, (char *) &extremite_distante.sin_addr,hote_distant->h_length);

   extremite_distante.sin_port=htons(atoi(argv[2]));

   if (connect(sock, (struct sockaddr *)&extremite_distante,sizeof(extremite_distante))==0){// On essaye de se connecter à un client distant
     if (connexion(sock,pseudo)==2){
     }
    }
   else printf("Echec de connexion a %s %d\n", inet_ntoa(extremite_distante.sin_addr),ntohs(extremite_distante.sin_port));
 }

 printf("En attente de connexion.........\n"); 

 if (listen(sock, 2)<0){
     perror("Erreur appel listen ");
     return EXIT_FAILURE;
 }

 while(1){
   int ear=accept(sock, (struct sockaddr *) &extremite_distante, &length);
   if (ear == -1){
     perror("Erreur appel accept ");
     return EXIT_FAILURE;
   }
   printf("\nConnection établie\n\n");
   
   printf("Connection sur la socket ayant le fd %d\nextremite distante\n sin_family : %d\n sin_addr.s_addr = %s\n sin_port = %d\n\n", ear, extremite_distante.sin_family, inet_ntoa(extremite_distante.sin_addr), ntohs(extremite_distante.sin_port));
   
   if (connexion(ear,pseudo)==2){
   }

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

void init_trame(trame *trame_init){

  trame_init->taille=0;
  trame_init->type_message.hello=0;
  trame_init->type_message.quit=0;
  trame_init->type_message.texte=0;
  trame_init->type_message.audio.proposition=0;
  trame_init->type_message.audio.acceptation=0;
  trame_init->type_message.audio.transfert=0;
  trame_init->type_message.video.proposition=0;
  trame_init->type_message.video.acceptation=0;
  trame_init->type_message.video.transfert=0;
  trame_init->type_message.image.proposition=0;
  trame_init->type_message.image.acceptation=0;
  trame_init->type_message.image.transfert=0;

}
