/*	$OpenBSD: smtpd-api.h,v 1.6 2013/07/19 19:53:33 eric Exp $	*/

/*
 * Copyright (c) 2013 Eric Faurot <eric@openbsd.org>
 * Copyright (c) 2011 Gilles Chehade <gilles@poolp.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef	_SMTPD_API_H_
#define	_SMTPD_API_H_

#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netdb.h>

#define	FILTER_API_VERSION	 50

struct mailaddr {
	char	user[SMTPD_MAXLOCALPARTSIZE];
	char	domain[SMTPD_MAXDOMAINPARTSIZE];
};

SPLAY_HEAD(_dict, dictentry);
SPLAY_HEAD(_tree, treeentry);

struct tree {
	struct _tree	tree;
	size_t		count;
};

struct dict {
	struct _dict	dict;
	size_t		count;
};

enum filter_status {
	FILTER_OK,
	FILTER_FAIL,
	FILTER_CLOSE,
};

enum filter_imsg {
	IMSG_FILTER_REGISTER,
	IMSG_FILTER_EVENT,
	IMSG_FILTER_QUERY,
	IMSG_FILTER_NOTIFY,
	IMSG_FILTER_DATA,
	IMSG_FILTER_RESPONSE,
};

#define	FILTER_ALTERDATA	0x01 /* The filter wants to alter the message */

/* XXX - server side requires mfa_session.c update on filter_hook changes */
enum filter_hook {
	HOOK_CONNECT		= 1 << 0,	/* req */
	HOOK_HELO		= 1 << 1,	/* req */
	HOOK_MAIL		= 1 << 2,	/* req */
	HOOK_RCPT		= 1 << 3,	/* req */
	HOOK_DATA		= 1 << 4,	/* req */
	HOOK_EOM		= 1 << 5,	/* req */

	HOOK_RESET		= 1 << 6,	/* evt */
	HOOK_DISCONNECT		= 1 << 7,	/* evt */
	HOOK_COMMIT		= 1 << 8,	/* evt */
	HOOK_ROLLBACK		= 1 << 9,	/* evt */

	HOOK_DATALINE		= 1 << 10,	/* data */
};

struct filter_connect {
	struct sockaddr_storage	 local;
	struct sockaddr_storage	 remote;
	const char		*hostname;
};

#define PROC_TABLE_API_VERSION	1

enum table_service {
	K_NONE		= 0x00,
	K_ALIAS		= 0x01,	/* returns struct expand	*/
	K_DOMAIN	= 0x02,	/* returns struct destination	*/
	K_CREDENTIALS	= 0x04,	/* returns struct credentials	*/
	K_NETADDR	= 0x08,	/* returns struct netaddr	*/
	K_USERINFO	= 0x10,	/* returns struct userinfo	*/
	K_SOURCE	= 0x20, /* returns struct source	*/
	K_MAILADDR	= 0x40, /* returns struct mailaddr	*/
	K_ADDRNAME	= 0x80, /* returns struct addrname	*/
};
#define K_ANY		  0xff

enum {
	PROC_TABLE_OK,
	PROC_TABLE_FAIL,
	PROC_TABLE_OPEN,
	PROC_TABLE_CLOSE,
	PROC_TABLE_UPDATE,
	PROC_TABLE_CHECK,
	PROC_TABLE_LOOKUP,
	PROC_TABLE_FETCH,
};

static inline uint32_t
evpid_to_msgid(uint64_t evpid)
{
	return (evpid >> 32);
}

static inline uint64_t
msgid_to_evpid(uint32_t msgid)
{
        return ((uint64_t)msgid << 32);
}

/* dict.c */
#define dict_init(d) do { SPLAY_INIT(&((d)->dict)); (d)->count = 0; } while(0)
#define dict_empty(d) SPLAY_EMPTY(&((d)->dict))
#define dict_count(d) ((d)->count)
int dict_check(struct dict *, const char *);
void *dict_set(struct dict *, const char *, void *);
void dict_xset(struct dict *, const char *, void *);
void *dict_get(struct dict *, const char *);
void *dict_xget(struct dict *, const char *);
void *dict_pop(struct dict *, const char *);
void *dict_xpop(struct dict *, const char *);
int dict_poproot(struct dict *, const char * *, void **);
int dict_root(struct dict *, const char * *, void **);
int dict_iter(struct dict *, void **, const char * *, void **);
int dict_iterfrom(struct dict *, void **, const char *, const char **, void **);
void dict_merge(struct dict *, struct dict *);

/* filter_api.c */
void filter_api_setugid(uid_t, gid_t);
void filter_api_set_chroot(const char *);
void filter_api_no_chroot(void);

void filter_api_loop(void);
void filter_api_accept(uint64_t);
void filter_api_accept_notify(uint64_t);
void filter_api_reject(uint64_t, enum filter_status);
void filter_api_reject_code(uint64_t, enum filter_status, uint32_t,
    const char *);
void filter_api_data(uint64_t, const char *);

void filter_api_on_notify(void(*)(uint64_t, enum filter_status));
void filter_api_on_connect(void(*)(uint64_t, uint64_t, struct filter_connect *));
void filter_api_on_helo(void(*)(uint64_t, uint64_t, const char *));
void filter_api_on_mail(void(*)(uint64_t, uint64_t, struct mailaddr *));
void filter_api_on_rcpt(void(*)(uint64_t, uint64_t, struct mailaddr *));
void filter_api_on_data(void(*)(uint64_t, uint64_t));
void filter_api_on_dataline(void(*)(uint64_t, const char *), int);
void filter_api_on_eom(void(*)(uint64_t, uint64_t));
void filter_api_on_event(void(*)(uint64_t, enum filter_hook));

/* table */
void table_api_on_update(int(*)(void));
void table_api_on_check(int(*)(int, const char *));
void table_api_on_lookup(int(*)(int, const char *, char *, size_t));
void table_api_on_fetch(int(*)(int, char *, size_t));
int table_api_dispatch(void);

/* tree.c */
#define tree_init(t) do { SPLAY_INIT(&((t)->tree)); (t)->count = 0; } while(0)
#define tree_empty(t) SPLAY_EMPTY(&((t)->tree))
#define tree_count(t) ((t)->count)
int tree_check(struct tree *, uint64_t);
void *tree_set(struct tree *, uint64_t, void *);
void tree_xset(struct tree *, uint64_t, void *);
void *tree_get(struct tree *, uint64_t);
void *tree_xget(struct tree *, uint64_t);
void *tree_pop(struct tree *, uint64_t);
void *tree_xpop(struct tree *, uint64_t);
int tree_poproot(struct tree *, uint64_t *, void **);
int tree_root(struct tree *, uint64_t *, void **);
int tree_iter(struct tree *, void **, uint64_t *, void **);
int tree_iterfrom(struct tree *, void **, uint64_t, uint64_t *, void **);
void tree_merge(struct tree *, struct tree *);

#endif
