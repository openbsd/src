#include <socket.h>
#include <netdb.h>
#include <errno.h>

#include "options.h"

static char *cvs_server;
static char *command;

extern int trace;

void
vms_start_server (int *tofd, int *fromfd,
                  char *client_user, char *server_user,
                  char *server_host, char *server_cvsroot)
{
  int fd, port;
  char *portenv;
  struct servent *sptr;

  if (! (cvs_server = getenv ("CVS_SERVER")))
      cvs_server = "cvs";
  command = xmalloc (strlen (cvs_server)
		     + strlen (server_cvsroot)
		     + 50);
  sprintf(command, "%s server", cvs_server);

  portenv = getenv("CVS_RCMD_PORT");
  if (portenv)
      port = atoi(portenv);
  else if ((sptr = getservbyname("shell", "tcp")) != NULL)
      port = sptr->s_port;
  else
      port = 514; /* shell/tcp */

  if(trace)
    {
    fprintf(stderr, "vms_start_server(): connecting to %s:%d\n",
            server_host, port);
    fprintf(stderr, "local_user = %s, remote_user = %s, CVSROOT = %s\n",
            client_user, (server_user ? server_user : client_user),
            server_cvsroot);
    }

  fd = rcmd(&server_host, port,
            client_user,
            (server_user ? server_user : client_user),
            command, 0);

  if (fd < 0)
     error (1, errno, "cannot start server via rcmd()");

  *tofd = fd;
  *fromfd = fd;
  free (command);
}
