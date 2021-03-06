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
#include <sys/stat.h>



#ifndef EWOULDBLOCK
	#define EWOULDBLOCK EAGAIN
#endif




static pthread_t th;
static pthread_cond_t *recu_depile;

fifo *recu, *serveur;

liste envois;

int saisir_texte(char *chaine, int longueur);
void send_file(int sock, char * chemin);
void receive_file(trame trame_read, int nchar_write, int taille_fichier, char *nom, int * RECU_A);
void recup_nom(char *chemin, char* nom);
void make_file_p(trame * trame_write, char * chemin);
void connectTO(char *adresse, int port);

void affichier_haut(char* datas){

      enfiler_fifo(recu, datas);
      pthread_cond_signal(recu_depile);

}

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



int join(char *nom_groupe,int sock){
	trame trame_read;
  trame trame_write;
  char reponse_info[TAILLE_MAX_MESSAGE+32];
  char reponse_ask[TAILLE_MAX_MESSAGE];
  char *pointeur_lecture;
  char nom[TAILLE_PSEUDO];
  char ip[16];
  char port[6];
  
  sprintf(trame_write.message, "%s", nom_groupe);
  trame_write.type_message=groupJoin;
  
  trame_write.taille=strlen(trame_write.message);
  tr_to_str(reponse_info,trame_write);
  write(sock,reponse_info,TAILLE_MAX_MESSAGE+32);
  
  usleep(10000);
  
  read(sock,reponse_info,TAILLE_MAX_MESSAGE+32);
  str_to_tr(reponse_info,&trame_read);
  
  if(strcmp("Ok",trame_read.message)!=0){
	  return -1;
  }
  
  trame_write.type_message=annuaireInfo;
  tr_to_str(reponse_info,trame_write);
  write(sock,reponse_info,TAILLE_MAX_MESSAGE+32);
  
  usleep(10000);
  read(sock,reponse_info,TAILLE_MAX_MESSAGE+32);
  str_to_tr(reponse_info,&trame_read);
  
  
  
  
  int continuer=1;
  sprintf(reponse_info,"%s",trame_read.message);
  pointeur_lecture=1+strchr(reponse_info,'\n');
  while(continuer){
	  bzero(nom,TAILLE_PSEUDO);
	  sscanf(pointeur_lecture,"%[^\n]",nom);
	  affichier_haut("Connect à");
	  affichier_haut(nom);
	  if((strcmp(recu->pseudo,nom))==0){
		  continuer=0;
	  }else{
		  sprintf(trame_write.message,"%s",nom);
		  trame_write.type_message=annuaireAsk;
		  trame_write.taille=strlen(trame_write.message);
		  
		  tr_to_str(reponse_ask,trame_write);
		  write(sock,reponse_ask,TAILLE_MAX_MESSAGE+32);
  
		  usleep(10000);
		  read(sock,reponse_ask,TAILLE_MAX_MESSAGE+32);
		  str_to_tr(reponse_ask,&trame_read);
		  
		  sscanf(trame_read.message,"%*s %s %s",ip,port);
		  connectTO(ip,atoi(port));
		  pointeur_lecture+=strlen(nom)+1;
	  }
  }
  
  
  return 0;
  
}



