#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unixio.h>

#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

int rcmd(char **remote_hostname, int remote_port,
         char *local_user, char *remote_user,
         char *command, int zero)
{
  struct hostent *remote_hp;
  struct hostent *local_hp;
  struct sockaddr_in remote_isa;
  struct sockaddr_in local_isa;
  char local_hostname[80];
  char ch;
  int s;
  int local_port;
  int rs;

  remote_hp = gethostbyname(*remote_hostname);
  if(!remote_hp)
    {
    perror("couldn't get remote host address");
    exit(-1);
    }

  /* Copy remote IP address into socket address structure */
  remote_isa.sin_family = AF_INET;
  remote_isa.sin_port = htons(remote_port);
  memcpy(&remote_isa.sin_addr, remote_hp->h_addr, sizeof(remote_isa.sin_addr));

  gethostname(local_hostname, 80);
  local_hp = gethostbyname(local_hostname);
  if(!local_hp)
    {
    perror("couldn't get local host address");
    exit(-1);
    }

  /* Copy local IP address into socket address structure */
  local_isa.sin_family = AF_INET;
  memcpy(&local_isa.sin_addr, local_hp->h_addr, sizeof(local_isa.sin_addr));

  /* Create the local socket */
  s = socket(AF_INET, SOCK_STREAM, 0);
  if(s < 0)
    {
    perror("socket failed\n");
    exit(-1);
    }

  /* Bind local socket with a port from IPPORT_RESERVED/2 to IPPORT_RESERVED - 1
     this requires the OPER privilege under VMS -- to allow communication with
     a stock rshd under UNIX */

  for(local_port = IPPORT_RESERVED - 1; local_port >= IPPORT_RESERVED/2; local_port--)
    {
    local_isa.sin_port = htons(local_port);
    rs = bind(s, (struct sockaddr *)&local_isa, sizeof(local_isa));
    if(rs == 0)
      break;
    }                  

  /* Bind local socket to an unprivileged port.  A normal rshd will drop the
     connection; you must be running a patched rshd invoked through inetd for
     this connection method to work */

  if (rs != 0)
    for(local_port = IPPORT_USERRESERVED - 1;
        local_port > IPPORT_RESERVED;
        local_port--)
      {
      local_isa.sin_port = htons(local_port);
      rs = bind(s, (struct sockaddr *)&local_isa, sizeof(local_isa));
      if(rs == 0)
        break;
      }
  
  rs = connect(s, (struct sockaddr *) &remote_isa, sizeof(remote_isa));
  if(rs == -1)
    {
    fprintf(stderr, "connect: errno = %d\n", errno);
    close(s);
    exit(-2);
    }

  /* Now supply authentication information */

  /* Auxiliary port number for error messages, we don't use it */
  write(s, "0\0", 2);

  /* Who are we */
  write(s, local_user, strlen(local_user) + 1);

  /* Who do we want to be */
  write(s, remote_user, strlen(remote_user) + 1);

  /* What do we want to run */
  write(s, command, strlen(command) + 1);

  /* NUL is sent back to us if information is acceptable */
  read(s, &ch, 1);
  if(ch != '\0')
    {
    errno = EPERM;
    return -1;
    }

  return s;
}
