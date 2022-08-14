/*	$OpenBSD: generate.c,v 1.5 2022/08/14 14:54:13 millert Exp $ */

/*
 * Copyright (c) 2017 Martin Pieuchot
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
#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/ctf.h>

#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

#ifdef ZLIB
#include <zlib.h>
#endif /* ZLIB */

#include "itype.h"
#include "xmalloc.h"
#include "hash.h"

#define ROUNDUP(x, y) ((((x) + (y) - 1) / (y)) * (y))

/*
 * Dynamic buffer, used for content & string table.
 */
struct dbuf {
	char		*data;	/* start data buffer */
	size_t		 size;	/* size of the buffer */

	char		*cptr; /* position in [data, data + size] */
	size_t		 coff; /* number of written bytes */
};

#define DBUF_CHUNKSZ	(64 * 1024)

/* In-memory representation of a CTF section. */
struct imcs {
	struct dbuf	 body;
	struct dbuf	 stab;	/* corresponding string table */
	struct hash	*htab;	/* hash table of known strings */
};

struct strentry {
	struct hash_entry se_key;	/* Must be first */
#define se_str se_key.hkey
	size_t		 se_off;
};

#ifdef ZLIB
char		*data_compress(const char *, size_t, size_t, size_t *);
#endif /* ZLIB */

void
dbuf_realloc(struct dbuf *dbuf, size_t len)
{
	assert(dbuf != NULL);
	assert(len != 0);

	dbuf->data = xrealloc(dbuf->data, dbuf->size + len);
	dbuf->size += len;
	dbuf->cptr = dbuf->data + dbuf->coff;
}

void
dbuf_copy(struct dbuf *dbuf, void const *data, size_t len)
{
	size_t left;

	assert(dbuf->cptr != NULL);
	assert(dbuf->data != NULL);
	assert(dbuf->size != 0);

	if (len == 0)
		return;

	left = dbuf->size - dbuf->coff;
	if (left < len)
		dbuf_realloc(dbuf, ROUNDUP((len - left), DBUF_CHUNKSZ));

	memcpy(dbuf->cptr, data, len);
	dbuf->cptr += len;
	dbuf->coff += len;
}

size_t
dbuf_pad(struct dbuf *dbuf, int align)
{
	int i = (align - (dbuf->coff % align)) % align;

	while (i-- > 0)
		dbuf_copy(dbuf, "", 1);

	return dbuf->coff;
}

size_t
imcs_add_string(struct imcs *imcs, const char *str)
{
	struct strentry *se;
	unsigned int slot;

	if (str == NULL || *str == '\0')
		return 0;

	se = (struct strentry *)hash_find(imcs->htab, str, &slot);
	if (se == NULL) {
		se = xmalloc(sizeof(*se));
		hash_insert(imcs->htab, slot, &se->se_key, str);
		se->se_off = imcs->stab.coff;

		dbuf_copy(&imcs->stab, str, strlen(str) + 1);
	}

	return se->se_off;
}

void
imcs_add_func(struct imcs *imcs, struct itype *it)
{
	uint16_t		 func, arg;
	struct imember		*im;
	int			 kind, root, vlen;

	vlen = it->it_nelems;
	kind = it->it_type;
	root = 0;

	func = (kind << 11) | (root << 10) | (vlen & CTF_MAX_VLEN);
	dbuf_copy(&imcs->body, &func, sizeof(func));

	if (kind == CTF_K_UNKNOWN)
		return;

	func = it->it_refp->it_idx;
	dbuf_copy(&imcs->body, &func, sizeof(func));

	TAILQ_FOREACH(im, &it->it_members, im_next) {
		arg = im->im_refp->it_idx;
		dbuf_copy(&imcs->body, &arg, sizeof(arg));
	}
}

void
imcs_add_obj(struct imcs *imcs, struct itype *it)
{
	uint16_t		 type;

	type = it->it_refp->it_idx;
	dbuf_copy(&imcs->body, &type, sizeof(type));
}

