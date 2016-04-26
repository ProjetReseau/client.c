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




//static char pseudo[TAILLE_PSEUDO];
static pthread_t th;
static pthread_cond_t *recu_depile;

fifo *recu, *serveur;

liste envois;

int saisir_texte(char *chaine, int longueur);
int send_file(int sock, char * chemin);
int receive_file(trame trame_read, int nchar_write, int taille_fichier, char *nom, int * RECU_A);
void recup_nom(char *chemin, char* nom);
void make_file_p(trame * trame_write, char * chemin);
void connectTO(char *adresse, int port);

void afficher_haut(char* datas){   //affiche à l'écran le contenu de datas

      enfiler_fifo(recu, datas);
      pthread_cond_signal(recu_depile);

}

void dessine_box(void){    //dessine les bords de la fenêtre.

	box(stdscr,0,0);
	mvwhline(stdscr,LINES-6, 0, ACS_HLINE, COLS);
	mvaddch(LINES-6, 0, ACS_LTEE);
	mvaddch(LINES-6, COLS-1, ACS_RTEE);

}

void regen_win(WINDOW **haut, WINDOW **bas){   //

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




void connexion_serveur(fifo* envoi){    //connexion au serveur

  int sock=envoi->sock;
  serveur=envoi;
  trame trame_read;
  trame trame_write;
  char datas[TAILLE_MAX_MESSAGE+32];
  ssize_t result_read;
  useconds_t timeToSleep=100;

  afficher_haut("Connecté au serveur d'annuaire.");

  while(1){
    bzero(trame_read.message,TAILLE_MAX_MESSAGE);
    bzero(datas,TAILLE_MAX_MESSAGE+32);
    timeToSleep=1000;

    //Phase lecture
    errno=0;
    if(0==read(sock,datas,TAILLE_MAX_MESSAGE+32)){   //si on ne lit plus rien, on affiche que la connexion est interrompue.
      sprintf(datas,"Connexion interrompue\n");
      afficher_haut(datas);
      break;
    }
    result_read=errno;


    if ((result_read != EWOULDBLOCK)&&(result_read != EAGAIN)){    //Si on a réussi à lire un message
      str_to_tr(datas,&trame_read);
      timeToSleep=1;
      if(trame_read.type_message==quit){ //Si le type du message reçu est quit , on ferme la connexion
		write(sock,datas,TAILLE_MAX_MESSAGE+32);
		bzero(datas,TAILLE_MAX_MESSAGE+32);
		sprintf(datas,"Fermeture de connexion (en toute tranquillité)\n");
		afficher_haut(datas);
		break;
      }else   //sinon, c'est un message texte, on l'affiche.
	  bzero(datas,TAILLE_MAX_MESSAGE+32);
	  sprintf(datas,"%s",trame_read.message);

      afficher_haut(datas);
    }

    //Phase écriture
    if(!(estVide_fifo(envoi))){   //Si la file d'envoi n'est pas vide, il y a des données à envoyer.

      bzero(trame_write.message,TAILLE_MAX_MESSAGE);
      timeToSleep=1;
      defiler_fifo(envoi,trame_write.message);   //On récupère le message dans la file.

	  
	  //Ici, on construit les trames en fonction du type du message.
	  
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
  int ENVOYE_P=0; // Flag qui indique si l'on a envoyé un message de type fileProposition. Si flag = 0 alors on ne l'a pas envoyé.
  int RECU_A=0; // Flag qui indique si l'on a bien reçu un message de type fileAcceptation. Si flag = 0 alors le fileAcceptation n'a pas été reçu

  //Dis "bonjour !" en envoyant son pseudo et son port
  bzero(trame1.message,TAILLE_MAX_MESSAGE);
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
      afficher_haut(datas);
      break;
    }
    result_read=errno;


    // Ici nous allons regarder le type du message reçu

    if ((result_read != EWOULDBLOCK)&&(result_read != EAGAIN)){   
      str_to_tr(datas,&trame_read); 				  // Passage de la chaine de caractère en trame
      timeToSleep=1;
      if (trame_read.type_message==hello){                        // Si le message reçu est de type hello alors on associe le pseudo avec cette connexion. 
		sscanf(trame_read.message, "%s", envoi->pseudo);
		if(0==strncmp("Serveurd'annuaire",envoi->pseudo,17)){     // Si le pseudo est "Serveurd'annuaire" alors on va se connecter au serveur d'annuaire
			supprimer_par_pseudo(&envois,envoi->pseudo);
			connexion_serveur(envoi);
			return;
		}
		bzero(datas,TAILLE_MAX_MESSAGE+32);
		sprintf(datas,"%s vient de se connecter \n",envoi->pseudo); // On affiche le nom de la personne qui vient de se connecter
		afficher_haut(datas);	

      }
	else if ((trame_read.type_message==fileTransfert) && (RECU_A==1)){   // Si on reçoit un message de type fileTransfert et que le flag RECU_A est à 1 alors on va rentrer dans la fonction receive_file qui va enregistrer les données dans un fichier
		receive_file(trame_read,trame_read.taille,taille_file_r,name_file_r,&RECU_A);		
	}
	else if (trame_read.type_message==fileProposition){		// Si on reçoit un message de type fileProposition alors on récupère la taille du fichier et le nom du fichier à recevoir.
		sscanf(trame_read.message,"%i %s", &taille_file_r, name_file_r);
		sprintf(datas, "Proposition d'envoi de %s par %s\nSi vous souhaitez recevoir ce fichier, tapez 'file_p_ok'", name_file_r, envoi->pseudo);
		afficher_haut(datas);
	}
	else if ((trame_read.type_message==fileAcceptation) && (ENVOYE_P==1)){ 		// Si on reçoit un message de type fileAcceptation et que le flag ENVOYE_P est à 1 alors on va rentrer dans la fonction send_file qui va envoyer le fichier.
		sprintf(datas,"%s a accepté de recevoir le fichier", envoi->pseudo);
		afficher_haut(datas);
		send_file(sock, chemin);
		ENVOYE_P=0;
	}	
	else if(trame_read.type_message==quit){  // Si on reçoit un message de type quit alors on affiche que la personne a été deconnectée.
	write(sock,datas,TAILLE_MAX_MESSAGE+32);
	bzero(datas,TAILLE_MAX_MESSAGE+32);
	sprintf(datas,"%s a été déconnecté.\n", envoi->pseudo);
	afficher_haut(datas);
	break;
	}
	else { 					// Si on reçoit un message de type texte alors on l'affiche précédé du pseudo de l'envoyeur
	  bzero(datas,TAILLE_MAX_MESSAGE+32);
	  sprintf(datas,"[%s] %s",envoi->pseudo,trame_read.message);	
 	  afficher_haut(datas); 
	}

    }

    //Phase écriture
    if(!(estVide_fifo(envoi))){  // Si la file d'envoie n'est pas vide alors il y a des données à envoyer.

      bzero(trame_write.message,TAILLE_MAX_MESSAGE);
      timeToSleep=1;
      defiler_fifo(envoi,trame_write.message);

      recup_nom(trame_write.message,nom); 

      if (strcmp(trame_write.message,nom)!=0){ // Ici on regarde si les données sont un chemin. Si oui alors on rentre dans la fonction make_file_p.
	strcpy(chemin,trame_write.message);
	make_file_p(&trame_write, chemin);
	ENVOYE_P=1;
      } 

      else if(0==strcmp("QUIT",trame_write.message)) 
	trame_write.type_message=quit;
      else if (strcmp("file_p_ok", trame_write.message)==0){ // Si les données à envoyer sont file_p_ok alors on construit une trame de type fileAcceptation
 	bzero(trame_write.message, TAILLE_MAX_MESSAGE);
	trame_write.type_message=fileAcceptation;
	RECU_A=1;
	}
      else trame_write.type_message=texte;
      
      trame_write.taille=strlen(trame_write.message);
      tr_to_str(datas,trame_write);			// Passage de la trame à envoyer en chaine de caractère
      write(sock,datas,TAILLE_MAX_MESSAGE+32);        //envoi
    }

    usleep(timeToSleep);

  }
  sleep(1);
  supprimer_par_pseudo(&envois,envoi->pseudo);     //On supprime la file associée à la connexion à la fin de cette connexion.
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
	afficher_haut(reponse);
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

      pthread_create(&th,NULL,connexion,(void*) envoy);   //Création d'un thread pour la connexion  

    }
   else{   //Si on a pas réussi à se connecter, on affiche un message d'erreur.
      char reponse[BUFSIZ];
      bzero(reponse,BUFSIZ);
      sprintf(reponse,"Echec de connexion a %s %d\n", inet_ntoa(extremite_distante.sin_addr),ntohs(extremite_distante.sin_port));
	  afficher_haut(reponse);
  }


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
 afficher_haut(datas);

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

    envoy=creer_fifo();
    envoy->sock=ear;
    ajouter_liste(&envois,envoy);


    pthread_create(&th,NULL,connexion,(void*) envoy);  // Si un client vient pour se connecter, on crée un thread pour la connexion.


   }

}


