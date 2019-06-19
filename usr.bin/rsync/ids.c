/*	$Id: ids.c,v 1.13 2019/05/08 21:30:11 benno Exp $ */
/*
 * Copyright (c) 2019 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include <assert.h>
#include <grp.h>
#include <inttypes.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

/*
 * Free a list of struct ident previously allocated with idents_gid_add().
 * Does nothing if the pointer is NULL.
 */
void
idents_free(struct ident *p, size_t sz)
{
	size_t	 i;

	if (p == NULL)
		return;
	for (i = 0; i < sz; i++)
		free(p[i].name);
	free(p);
}

/*
 * Given a list of files with the identifiers as set by the sender,
 * re-assign the identifiers from the list of remapped ones.
 * Don't ever remap wheel/root.
 * If we can't find the gid in the list (when, e.g., being sent by the
 * daemon), don't try to map it.
 */
void
idents_assign_gid(struct sess *sess, struct flist *fl, size_t flsz,
	const struct ident *ids, size_t idsz)
{
	size_t	 i, j;

	assert(!sess->opts->numeric_ids);

	for (i = 0; i < flsz; i++) {
		if (fl[i].st.gid == 0)
			continue;
		for (j = 0; j < idsz; j++)
			if ((int32_t)fl[i].st.gid == ids[j].id)
				break;
		if (j < idsz)
			fl[i].st.gid = ids[j].mapped;
	}
}

/*
 * Like idents_assign_gid().
 */
void
idents_assign_uid(struct sess *sess, struct flist *fl, size_t flsz,
	const struct ident *ids, size_t idsz)
{
	size_t	 i, j;

	assert(!sess->opts->numeric_ids);

	for (i = 0; i < flsz; i++) {
		if (fl[i].st.uid == 0)
			continue;
		for (j = 0; j < idsz; j++)
			if ((int32_t)fl[i].st.uid == ids[j].id)
				break;
		if (j < idsz)
			fl[i].st.uid = ids[j].mapped;
	}
}

/*
 * Given a list of identifiers from the remote host, fill in our local
 * identifiers of the same names.
 * Use the remote numeric identifier if we can't find the identifier OR the
 * identifier is zero (wheel/root).
 * FIXME: we should at least warn when we can't find an identifier, use
 * the numeric id, and that numeric id is assigned to a different user.
 */
void
idents_remap(struct sess *sess, int isgid, struct ident *ids, size_t idsz)
{
	size_t		 i;
	struct group	*grp;
	struct passwd	*usr;
	uint32_t	 id;
	int		valid;

	assert(!sess->opts->numeric_ids);

	for (i = 0; i < idsz; i++) {
		assert(ids[i].id != 0);

		/* Start by getting our local representation. */

		valid = id = 0;
		if (isgid) {
			grp = getgrnam(ids[i].name);
			if (grp) {
				id = grp->gr_gid;
				valid = 1;
			}
		} else {
			usr = getpwnam(ids[i].name);
			if (usr) {
				id = usr->pw_uid;
				valid = 1;
			}
		}

		/*
		 * (1) Empty names inherit.
		 * (2) Unknown identifier names inherit.
		 * (3) Wheel/root inherits.
		 * (4) Otherwise, use the local identifier.
		 */

		if (ids[i].name[0] == '\0')
			ids[i].mapped = ids[i].id;
		else if (!valid)
			ids[i].mapped = ids[i].id;
		else
			ids[i].mapped = id;

		LOG4("remapped identifier %s: %d -> %d",
		    ids[i].name, ids[i].id, ids[i].mapped);
	}
}

/*
 * If "id" is not part of the list of known users or groups (depending
 * upon "isgid", add it.
 * This also verifies that the name isn't too long.
 * Does nothing with user/group zero.
 * Return zero on failure, non-zero on success.
 */
