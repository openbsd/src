/*
 * Copyright (c) 2000 Kungliga Tekniska Högskolan
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

RCSID("$arla: kalog.c,v 1.8 2003/01/17 03:30:21 lha Exp $");

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>

#include <assert.h>

#include <ko.h>
#include <ports.h>
#include <log.h>

#include <arlalib.h>

#include <ka.cs.h>
#ifdef HAVE_OPENSSL
#include <openssl/des.h>
#else
#include <des.h>
#endif
#include <krb.h>

#include <ka-procs.h>

#include <roken.h>
#include <err.h>

#include <vers.h>

static void
parse_user (char *argv1, const char **user, const char **cell)
{
    char *at = strchr(argv1, '@');
    char *tmp_cell;
    
    if(at) {
	*at = '\0';
	
	*user = argv1;
	at++;
	tmp_cell = at;
	
	if(*tmp_cell != '\0')
	    *cell = tmp_cell;
    } else {
	*user = argv1;
    }
    
}

int
main (int argc, char **argv)
{
    int ret;
    Log_method *method;
    const char *cellname;
    const char *user;

    set_progname (argv[0]);
    tzset();

    method = log_open (getprogname(), "/dev/stderr:notime");
    if (method == NULL)
	errx (1, "log_open failed");
    cell_init(0, method);
    ports_init();
    
    cellname = cell_getthiscell();

    if (argc == 1)
	user = get_default_username();
    else if (strcmp("-version", argv[1]) == 0) {
	print_version(NULL);
	return 0;
    } else if (argc == 2)
	parse_user (argv[1], &user, &cellname);
    else {
	fprintf (stderr, "usage: %s [-version] [username[@cell]]\n",
		 getprogname());
	exit (1);
    }

    printf ("Getting ticket for %s@%s\n", user, cellname);
    ret = ka_authenticate (user, "", cellname, NULL, 8 * 3600,
			   KA_AUTH_TICKET|KA_AUTH_TOKEN);
    if (ret)
	errx (1, "ka_authenticate failed with %s (%d)",
	      koerr_gettext(ret), ret);

    return 0;
}
