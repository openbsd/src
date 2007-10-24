/*	$OpenBSD: auth.c,v 1.9 2007/10/24 20:52:50 claudio Exp $ */

/*
 * Copyright (c) 2006 Michele Marchetto <mydecay@openbeer.it>
 * Copyright (c) 2004, 2005 Esben Norby <norby@openbsd.org>
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
#include <limits.h>
#include <md5.h>
#include <stdlib.h>
#include <string.h>

#include "ripd.h"
#include "rip.h"
#include "log.h"
#include "ripe.h"

u_int32_t	 auth_calc_modulator(struct auth_md *md);
struct auth_md	*md_list_find(struct auth_md_head *, u_int8_t);
void		 auth_trailer_header_gen(struct buf *);
u_int32_t	 auth_get_seq_num(struct auth_md*);

u_int32_t
auth_calc_modulator(struct auth_md *md)
{
	u_int32_t		r;
	MD5_CTX			md5ctx;
	u_int8_t		digest[MD5_DIGEST_LENGTH];

	MD5Init(&md5ctx);
	MD5Update(&md5ctx, (void *)&md->keyid, sizeof(md->keyid));
	MD5Update(&md5ctx, (void *)&md->key, MD5_DIGEST_LENGTH);
	MD5Final(digest, &md5ctx);
	
	bcopy(&digest, &r, sizeof(r));

	return ((r >> 1) - time(NULL));
}

u_int32_t
auth_get_seq_num(struct auth_md *md)
{
	return (time(NULL) + md->seq_modulator);
}

void
auth_trailer_header_gen(struct buf *buf)
{
	u_int16_t	 field1 = 0xFFFF;
	u_int16_t	 field2 = htons(0x01);

	buf_add(buf, &field1, sizeof(field1));
	buf_add(buf, &field2, sizeof(field2));
}

/* XXX add the support for key lifetime and rollover */
int
auth_validate(u_int8_t **buf, u_int16_t *len, struct iface *iface,
    struct nbr *nbr, struct nbr_failed *nbr_failed, u_int32_t *crypt_seq_num)
{
	MD5_CTX			 hash;
	u_int8_t		 digest[MD5_DIGEST_LENGTH];
	u_int8_t		 recv_digest[MD5_DIGEST_LENGTH];
	char			 pwd[MAX_SIMPLE_AUTH_LEN];
	struct rip_auth		*auth_head;
	struct md5_auth		*a;
	struct auth_md		*md;
	u_int8_t		*auth_data;
	u_int8_t		*b = *buf;

	*buf += RIP_HDR_LEN;
	*len -= RIP_HDR_LEN;

	auth_head = (struct rip_auth *)(*buf);

	if (auth_head->auth_fixed != AUTH) {
		if (iface->auth_type != AUTH_NONE) {
			log_debug("auth_validate: packet carrying no"
			    " authentication");
			return (-1);
		}
		return (0);
	} else {
		if (ntohs(auth_head->auth_type) !=
		    (u_int16_t)iface->auth_type) {
			log_debug("auth_validate: wrong auth type");
			return (-1);
		}
	}

	switch (iface->auth_type) {
	case AUTH_SIMPLE:
		bcopy(*buf+sizeof(*auth_head), pwd, MAX_SIMPLE_AUTH_LEN);
		if (bcmp(pwd, iface->auth_key, MAX_SIMPLE_AUTH_LEN)) {
			log_debug("auth_validate: wrong password, "
			    "interface: %s", iface->name);
			return (-1);
		}
		break;
	case AUTH_CRYPT:
		a = (struct md5_auth *)(*buf + sizeof(*auth_head));

		if ((md = md_list_find(&iface->auth_md_list,
		    a->auth_keyid)) == NULL) {
			log_debug("auth_validate: keyid %d not configured, "
			    "interface %s", a->auth_keyid,
			    iface->name);
			return (-1);
		}

		if (nbr != NULL) {
			if (ntohl(a->auth_seq) < nbr->auth_seq_num) {
				log_debug("auth_validate: decreasing seq num, "
				    "interface %s", iface->name);
				return (-1);
			}
		} else if (nbr_failed != NULL) {
			if (ntohl(a->auth_seq) < nbr_failed->auth_seq_num &&
			    ntohl(a->auth_seq)) {
				log_debug("auth_validate: decreasing seq num, "
				    "interface %s", iface->name);
				return (-1);
			}
		}

		/* XXX: maybe validate also the trailer header */
		if (a->auth_length != MD5_DIGEST_LENGTH + AUTH_TRLR_HDR_LEN) {
			log_debug("auth_validate: invalid key length, "
			    "interface %s", iface->name);
			return (-1);
		}

		auth_data = *buf;
		auth_data += ntohs(a->auth_offset);

		/* save the received digest and clear it in the packet */
		bcopy(auth_data, recv_digest, sizeof(recv_digest));
		bzero(auth_data, MD5_DIGEST_LENGTH);

		/* insert plaintext key */
		memcpy(digest, md->key, MD5_DIGEST_LENGTH);

		/* calculate MD5 digest */
		MD5Init(&hash);
		MD5Update(&hash, b, ntohs(a->auth_offset) + RIP_HDR_LEN);
		MD5Update(&hash, digest, MD5_DIGEST_LENGTH);
		MD5Final(digest, &hash);

		if (bcmp(recv_digest, digest, sizeof(digest))) {
			log_debug("auth_validate: invalid MD5 digest, "
			    "interface %s", iface->name);
			return (-1);
		}

		*crypt_seq_num = ntohl(a->auth_seq);

		*len -= AUTH_TRLR_HDR_LEN + MD5_DIGEST_LENGTH;

		break;
	default:
		log_debug("auth_validate: unknown auth type, interface %s",
		    iface->name);
		return (-1);
	}