void connexion_serveur(fifo* envoi){

  int sock=envoi->sock;
  serveur=envoi;
  trame trame_read;
  trame trame_write;
  char datas[TAILLE_MAX_MESSAGE+32];
  ssize_t result_read;
  useconds_t timeToSleep=100;

  affichier_haut("Connecté au serveur d'annuaire.");

  while(1){
    bzero(trame_read.message,TAILLE_MAX_MESSAGE);
    bzero(datas,TAILLE_MAX_MESSAGE+32);
    timeToSleep=1000;

    //Phase lecture
    errno=0;
    if(0==read(sock,datas,TAILLE_MAX_MESSAGE+32)){
      sprintf(datas,"Connexion interrompue\n");
      affichier_haut(datas);
      break;
    }
    result_read=errno;


    if ((result_read != EWOULDBLOCK)&&(result_read != EAGAIN)){
      str_to_tr(datas,&trame_read);
      timeToSleep=1;
      if(trame_read.type_message==quit){
	write(sock,datas,TAILLE_MAX_MESSAGE+32);
	bzero(datas,TAILLE_MAX_MESSAGE+32);
	sprintf(datas,"Fermeture de connexion (en toute tranquillité)\n");
	affichier_haut(datas);
	break;
      }else
	bzero(datas,TAILLE_MAX_MESSAGE+32);
	sprintf(datas,"%s",trame_read.message);

      affichier_haut(datas);
    }

    //Phase écriture
    if(!(estVide_fifo(envoi))){

      bzero(trame_write.message,TAILLE_MAX_MESSAGE);
      timeToSleep=1;
      defiler_fifo(envoi,trame_write.message);

      if(0==strcmp("QUIT",trame_write.message))
	trame_write.type_message=quit;
            else if (0==strncasecmp("INFO",trame_write.message, 4)){
                trame_write.type_message=annuaireInfo;
                sprintf(trame_write.message, "%s", trame_write.message+strlen("INFO")+1);
            }
            else if(strncasecmp("ASK", trame_write.message, 3)==0){
                sprintf(trame_write.message, "%s", trame_write.message+strlen("ASK")+1);
                trame_write.type_message=annuaireAsk;
            }
            else if(strncasecmp("NEW", trame_write.message, 3)==0){
                sprintf(trame_write.message, "%s", trame_write.message+strlen("NEW")+1);
                trame_write.type_message=annuaireNew;
            }
            else if (strncasecmp("JOIN", trame_write.message, 4)==0){
	      join(trame_write.message+strlen("JOIN")+1,sock);
                sprintf(trame_write.message, "%s", trame_write.message+strlen("JOIN")+1);
                trame_write.type_message=annuaireInfo;
            }


      trame_write.taille=strlen(trame_write.message);
      tr_to_str(datas,trame_write);
      write(sock,datas,TAILLE_MAX_MESSAGE+32);
    }

    usleep(timeToSleep);

  }
  sleep(1);
  serveur=NULL;
  supprimer_fifo(envoi);
  close(sock);


}


