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
#include <ncurses.h>


#ifndef EWOULDBLOCK
	#define EWOULDBLOCK EAGAIN
#endif




static char pseudo[TAILLE_PSEUDO];
static pthread_t th;
static pthread_cond_t *recu_depile;

fifo *recu;

fifo* envois[50];		//à remplacer par une liste dynamique (à implémenter)
int nbEnvoi=0;

int saisir_texte(char *chaine, int longueur);



void dessine_box(void){

	box(stdscr,0,0);
	mvwhline(stdscr,LINES-6, 0, ACS_HLINE, COLS);
	mvaddch(LINES-6, 0, ACS_LTEE);
	mvaddch(LINES-6, COLS-1, ACS_RTEE);

}

void regen_win(WINDOW **haut, WINDOW **bas){

	wclear(stdscr);
	delwin(*haut);
	delwin(*bas);

	*haut= subwin(stdscr, LINES-7,COLS-2, 1, 1);
	scrollok(*haut,TRUE);
	*bas= subwin(stdscr, 4,COLS-2, LINES-5, 1);

	wmove(*haut, LINES-8,0);

}




void * connexion(void* envoy){	//Thread de connexion, 1 par connexion client-client
  fifo* envoi=(fifo*)envoy;
  int sock=envoi->sock;
  printf("sock=%i\n",sock);
  trame trame_read;
  trame trame_write;
  trame trame1;
  char pseudo_dist[TAILLE_PSEUDO];
  char datas[TAILLE_MAX_MESSAGE+32];
  ssize_t result_read;
  useconds_t timeToSleep=100;

  //Dis "bonjour !" en envoyant son pseudo
  bzero(trame1.message,TAILLE_MAX_MESSAGE);
  strcpy(trame1.message,pseudo);
  trame1.type_message=hello;
  trame1.taille=sizeof(trame1);
  fcntl(sock,F_SETFL,fcntl(sock,F_GETFL)|O_NONBLOCK);
  write(sock,(void *)&trame1,trame1.taille);




  while(1){
    bzero(trame_read.message,TAILLE_MAX_MESSAGE);
    timeToSleep=1000;

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
	sprintf(datas,"%s vient de se connecter \n",pseudo_dist);	

      }else if(trame_read.type_message==quit){
	sprintf(datas,"Fermeture de connexion (en toute tranquillité)\n");
	write(sock,(void *)&trame_read,trame_read.taille);
	close(sock);
	return;
      }else
	sprintf(datas,"[%s] %s\n",pseudo_dist,trame_read.message);	

      enfiler_fifo(recu, datas);
      pthread_cond_signal(recu_depile);
    }

    //Phase écriture
    if(!(estVide_fifo(envoi))){

      bzero(trame_write.message,TAILLE_MAX_MESSAGE);
      timeToSleep=1;
      defiler_fifo(envoi,trame_write.message);

      if(0==strcmp("QUIT",trame_write.message))
	trame_write.type_message=quit;
      else
	trame_write.type_message=texte;
      trame_write.taille=sizeof(trame_write);
      write(sock,(void *)&trame_write,trame_write.taille);
    }

    usleep(timeToSleep);

  }
}



void connectTO(char *adresse, int port){	//Etablit une connexion vers un client attendant

 int sock;
 struct sockaddr_in extremite_locale, extremite_distante;
 socklen_t length = sizeof(struct sockaddr_in);
 struct hostent *hote_distant;
 fifo *envoy;

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
      envoy=creer_fifo();
      envoy->sock=sock;
      envois[nbEnvoi]=envoy;
      nbEnvoi++;
    pthread_create(&th,NULL,connexion,(void*) envoy);

    }
   else printf("Echec de connexion a %s %d\n", inet_ntoa(extremite_distante.sin_addr),ntohs(extremite_distante.sin_port));

//sleep(1); à supprimer car cette implémentation n'efface plus le port en fin de fonction

}

void * waitConnectFROM(){	//Attends d'autres clients pour connexion

 int sock;
 struct sockaddr_in extremite_locale, extremite_distante;
 socklen_t length = sizeof(struct sockaddr_in);
 struct hostent *hote_distant;
 fifo *envoy;


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

 char datas[200];
 sprintf(datas,"Ouverture d'une socket (n°%i) sur le port %i on mode connecté\n", sock, ntohs(extremite_locale.sin_port));
 enfiler_fifo(recu,datas);
 pthread_cond_signal(recu_depile);
 //printf("extremite locale :\n sin_family = %d\n sin_addr.s_addr = %s\n sin_port = %d\n\n", extremite_locale.sin_family, inet_ntoa(extremite_locale.sin_addr), ntohs(extremite_locale.sin_port));

  //printf("En attente de connexion.........\n");

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
   //printf("\nConnection établie\n\n");

   //printf("Connection sur la socket ayant le fd %d\nextremite distante\n sin_family : %d\n sin_addr.s_addr = %s\n sin_port = %d\n\n", ear, extremite_distante.sin_family, inet_ntoa(extremite_distante.sin_addr), ntohs(extremite_distante.sin_port));

    envoy=creer_fifo();
    envoy->sock=ear;
    envois[nbEnvoi]=envoy;
    nbEnvoi++;

   pthread_create(&th,NULL,connexion,(void*) envoy);


   }

}


void * recive(void *haut){
  
  pthread_mutex_t *mutex=malloc(sizeof(pthread_mutex_t));
  pthread_mutex_init(mutex,NULL);
  char datas[TAILLE_MAX_MESSAGE+32];
    
  while(1){

    pthread_cond_wait (recu_depile,mutex);

    while(!(estVide_fifo(recu))){
    
      defiler_fifo(recu, datas);
      wprintw(haut,"%s\n",datas);
      
    }
    
  
  wnoutrefresh(haut);
  wnoutrefresh(stdscr);
  doupdate();
  }
  
}



int main(int argc, char ** argv){

  recu=creer_fifo();
  recu_depile=malloc(sizeof(pthread_cond_t));
  pthread_cond_init(recu_depile, NULL);
  char datas[TAILLE_MAX_MESSAGE];
  int agc;
  int i;
  WINDOW *haut, *bas;


  printf("Choisir un pseudo: ");
  saisir_texte(pseudo,TAILLE_PSEUDO);



  initscr();

  haut= subwin(stdscr, LINES-7,COLS-2, 1, 1);
  bas= subwin(stdscr, 4,COLS-2, LINES-5, 1);
  dessine_box();
	
  echo();
  scrollok(haut,TRUE);
  wmove(haut, LINES-8,0);
  refresh();
 
  pthread_create(&th,NULL,recive,(void*) haut);
  sleep(1);
  for(agc=2;agc<argc;agc+=2)
      connectTO(argv[(agc-1)], atoi(argv[agc]));


  pthread_create(&th,NULL,waitConnectFROM,NULL);

  while (strcmp((char*) &datas, "QUIT")){
    
    bzero(datas,TAILLE_MAX_MESSAGE);

    if (KEY_RESIZE!=mvwgetstr(bas, 0, 0, (char*) &datas)){

	for(i=0;i<nbEnvoi;i++)
	  enfiler_fifo(envois[i], datas);
	
	enfiler_fifo(recu, datas);
	pthread_cond_signal(recu_depile);
    }

    werase(bas);
    wnoutrefresh(bas);
    wnoutrefresh(stdscr);
    doupdate();

  }

			
	endwin();

  return EXIT_SUCCESS;
}





int saisir_texte(char *chaine, int longueur){	//un fgets perso

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