void * recive(void *haut){   //Attend qu'on veuille afficher quelque chose dans l'interface en l'empilant dans reçu et elle l'affiche.

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


void envoi_a_tous(char* message){    //Envoie le message à toutes les personnes connectées.

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
  WINDOW *haut, *bas;


  printf("Choisir un pseudo: ");   //Demande à l'utilisateur de choisir un pseudo et le stocke dans reçu->pseudo
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
  for(agc=2;agc<argc;agc+=2)   //Si on lance le programme avec comme argument l'adresse ip et le port auquels on veut se connecter.
      connectTO(argv[(agc-1)], atoi(argv[agc]));


  while (1){

    bzero(datas,TAILLE_MAX_MESSAGE);

    if (KEY_RESIZE!=mvwgetstr(bas, 0, 0, (char*) &datas)){   //Récupère ce qu'on tape au clavier

      if(datas[0]=='/'){    //Si le message commence par un "/", alors c'est une commande

		if(0==strncasecmp("/quit",datas,5)){   //Si c'est la commande "quit", le message est envoyé à tous et on quitte le programme.
		    envoi_a_tous("QUIT");
		    sleep(1);
		    exit(0);
		}else if(0==strncasecmp("/me",datas,3)){  //Sinon, si c'est la commande "me", on affiche notre pseudo.
		    bzero(datas2,TAILLE_MAX_MESSAGE+TAILLE_PSEUDO+32);
		    sprintf(datas2,"Votre pseudo est %s",recu->pseudo);

		}else if(0==strncasecmp("/connect",datas,8)){   //Sinon, si c'est la commande "connect", l'utilisateur tape à la suite l'adresse ip et le port auquels il veut se connecter, cette commande le connecte.
		    arg1[0]='\0';
		    *datas2='\0';
		    sscanf(datas,"%*s %s %s",arg1,datas2);
		    connectTO(arg1,atoi(datas2));
		    *datas2='\0';

		}else if(0==strncasecmp("/mp",datas,3)){    //Sinon, si c'est la commande "mp", suivie d'un pseudo et d'un message, on envoi le message uniquement à la personne désignée.
		  sscanf(datas,"%*s %s",arg1);
			if(!rechercher_par_pseudo(&envois,arg1, &fifo_recherche)){   //Si le pseudo n'est pas trouvé, on affiche un message d'erreur.
				bzero(datas2,TAILLE_MAX_MESSAGE+TAILLE_PSEUDO+32);
				sprintf(datas2,"%s : pseudo non trouvé",arg1);
			}else{
				enfiler_fifo(fifo_recherche,datas+5+strlen(arg1));
				bzero(datas2,TAILLE_MAX_MESSAGE+TAILLE_PSEUDO+32);
				sprintf(datas2,"[VOUS->%s] %s",arg1,datas+5+strlen(arg1));
				}
		  }
		else if(0==strncasecmp("/change",datas,7)){    //Sinon, si c'est la commande "change", suivie d'un nouveau pseudo, on remplace notre pseudo par le nouveau.
			sscanf(datas,"%*s %s",arg1);
			bzero(recu->pseudo,TAILLE_PSEUDO);
			strcpy(recu->pseudo, arg1);
		}	
		else if ((0==strncasecmp("/INFO",datas, 5)) || (strncasecmp("/ASK", datas, 4)==0) || (strncasecmp("/NEW", datas, 4)==0) || (strncasecmp("/JOIN", datas, 5)==0)){   //Sinon, si c'est la commande "INFO" ou "ASK" ou "NEW" ou "JOIN", on enfile dans la file du serveur.
			if(serveur!=NULL)
				enfiler_fifo(serveur,datas+1);
			else{    //Si le serveur n'a pas été trouvé, on affiche un message d'erreur.
				bzero(datas2,TAILLE_MAX_MESSAGE+TAILLE_PSEUDO+32);
				sprintf(datas2,"Aucun serveur connu, la requête n'a pas aboutit");
			}	
		}
		else if((0==strncasecmp("/help",datas,5)) || (0==strncasecmp("/h",datas,2)) || (0==strncasecmp("/?",datas,2))){     //Sinon, si c'est la commande "help" ou "h" ou "?", on affiche l'aide pour les commandes.
			bzero(datas2,TAILLE_MAX_MESSAGE+TAILLE_PSEUDO+32);
			sprintf(datas2,"[AIDE]\n /quit : quitter le programme\n /me : retourne votre pseudo\n /connect <adresse ip> <port> : vous connecte à la personne indiquée\n /mp <pseudo> <message> : envoie un message privé à la personne indiquée\n /send <pseudo> <chemin>: envoie le fichier correspondant à chemin à la personne indiquée");
			enfiler_fifo(recu,datas2);
			bzero(datas2,TAILLE_MAX_MESSAGE+TAILLE_PSEUDO+32);
			sprintf(datas2," /info : Liste tout ceux qui se sont enregistrés auprès du serveur d'annuaire, ainsi que la liste des groupes.\n /info <nom de groupe> : Liste les membres du groupe.\n /ask <pseudo> : demande l'adresse ip et le port de connexion d'une personne.\n /new <nom du groupe> : Crée un groupe.\n /join <nom du groupe> : Permet de rejoindre un groupe.");
		}
		else if (0==strncasecmp("/send",datas,5)){    //Sinon, si c'est la commande "send", suivie d'un pseudo et d'un chemin, on envoi le fichier correspondant au chemin à la personne désignée.
			sscanf(datas,"%*s %s %s", arg1, datas2);
			if (rechercher_par_pseudo(&envois,arg1,&fifo_recherche)){
				enfiler_fifo(fifo_recherche, datas+7+strlen(arg1));
				bzero(datas2, TAILLE_MAX_MESSAGE+TAILLE_PSEUDO+32);
				sprintf(datas2, "[VOUS->%s] Proposition d'envoi de %s", arg1, datas+7+strlen(arg1));
			}
		}
		else{   //Sinon, on ne connaît pas la commande.
			bzero(datas2,TAILLE_MAX_MESSAGE+TAILLE_PSEUDO+32);
			sprintf(datas2,"La commande %s est inconnue.",datas);
		}
    }
	else{   //Sinon, c'est un message, on l'envoie à tous.
	envoi_a_tous(datas);
	bzero(datas2,TAILLE_MAX_MESSAGE+TAILLE_PSEUDO+32);

	sprintf(datas2,"[VOUS] %s",datas);
    }

      afficher_haut(datas2);
  }

    werase(bas);
    wnoutrefresh(bas);
    wnoutrefresh(stdscr);
    doupdate();

  }


	endwin();

  return EXIT_SUCCESS;
}


int saisir_texte(char *chaine, int longueur){	//fonction qui permet la saisie de texte.

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

void make_file_p(trame * trame_write, char * chemin){    //construit une trame de type fileProposition

  struct stat fichier;
  char nom[30];

  recup_nom(chemin,nom);

  lstat(chemin,&fichier);    //permet de récupérer la taille du fichier.

  trame_write->type_message=fileProposition;
  sprintf(trame_write->message,"%i %s", (int)fichier.st_size,nom);
  trame_write->taille=strlen(trame_write->message);

}

int send_file(int sock, char* chemin){    //permet d'envoyer un fichier
	
	FILE * file;
	char buffer[TAILLE_MAX_MESSAGE+32];
	trame trame_write;
	int nchar=0;
	int nwrite=0;
	int size_tot=0;
	char datas[TAILLE_MAX_MESSAGE+32];

	file=fopen(chemin, "r");

	if (file==NULL){   //Si le fopen n'a pas fonctionné, on affiche un message d'erreur et on retourne un EXIT_FAILURE.
		sprintf(datas, "Erreur fopen");
		afficher_haut(datas);
		return(EXIT_FAILURE);
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
		sleep(1);
		bzero(buffer,TAILLE_MAX_MESSAGE+32);
		bzero(trame_write.message,TAILLE_MAX_MESSAGE);	
	}
	sprintf(datas, "Fichier envoyé.");   //Quand le fichier est envoyé, on confirme à l'envoyeur.
	afficher_haut(datas);	
	fclose(file);
	return (EXIT_SUCCESS);

}

void recup_nom(char *chemin, char *nom){   //fonction qui permet de récupérer le nom d'un fichier à partir du chemin.
	
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

int receive_file(trame trame_read, int nchar, int taille_fichier, char * nom, int * RECU_A){   //réception d'un fichier

  static  FILE *dest=NULL;
  int taille_re=taille_fichier;
  int taille_w=0;
  static int taille_w2=0;
  static int ouvert=0;   //Flag qui indique si le fichier est ouvert. Si il est égal à 0, alors il ne l'est pas.
  char chemin_dest[50];
  char datas[TAILLE_MAX_MESSAGE+32];

  sprintf(chemin_dest,"./%s",nom);

  if (!ouvert){     //Si le fichier n'est pas ouvert, on le crée dans le répertoire courant.
	  dest=fopen(chemin_dest,"w");
	  ouvert=1;
  }

  if (dest==NULL){     //Si le fopen n'a pas fonctionné, on affiche un message d'erreur et on retourne un EXIT_FAILURE.
  	sprintf(datas,"Erreur open\n");
	afficher_haut(datas);
  	return(EXIT_FAILURE);
  }

  sprintf(datas, "Reception du fichier %s....\n", nom);    //pendant la réception, on affiche sur l'écran du récepteur que le fichier est en train d'être reçu.
  afficher_haut(datas);
  taille_w=fwrite(trame_read.message,sizeof(char),nchar,dest);
  taille_w2+=taille_w;

  if (taille_re==taille_w2){  //Si la taille reçu est la même que la taille à recevoir, on ferme le fichier et on affiche au récepteur que le fichier est fermé.
  	fclose(dest);
	RECU_A=0;
  	sprintf(datas, "Fichier %s fermé\n", nom);
	afficher_haut(datas);
	taille_re=0;
	taille_w2=0;
	ouvert=0;
  }
  
  return(EXIT_SUCCESS);

}


