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

/* $arla: arlalib.h,v 1.50 2003/04/24 11:51:24 lha Exp $ */

#ifndef ARLALIB_H
#define ARLALIB_H 1

/*
 * Credentinals
 */

typedef enum { AUTHFLAGS_NOAUTH		= 0x0,
	       AUTHFLAGS_ANY		= 0x1,
	       AUTHFLAGS_LOCALAUTH	= 0x2,
	       AUTHFLAGS_TICKET		= 0x4,
	       AUTHFLAGS_TOKEN		= 0x8,
	       AUTHFLAGS_DISALLOW_NOAUTH = 0x10
} arlalib_authflags_t;

arlalib_authflags_t 
arlalib_getauthflag (int noauth,
		     int localauth,
		     int ticket,
		     int token);

/*
 * Connections
 */

struct rx_connection *
arlalib_getconnbyaddr(const char *cell, int32_t addr, const char *host,
		      int32_t port, int32_t servid, 
		      arlalib_authflags_t auth);

struct rx_connection *
arlalib_getconnbyname(const char *cell, const char *host,
		      int32_t port, int32_t servid, 
		      arlalib_authflags_t auth);

int arlalib_destroyconn(struct rx_connection *conn);
int arlalib_getservername(uint32_t serverNumber, char **servername);
struct rx_securityClass*
arlalib_getsecurecontext(const char *cell, const char *host, 
			 arlalib_authflags_t auth, int *secidx);
int arlalib_getsyncsite(const char *cell, const char *host, int32_t port, 
			uint32_t *synchost, arlalib_authflags_t auth);


/*
 * Token management
 */

struct ClearToken;

typedef int (*arlalib_token_iter_func) (const char *secret, size_t secret_sz,
					const struct ClearToken *ct,
					const char *cell,
					void *arg);

int
arlalib_token_iter (const char *cell, 
		    arlalib_token_iter_func func, void *arg);


/*
 * Wrappers around pioctl calls
 */

void fserr(const char *progname, int error, const char *realpath);

int fs_getfid (char *path, VenusFid *fid);
int fs_getfilecellname (char *path, char *cell, size_t len);
int fs_nop(void);

/* arla extensions */

int fs_setcrypt (uint32_t level);
int fs_getcrypt (uint32_t *level);

int fs_connect (int32_t type, int32_t *flags);

int fs_setfprio(VenusFid fid, int16_t prio);
int fs_getfprio(VenusFid fid, int16_t *prio);
int fs_setmaxfprio(int16_t maxprio);
int fs_getmaxfprio(int16_t *maxprio);

int fs_gcpags(void);

int fs_calculate_cache(uint32_t *calculated,
		       uint32_t *usedbytes);

int fs_getfilecachestats(int64_t *max_bytes,
			 int64_t *used_bytes,
			 int64_t *low_bytes,
			 int64_t *max_vnodes,
			 int64_t *used_vnodes,
			 int64_t *low_vnodes);

int fs_getaviatorstats(uint32_t *max_workers,
		       uint32_t *used_workers);

int fs_checkservers(char *cell, int32_t flags,
		    uint32_t *hosts, int numhosts);

int fs_checkvolumes (void);

int
fs_set_sysname (const char *sys);

int
fs_get_sysname (char *sys, size_t sys_sz);

int
fs_setcache(int lv, int hv, int lb, int hb);

int
fs_wscell (char *cell, size_t cell_sz);

int
fs_flushvolume (const char *path);

int
fs_flush (const char *path);

int
fs_venuslog (void);

int
fs_newcell (const char *cell, int nservers, char **servers);

int
fs_getcells (int32_t num, uint32_t *server, int numservers,
	     char *cell, size_t cell_sz);

int
fs_getcellstatus (char *cellname, uint32_t *flags);

int
fs_invalidate (const char *path);

int
fs_lsmount (const char *path);

int
fs_rmmount (const char *path);

int
fs_incompat_renumber (int *ret);

int
fs_statistics_list(uint32_t *host, uint32_t *part, int *n);

int
fs_statistics_entry(uint32_t host, uint32_t part, uint32_t type,
		    uint32_t items_slot, uint32_t *count,
		    int64_t *items_total, int64_t *total_time);

int
arlalib_get_viceid (const char *username, const char *cellname,
		    int32_t *viceId);

int
arlalib_get_viceid_servers (const char *username, const char *cellname,
			    int nservers, const char *servers[],
			    int32_t *viceId);

int nnpfs_debug(int inflags, int *outflags);
int nnpfs_debug_print(int inflags, char *);
int arla_debug(int inflags, int *outflags);

/* db server context */

struct db_server_context {
  const char *cell;
  int port;
  int serv_id;
  arlalib_authflags_t auth;
  const char **hosts;
  int nhosts;
  int curhost;
  struct rx_connection **conn;
};

struct rx_connection*
arlalib_first_db(struct db_server_context *context, const char *cell,
		 const char *host,
		 int port, int serv_id, arlalib_authflags_t auth);

struct rx_connection*
arlalib_next_db(struct db_server_context *context);

int
arlalib_try_next_db (int error);

void
free_db_server_context(struct db_server_context *context);

int
arlalib_get_token_id (const char *username, const char *cellname,
		      int32_t *token_id);

int
arlalib_get_token_id_servers (const char *username, const char *cellname,
			      int nservers, const char *servers[],
			      int32_t *token_id);

void
arlalib_host_to_name (uint32_t addr, char *str, size_t str_sz);

int
arlalib_name_to_host (const char *str, uint32_t *addr);

int
arlalib_version_cmd(int argc, char **argv);

#endif /* ARLALIB_H */
