/*
 * Copyright (c) 1995, 1996, 1997, 1998, 1999 Kungliga Tekniska Högskolan
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

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <rx/rx.h>
#include <rx/rx_null.h>

#include <ports.h>
#include <ko.h>

#ifdef KERBEROS
#include <des.h>
#include <krb.h>
#include <rxkad.h>
#endif

#include <err.h>
#include <assert.h>
#include <ctype.h>

#ifndef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <service.h>
#include <msecurity.h>
#include <netinit.h>

#include <agetarg.h>

#include "rx/rxgencon.h"
#include "vldb.h"
#include "vldb.ss.h"
#include "ubik.ss.h"


extern int vl_database;
extern vlheader vl_header;
extern off_t file_length;

void
vlservdebug (char *fmt, ...)
    __attribute__ ((format (printf, 1, 2)));


void  vldb_write_header (void);
void  vldb_read_header (void);
void  vldb_get_file_length (void);
off_t vldb_find_first_free (void);
int   vldb_write_entry (off_t offset, disk_vlentry *vl_entry);
int   vldb_read_entry (off_t offset, disk_vlentry *vl_entry);
unsigned long vldb_get_id_hash (long id);
unsigned long vldb_get_name_hash (const char *name);
int   vldb_get_first_id_entry (unsigned long hash_id, long type,
			       disk_vlentry *vl_entry);
int   vldb_get_first_name_entry (unsigned long hash_name,
				 disk_vlentry *vl_entry);
int   vldb_insert_entry (disk_vlentry *vl_entry);

void  vldb_create (char *databaseprefix);
void  vldb_init (char *databaseprefix);
int   vldb_print_entry (struct disk_vlentry *entry, int long_print);
int   vldb_setdebug (int debug);
void  vldb_debug (char *fmt, ...);

    
