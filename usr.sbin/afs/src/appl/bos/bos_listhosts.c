/*
 * Copyright (c) 1995 - 2000 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "appl_locl.h"
#include <sl.h>
#include "bos_local.h"
#include <bos.h>
#include <bos.cs.h>


RCSID("$arla: bos_listhosts.c,v 1.7 2000/10/03 00:07:13 lha Exp $");

static int
printhosts(const char *cell, const char *host,
	   int noauth, int localauth, int verbose, int cellservdb,
	   const char *comment_text)
{
    struct rx_connection *conn = NULL;
    unsigned int nservers, i;
    int error;
    char **server_names = NULL;
    char cell_name[BOZO_BSSIZE];

    if (comment_text == NULL)
	comment_text = "";
    
    conn = arlalib_getconnbyname(cell,
				 host,
				 afsbosport,
				 BOS_SERVICE_ID,
				 arlalib_getauthflag(noauth, localauth,0,0));
    
    if (conn == NULL)
	return -1;
    
    /* which cell is this anyway ? */
    error = BOZO_GetCellName(conn, cell_name);
    if (error) {
	printf ("bos GetCellName %s: %s\n", host, koerr_gettext (error));
	return 0;
    }
  
    /* Who are the DB-servers ? */
    nservers = 0;
    while (1) {
	server_names = erealloc (server_names,
				 (nservers + 1) * sizeof(*server_names));
	server_names[nservers] = emalloc (BOZO_BSSIZE);
	error =  BOZO_GetCellHost(conn, nservers,  server_names[nservers]);
	if (error)
	    break;
	nservers++;
    }
    
    if (error != BZDOM) {
	printf ("bos listhosts: %s\n", koerr_gettext (error));
    } else {
	if (!cellservdb) {
	    printf("Cell name is %s\n", cell_name);
	    for (i = 0; i < nservers; i++)
		printf ("\t%s\n", server_names[i]);
	} else {
	    printf (">%s		#%s\n", cell_name, comment_text);
	    for (i = 0; i < nservers; i++) {
		struct hostent *he;
		char *addr;
		struct in_addr inaddr;

		he = gethostbyname(server_names[i]);
		if (he == NULL)
		    addr = "not-in-dns";
		else {
		    memcpy(&inaddr, he->h_addr, sizeof(inaddr));
		    addr = inet_ntoa(inaddr);
		}
		printf ("%s		#%s\n", addr, server_names[i]);
	    }
	}
    }
    for (i = 0; i < nservers; i++)
	free (server_names[i]);
    free (server_names);

    arlalib_destroyconn(conn);
    return 0;
}


static int helpflag;
static int dbflag;
static const char *comment_text; 
static const char *server;
static const char *cell;
static int noauth;
static int localauth;
static int verbose;

static struct agetargs args[] = {
    {"server",	0, aarg_string,	&server,	"server", NULL, aarg_mandatory},
    {"db", 0, aarg_flag,	&dbflag,	"print in CellServDB format", NULL},
    {"comment",	0, aarg_string,	&comment_text,	
     "comment text when CellServDB format is used",	  NULL},
    {"cell",	0, aarg_string,	&cell,		"cell",	  NULL},
    {"noauth",	0, aarg_flag,	&noauth,	"do not authenticate", NULL},
    {"local",	0, aarg_flag,	&localauth,	"localauth"},
    {"verbose",	0, aarg_flag,	&verbose,	"be verbose", NULL},
    {"help",	0, aarg_flag,	&helpflag,	NULL, NULL},
    {NULL,	0, aarg_end,	NULL,		NULL, NULL}
};

static void
usage (void)
{
    aarg_printusage (args, "bos listhosts", "", AARG_AFSSTYLE);
}

int
bos_listhosts(int argc, char **argv)
{
    int optind = 0;
    
    if (agetarg (args, argc, argv, &optind, AARG_AFSSTYLE)) {
	usage ();
	return 0;
    }
    
    if (helpflag) {
	usage ();
	return 0;
    }
    
    argc -= optind;
    argv += optind;
    
    if (server == NULL) {
	printf ("bos listhosts: missing -server\n");
	return 0;
    }
    
    if (cell == NULL)
	cell = cell_getcellbyhost (server);
    
    printhosts (cell, server, noauth, localauth, verbose, dbflag,
		comment_text);
    return 0;
}
