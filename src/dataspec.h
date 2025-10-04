#ifndef DATASPEC_INCLUDE_H
#define DATASPEC_INCLUDE_H

#include <libssh/libssh.h>
/* module datathread : donnees specifiques */

/* donnees specifiques */
typedef struct DataSpec_t {
  pthread_t id;               /* identifiant du thread */
  int libre;
  ssh_session session;
  int id_client;
} DataSpec;

#endif
