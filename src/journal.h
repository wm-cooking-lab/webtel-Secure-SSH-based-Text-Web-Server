#ifndef JOURNAL_INCLUDE_H
#define JOURNAL_INCLUDE_H

enum type_journal
{
	JOURNAL_COMMANDE,
	JOURNAL_PAGE
};

extern pthread_mutex_t lock_connexions, lock_commandes, lock_pages;

void journaliser_connexion(int id_client, const char* utilisateur, const char* bannière, const char* ip);
void journaliser(enum type_journal tj, int id_client, const char* commande);

#endif
