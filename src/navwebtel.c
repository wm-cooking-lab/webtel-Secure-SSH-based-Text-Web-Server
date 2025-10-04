#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <libssh/libssh.h>

#include "navwebtel.h"
#include "journal.h"

const char* échappement_titres[] =
{
	"",
	"    \033[1;4;36m",
	"        \033[1;4m",
	"            \033[4m",
	"                \033[3m"
};

static void nwt_formater_titre (char* titre, int niveau)
{
	char titre_brut[128]  = "";
	strcpy(titre_brut,titre);
	if(niveau > 4) niveau = 4; // Le niveau maximal est 4
	strcpy(titre, échappement_titres[niveau]);
	strcat(titre,titre_brut);
	strcat(titre,"\033[0m");
}

static void nwt_extraire_commande (struct nwt_commande* commande, char ligne[])
{
	char * token = strtok(ligne," \n");
	strcpy(commande->mot_clef,token);
	token = strtok(NULL," \n");
	if (token == NULL) // Commande vide => ligne vaut " fichier.nwt"
	{
		strcpy(commande->mot_clef,"");
		commande->type_action = nwt_DOCUMENT;
		strcpy(commande->cible,&ligne[1]);
	}
	else // Commande non vide => ligne vaut par exemple "f fichier.nwt"
	{
		commande->type_action = nwt_DOCUMENT;
		strcpy(commande->cible,token);
	}
}

static void nwt_extraire_page(struct nwt_page* page, char chemin_fichier[])
{
	FILE *fichier;
	const char tirets[] = "---------------------------------";
	const char racine[] = "nwt/";
	sprintf(page->contenu,"\n\r%s%s%s\n\n\r", tirets, chemin_fichier, tirets);
	char nwt_chemin_fichier[64] = "";
	sprintf(nwt_chemin_fichier, "%s%s", racine, chemin_fichier);
	fichier = fopen(nwt_chemin_fichier,"r");
	if(fichier == NULL)
	{
		fprintf(stderr, "Erreur : fopen(3)\n");
		return;
	}
	char ligne[128];
	int commande_compteur = 0;
	while (fgets(ligne,128,fichier) != NULL)
	{
		switch (ligne[0])
		{
			case '#': // Titre Markdown.
			{
				int niveau = 0;
				char dièze = ligne[0];
				while (dièze == '#')
				{
					niveau++;
					dièze=ligne[niveau];
				}
				char titre[128] = "";
				strcpy(titre,&ligne[niveau]);
				nwt_formater_titre(titre,niveau);
				strcat(page->contenu,titre);
				break;
			}
			case '/': // Ligne de commentaire.
			{
				break;
			}
			case ':': // Commande nwt.
			{
				char ligne_coupée[128] = "";
				strcpy(ligne_coupée, &ligne[1]);
				nwt_extraire_commande(&(page->commandes[commande_compteur]),ligne_coupée);
				commande_compteur++;
				break;
			}
			default : // Texte de la page
				strcat(page->contenu, ligne);
				strcat(page->contenu, "\r");
		}
	}
	fclose(fichier);
	return;
}

static void lire_ligne(ssh_channel canal, char* ligne)
{
	char c = 0;
	int i;
	for (i = 0; c != '\r'; i++)
	{
		if(ssh_channel_read(canal, &c, 1, 0) != 1) // lecture d'un caractère
		{
			fprintf(stderr, "Erreur lors de la lecture\n");
			return;
		}
		if (ssh_channel_write(canal, &c, 1) < 0) // écho
		{
			fprintf(stderr, "Erreur lors de l'écho\n");
			return;
		}
		ligne[i] = c;
	}
	ligne[i] = '\n'; // Seuelemnt pour l'écho
	if (ssh_channel_write(canal, ligne+i, 1) <0) // écho
	{
		fprintf(stderr, "Erreur lors de l'écho\n");
		return;
	}
	ligne[i-1] = 0; // On supprime \r et \n
	return;
}

int navwebtel(ssh_channel canal, int id_client)
{
	char page_courante[32] = "index.nwt";
	while (1)
	{
		struct nwt_page page;
		nwt_extraire_page(&page, page_courante);
		journaliser(JOURNAL_PAGE, id_client, page_courante);
		printf("%d> PAGE %s\n", id_client, page_courante);

		// Écriture du contenu de la page sur le canal SSH
		if (ssh_channel_write(canal, page.contenu, strlen(page.contenu)) < 0)
		{
			fprintf(stderr, "Erreur lors de l'écriture des données sur le canal\n");
			break;
		}
		int cmd_ok = 0;
		while (cmd_ok == 0)
		{
			char requête[64];
			lire_ligne(canal, requête);
			journaliser(JOURNAL_COMMANDE, id_client, requête);
			printf("%d> COMMANDE %s\n", id_client, requête);
			if (strcmp(requête,"Q")==0)
			{	
				const char *message = "Déconnexion.\r\n\n";
   				// Écriture du message de déconnexion sur le canal SSH
				if (ssh_channel_write(canal, message, strlen(message)) < 0)
				{
					fprintf(stderr, "Erreur lors de l'écriture du message de déconnexion sur le canal\n");
				}
    				return 0;
			}
			else if (strcmp(requête,"H")==0)
			{
				strcpy(page_courante,"index.nwt");
				cmd_ok = 1;
			}
			for (int i=0; page.commandes[i].type_action != nwt_RIEN; i++)
			{
				if (strcmp(page.commandes[i].mot_clef,requête)==0)
				{
					if (page.commandes[i].type_action == nwt_DOCUMENT)
					{
						strcpy(page_courante,page.commandes[i].cible);
						cmd_ok = 1;
						break;
					}
				}
			}
		}
	} 
	return 0;
}
