/*	$OpenBSD: nca.c,v 1.1 2026/06/22 21:25:44 job Exp $ */
/*
 * Copyright (c) 2025 Theo Buehler <tb@openbsd.org>
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

#include <err.h>
#include <string.h>

#include "extern.h"

/*
 * Add each CA cert into the non-functional CA tree.
 */
void
nca_tree_insert_cert(struct nca_tree *tree, const struct cert *cert)
{
	struct nonfunc_ca *nca;

	if ((nca = calloc(1, sizeof(*nca))) == NULL)
		err(1, NULL);
	if ((nca->location = strdup(cert->path)) == NULL)
		err(1, NULL);
	if ((nca->carepo = strdup(cert->repo)) == NULL)
		err(1, NULL);
	if ((nca->mfturi = strdup(cert->mft)) == NULL)
		err(1, NULL);
	if ((nca->ski = strdup(cert->ski)) == NULL)
		err(1, NULL);
	nca->repoid = cert->repoid;
	nca->certid = cert->certid;
	nca->talid = cert->talid;

	if (RB_INSERT(nca_tree, tree, nca) != NULL)
		errx(1, "non-functional CA tree corrupted");
}

void
nca_tree_remove_cert(struct nca_tree *tree, int cid)
{
	struct nonfunc_ca *found, needle = { .certid = cid };

	if ((found = RB_FIND(nca_tree, tree, &needle)) != NULL) {
		RB_REMOVE(nca_tree, tree, found);
		free(found->location);
		free(found->carepo);
		free(found->mfturi);
		free(found->ski);
		free(found);
	}
}

static inline int
certidcmp(const struct nonfunc_ca *a, const struct nonfunc_ca *b)
{
	if (a->certid < b->certid)
		return -1;
	if (a->certid > b->certid)
		return 1;

	return 0;
}

RB_GENERATE(nca_tree, nonfunc_ca, entry, certidcmp);
