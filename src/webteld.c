#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/socket.h>
#include <libssh/libssh.h>
#include <libssh/server.h>

#include "datathread.h"
#include "navwebtel.h"
#include "journal.h"

pthread_mutex_t lock_connexions, lock_commandes, lock_pages;

// Gestion du signal d'interruption
static volatile sig_atomic_t ctrl_C = 0;
static void quitter(int signal) { ctrl_C = 1; }

static void formater_adresse_ip(char* stock, ssh_session session)
{
	struct sockaddr_storage tmp;
	struct sockaddr_in *socket;
	unsigned int taille = 46; // longueur max ipv6
	getpeername(ssh_get_fd(session), (struct sockaddr*)&tmp, &taille);
	socket = (struct sockaddr_in *)&tmp;
	inet_ntop(AF_INET, &socket->sin_addr, stock, taille);
}

static void* session_client(void* arg)
{
	DataSpec* spec = (DataSpec*) arg;

	printf("%d> Connexion du client N°%d\n", spec->id_client, spec->id_client);

	if (ssh_handle_key_exchange(spec->session) != SSH_OK)
	{
		fprintf(stderr, "%d> Imposible d'échanger les clefs : %s\n", spec->id_client, ssh_get_error(spec->session));
		return NULL;
	}
	printf("%d> Échange de clefs terminé.\n", spec->id_client);

	// Variables à définir dans le switch
	char utilisateur[64];
	ssh_channel canal;

	char attente_message = 1;
	while(attente_message)
	{
		// Attente bloquante d'un nouveau message
		ssh_message message = ssh_message_get(spec->session);
		switch(ssh_message_type(message))
		{
			// Les cas sont dans l'ordre d'envoi par le client de test
			case SSH_REQUEST_SERVICE:
				printf("%d> Le client demande la liste des services.\n", spec->id_client);
				ssh_message_reply_default(message); // rejet
				printf("%d>     On rejette cette demande.\n", spec->id_client);
				break;

			case  SSH_REQUEST_AUTH:
				sprintf(utilisateur, ssh_message_auth_user(message));
				printf("%d> L'utilisateur %s demande à s'authentifier.\n", spec->id_client, utilisateur);
				ssh_message_auth_reply_success(message,0);
				printf("%d>     On accepte sans condition.\n", spec->id_client);
				break;

			// Les "channel" sont une sorte de sous-socket de communication
			// Ils permennt de multiplexer la session
			case SSH_REQUEST_CHANNEL_OPEN:
				printf("%d> Le client demande un canal de communication chiffré...\n", spec->id_client);
				if (ssh_message_subtype(message) == SSH_CHANNEL_SESSION)
				{
					printf("%d>     ... pour dialoguer.\n", spec->id_client);
					canal = ssh_message_channel_request_open_reply_accept(message);
					printf("%d>     On accepte et on ouvre le canal.\n", spec->id_client);
				}
				else
				{
					printf("%d>     ... pour autre chose (redirection de port, redirection X11, etc.\n", spec->id_client);
					ssh_message_reply_default(message);
					printf("%d>     On refuse.\n", spec->id_client); //Si ça ne marche pas, ne pas répondre
				}
				break;

			case SSH_REQUEST_CHANNEL:
				switch(ssh_message_subtype(message))
				{
					case SSH_CHANNEL_REQUEST_PTY:
						printf("%d> Le client demande un pseudo-terminal.\n", spec->id_client);
						ssh_message_channel_request_reply_success(message);
						printf("%d>     On lui dit de ne pas s'inquiéter.\n", spec->id_client);
						break;

					case SSH_CHANNEL_REQUEST_ENV:
						printf("%d> Le client demande les variables d'environnement.\n", spec->id_client);
						ssh_message_reply_default(message);
						printf("%d>     On refuse.\n", spec->id_client);
						break;

					case SSH_CHANNEL_REQUEST_SHELL:
						printf("%d> Le client demande un invite de commandes.\n", spec->id_client);
						ssh_message_channel_request_reply_success(message);
						printf("%d>     Pas de souci. Mais on va plutôt le connecter à Webtel.\n", spec->id_client);
						attente_message = 0; // On arrête d'écouter les messages
						break;

					default:
						printf("%d> Le client demande à transférer des fichiers, exécuter immédiatement une commande, ouvrir une session graphique...\n", spec->id_client);
						ssh_message_reply_default(message);
						printf("%d>     On refuse.\n", spec->id_client);
				}
				break;

			case SSH_REQUEST_GLOBAL:
				printf("%d> Requête non supportée\n", spec->id_client);
				ssh_message_reply_default(message);
				break;

			// Tous les cas sont traités
		} // switch sur le type de message
		ssh_message_free(message);
	} // boucle de traitement des messages

	// Le canal doit avoir été initialisé dans le switch
	if(canal == NULL)
	{
		fprintf(stderr, "%d> Erreur à l'ouverture du canal : %s\n", spec->id_client, ssh_get_error(spec->session));
		return NULL;
	}

	char ip[46];
	formater_adresse_ip(ip, spec->session);
	journaliser_connexion(spec->id_client, utilisateur, ssh_get_clientbanner(spec->session), ip);
	// la bannière est une sorte de user-agent

	printf("%d> Connection ajoutée au journal.\n", spec->id_client);

	// Démarrage de l'application
	navwebtel(canal, spec->id_client);

	// Fin de la session client
	printf("%d> Déconnexion du client N°%d\n", spec->id_client, spec->id_client);
	ssh_channel_send_eof(canal);
	ssh_channel_close(canal);
	ssh_disconnect(spec->session);
	ssh_free(spec->session);
	return NULL;
}