void * connexion(void* envoy){	//Thread de connexion, 1 par connexion client-client
  fifo* envoi=(fifo*)envoy;
  int sock=envoi->sock;
  trame trame_read;
  trame trame_write;
  trame trame1;
  char datas[TAILLE_MAX_MESSAGE+32];
  ssize_t result_read;
  useconds_t timeToSleep=100;
  char name_file_r[50];
  char nom[50];
  int taille_file_r;
  char chemin[100];
  int ENVOYE_P=0;
  int RECU_A=0;

  //Dis "bonjour !" en envoyant son pseudo et son port
  bzero(trame1.message,TAILLE_MAX_MESSAGE);
  //strcpy(trame1.message,recu->pseudo);
  sprintf(trame1.message,"%s %s",recu->pseudo, recu->ext_dist);
  trame1.type_message=hello;
  trame1.taille=strlen(trame1.message);
  fcntl(sock,F_SETFL,fcntl(sock,F_GETFL)|O_NONBLOCK);

  tr_to_str(datas,trame1);
  write(sock,datas,TAILLE_MAX_MESSAGE+32);


  while(1){
    bzero(trame_read.message,TAILLE_MAX_MESSAGE);
    timeToSleep=1000;

    //Phase lecture
    errno=0;
    if(0==read(sock,datas,TAILLE_MAX_MESSAGE+32)){
      bzero(datas,TAILLE_MAX_MESSAGE+32);
      sprintf(datas,"Connexion interrompue\n");
      affichier_haut(datas);
      break;
    }
    result_read=errno;


    if ((result_read != EWOULDBLOCK)&&(result_read != EAGAIN)){
      str_to_tr(datas,&trame_read);
      timeToSleep=1;
      if (trame_read.type_message==hello){
	//strcpy(envoi->pseudo,trame_read.message);
	sscanf(trame_read.message, "%s", envoi->pseudo);
	if(0==strncmp("Serveurd'annuaire",envoi->pseudo,17)){
	  supprimer_par_pseudo(&envois,envoi->pseudo);
	  connexion_serveur(envoi);
	  return;
	}

	bzero(datas,TAILLE_MAX_MESSAGE+32);
	sprintf(datas,"%s vient de se connecter \n",envoi->pseudo);
	affichier_haut(datas);	

      }
	else if ((trame_read.type_message==fileTransfert) && (RECU_A==1)){
		receive_file(trame_read,trame_read.taille,taille_file_r,name_file_r,&RECU_A);		
	}
	else if (trame_read.type_message==fileProposition){
		sscanf(trame_read.message,"%i %s", &taille_file_r, name_file_r);
		sprintf(datas, "Proposition d'envoi de %s par %s\nSi vous souhaitez recevoir ce fichier, tapez 'file_p_ok'", name_file_r, envoi->pseudo);
		affichier_haut(datas);
	}
	else if ((trame_read.type_message==fileAcceptation) && (ENVOYE_P==1)){
		sprintf(datas,"%s a accepté de recevoir le fichier", envoi->pseudo);
		affichier_haut(datas);
		send_file(sock, chemin);
		ENVOYE_P=0;
	}	
	else if(trame_read.type_message==quit){
	write(sock,datas,TAILLE_MAX_MESSAGE+32);
	bzero(datas,TAILLE_MAX_MESSAGE+32);
	sprintf(datas,"%s a été déconnecté.\n", envoi->pseudo);

	affichier_haut(datas);
	break;
	}
	else {
	  bzero(datas,TAILLE_MAX_MESSAGE+32);
	  sprintf(datas,"[%s] %s",envoi->pseudo,trame_read.message);	
 	  affichier_haut(datas); 
	}


    }

    //Phase écriture
        if(!(estVide_fifo(envoi))){

      bzero(trame_write.message,TAILLE_MAX_MESSAGE);
      timeToSleep=1;
      defiler_fifo(envoi,trame_write.message);

      recup_nom(trame_write.message,nom);

      if (strcmp(trame_write.message,nom)!=0){
	strcpy(chemin,trame_write.message);
	make_file_p(&trame_write, chemin);
	ENVOYE_P=1;
      } 

      else if(0==strcmp("QUIT",trame_write.message))
	trame_write.type_message=quit;
      else if (strcmp("file_p_ok", trame_write.message)==0){
	bzero(trame_write.message, TAILLE_MAX_MESSAGE);
	trame_write.type_message=fileAcceptation;
	RECU_A=1;
	}

      else trame_write.type_message=texte;
      
      trame_write.taille=strlen(trame_write.message);
      tr_to_str(datas,trame_write);
      write(sock,datas,TAILLE_MAX_MESSAGE+32);
    }

    usleep(timeToSleep);

  }
  sleep(1);
  supprimer_par_pseudo(&envois,envoi->pseudo);
  supprimer_fifo(envoi);
  close(sock);


}


void connectTO(char *adresse, int port){	//Etablit une connexion vers un client attendant

 int sock;
 struct sockaddr_in extremite_locale, extremite_distante;
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
	char reponse[BUFSIZ];
	    bzero(reponse,BUFSIZ);
	sprintf(reponse, "chat : hôte inconnu : %s", adresse);
	affichier_haut(reponse);
	return;
   }

   bzero((char *)&extremite_distante, sizeof(extremite_distante));
   extremite_distante.sin_family=AF_INET;
   (void) bcopy ((char *)hote_distant->h_addr, (char *) &extremite_distante.sin_addr,hote_distant->h_length);

   extremite_distante.sin_port=htons(port);

   if (connect(sock, (struct sockaddr *)&extremite_distante,sizeof(extremite_distante))==0){// On essaye de se connecter à un client distant
      envoy=creer_fifo();
      envoy->sock=sock;
      ajouter_liste(&envois,envoy);

    pthread_create(&th,NULL,connexion,(void*) envoy);

    }
   else{
     char reponse[BUFSIZ];
         bzero(reponse,BUFSIZ);
     sprintf(reponse,"Echec de connexion a %s %d\n", inet_ntoa(extremite_distante.sin_addr),ntohs(extremite_distante.sin_port));
	affichier_haut(reponse);
  }

