/*
 * Copyright (C) Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: compress.c,v 1.5 2020/02/18 18:11:27 florian Exp $ */

/*! \file */

#include <stdlib.h>

#include <isc/util.h>

#include <dns/compress.h>
#include <dns/fixedname.h>

/***
 ***	Compression
 ***/

isc_result_t
dns_compress_init(dns_compress_t *cctx, int edns) {
	unsigned int i;

	REQUIRE(cctx != NULL);

	cctx->allowed = 0;
	cctx->edns = edns;
	for (i = 0; i < DNS_COMPRESS_TABLESIZE; i++)
		cctx->table[i] = NULL;
	cctx->count = 0;
	return (ISC_R_SUCCESS);
}

void
dns_compress_invalidate(dns_compress_t *cctx) {
	dns_compressnode_t *node;
	unsigned int i;

	for (i = 0; i < DNS_COMPRESS_TABLESIZE; i++) {
		while (cctx->table[i] != NULL) {
			node = cctx->table[i];
			cctx->table[i] = cctx->table[i]->next;
			if (node->count < DNS_COMPRESS_INITIALNODES)
				continue;
			free(node);
		}
	}
	cctx->allowed = 0;
	cctx->edns = -1;
}

void
dns_compress_setmethods(dns_compress_t *cctx, unsigned int allowed) {
	cctx->allowed &= ~DNS_COMPRESS_ALL;
	cctx->allowed |= (allowed & DNS_COMPRESS_ALL);
}

unsigned int
dns_compress_getmethods(dns_compress_t *cctx) {
	return (cctx->allowed & DNS_COMPRESS_ALL);
}

#define NODENAME(node, name) \
do { \
	(name)->length = (node)->r.length; \
	(name)->labels = (node)->labels; \
	(name)->ndata = (node)->r.base; \
	(name)->attributes = DNS_NAMEATTR_ABSOLUTE; \
} while (0)

/*
 * Find the longest match of name in the table.
 * If match is found return ISC_TRUE. prefix, suffix and offset are updated.
 * If no match is found return ISC_FALSE.
 */
isc_boolean_t
dns_compress_findglobal(dns_compress_t *cctx, const dns_name_t *name,
			dns_name_t *prefix, uint16_t *offset)
{
	dns_name_t tname, nname;
	dns_compressnode_t *node = NULL;
	unsigned int labels, hash, n;

	REQUIRE(dns_name_isabsolute(name) == ISC_TRUE);
	REQUIRE(offset != NULL);

	if (cctx->count == 0)
		return (ISC_FALSE);

	labels = dns_name_countlabels(name);
	INSIST(labels > 0);

	dns_name_init(&tname, NULL);
	dns_name_init(&nname, NULL);

	for (n = 0; n < labels - 1; n++) {
		dns_name_getlabelsequence(name, n, labels - n, &tname);
		hash = dns_name_hash(&tname, ISC_FALSE) %
		       DNS_COMPRESS_TABLESIZE;
		for (node = cctx->table[hash]; node != NULL; node = node->next)
		{
			NODENAME(node, &nname);
			if ((cctx->allowed & DNS_COMPRESS_CASESENSITIVE) != 0) {
				if (dns_name_caseequal(&nname, &tname))
					break;
			} else {
				if (dns_name_equal(&nname, &tname))
					break;
			}
		}
		if (node != NULL)
			break;
	}

	/*
	 * If node == NULL, we found no match at all.
	 */
	if (node == NULL)
		return (ISC_FALSE);

	if (n == 0)
		dns_name_reset(prefix);
	else
		dns_name_getlabelsequence(name, 0, n, prefix);

	*offset = node->offset;
	return (ISC_TRUE);
}

void
dns_compress_add(dns_compress_t *cctx, const dns_name_t *name,
		 const dns_name_t *prefix, uint16_t offset)
{
	dns_name_t tname;
	unsigned int start;
	unsigned int n;
	unsigned int count;
	unsigned int hash;
	dns_compressnode_t *node;
	unsigned int length;
	unsigned int tlength;
	uint16_t toffset;

	REQUIRE(dns_name_isabsolute(name));

	dns_name_init(&tname, NULL);

	n = dns_name_countlabels(name);
	count = dns_name_countlabels(prefix);
	if (dns_name_isabsolute(prefix))
		count--;
	start = 0;
	length = name->length;
	while (count > 0) {
		if (offset >= 0x4000)
			break;
		dns_name_getlabelsequence(name, start, n, &tname);
		hash = dns_name_hash(&tname, ISC_FALSE) %
		       DNS_COMPRESS_TABLESIZE;
		tlength = tname.length;
		toffset = (uint16_t)(offset + (length - tlength));
		/*
		 * Create a new node and add it.
		 */
		if (cctx->count < DNS_COMPRESS_INITIALNODES)
			node = &cctx->initialnodes[cctx->count];
		else {
			node = malloc(sizeof(dns_compressnode_t));
			if (node == NULL)
				return;
		}
		node->count = cctx->count++;
		node->offset = toffset;
		dns_name_toregion(&tname, &node->r);
		node->labels = (uint8_t)dns_name_countlabels(&tname);
		node->next = cctx->table[hash];
		cctx->table[hash] = node;
		start++;
		n--;
		count--;
	}
}

void
dns_compress_rollback(dns_compress_t *cctx, uint16_t offset) {
	unsigned int i;
	dns_compressnode_t *node;

	for (i = 0; i < DNS_COMPRESS_TABLESIZE; i++) {
		node = cctx->table[i];
		/*
		 * This relies on nodes with greater offsets being
		 * closer to the beginning of the list, and the
		 * items with the greatest offsets being at the end
		 * of the initialnodes[] array.
		 */
		while (node != NULL && node->offset >= offset) {
			cctx->table[i] = node->next;
			if (node->count >= DNS_COMPRESS_INITIALNODES)
				free(node);
			cctx->count--;
			node = cctx->table[i];
		}
	}
}

/***
 ***	Decompression
 ***/

void
dns_decompress_init(dns_decompress_t *dctx, int edns,
		    dns_decompresstype_t type) {

	REQUIRE(dctx != NULL);
	REQUIRE(edns >= -1 && edns <= 255);

	dctx->allowed = DNS_COMPRESS_NONE;
	dctx->edns = edns;
	dctx->type = type;
}

void
dns_decompress_setmethods(dns_decompress_t *dctx, unsigned int allowed) {
	switch (dctx->type) {
	case DNS_DECOMPRESS_ANY:
		dctx->allowed = DNS_COMPRESS_ALL;
		break;
	case DNS_DECOMPRESS_NONE:
		dctx->allowed = DNS_COMPRESS_NONE;
		break;
	case DNS_DECOMPRESS_STRICT:
		dctx->allowed = allowed;
		break;
	}
}
