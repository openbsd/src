/*
 * Copyright 1997,1998 Niels Provos <provos@physnet.uni-hamburg.de>
 * All rights reserved.
 *
 * Parts derived from code by Angelos D. Keromytis, kermit@forthnet.gr
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Niels Provos.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * server.c:
 * SERVER handling functions
 */

#ifndef lint
static char rcsid[] = "$Id: server.c,v 1.1 1998/11/14 23:37:28 deraadt Exp $";
#endif

#define _SERVER_C_
#include <stdio.h>
#include <stdlib.h> 
#include <fcntl.h>
#include <sys/types.h>  
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/socket.h>  
#include <netinet/in.h>  
#include <arpa/inet.h>  
#include <netdb.h> 
#include <sys/ioctl.h> 
#include <net/if.h> 
#include <string.h>
#include <unistd.h>
#ifdef _AIX 
#include <sys/select.h> 
#endif 
#include <errno.h>
#include "config.h"
#include "photuris.h"
#include "server.h"
#include "api.h"
#include "packet.h"
#include "schedule.h"
#include "errlog.h"
#include "buffer.h"
#ifdef IPSEC
#include "spi.h"
#include "attributes.h"
#include "kernel.h"
#endif

int
init_server(void)
{
     struct sockaddr_in sin, *sin2; 
     struct protoent *proto; 
     struct stat sb;
     int sock, d, i, ip, on = 1; 
     struct ifconf ifconf; 
     void *newbuf;
     char buf[1024];

     readfds = normfds = NULL;

     if (global_port == 0) {
#ifndef PHOTURIS_PORT  
	  struct servent *ser;

	  if ((ser = getservbyname("photuris", "udp")) == (struct servent *) NULL)
	       crit_error(1, "getservbyname(\"photuris\") in init_server()");

	  global_port = ser->s_port;
#else  
	  global_port = PHOTURIS_PORT;
#endif  
     }

     if ((proto = getprotobyname("udp")) == (struct protoent *) NULL)
          crit_error(1, "getprotobyname() in init_server()"); 
 
     if ((global_socket = socket(PF_INET, SOCK_DGRAM, proto->p_proto)) < 0)
          crit_error(1, "socket() in init_server()"); 
     
     setsockopt(global_socket, SOL_SOCKET, SO_REUSEADDR, (void *)&on, 
		sizeof(on));
#ifdef IPSEC
     kernel_set_socket_policy(global_socket);
#endif	  

     /* get the local addresses */ 
 
     ifconf.ifc_len = 1024; 
     ifconf.ifc_buf = buf; 
     bzero(buf, 1024); 
 
     if (ioctl(global_socket, SIOCGIFCONF, &ifconf) == -1) 
          crit_error(1, "ioctl() in init_server()"); 

     sin.sin_port = htons(global_port);
     sin.sin_addr.s_addr = INADDR_ANY; 
     sin.sin_family = AF_INET; 
     
     if (bind(global_socket, (struct sockaddr *)&sin, sizeof(struct sockaddr)) < 0)
	  crit_error(1, "bind() in init_server()"); 
 
     /* Save interfaces addresses here */
     addresses = (char **) calloc(1+1, sizeof(char *)); 
     if (addresses == (char **) NULL) 
	  crit_error(1, "calloc() in init_server()"); 
     addresses[1] = (char *) NULL; 
 
     sockets = (int *) calloc(1+1, sizeof(int)); 
     if (sockets == (int *) NULL) 
	  crit_error(1, "calloc() in init_server()"); 
     sockets[1] = -1;
 
     if (lstat(PHOTURIS_FIFO, &sb) == -1) {
	  if (errno != ENOENT)
	       crit_error(1, "stat() in init_server()");
	  if (mkfifo(PHOTURIS_FIFO, 0660) == -1)
	       crit_error(1, "mkfifo() in init_server()");
     } else if (!(sb.st_mode & S_IFIFO))
	  log_error(0, "%s is not a FIFO in init_server()", PHOTURIS_FIFO);

     /* We listen on a named pipe */
#if defined(linux) || defined(_AIX)
     if ((sockets[0] = open(PHOTURIS_FIFO, O_RDWR| O_NONBLOCK, 0)) == -1)
#else
     if ((sockets[0] = open(PHOTURIS_FIFO, O_RDONLY | O_NONBLOCK, 0)) == -1)
#endif
	  crit_error(1, "open() in init_server()");
     i = 1;                  /* One interface already */

#ifdef IPSEC
     /* We also listen on PF_ENCAP for notify messages */
     newbuf = realloc(addresses, (i + 2) * sizeof(char *)); 
     if (newbuf == NULL) {
	  if (addresses != NULL)
	       free (addresses);
	  crit_error(1, "realloc() in init_server()"); 
     }
     addresses = (char **) newbuf;
     
     addresses[i + 1] = (char *) NULL; 

     newbuf = realloc(sockets, (i + 2)* sizeof(int));
     if (newbuf == NULL) {
	  if (sockets != NULL)
	       free (sockets);
	  crit_error(1, "realloc() in init_server()");
     }
     sockets = (int *) newbuf;

     sockets[i] = kernel_get_socket();
     sockets[i+1] = -1;

     i++;                    /* Next interface */
#endif

     for (ip = 0, d = 0; d < ifconf.ifc_len; d += IFNAMSIZ + 
#if defined(__NetBSD__) || defined(__OpenBSD__) || defined(_AIX)
	       buf[IFNAMSIZ + d] 
#else 
	       sizeof(struct sockaddr) 
#endif 
	       , i++, ip++) {
	  sin2 = (struct sockaddr_in *) &buf[IFNAMSIZ + d]; 
 
	  if (sin2->sin_family != AF_INET) { 
	       i--; ip--;
	       continue; 
	  } 
 
	  newbuf =  realloc(addresses, (i + 2) * sizeof(char *)); 
	  if (newbuf == NULL) {
	       if (addresses != NULL)
		    free (addresses);
	       crit_error(1, "realloc() in init_server()"); 
	  }
	  addresses = (char **) newbuf;
	     
	  addresses[i] = strdup(inet_ntoa(sin2->sin_addr)); 
	  if (addresses[i] == (char *) NULL) 
	       crit_error(1, "strdup() in init_server()"); 
	  addresses[i + 1] = (char *) NULL; 

	  newbuf = realloc(sockets, (i + 2)* sizeof(int));
	  if (newbuf == NULL) {
	       if (sockets != NULL)
		    free (sockets);
	       crit_error(1, "realloc() in init_server()");
	  }
	  sockets = (int *) newbuf;

	  sockets[i+1] = -1;

	  if ((sock = socket(PF_INET, SOCK_DGRAM, proto->p_proto)) < 0)
	       crit_error(1, "socket() in init_server()"); 
	  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void *)&on, 
		     sizeof(on)); 