void
imcs_add_type(struct imcs *imcs, struct itype *it)
{
	struct imember		*im;
	struct ctf_type		 ctt;
	struct ctf_array	 cta;
	unsigned int		 eob;
	uint32_t		 size;
	uint16_t		 arg;
	size_t			 ctsz;
	int			 kind, root, vlen;

	assert(it->it_type != CTF_K_UNKNOWN && it->it_type != CTF_K_FORWARD);

	vlen = it->it_nelems;
	size = it->it_size;
	kind = it->it_type;
	root = 0;

	ctt.ctt_name = imcs_add_string(imcs, it_name(it));
	ctt.ctt_info = (kind << 11) | (root << 10) | (vlen & CTF_MAX_VLEN);

	/* Base types don't have reference, typedef & pointer don't have size */
	if (it->it_refp != NULL && kind != CTF_K_ARRAY) {
		ctt.ctt_type = it->it_refp->it_idx;
		ctsz = sizeof(struct ctf_stype);
	} else if (size <= CTF_MAX_SIZE) {
		ctt.ctt_size = size;
		ctsz = sizeof(struct ctf_stype);
	} else {
		ctt.ctt_lsizehi = CTF_SIZE_TO_LSIZE_HI(size);
		ctt.ctt_lsizelo = CTF_SIZE_TO_LSIZE_LO(size);
		ctt.ctt_size = CTF_LSIZE_SENT;
		ctsz = sizeof(struct ctf_type);
	}

	dbuf_copy(&imcs->body, &ctt, ctsz);

	switch (kind) {
		assert(1 == 0);
		break;
	case CTF_K_INTEGER:
	case CTF_K_FLOAT:
		eob = CTF_INT_DATA(it->it_enc, 0, size);
		dbuf_copy(&imcs->body, &eob, sizeof(eob));
		break;
	case CTF_K_ARRAY:
		memset(&cta, 0, sizeof(cta));
		cta.cta_contents = it->it_refp->it_idx;
		cta.cta_index = long_tidx;
		cta.cta_nelems = it->it_nelems;
		dbuf_copy(&imcs->body, &cta, sizeof(cta));
		break;
	case CTF_K_STRUCT:
	case CTF_K_UNION:
		if (size < CTF_LSTRUCT_THRESH) {
			struct ctf_member	 ctm;

			memset(&ctm, 0, sizeof(ctm));
			TAILQ_FOREACH(im, &it->it_members, im_next) {
				ctm.ctm_name =
				    imcs_add_string(imcs, im_name(im));
				ctm.ctm_type = im->im_refp->it_idx;
				ctm.ctm_offset = im->im_off;

				dbuf_copy(&imcs->body, &ctm, sizeof(ctm));
			}
		} else {
			struct ctf_lmember	 ctlm;

			memset(&ctlm, 0, sizeof(ctlm));
			TAILQ_FOREACH(im, &it->it_members, im_next) {
				ctlm.ctlm_name =
				    imcs_add_string(imcs, im_name(im));
				ctlm.ctlm_type = im->im_refp->it_idx;
				ctlm.ctlm_offsethi =
				    CTF_OFFSET_TO_LMEMHI(im->im_off);
				ctlm.ctlm_offsetlo =
				    CTF_OFFSET_TO_LMEMLO(im->im_off);


				dbuf_copy(&imcs->body, &ctlm, sizeof(ctlm));
			}
		}
		break;
	case CTF_K_FUNCTION:
		TAILQ_FOREACH(im, &it->it_members, im_next) {
			arg = im->im_refp->it_idx;
			dbuf_copy(&imcs->body, &arg, sizeof(arg));
		}
		if (vlen & 1) {
			arg = 0;
			dbuf_copy(&imcs->body, &arg, sizeof(arg));
		}
		break;
	case CTF_K_ENUM:
		TAILQ_FOREACH(im, &it->it_members, im_next) {
			struct ctf_enum	cte;

			cte.cte_name = imcs_add_string(imcs, im_name(im));
			cte.cte_value = im->im_ref;

			dbuf_copy(&imcs->body, &cte, sizeof(cte));
		}
		break;
	case CTF_K_POINTER:
	case CTF_K_TYPEDEF:
	case CTF_K_VOLATILE:
	case CTF_K_CONST:
	case CTF_K_RESTRICT:
	default:
		break;
	}
}

