/*	$OpenBSD: ko.h,v 1.2 1999/04/30 01:59:11 art Exp $	*/
/*
 * Copyright (c) 1998, 1999 Kungliga Tekniska Högskolan
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      Högskolan and its contributors.
 * 
 * 4. Neither the name of the Institute nor the names of its contributors
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

/* $KTH: ko.h,v 1.16 1999/03/03 15:39:51 assar Exp $ */

#ifndef __KO_H
#define __KO_H 1

#include <atypes.h>
#include <bool.h>

typedef int32_t koerr_t;

/*
 * Error messages
 */

const char    *koerr_gettext(koerr_t err);

/*
 * sysname
 */

const char *arla_getsysname(void);


/*
 * Cell managing
 */

typedef struct {
     const char *name;
     struct in_addr addr;
} cell_db_entry;

typedef struct {
    int32_t id;		/* Cell-ID */
    const char *name;		/* Domain-style name */
    const char *expl;		/* Longer name */
    unsigned ndbservers;	/* # of database servers */
    cell_db_entry *dbservers;	/* Database servers */
    enum { NOSUID_CELL, SUID_CELL } suid_cell ; /* if this is a suid cell */
} cell_entry;

void           cell_init (int cellcachesize);

const cell_db_entry *cell_dbservers (int32_t cell, int *);

const char    *cell_findnamedbbyname (const char *cell);
const char    *cell_getthiscell (void);
const char    *cell_getcellbyhost(const char *host);
int32_t        cell_name2num (const char *cell);
const char    *cell_num2name (int32_t cell);
cell_entry    *cell_get_by_name (const char *cellname);
cell_entry    *cell_get_by_id (int32_t cell);
cell_entry    *cell_new (const char *name);
Bool           cell_issuid (cell_entry *c);
Bool           cell_issuid_by_num (int32_t cell);
Bool           cell_issuid_by_name (const char *cell);
Bool	       cell_setsuid_by_num (int32_t cell);

/*
 * misc vl
 */

#include <vldb.h>
#include <volumeserver.h>

void vldb2vldbN (const vldbentry *old, nvldbentry *new);
void volintInfo2xvolintInfo (const volintInfo *old, xvolintInfo *new);

#endif /* __KO_H */
