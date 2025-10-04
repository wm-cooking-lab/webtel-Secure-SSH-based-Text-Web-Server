#ifndef NAVWEBTEL_INCLUDE_H
#define NAVWEBTEL_INCLUDE_H

enum nwt_action
{
	nwt_RIEN,
	nwt_DOCUMENT,
};

struct nwt_commande
{
	char mot_clef[32];
	enum nwt_action type_action;
	char cible[64];
};

struct nwt_page
{
	char contenu[16384];
	struct nwt_commande commandes[16];
};

int navwebtel(ssh_channel canal, int id_client);

#endif
