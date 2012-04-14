/*	$OpenBSD: asr.h,v 1.1 2012/04/14 09:24:18 eric Exp $	*/
/*
 * Copyright (c) 2012 Eric Faurot <eric@openbsd.org>
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

#include <sys/types.h>
#include <sys/socket.h>

#include <netdb.h>
#include <netinet/in.h>

/*
 * This part is the generic API for the async mechanism.  It could be useful
 * beyond the resolver.
 */

/* Return values for async_run() */
#define ASYNC_COND	0 /* wait for fd condition */
#define ASYNC_YIELD	1 /* partial result */
#define ASYNC_DONE	2 /* done */

/* Expected fd conditions  */
#define ASYNC_READ	1
#define ASYNC_WRITE	2

/* This opaque structure holds an async query state. */
struct async;

/*
 * This is the structure through which async_run() returns async
 * results to the caller.
 */
struct async_res {
	int	 ar_cond;
	int	 ar_fd;
	int	 ar_timeout;

	int	 ar_errno;
	int	 ar_h_errno;
	int	 ar_gai_errno;
	int	 ar_rrset_errno;

	int	 ar_rcode;
	void	*ar_data;
	int	 ar_datalen;
	union {
		struct sockaddr		sa;
		struct sockaddr_in	sain;
		struct sockaddr_in6	sain6;
	}	 ar_sa;
	char	*ar_cname;
	int	 ar_count;

	struct addrinfo	 *ar_addrinfo;
	struct rrsetinfo *ar_rrsetinfo;
	struct hostent	 *ar_hostent;
	struct netent	 *ar_netent;
};

int  async_run(struct async *, struct async_res *);
int  async_run_sync(struct async *, struct async_res *);
void async_abort(struct async *);

/* This opaque structure holds an async resolver context. */
struct asr;

struct asr *async_resolver(const char*);
void	    async_resolver_done(struct asr*);

/* Async version of the resolver API */

struct async *res_send_async(const unsigned char *, int, unsigned char *, int,
    struct asr *);
struct async *res_query_async(const char *, int, int, unsigned char *, int,
    struct asr *);
struct async *res_search_async(const char *, int, int, unsigned char *, int,
    struct asr *);

struct async *getrrsetbyname_async(const char *, unsigned int, unsigned int,
    unsigned int, struct asr *);

struct async *gethostbyname_async(const char *, struct asr *);
struct async *gethostbyname2_async(const char *, int, struct asr *);
struct async *gethostbyaddr_async(const void *, socklen_t, int, struct asr *);
void freehostent(struct hostent *);

struct async *getnetbyname_async(const char *, struct asr *);
struct async *getnetbyaddr_async(in_addr_t, int, struct asr *);
void freenetent(struct netent *);

struct async *getaddrinfo_async(const char *, const char *,
    const struct addrinfo *, struct asr *);
struct async *getnameinfo_async(const struct sockaddr *, socklen_t, char *,
    size_t, char *, size_t, int, struct asr *);