//sleep(1); à supprimer car cette implémentation n'efface plus le port en fin de fonction

}

void * waitConnectFROM(){	//Attends d'autres clients pour connexion

 int sock;
 struct sockaddr_in extremite_locale, extremite_distante;
 socklen_t length = sizeof(struct sockaddr_in);
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
 bzero(datas,200);
 bzero(recu->ext_dist,25);
 sprintf(recu->ext_dist, "%d", ntohs(extremite_locale.sin_port));
 sprintf(datas,"Ouverture d'une socket (n°%i) sur le port %i on mode connecté\n", sock, ntohs(extremite_locale.sin_port));
 affichier_haut(datas);
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
    ajouter_liste(&envois,envoy);


   pthread_create(&th,NULL,connexion,(void*) envoy);


   }

}


void * recive(void *haut){

  pthread_mutex_t *mutex=malloc(sizeof(pthread_mutex_t));
  pthread_mutex_init(mutex,NULL);
  char datas[TAILLE_MAX_MESSAGE+TAILLE_PSEUDO+32];

  while(1){

    pthread_cond_wait (recu_depile,mutex);

    while(!(estVide_fifo(recu))){
      bzero(datas,TAILLE_MAX_MESSAGE+TAILLE_PSEUDO+32);
      defiler_fifo(recu, datas);
      wprintw(haut,"%s\n",datas);
			//wprintw(haut,"Affichage\n");

    }


  wnoutrefresh(haut);
  wnoutrefresh(stdscr);
  doupdate();
  }

}


void envoi_a_tous(char* message){

  liste_elmt* current;
  int i;

   current=envois.premier;

    for(i=0;i<envois.taille;i++){
      enfiler_fifo(current->file, message);
      current=current->suiv;

    }

}