int
idents_add(int isgid, struct ident **ids, size_t *idsz, int32_t id)
{
	struct group	*grp;
	struct passwd	*usr;
	size_t		 i, sz;
	void		*pp;
	const char	*name;

	if (id == 0)
		return 1;

	for (i = 0; i < *idsz; i++)
		if ((*ids)[i].id == id)
			return 1;

	/*
	 * Look up the reference in a type-specific way.
	 * Make sure that the name length is sane: we transmit it using
	 * a single byte.
	 */

	assert(i == *idsz);
	if (isgid) {
		if ((grp = getgrgid((gid_t)id)) == NULL) {
			ERR("%d: unknown gid", id);
			return 0;
		}
		name = grp->gr_name;
	} else {
		if ((usr = getpwuid((uid_t)id)) == NULL) {
			ERR("%d: unknown uid", id);
			return 0;
		}
		name = usr->pw_name;
	}

	if ((sz = strlen(name)) > UINT8_MAX) {
		ERRX("%d: name too long: %s", id, name);
		return 0;
	} else if (sz == 0) {
		ERRX("%d: zero-length name", id);
		return 0;
	}

	/* Add the identifier to the array. */

	pp = reallocarray(*ids, *idsz + 1, sizeof(struct ident));
	if (pp == NULL) {
		ERR("reallocarray");
		return 0;
	}
	*ids = pp;
	(*ids)[*idsz].id = id;
	(*ids)[*idsz].name = strdup(name);
	if ((*ids)[*idsz].name == NULL) {
		ERR("strdup");
		return 0;
	}

	LOG4("adding identifier to list: %s (%u)",
	    (*ids)[*idsz].name, (*ids)[*idsz].id);
	(*idsz)++;
	return 1;
}

/*
 * Send a list of struct ident.
 * See idents_recv().
 * We should only do this if we're preserving gids/uids.
 * Return zero on failure, non-zero on success.
 */
int
idents_send(struct sess *sess,
	int fd, const struct ident *ids, size_t idsz)
{
	size_t	 i, sz;

	for (i = 0; i < idsz; i++) {
		assert(ids[i].name != NULL);
		assert(ids[i].id != 0);
		sz = strlen(ids[i].name);
		assert(sz > 0 && sz <= UINT8_MAX);
		if (!io_write_uint(sess, fd, ids[i].id)) {
			ERRX1("io_write_uint");
			return 0;
		} else if (!io_write_byte(sess, fd, sz)) {
			ERRX1("io_write_byte");
			return 0;
		} else if (!io_write_buf(sess, fd, ids[i].name, sz)) {
			ERRX1("io_write_buf");
			return 0;
		}
	}

	if (!io_write_int(sess, fd, 0)) {
		ERRX1("io_write_int");
		return 0;
	}

	return 1;
}

/*
 * Receive a list of struct ident.
 * See idents_send().
 * We should only do this if we're preserving gids/uids.
 * Return zero on failure, non-zero on success.
 */
int
idents_recv(struct sess *sess,
	int fd, struct ident **ids, size_t *idsz)
{
	int32_t	 id;
	uint8_t	 sz;
	void	*pp;

	for (;;) {
		if (!io_read_uint(sess, fd, &id)) {
			ERRX1("io_read_uint");
			return 0;
		} else if (id == 0)
			break;

		pp = reallocarray(*ids,
			*idsz + 1, sizeof(struct ident));
		if (pp == NULL) {
			ERR("reallocarray");
			return 0;
		}
		*ids = pp;
		memset(&(*ids)[*idsz], 0, sizeof(struct ident));

		/*
		 * When reading the size, warn if we get a size of zero.
		 * The spec doesn't allow this, but we might have a
		 * noncomformant or adversarial sender.
		 */

		if (!io_read_byte(sess, fd, &sz)) {
			ERRX1("io_read_byte");
			return 0;
		} else if (sz == 0)
			WARNX("zero-length name in identifier list");

		(*ids)[*idsz].id = id;
		(*ids)[*idsz].name = calloc(sz + 1, 1);
		if ((*ids)[*idsz].name == NULL) {
			ERR("calloc");
			return 0;
		}
		if (!io_read_buf(sess, fd, (*ids)[*idsz].name, sz)) {
			ERRX1("io_read_buf");
			return 0;
		}
		(*idsz)++;
	}

	return 1;
}
