/*
 * Copyright 1997,1998 Niels Provos <provos@physnet.uni-hamburg.de>
 * All rights reserved.
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
 * photurisd.c: photuris daemon and stuff.
 */

#ifndef lint 
static char rcsid[] = "$Id: photurisd.c,v 1.9 1998/06/30 16:58:38 provos Exp $";
#endif 

#define _PHOTURIS_C_

#include <stdio.h> 
#include <stdlib.h>
#include <sys/types.h> 
#include <netinet/in.h>
#include <signal.h> 
#include <errno.h> 
#include <string.h>
#include <unistd.h>
#include "config.h"
#include "photuris.h"
#include "server.h"
#include "buffer.h"
#include "cookie.h"
#include "identity.h"
#include "spi.h"
#include "packet.h"
#include "schedule.h"
#include "errlog.h"
#ifdef IPSEC
#include "attributes.h"
#include "kernel.h"
#endif

static int init_vars(void);
static void usage(void);

static void
usage(void)
{
     FILE *f = stderr;

     fprintf(f, "usage: photurisd [-cvi] [-d directory] [-p port]\n");
     fprintf(f, "\t-c  check primes on startup\n");
     fprintf(f, "\t-v  start in VPN mode\n");
     fprintf(f, "\t-i  ignore startup file %s\n", PHOTURIS_STARTUP);
     fprintf(f, "\t-d  specifies the startup dir\n");
     fprintf(f, "\t-p  specifies the local port to bind to\n");
     exit(1);
}
     

static int 
init_vars(void)
{
     global_schemes = NULL;
     global_schemesize = 0;

     config_file = NULL;
     attrib_file = NULL;

     if ((config_file = calloc(1, sizeof(PHOTURIS_CONFIG))) == NULL)
	  crit_error(1, "no memory in init_vars()" );
     strcpy(config_file, PHOTURIS_CONFIG);

     if ((secret_file = calloc(1, sizeof(PHOTURIS_SECRET))) == NULL)
	  crit_error(1, "no memory in init_vars()" );
     strcpy(secret_file, PHOTURIS_SECRET);

     if ((attrib_file = calloc(1, sizeof(PHOTURIS_ATTRIB))) == NULL)
	  crit_error(1, "no memory in init_vars()");
     strcpy(attrib_file, PHOTURIS_ATTRIB);

     reset_secret();

     max_retries = MAX_RETRIES;
     retrans_timeout = RETRANS_TIMEOUT;
     exchange_timeout = EXCHANGE_TIMEOUT;
     exchange_lifetime = EXCHANGE_LIFETIME;
     spi_lifetime = SPI_LIFETIME;

     return 1;
}

int
main(int argc, char **argv)
{
     int ch;
     int primes = 0, ignore = 0;
     char *dir = PHOTURIS_DIR;

     daemon_mode = 0;
     global_port = 0;
     vpn_mode = 0;

     while ((ch = getopt(argc, argv, "vcid:p:")) != -1)
	  switch((char)ch) {
	  case 'c':
	       primes = 1;
	       break;
	  case 'v':
	       vpn_mode = 1;
	       break;
	  case 'i':
	       ignore = 1;
	       break;
	  case 'd':
	       dir = optarg;
	       break;
	  case 'p':
	       global_port = atoi(optarg);
	       break;
	  case '?':
	  default:
	       usage();
	  }

     if (chdir(dir) == -1)
	  crit_error(1, "chdir(\"%s\") in main()", dir);
	  

     argc -= optind;
     argv += optind;
     
     init_vars();

     init_times();

     init_moduli(primes);

     init_schemes();

     init_attributes(); 

     if (init_identities(NULL,NULL) == -1)
	  exit(-1);
     
     init_schedule();

#ifdef IPSEC
     init_kernel();
#endif

     init_server();

     /* Startup preconfigured exchanges */
     if( !ignore && !vpn_mode)
	  init_startup();

#ifndef DEBUG
     init_signals();
     if (fork())
	  exit(0);
     daemon_mode = 1;
#endif

     server();
     exit(0);
}
