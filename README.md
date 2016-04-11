# client.c
Code source du client. Nécessite les fichiers de Protocole et de File.


* **11/04/2016** : ajout de beaucoup de choses dans clientNcurses.c :
  1. Ajout d'une liste dynamique (_envois_) pour gérer toutes les connexions
  2. sachant qu'il y a 1 file/connexion, la tête de file contient le pseudo. La file _recu_ (qui permet en fait d'écrire sur l'écran) contient le pseudo de l'utilisateur.
  3. Ajout de code pour afficher les infos correctement dans l'interface ncurses (avec `enfiler_fifo(recu,"texte")` et `pthread_cond_signal(recu_depile);`).
  4. Création de la fonction `envoi_a_tous(char* message)` qui envoie un message à toutes les personnes connues.
  5. Ajout de commandes "/_commande_ [_paramètres_]":
    * /**quit** : on quitte le programme
    * /**me** : revoit le pseudo
    * /**connect** _adresse_ip_ _port_ : permet de se connecter à la personne désignée.
    * /**mp** _pseudo message_ : permet d'écrire un message à une persone en particulier.

* **03/04/2016** : Ajout de clientNcurses.c, comportant une interface ncurses. À compiler avec la bibliothèque -lncurses
* **21/03/2016** : Fusion de la branche `connexions multiples`, le client gère maintenant les connexions multiples.
* **17/03/2016** : Création de 2 branches : une ajoutant la geston des fichiers, l'autre les connexions multiples.
Il sera nécessaire de les implémenter toutes les deux dans le projet principal.
