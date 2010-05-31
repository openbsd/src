/*	$OpenBSD: queue_backend.h,v 1.1 2010/05/31 23:38:56 jacekm Exp $	*/

/*
 * Copyright (c) 2010 Jacek Masiulaniec <jacekm@dobremiasto.net>
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

#define INVALID_ID	1

struct action_be {
	u_int64_t	 content_id;
	u_int64_t	 action_id;
	time_t		 birth;
	char		*aux;
	char		*status;
};

int		 queue_be_content_create(u_int64_t *);
int		 queue_be_content_open(u_int64_t, int);
void		 queue_be_content_delete(u_int64_t);

int		 queue_be_action_new(u_int64_t, u_int64_t *, char *);
int		 queue_be_action_read(struct action_be *, u_int64_t, u_int64_t);
int		 queue_be_action_status(u_int64_t, u_int64_t, char *);
void		 queue_be_action_delete(u_int64_t, u_int64_t);

int		 queue_be_commit(u_int64_t);

u_int64_t	 queue_be_encode(const char *);
char		*queue_be_decode(u_int64_t);

int		 queue_be_init(char *, uid_t, gid_t);
int		 queue_be_getnext(struct action_be *);
