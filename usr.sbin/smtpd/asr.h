/*	$OpenBSD: asr.h,v 1.4 2011/03/27 17:39:17 eric Exp $	*/
/*
 * Copyright (c) 2010,2011 Eric Faurot <eric@openbsd.org>
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

enum {
	ASR_COND,
	ASR_YIELD,
	ASR_DONE
};

#define ASR_READ	0x01
#define ASR_WRITE	0x02

#define ASR_NOREC	0x01

enum {
	ASR_OK = 0,
	EASR_MEMORY,
	EASR_TIMEDOUT,
	EASR_NAMESERVER,
	EASR_FAMILY,
	EASR_NOTFOUND,
	EASR_NAME,
	EASR_PARAM
};

struct asr_result {
	int		 ar_err;
	const char	*ar_errstr;

	int		 ar_cond;
	int		 ar_fd;
	int		 ar_timeout;

	int		 ar_count;
	struct addrinfo	*ar_ai;
	char		*ar_cname;
	void		*ar_data;
	size_t		 ar_datalen;
	union {
		struct sockaddr		sa;
		struct sockaddr_in	sain;
		struct sockaddr_in6	sain6;
	}	ar_sa;
};

struct asr_query;

struct asr	 *asr_resolver(const char*);
void		  asr_done(struct asr*);

int		  asr_run(struct asr_query*, struct asr_result*);
int		  asr_run_sync(struct asr_query*, struct asr_result*);
void		  asr_abort(struct asr_query*);

struct asr_query *asr_query_dns(struct asr*,
				uint16_t,
				uint16_t,
				const char*,
				int);

struct asr_query *asr_query_host(struct asr*,
				 const char*,
				 int);

struct asr_query *asr_query_addrinfo(struct asr*,
				     const char*,
				     const char*,
				     const struct addrinfo*);

struct asr_query *asr_query_cname(struct asr*,
				  const struct sockaddr*,
				  socklen_t);