void
imcs_generate(struct imcs *imcs, struct ctf_header *cth, const char *label)
{
	struct itype		*it;
	struct ctf_lblent	 lbl;

	memset(imcs, 0, sizeof(*imcs));

	dbuf_realloc(&imcs->body, DBUF_CHUNKSZ);
	dbuf_realloc(&imcs->stab, DBUF_CHUNKSZ);

	imcs->htab = hash_init(10);
	if (imcs->htab == NULL)
		err(1, "hash_init");

	/* Add empty string */
	dbuf_copy(&imcs->stab, "", 1);

	/* We don't use parent label */
	cth->cth_parlabel = 0;
	cth->cth_parname = 0;

	/* Insert a single label for all types. */
	cth->cth_lbloff = 0;
	lbl.ctl_label = imcs_add_string(imcs, label);
	lbl.ctl_typeidx = tidx;
	dbuf_copy(&imcs->body, &lbl, sizeof(lbl));

	/* Insert objects */
	cth->cth_objtoff = dbuf_pad(&imcs->body, 2);
	TAILQ_FOREACH(it, &iobjq, it_symb)
		imcs_add_obj(imcs, it);

	/* Insert functions */
	cth->cth_funcoff = dbuf_pad(&imcs->body, 2);
	TAILQ_FOREACH(it, &ifuncq, it_symb)
		imcs_add_func(imcs, it);

	/* Insert types */
	cth->cth_typeoff = dbuf_pad(&imcs->body, 4);
	TAILQ_FOREACH(it, &itypeq, it_next) {
		if (it->it_flags & (ITF_FUNC|ITF_OBJ))
			continue;

		imcs_add_type(imcs, it);
	}

	/* String table is written from its own buffer. */
	cth->cth_stroff = imcs->body.coff;
	cth->cth_strlen = imcs->stab.coff;
}

/*
 * Generate a CTF buffer from the internal type representation.
 */
int
generate(const char *path, const char *label, int compress)
{
	char			*p, *ctfdata = NULL;
	ssize_t			 ctflen;
	struct ctf_header	 cth;
	struct imcs		 imcs;
	int			 error = 0, fd;

	memset(&cth, 0, sizeof(cth));

	cth.cth_magic = CTF_MAGIC;
	cth.cth_version = CTF_VERSION;

#ifdef ZLIB
	if (compress)
		cth.cth_flags = CTF_F_COMPRESS;
#endif /* ZLIB */

	imcs_generate(&imcs, &cth, label);

	ctflen = sizeof(cth) + imcs.body.coff + imcs.stab.coff;
	p = ctfdata = xmalloc(ctflen);

	memcpy(p, &cth, sizeof(cth));
	p += sizeof(cth);

	memcpy(p, imcs.body.data, imcs.body.coff);
	p += imcs.body.coff;

	memcpy(p, imcs.stab.data, imcs.stab.coff);
	p += imcs.stab.coff;

	assert((p - ctfdata) == ctflen);

#ifdef ZLIB
	if (compress) {
		char *cdata;
		size_t clen;

		cdata = data_compress(ctfdata + sizeof(cth),
		    ctflen - sizeof(cth), ctflen - sizeof(cth), &clen);
		if (cdata == NULL) {
			warnx("compressing CTF data");
			free(ctfdata);
			return -1;
		}

		memcpy(ctfdata + sizeof(cth), cdata, clen);
		ctflen = clen + sizeof(cth);

		free(cdata);
	}
#endif /* ZLIB */

	fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd == -1) {
		warn("open %s", path);
		free(ctfdata);
		return -1;
	}

	if (write(fd, ctfdata, ctflen) != ctflen) {
		warn("unable to write %zd bytes for %s", ctflen, path);
		error = -1;
	}

	close(fd);
	free(ctfdata);
	return error;
}

#ifdef ZLIB
char *
data_compress(const char *buf, size_t size, size_t len, size_t *pclen)
{
	z_stream		 stream;
	char			*data;
	int			 error;

	data = malloc(len);
	if (data == NULL) {
		warn(NULL);
		return NULL;
	}

	memset(&stream, 0, sizeof(stream));
	stream.zalloc = Z_NULL;
	stream.zfree = Z_NULL;
	stream.opaque = Z_NULL;

	if ((error = deflateInit(&stream, Z_BEST_COMPRESSION)) != Z_OK) {
		warnx("zlib deflateInit failed: %s", zError(error));
		goto exit;
	}

	stream.next_in = (void *)buf;
	stream.avail_in = size;
	stream.next_out = (unsigned char *)data;
	stream.avail_out = len;

	if ((error = deflate(&stream, Z_FINISH)) != Z_STREAM_END) {
		warnx("zlib deflate failed: %s", zError(error));
		deflateEnd(&stream);
		goto exit;
	}

	if ((error = deflateEnd(&stream)) != Z_OK) {
		warnx("zlib deflateEnd failed: %s", zError(error));
		goto exit;
	}

	if (pclen != NULL)
		*pclen = stream.total_out;

	return data;

exit:
	free(data);
	return NULL;
}
#endif /* ZLIB */
