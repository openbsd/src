/*
 * Copyright (c) 1999 Kungliga Tekniska Högskolan
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

#include "ko_locl.h"
#include "cellconfig.h"
#include "ports.h"
#include <log.h>

RCSID("$arla: afsconf.c,v 1.5 2000/10/03 00:28:53 lha Exp $");

/*
 * Currently only handles dir_path == NULL
 */

struct afsconf_dir *
afsconf_Open(const char *dir_path)
{
    struct afsconf_dir *ret;
    Log_method *method;

    assert (dir_path == NULL);
    ret = malloc (sizeof (*ret));
    if (ret == NULL)
	return NULL;

    method = log_open ("afsconf", "/dev/stderr");
    if (method == NULL)
	errx (1, "log_open failed");
    cell_init(0, method);
    ports_init ();

    return ret;
}

/*
 * get the name of the local cell,
 * return value == 0 -> success, otherwise -> failure
 */

int
afsconf_GetLocalCell (struct afsconf_dir *ctx, char *cell, size_t len)
{
    strlcpy (cell, cell_getthiscell (), len);
    return 0;
}

/*
 *
 */

int
afsconf_GetCellInfo (struct afsconf_dir *ctx,
		     const char *cellname,
		     char *unknown, /* XXX */
		     struct afsconf_cell *conf)
{
    cell_entry *entry = cell_get_by_name (cellname);
    int i;

    if (entry == NULL)
	return -1;

    strlcpy (conf->name, entry->name, sizeof(conf->name));
    conf->numServers = entry->ndbservers;
    conf->flags = 0;
    for (i = 0; i < entry->ndbservers; ++i) {
	memset (&conf->hostAddr[i], 0, sizeof(conf->hostAddr[i]));
	conf->hostAddr[i].sin_family = AF_INET;
	conf->hostAddr[i].sin_addr   = entry->dbservers[i].addr;
	conf->hostAddr[i].sin_port   = afsvldbport;
	strlcpy (conf->hostName[i], entry->dbservers[i].name,
		 sizeof(conf->hostName[i]));
    }
    conf->linkedCell = NULL;
    return 0;
}

/*
 * destroy ctx
 */

int
afsconf_Close(struct afsconf_dir *ctx)
{
    free (ctx);
    return 0;
}
