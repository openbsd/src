/*	$OpenBSD: dh.h,v 1.1 2010/06/03 16:41:12 reyk Exp $	*/
/*	$vantronix: dh.h,v 1.8 2010/06/02 12:22:58 reyk Exp $	*/

/*
 * Copyright (c) 2010 Reyk Floeter <reyk@vantronix.net>
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

#ifndef _DH_H_
#define _DH_H_

#include <sys/types.h>

enum group_type {
	GROUP_MODP	= 0,
	GROUP_EC	= 1
};

struct group_id {
	enum group_type	 type;
	u_int		 id;
	int		 bits;
	char		*prime;
	char		*generator;
	int		 nid;
};

struct group {
	int		 id;
	struct group_id	*spec;

	void		*dh;
	void		*ec;

	int		(*init)(struct group *);
	int		(*getlen)(struct group *);
	int		(*exchange)(struct group *, u_int8_t *);
	int		(*shared)(struct group *, u_int8_t *, u_int8_t *);
};

void            group_init(void);
void            group_free(struct group *);
struct group   *group_get(u_int32_t);

int	dh_init(struct group *);
int	dh_getlen(struct group *);
int	dh_create_exchange(struct group *, u_int8_t *);
int	dh_create_shared(struct group *, u_int8_t *, u_int8_t *);
void	dh_selftest(void);

#endif /* _DH_H_ */