#ifdef IPSEC
	  kernel_set_socket_policy(sock);
#endif	  
	  sockets[i] = sock;

#ifdef DEBUG 
	  printf("Local interface %s, address %s.\n", buf + d, 
		 addresses[i]); 
#endif 

	  bzero((void *)&sin, sizeof(sin));
	  sin.sin_port = htons(global_port);
	  sin.sin_addr.s_addr = inet_addr(addresses[i]);
	  sin.sin_family = AF_INET; 
     
	  if (bind(sockets[i], (struct sockaddr *)&sin, sizeof(struct sockaddr)) < 0)
	       crit_error(1, "bind() in init_server()"); 
  
     } 

     num_ifs = i;
 
#ifdef DEBUG 
     printf("%d local interfaces supporting IP found.\n", ip); 
#endif 

     return 1;
}

int 
server(void)
{
     struct sockaddr_in sin; 
     struct timeval timeout; 
     int i, d, size;

     setvbuf(stdout, (char *)NULL, _IOLBF, 0);

     size = howmany(sockets[num_ifs-1], NFDBITS) * sizeof(fd_mask);
     normfds = (fd_set *)malloc(size);
     if (normfds == NULL)
	  crit_error(1, "malloc(%d) for fd_set", size);

     readfds = (fd_set *)malloc(size);
     if (readfds == NULL)
	  crit_error(1, "malloc(%d) for fd_set", size);

     memset((void *)normfds, 0, size); 

     for (i=0; i<num_ifs; i++)
	  FD_SET(sockets[i], normfds);

     while (1) {
	  bcopy(normfds, readfds, size);

	  /* Timeout till next job */
	  timeout.tv_usec = 0;
	  timeout.tv_sec = schedule_next();

#ifdef DEBUG2
	  printf("Sleeping for %ld seconds\n", timeout.tv_sec);
#endif

	  if (select(sockets[num_ifs-1]+1, 
		     readfds, (fd_set *) NULL, (fd_set *) NULL, 
		     (timeout.tv_sec == -1 ? NULL : &timeout)) < 0) 
	       if (errno == EINTR) 
                    continue; 
	       else
                    crit_error(1, "select() in server()"); 

	  for (i=0; i<num_ifs; i++) {
	       if (FD_ISSET(sockets[i], readfds)) {
#ifdef IPSEC
		    if (i == 1)       /* PF_ENCAP NOTIFIES */
			 kernel_handle_notify(sockets[i]);
		    else
#endif
			 if (addresses[i] == NULL)
			 process_api(sockets[i], global_socket); 
			 else if (strcmp("127.0.0.1", inet_ntoa(sin.sin_addr))) {
			 d = sizeof(struct sockaddr_in);
			 if (recvfrom(sockets[i], 
#ifdef BROKEN_RECVFROM
				      (char *) buffer, 1,
#else
				      (char *) NULL, 0, 
#endif
				      MSG_PEEK, 
				      (struct sockaddr *) &sin, &d) == -1) {
			      log_error(1, "recvfrom() in server()"); 
			      return -1;
			 }
			 handle_packet(sockets[i], addresses[i]);
 		    } else {
			 /* XXX - flush it. APUE */
			 d = sizeof(struct sockaddr_in);
			 recvfrom(sockets[i], (char *)buffer, BUFFER_SIZE, 0,
				  (struct sockaddr *) &sin, &d);
		    }
	       } 
	  }

	  schedule_process(global_socket);
	  fflush(stdout);
	  fflush(stderr);
     }

     /* We will never reach this place - it's called limbo */

}
