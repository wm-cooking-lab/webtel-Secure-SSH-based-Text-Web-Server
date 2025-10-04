#include <time.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "journal.h"

const int OPEN_FANIONS = O_WRONLY | O_CREAT | O_APPEND;
const short OPEN_MODE = 0644;

static void horodater(char* hd)
{
	time_t ts;
       	time(&ts);
	struct tm *horodatage = localtime(&ts);
	sprintf
	(
		hd,
		"\"%d-%02d-%02d\",\"%02d:%02d:%02d\",\"PARIS\"",
		horodatage->tm_year+1900,
		horodatage->tm_mon+1,
		horodatage->tm_mday,
		horodatage->tm_hour,
		horodatage->tm_min,
		horodatage->tm_sec
	);
	return;
}

void journaliser_connexion(int id_client, const char* utilisateur, const char* bannière, const char* ip)
{
	pthread_mutex_lock(&lock_connexions);
	int df = open("connexions.csv", OPEN_FANIONS, OPEN_MODE);
	if(df < 0)
	{
		fprintf(stderr, "Erreur : open(2)\n");
		return;
	}
	char hd[64];
	horodater(hd);
	dprintf
	(
		df,
		"\"%d\",%s,\"%s\",\"%s\",\"%s\"\n",
		id_client,
		hd,
		ip, utilisateur, bannière
	);
	close(df);
	pthread_mutex_unlock(&lock_connexions);
	return;
}

void journaliser(enum type_journal tj, int id_client, const char* action)
{
	char journal[64];
	strcpy(journal,(tj == JOURNAL_COMMANDE) ? "commandes.csv" : "pages.csv");
	pthread_mutex_lock((tj == JOURNAL_COMMANDE) ? &lock_commandes : &lock_pages);
	int df = open(journal, OPEN_FANIONS, OPEN_MODE);
	if(df < 0)
	{
		fprintf(stderr, "Erreur : open(2)\n");
		return;
	}
	char hd[64];
	horodater(hd);
	dprintf
	(
		df,
		"\"%d\",%s,\"%s\"\n",
		id_client, hd, action
	);
	close(df);
	pthread_mutex_unlock((tj == JOURNAL_COMMANDE) ? &lock_commandes : &lock_pages);
	return;
}