int main(int argc, char **argv)
{
	signal(SIGINT, quitter); // Gestion du signal CTRL-C

	if(argc != 3)
	{
		printf("Usage : ./sshwebtel PORT FICHIER_CLEF\n");
		return 1;
	}

	// sshbind est l'équivalent du socket d'écoute
	ssh_bind sshbind = ssh_bind_new();
	if (sshbind == NULL)
	{
		fprintf(stderr, "Erreur ssh_bind_new\n");
		return 1;
	}

	// Paramétrage de la connexion : port, clef et bindaddr
	ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_BINDPORT_STR, argv[1]);
	ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_HOSTKEY, argv[2]);
	ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_BINDADDR, INADDR_ANY);

	if(ssh_bind_listen(sshbind) < 0) // listen
	{
		fprintf(stderr, "%s\n", ssh_get_error(sshbind));
		ssh_bind_free(sshbind);
		return 1;
	}

	// Init. de la liste chaînée de treads et des mutexes
	initDataThread();
	pthread_mutex_init(&lock_connexions, NULL);
	pthread_mutex_init(&lock_commandes, NULL);
	pthread_mutex_init(&lock_pages, NULL);

	for (int i=0 ;; i++) // Boucle principale infinie
	{
		// session est l'équivalent du socket de communication
		ssh_session session = ssh_new();
		if (session == NULL)
		{
			fprintf(stderr, "Erreur : nouvelle session\n");
			continue;
		}

		if(ssh_bind_accept(sshbind, session) == SSH_ERROR) // accept
		{
			fprintf(stderr, "Erreur : accept\n");
			ssh_free(session);
			continue;
		}

		if(ctrl_C)
		{
			printf("\nArrêt du serveur demandé.\n");
			ssh_free(session);
			break;
		}

		DataThread* thread = ajouterDataThread();
		thread->spec.session = session;
		thread->spec.id_client = i;

		// Démarrage d'un nouveau thread
		if (pthread_create(&thread->spec.id, NULL, session_client, (void *)&thread->spec) < 0)
		{
			fprintf(stderr, "Erreur : création d'un thread\n");
		}

	} // Boucle principale arrêtée par CTRL-C
	// Un client doit se connecter pour débloquer l'appel à accept
	printf("Attente de la déconnexion de tous les clients.\n");
	joinDataThread();
	printf("Tous les clients sont déconnectés. Bye bye !\n");
	libererDataThread();
	pthread_mutex_destroy(&lock_connexions);
	pthread_mutex_destroy(&lock_commandes);
	pthread_mutex_destroy(&lock_pages);
	ssh_bind_free(sshbind);
	return 0;
}