	*buf += RIP_ENTRY_LEN;
	*len -= RIP_ENTRY_LEN;

	return (0);
}

int
auth_gen(struct buf *buf, struct iface *iface)
{
	struct rip_auth		 auth_head;
	struct md5_auth		 a;
	struct auth_md		 *md;

	auth_head.auth_fixed = AUTH;
	auth_head.auth_type = htons(iface->auth_type);

	buf_add(buf, &auth_head, sizeof(auth_head));

	switch (iface->auth_type) {
	case AUTH_SIMPLE:
		buf_add(buf, &iface->auth_key, MAX_SIMPLE_AUTH_LEN);
		break;
	case AUTH_CRYPT:
		if ((md = md_list_find(&iface->auth_md_list,
		    iface->auth_keyid)) == NULL) {
			log_debug("auth_gen: keyid %d not configured, "
			    "interface %s", iface->auth_keyid, iface->name);
			return (-1);
		}
		bzero(&a, sizeof(a));
		a.auth_keyid = iface->auth_keyid;
		a.auth_seq = htonl(auth_get_seq_num(md));
		a.auth_length = MD5_DIGEST_LENGTH + AUTH_TRLR_HDR_LEN;

		buf_add(buf, &a, sizeof(a));
		break;
	default:
		log_debug("auth_gen: unknown auth type, interface %s",
		    iface->name);
		return (-1);
	}

	return (0);
}

int
auth_add_trailer(struct buf *buf, struct iface *iface)
{
	MD5_CTX			 hash;
	u_int8_t		 digest[MD5_DIGEST_LENGTH];
	struct auth_md		*md;
	struct md5_auth		*a;
	int			 pos;

	pos = sizeof(struct rip_hdr) + sizeof(struct rip_auth);

	/* add offset to header */
	a = buf_seek(buf, pos, sizeof(*a));
	a->auth_offset = htons(buf->wpos);

	/* insert plaintext key */
	if ((md = md_list_find(&iface->auth_md_list,
	    iface->auth_keyid)) == NULL) {
		log_debug("auth_add_trailer: keyid %d not configured, "
		    "interface %s", iface->auth_keyid, iface->name);
			return (-1);
	}

	memcpy(digest, md->key, MD5_DIGEST_LENGTH);

	auth_trailer_header_gen(buf);

	/* calculate MD5 digest */
	MD5Init(&hash);
	MD5Update(&hash, buf->buf, buf->wpos);
	MD5Update(&hash, digest, MD5_DIGEST_LENGTH);
	MD5Final(digest, &hash);

	return (buf_add(buf, digest, MD5_DIGEST_LENGTH));
}

/* md list */
int
md_list_add(struct auth_md_head *head, u_int8_t keyid, char *key)
{
	struct auth_md	*md;

	if (strlen(key) > MD5_DIGEST_LENGTH)
		return (-1);

	if ((md = md_list_find(head, keyid)) != NULL) {
		/* update key */
		bzero(md->key, sizeof(md->key));
		memcpy(md->key, key, strlen(key));
		return (0);
	}

	if ((md = calloc(1, sizeof(struct auth_md))) == NULL)
		fatalx("md_list_add");

	md->keyid = keyid;
	memcpy(md->key, key, strlen(key));
	md->seq_modulator = auth_calc_modulator(md);
	TAILQ_INSERT_TAIL(head, md, entry);

	return (0);
}

void
md_list_copy(struct auth_md_head *to, struct auth_md_head *from)
{
	struct auth_md	*m, *md;

	TAILQ_INIT(to);

	TAILQ_FOREACH(m, from, entry) {
		if ((md = calloc(1, sizeof(struct auth_md))) == NULL)
			fatalx("md_list_copy");

		md->keyid = m->keyid;
		memcpy(md->key, m->key, sizeof(md->key));
		md->seq_modulator = m->seq_modulator;
		TAILQ_INSERT_TAIL(to, md, entry);
	}
}

void
md_list_clr(struct auth_md_head *head)
{
	struct auth_md	*m;

	while ((m = TAILQ_FIRST(head)) != NULL) {
		TAILQ_REMOVE(head, m, entry);
		free(m);
	}
}

struct auth_md *
md_list_find(struct auth_md_head *head, u_int8_t keyid)
{
	struct auth_md	*m;

	TAILQ_FOREACH(m, head, entry)
		if (m->keyid == keyid)
			return (m);

	return (NULL);
}