int main(int argc, char ** argv){

  recu=creer_fifo();
  serveur=NULL;
  fifo* fifo_recherche;
  recu_depile=malloc(sizeof(pthread_cond_t));
  pthread_cond_init(recu_depile, NULL);
  envois.mutex_liste=malloc(sizeof(pthread_mutex_t));
  pthread_mutex_init(envois.mutex_liste,NULL);

  char datas[TAILLE_MAX_MESSAGE];
  char datas2[TAILLE_MAX_MESSAGE+TAILLE_PSEUDO+32];
  char arg1[TAILLE_PSEUDO];
  int agc;
  int i;
  WINDOW *haut, *bas;


  printf("Choisir un pseudo: ");
  saisir_texte(recu->pseudo,TAILLE_PSEUDO);
  envois.taille=0;
  envois.premier=NULL;



  initscr();
  atexit(&endwin);

  haut= subwin(stdscr, LINES-7,COLS-2, 1, 1);
  bas= subwin(stdscr, 4,COLS-2, LINES-5, 1);
  dessine_box();

  echo();
  scrollok(haut,TRUE);
  wmove(haut, LINES-8,0);
  refresh();
  
  
  pthread_create(&th,NULL,recive,(void*) haut);
  pthread_create(&th,NULL,waitConnectFROM,NULL);
  
  sleep(1);
  for(agc=2;agc<argc;agc+=2)
      connectTO(argv[(agc-1)], atoi(argv[agc]));


  while (1){

    bzero(datas,TAILLE_MAX_MESSAGE);

    if (KEY_RESIZE!=mvwgetstr(bas, 0, 0, (char*) &datas)){

      if(datas[0]=='/'){

	if(0==strncasecmp("/quit",datas,5)){
	  envoi_a_tous("QUIT");
	  sleep(1);
	  exit(0);
	}else if(0==strncasecmp("/me",datas,3)){
	  bzero(datas2,TAILLE_MAX_MESSAGE+TAILLE_PSEUDO+32);
	  sprintf(datas2,"Votre pseudo est %s",recu->pseudo);

	}else if(0==strncasecmp("/connect",datas,8)){
	  arg1[0]='\0';
	  *datas2='\0';
	  sscanf(datas,"%*s %s %s",arg1,datas2);
	  connectTO(arg1,atoi(datas2));
	  *datas2='\0';

	}else if(0==strncasecmp("/mp",datas,3)){
	  sscanf(datas,"%*s %s",arg1);
	    if(!rechercher_par_pseudo(&envois,arg1, &fifo_recherche)){
	        bzero(datas2,TAILLE_MAX_MESSAGE+TAILLE_PSEUDO+32);
	        sprintf(datas2,"%s : pseudo non trouvé",arg1);
	    }else{
	        enfiler_fifo(fifo_recherche,datas+5+strlen(arg1));
      	        bzero(datas2,TAILLE_MAX_MESSAGE+TAILLE_PSEUDO+32);
	        sprintf(datas2,"[VOUS->%s] %s",arg1,datas+5+strlen(arg1));
			}
	  }
		else if(0==strncasecmp("/change",datas,7)){
			sscanf(datas,"%*s %s",arg1);
			bzero(recu->pseudo,TAILLE_PSEUDO);
			strcpy(recu->pseudo, arg1);
	}else if ((0==strncasecmp("/INFO",datas, 5)) || (strncasecmp("/ASK", datas, 4)==0) || (strncasecmp("/NEW", datas, 4)==0) || (strncasecmp("/JOIN", datas, 5)==0)){
                if(serveur!=NULL)
		  enfiler_fifo(serveur,datas+1);
		else{
		  bzero(datas2,TAILLE_MAX_MESSAGE+TAILLE_PSEUDO+32);
		  sprintf(datas2,"Aucun serveur connu, la requête n'a pas aboutit");
		}
	 }else if((0==strncasecmp("/help",datas,5)) || (0==strncasecmp("/h",datas,2)) || (0==strncasecmp("/?",datas,2))){
	   bzero(datas2,TAILLE_MAX_MESSAGE+TAILLE_PSEUDO+32);
	   sprintf(datas2,"[AIDE]\n /quit : quitter le programme\n /me : retourne votre pseudo\n /connect <adresse ip> <port> : vous connecte à la personne indiquée\n /mp <pseudo> <message> : envoie un message privé à la personne indiquée\n /send <pseudo> <chemin>: envoie le fichier correspondant à chemin à la personne indiquée");
	   enfiler_fifo(recu,datas2);
	   bzero(datas2,TAILLE_MAX_MESSAGE+TAILLE_PSEUDO+32);
	   sprintf(datas2," /info : Liste tout ceux qui se sont enregistrés auprès du serveur d'annuaire, ainsi que la liste des groupes.\n /info <nom de groupe> : Liste les membres du groupe.\n /ask <pseudo> : demande l'adresse ip et le port de connexion d'une personne.\n /new <nom du groupe> : Crée un groupe.\n /join <nom du groupe> : Permet de rejoindre un groupe.");
         }else if (0==strncasecmp("/send",datas,5)){
		sscanf(datas,"%*s %s %s", arg1, datas2);
		if (rechercher_par_pseudo(&envois,arg1,&fifo_recherche)){
			enfiler_fifo(fifo_recherche, datas+7+strlen(arg1));
			bzero(datas2, TAILLE_MAX_MESSAGE+TAILLE_PSEUDO+32);
			sprintf(datas2, "[VOUS->%s] Proposition d'envoi de %s", arg1, datas+7+strlen(arg1));
		}
	}else{
	  bzero(datas2,TAILLE_MAX_MESSAGE+TAILLE_PSEUDO+32);
	  sprintf(datas2,"La commande %s est inconnue.",datas);
	}
      }else{

	envoi_a_tous(datas);
	bzero(datas2,TAILLE_MAX_MESSAGE+TAILLE_PSEUDO+32);
	/*snprintf(datas, TAILLE_MAX_MESSAGE+TAILLE_PSEUDO+32, "[VOUS] %s", datas);
	datas2[TAILLE_MAX_MESSAGE+TAILLE_PSEUDO+31]='\0';
  */

	sprintf(datas2,"[VOUS] %s",datas);
      }

      affichier_haut(datas2);
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

void make_file_p(trame * trame_write, char * chemin){

  struct stat fichier;
  char nom[30];

  recup_nom(chemin,nom);

  lstat(chemin,&fichier);

  trame_write->type_message=fileProposition;
  sprintf(trame_write->message,"%i %s", (int)fichier.st_size,nom);
  trame_write->taille=strlen(trame_write->message);

}

void send_file(int sock, char* chemin){
	
	FILE * file;
	char buffer[TAILLE_MAX_MESSAGE+32];
	trame trame_write;
//	struct stat fichier;
	int nchar=0;
	int nwrite=0;
	int size_tot=0;
//	int data_send=0;
	char datas[TAILLE_MAX_MESSAGE+32];

	file=fopen(chemin, "r");

	if (file==NULL){
		printf("Erreur fopen\n");
		exit(EXIT_FAILURE);
	}

	while ((nchar=fread(trame_write.message,sizeof(char),TAILLE_MAX_MESSAGE,file))){	
		trame_write.type_message=fileTransfert;
		trame_write.taille=nchar;
		tr_to_str(buffer,trame_write);
		size_tot=trame_write.taille+sizeof(trame_write.taille)+sizeof(trame_write.type_message);
		nwrite=write(sock,buffer,size_tot);
		if (nwrite==-1){
			perror("Erreur write: ");
		}
		/*if (nwrite!=size_tot){
			fseek(file,-nchar,SEEK_CUR);
		}
		else data_send+=nchar;*/
		sleep(1);
		bzero(buffer,TAILLE_MAX_MESSAGE+32);
		bzero(trame_write.message,TAILLE_MAX_MESSAGE);	
	}
	sprintf(datas, "Fichier envoyé.");
	affichier_haut(datas);	
	fclose(file);

}

void recup_nom(char *chemin, char *nom){
	
  char *ptr=NULL;

  ptr=strchr(chemin,'\0');

  if (ptr==NULL){
	printf("Chemin invalide\n");
  }

  while ((*(ptr-1)!='/') && (ptr!=chemin)){
	ptr--;
  }

  strcpy(nom,ptr);

}

void receive_file(trame trame_read, int nchar, int taille_fichier, char * nom, int * RECU_A){

  static  FILE *dest=NULL;
  int taille_re=taille_fichier;
  int taille_w=0;
  static int taille_w2=0;
  static int ouvert=0;
  char chemin_dest[50];
  char datas[TAILLE_MAX_MESSAGE+32];

  sprintf(chemin_dest,"./%s",nom);

  if (!ouvert){
	  dest=fopen(chemin_dest,"w");
	  ouvert=1;
  }

  if (dest==NULL){
  	printf("Erreur open\n");
  	exit(EXIT_FAILURE);
  }

  sprintf(datas, "Reception du fichier %s....\n", nom);
  affichier_haut(datas);
  taille_w=fwrite(trame_read.message,sizeof(char),nchar,dest);
  taille_w2+=taille_w;

  if (taille_re==taille_w2){
  	fclose(dest);
	RECU_A=0;
  	sprintf(datas, "Fichier %s fermé\n", nom);
	affichier_haut(datas);
	taille_re=0;
	taille_w2=0;
	ouvert=0;
  }

}
