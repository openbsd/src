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

/*
 * RCSID("$arla: vos_local.h,v 1.22 2002/05/30 00:51:35 mattiasa Exp $");
 */

#define LISTVOL_PART      0x1
#define LISTVOL_NOAUTH    0x2
#define LISTVOL_LOCALAUTH 0x4
#define LISTVOL_FAST      0x8

/* this program needs __progname defined as a macro */
#define __progname "vos"
#define PROGNAME (vos_interactive ? "" : __progname" ")

/* if this is set the program runs in interactive mode */
extern int vos_interactive;

int vos_examine (int, char **);
int vos_vldbexamine (int, char **);
int vos_listpart (int, char **);
int vos_listvol (int, char **);
int vos_lock (int, char **);
int vos_unlock (int, char **);
int vos_listvldb(int argc, char **argv);
int vos_partinfo (int, char **);
int vos_status (int, char **);
int vos_createentry (int, char **);
int vos_syncsite (int, char **);
int vos_dump (int, char **);
int vos_create(int argc, char **argv);
int vos_endtrans(int argc, char **argv);
int vos_backup(int argc, char **argv);
int vos_backupsys(int argc, char **argv);
int vos_zap(int argc, char **argv);
int vos_listaddrs(int argc, char **argv);

int vos_listvldb_iter (const char *host, const char *cell, const char *volname,
		       const char *fileserver, const char *part,
		       arlalib_authflags_t auth, 
		       int (*proc)(void *data, struct vldbentry *),
		       void *data);

int getlistparts(const char *cell, const char *host,
		 part_entries *parts, arlalib_authflags_t auth);
int getlistparts_conn(struct rx_connection *connvolser, part_entries *parts);
int printlistvol(struct rx_connection *connvolser, const char *host, 
		 int part, int flags);
int get_vlentry (const char *cell, const char *host, const char *volname,
		 arlalib_authflags_t auth, nvldbentry *nvlentry);
int new_vlentry (struct rx_connection *conn, const char *cell, const char *host,
		 nvldbentry *nvldbentry, arlalib_authflags_t auth);

int vos_endtransaction (const char *cell, const char *host,
			int32_t trans, arlalib_authflags_t auth, int verbose);

void find_db_cell_and_host (const char **cell, const char **host);
