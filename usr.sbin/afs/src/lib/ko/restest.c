/*
 * Copyright (c) 2002, Stockholms Universitet
 * (Stockholm University, Stockholm Sweden)
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
 * 3. Neither the name of the university nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "ko_locl.h"

RCSID("$arla: restest.c,v 1.2 2002/07/24 21:26:22 lha Exp $");

static void
print_host(cell_db_entry *e)
{
    printf("host: %s ttl: %d addr: %s\n", 
	   e->name, (int)e->timeout,
	   inet_ntoa(e->addr));
}

int
main(int argc, char **argv)
{
    cell_db_entry dbservers[256];
    int lowest_ttl = INT_MAX;
    int ret, i, dbnum = 0;

    char *cell = "e.kth.se";

    memset(dbservers, 0, sizeof(dbservers)/sizeof(dbservers[0]));

    _ko_resolve_init();

    ret = _ko_resolve_cell(cell, dbservers, 
			   sizeof(dbservers)/sizeof(dbservers[0]),
			   &dbnum, &lowest_ttl);
    if (ret)
	errx(1, "_ko_resolve_cell failed with %d\n", ret);

    printf("cell %s, lowest_ttl %d\n", cell, lowest_ttl);

    for (i = 0; i < dbnum; i++)
	print_host(&dbservers[i]);


    dbservers[0].name = strdup("anden.e.kth.se");
    if (dbservers[0].name == NULL)
	err(1, "strdup");

    ret = _ko_resolve_host(dbservers[0].name, &dbservers[0]);
    if (ret)
	errx(1, "_ko_resolve_host failed with %d\n", ret);
    
    print_host(&dbservers[0]);

    exit(0);
}
