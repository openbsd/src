/*
 * Copyright (c) 1998 - 2001 Kungliga Tekniska Högskolan
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

/* $arla: ko.h,v 1.36 2002/12/06 05:00:04 lha Exp $ */

#ifndef __KO_H
#define __KO_H 1

#include <sys/types.h>
#include <netinet/in.h>
#include <atypes.h>
#include <bool.h>
#include <log.h>

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
    time_t timeout;		/* timeout of address */
} cell_db_entry;

enum { SUID_CELL 	= 0x1,	/* if this is a suid cell */
       DYNROOT_CELL	= 0x2	/* cell should show up in dynroot */
};

enum { DYNROOT_CELLID = 0 };

enum { 
    DYNROOT_ALIAS_READONLY = 0,
    DYNROOT_ALIAS_READWRITE = 1
};

typedef struct {
    int32_t id;			/* Cell-ID */
    const char *name;		/* Domain-style name */
    const char *expl;		/* Longer name */
    unsigned ndbservers;	/* # of database servers */
    unsigned active_hosts;	/* # of active db servers */
    cell_db_entry *dbservers;	/* Database servers */
    unsigned flags;		/* Various flags, like SUID_CELL */
    time_t timeout;		/* when this entry expire */
    time_t poller_timeout;	/* delta time between poller calls */
} cell_entry;

void	      cell_init (int cellcachesize, Log_method *logm);

const cell_db_entry *cell_dbservers_by_id (int32_t cell, int *);

const char    *cell_findnamedbbyname (const char *cell);
const char    *cell_getthiscell (void);
const char    *cell_getcellbyhost(const char *host);
int32_t        cell_name2num (const char *cell);
const char    *cell_num2name (int32_t cell);
cell_entry    *cell_get_by_name (const char *cellname);
cell_entry    *cell_get_by_id (int32_t cell);
cell_entry    *cell_new (const char *name);
cell_entry    *cell_new_dynamic (const char *name);
Bool	       cell_dynroot (const cell_entry *c);
Bool           cell_issuid (const cell_entry *c);
Bool           cell_issuid_by_num (int32_t cell);
Bool           cell_issuid_by_name (const char *cell);
Bool	       cell_setsuid_by_num (int32_t cell);
int            cell_setthiscell (const char *cell);
int	       cell_foreach (int (*func) (const cell_entry *, void *), 
			     void *arg);
typedef	int    (*cell_alias_fn)(const char *, const char *, int, void *);
int	       cell_alias_foreach (cell_alias_fn, void *);
int	       cell_addalias(const char *, const char *, const char *);
const char    *cell_expand_cell (const char *cell);
unsigned long  cell_get_version(void);
Bool 	       cell_is_sanep (int cell);
const char **  cell_thesecells (void);
void 	       cell_print_cell (const cell_entry *c, FILE *out);
void	       cell_status (FILE *f);
time_t	       cell_get_poller_time(const cell_entry *c);
void	       cell_set_poller_time(cell_entry *c, time_t time);


/*
 * misc vl
 */

#include <vldb.h>
#include <volumeserver.h>

void vldb2vldbN (const vldbentry *old, nvldbentry *new);
void volintInfo2xvolintInfo (const volintInfo *old, xvolintInfo *new);

int volname_canonicalize (char *volname);
size_t volname_specific (const char *volname, int type,
			 char *buf, size_t buf_sz);
const char *volname_suffix (int type);

char *vol_getopname(int32_t op, char *str, size_t sz);
const char *volumetype_from_serverflag(int32_t flag);
const char *volumetype_from_volsertype(int32_t type);

/*
 * misc
 */

int
VenusFid_cmp (const VenusFid *fid1, const VenusFid *fid2);

#endif /* __KO_H */
