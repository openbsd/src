/*
 * Copyright (c) 1995 - 2001 Kungliga Tekniska Högskolan
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
#ifdef HAVE_OPENSSL
#include <openssl/des.h>
#else
#include <des.h>
#endif
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

#include <mlog.h>
#include <mdebug.h>
#include <mdb.h>

#include "rx/rxgencon.h"
#include "vldb.h"
#include "vldb.ss.h"
#include "ubik.ss.h"


extern int vl_database;
extern vital_vlheader vl_header;

void  vldb_write_header (void);
void  vldb_read_header (void);
int   vldb_write_entry(const disk_vlentry *vldb_entry);
int   vldb_read_entry (const char *name, disk_vlentry *entry);
int   vldb_delete_entry (const char *name);
int   vldb_id_to_name (const int32_t volid, char **name);
int   vldb_write_id (const char *name, const uint32_t volid);
int   vldb_delete_id (const char *name, const uint32_t volid);
void  vldb_close(void);
void  vldb_flush(void);


unsigned long vldb_get_id_hash (long id);
unsigned long vldb_get_name_hash (const char *name);
void  vldb_create (char *databaseprefix);
void  vldb_init (char *databaseprefix);
int   vldb_print_entry (vldbentry *entry, int long_print);

void  vldb_entry_to_disk(const struct vldbentry *newentry,
			 struct disk_vlentry *diskentry);
void  vldb_nentry_to_disk(const struct nvldbentry *entry,
			  struct disk_vlentry *diskentry);
void  vldb_disk_to_entry(const struct disk_vlentry *diskentry,
			 struct vldbentry *entry);
void  vldb_disk_to_nentry(const struct disk_vlentry *diskentry,
			  struct nvldbentry *entry);

void  vldb_free_diskentry(struct disk_vlentry *diskentry);
