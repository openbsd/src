/*
 * Portions Copyright (C) 2000, 2001  Internet Software Consortium.
 * Portions Copyright (C) 1995-2000 by Network Associates, Inc.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM AND
 * NETWORK ASSOCIATES DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE CONSORTIUM OR NETWORK
 * ASSOCIATES BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $ISC: dnssec-makekeyset.c,v 1.52.2.1 2001/10/05 00:21:45 bwelling Exp $ */

#include <config.h>

#include <stdlib.h>

#include <isc/commandline.h>
#include <isc/entropy.h>
#include <isc/mem.h>
#include <isc/string.h>
#include <isc/util.h>

#include <dns/db.h>
#include <dns/dnssec.h>
#include <dns/fixedname.h>
#include <dns/log.h>
#include <dns/rdata.h>
#include <dns/rdatalist.h>
#include <dns/rdataset.h>
#include <dns/result.h>
#include <dns/secalg.h>
#include <dns/time.h>

#include <dst/dst.h>

#include "dnssectool.h"

#define BUFSIZE 2048

const char *program = "dnssec-makekeyset";
int verbose;

typedef struct keynode keynode_t;
struct keynode {
	dst_key_t *key;
	ISC_LINK(keynode_t) link;
};
typedef ISC_LIST(keynode_t) keylist_t;

static isc_stdtime_t starttime = 0, endtime = 0, now;
static int ttl = -1;

static isc_mem_t *mctx = NULL;
static isc_entropy_t *ectx = NULL;

static keylist_t keylist;

static void
usage(void) {
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "\t%s [options] keys\n", program);

	fprintf(stderr, "\n");

	fprintf(stderr, "Options: (default value in parenthesis) \n");
	fprintf(stderr, "\t-a\n");
	fprintf(stderr, "\t\tverify generated signatures\n");
	fprintf(stderr, "\t-s YYYYMMDDHHMMSS|+offset:\n");
	fprintf(stderr, "\t\tSIG start time - absolute|offset (now)\n");
	fprintf(stderr, "\t-e YYYYMMDDHHMMSS|+offset|\"now\"+offset]:\n");
	fprintf(stderr, "\t\tSIG end time  - "
			     "absolute|from start|from now (now + 30 days)\n");
	fprintf(stderr, "\t-t ttl\n");
	fprintf(stderr, "\t-p\n");
	fprintf(stderr, "\t\tuse pseudorandom data (faster but less secure)\n");
	fprintf(stderr, "\t-r randomdev:\n");
	fprintf(stderr, "\t\ta file containing random data\n");
	fprintf(stderr, "\t-v level:\n");
	fprintf(stderr, "\t\tverbose level (0)\n");

	fprintf(stderr, "\n");

	fprintf(stderr, "keys:\n");
	fprintf(stderr, "\tkeyfile (Kname+alg+tag)\n");

	fprintf(stderr, "\n");

	fprintf(stderr, "Output:\n");
	fprintf(stderr, "\tkeyset (keyset-<name>)\n");
	exit(0);
}

static isc_boolean_t
zonekey_on_list(dst_key_t *key) {
	keynode_t *keynode;
	for (keynode = ISC_LIST_HEAD(keylist);
	     keynode != NULL;
	     keynode = ISC_LIST_NEXT(keynode, link))
	{
		if (dst_key_compare(keynode->key, key))
			return (ISC_TRUE);
	}
	return (ISC_FALSE);
}

static isc_boolean_t
rdata_on_list(dns_rdata_t *rdata, dns_rdatalist_t *list) {
	dns_rdata_t *trdata;
	for (trdata = ISC_LIST_HEAD(list->rdata);
	     trdata != NULL;
	     trdata = ISC_LIST_NEXT(trdata, link))
	{
		if (dns_rdata_compare(trdata, rdata) == 0)
			return (ISC_TRUE);
	}
	return (ISC_FALSE);
}

int
main(int argc, char *argv[]) {
	int i, ch;
	char *startstr = NULL, *endstr = NULL;
	char *randomfile = NULL;
	dns_fixedname_t fdomain;
	dns_name_t *domain = NULL;
	char *output = NULL;
	char *endp;
	unsigned char *data;
	dns_db_t *db;
	dns_dbnode_t *node;
	dns_dbversion_t *version;
	dst_key_t *key = NULL;
	dns_rdata_t *rdata;
	dns_rdatalist_t rdatalist, sigrdatalist;
	dns_rdataset_t rdataset, sigrdataset;
	isc_result_t result;
	isc_buffer_t b;
	isc_region_t r;
	isc_log_t *log = NULL;
	keynode_t *keynode;
	dns_name_t *savedname = NULL;
	unsigned int eflags;
	isc_boolean_t pseudorandom = ISC_FALSE;
	isc_boolean_t tryverify = ISC_FALSE;

	result = isc_mem_create(0, 0, &mctx);
	if (result != ISC_R_SUCCESS)
		fatal("failed to create memory context: %s",
		      isc_result_totext(result));

	dns_result_register();

	while ((ch = isc_commandline_parse(argc, argv, "as:e:t:r:v:ph")) != -1)
	{
		switch (ch) {
		case 'a':
			tryverify = ISC_TRUE;
			break;
		case 's':
			startstr = isc_commandline_argument;
			break;

		case 'e':
			endstr = isc_commandline_argument;
			break;

		case 't':
			endp = NULL;
			ttl = strtol(isc_commandline_argument, &endp, 0);
			if (*endp != '\0')
				fatal("TTL must be numeric");
			break;

		case 'r':
			randomfile = isc_commandline_argument;
			break;

		case 'v':
			endp = NULL;
			verbose = strtol(isc_commandline_argument, &endp, 0);
			if (*endp != '\0')
				fatal("verbose level must be numeric");
			break;

		case 'p':
			pseudorandom = ISC_TRUE;
			break;

		case 'h':
		default:
			usage();

		}
	}

	argc -= isc_commandline_index;
	argv += isc_commandline_index;

	if (argc < 1)
		usage();

	setup_entropy(mctx, randomfile, &ectx);
	eflags = ISC_ENTROPY_BLOCKING;
	if (!pseudorandom)
		eflags |= ISC_ENTROPY_GOODONLY;
	result = dst_lib_init(mctx, ectx, eflags);
	if (result != ISC_R_SUCCESS)
		fatal("could not initialize dst: %s",
		      isc_result_totext(result));

	isc_stdtime_get(&now);

	if (startstr != NULL)
		starttime = strtotime(startstr, now, now);
	else
		starttime = now;

	if (endstr != NULL)
		endtime = strtotime(endstr, now, starttime);
	else
		endtime = starttime + (30 * 24 * 60 * 60);

	if (ttl == -1) {
		ttl = 3600;
		fprintf(stderr, "%s: TTL not specified, assuming 3600\n",
			program);
	}

	setup_logging(verbose, mctx, &log);

	dns_rdatalist_init(&rdatalist);
	rdatalist.rdclass = 0;
	rdatalist.type = dns_rdatatype_key;
	rdatalist.covers = 0;
	rdatalist.ttl = ttl;

	ISC_LIST_INIT(keylist);

	for (i = 0; i < argc; i++) {
		char namestr[DNS_NAME_FORMATSIZE];
		isc_buffer_t namebuf;

		key = NULL;
		result = dst_key_fromnamedfile(argv[i], DST_TYPE_PUBLIC,
					       mctx, &key);
		if (result != ISC_R_SUCCESS)
			fatal("error loading key from %s: %s", argv[i],
			      isc_result_totext(result));
		if (rdatalist.rdclass == 0)
			rdatalist.rdclass = dst_key_class(key);

		isc_buffer_init(&namebuf, namestr, sizeof namestr);
		result = dns_name_tofilenametext(dst_key_name(key),
						 ISC_FALSE,
						 &namebuf);
		check_result(result, "dns_name_tofilenametext");
		isc_buffer_putuint8(&namebuf, 0);
		
		if (savedname == NULL) {
			savedname = isc_mem_get(mctx, sizeof(dns_name_t));
			if (savedname == NULL)
				fatal("out of memory");
			dns_name_init(savedname, NULL);
			result = dns_name_dup(dst_key_name(key), mctx,
					      savedname);
			if (result != ISC_R_SUCCESS)
				fatal("out of memory");
		} else {
			char savednamestr[DNS_NAME_FORMATSIZE];
			dns_name_format(savedname, savednamestr,
					sizeof savednamestr);
			if (!dns_name_equal(savedname, dst_key_name(key)) != 0)
				fatal("all keys must have the same owner - %s "
				      "and %s do not match",
				      savednamestr, namestr);
		}
		if (output == NULL) {
			output = isc_mem_allocate(mctx,
						  strlen("keyset-") +
						  strlen(namestr) + 1);
			if (output == NULL)
				fatal("out of memory");
			strcpy(output, "keyset-");
			strcat(output, namestr);
		}
		if (domain == NULL) {
			dns_fixedname_init(&fdomain);
			domain = dns_fixedname_name(&fdomain);
			dns_name_copy(dst_key_name(key), domain, NULL);
		}
		if (dst_key_iszonekey(key)) {
			dst_key_t *zonekey = NULL;
			result = dst_key_fromnamedfile(argv[i],
						       DST_TYPE_PUBLIC |
						       DST_TYPE_PRIVATE,
						       mctx, &zonekey);
			if (result != ISC_R_SUCCESS)
				fatal("failed to read private key %s: %s",
				      argv[i], isc_result_totext(result));
			if (!zonekey_on_list(zonekey)) {
				keynode = isc_mem_get(mctx,
						      sizeof (keynode_t));
				if (keynode == NULL)
					fatal("out of memory");
				keynode->key = zonekey;
				ISC_LIST_INITANDAPPEND(keylist, keynode, link);
			} else
				dst_key_free(&zonekey);
		}
		rdata = isc_mem_get(mctx, sizeof(dns_rdata_t));
		if (rdata == NULL)
			fatal("out of memory");
		dns_rdata_init(rdata);
		data = isc_mem_get(mctx, BUFSIZE);
		if (data == NULL)
			fatal("out of memory");
		isc_buffer_init(&b, data, BUFSIZE);
		result = dst_key_todns(key, &b);
		if (result != ISC_R_SUCCESS)
			fatal("failed to convert key %s to a DNS KEY: %s",
			      argv[i], isc_result_totext(result));
		isc_buffer_usedregion(&b, &r);
		dns_rdata_fromregion(rdata, rdatalist.rdclass,
				     dns_rdatatype_key, &r);
		if (!rdata_on_list(rdata, &rdatalist))
			ISC_LIST_APPEND(rdatalist.rdata, rdata, link);
		else {
			isc_mem_put(mctx, data, BUFSIZE);
			isc_mem_put(mctx, rdata, sizeof *rdata);
		}
		dst_key_free(&key);
	}

	dns_rdataset_init(&rdataset);
	result = dns_rdatalist_tordataset(&rdatalist, &rdataset);
	check_result(result, "dns_rdatalist_tordataset()");

	dns_rdatalist_init(&sigrdatalist);
	sigrdatalist.rdclass = rdatalist.rdclass;
	sigrdatalist.type = dns_rdatatype_sig;
	sigrdatalist.covers = dns_rdatatype_key;
	sigrdatalist.ttl = ttl;

	if (ISC_LIST_EMPTY(keylist))
		fprintf(stderr,
			"%s: no private zone key found; not self-signing\n",
			program);
	for (keynode = ISC_LIST_HEAD(keylist);
	     keynode != NULL;
	     keynode = ISC_LIST_NEXT(keynode, link))
	{
		rdata = isc_mem_get(mctx, sizeof(dns_rdata_t));
		if (rdata == NULL)
			fatal("out of memory");
		dns_rdata_init(rdata);
		data = isc_mem_get(mctx, BUFSIZE);
		if (data == NULL)
			fatal("out of memory");
		isc_buffer_init(&b, data, BUFSIZE);
		result = dns_dnssec_sign(domain, &rdataset, keynode->key,
					 &starttime, &endtime, mctx, &b,
					 rdata);
		isc_entropy_stopcallbacksources(ectx);
		if (result != ISC_R_SUCCESS) {
			char keystr[KEY_FORMATSIZE];
			key_format(keynode->key, keystr, sizeof keystr);
			fatal("failed to sign keyset with key %s: %s",
			      keystr, isc_result_totext(result));
		}
		if (tryverify) {
			result = dns_dnssec_verify(domain, &rdataset,
						   keynode->key, ISC_TRUE,
						   mctx, rdata);
			if (result != ISC_R_SUCCESS) {
				char keystr[KEY_FORMATSIZE];
				key_format(keynode->key, keystr, sizeof keystr);
				fatal("signature from key '%s' failed to "
				      "verify: %s",
				      keystr, isc_result_totext(result));
			}
		}
		ISC_LIST_APPEND(sigrdatalist.rdata, rdata, link);
		dns_rdataset_init(&sigrdataset);
		result = dns_rdatalist_tordataset(&sigrdatalist, &sigrdataset);
		check_result(result, "dns_rdatalist_tordataset()");
	}

	db = NULL;
	result = dns_db_create(mctx, "rbt", dns_rootname, dns_dbtype_zone,
			       rdataset.rdclass, 0, NULL, &db);
	if (result != ISC_R_SUCCESS) {
		char domainstr[DNS_NAME_FORMATSIZE];
		dns_name_format(domain, domainstr, sizeof domainstr);
		fatal("failed to create a database for %s", domainstr);
	}

	version = NULL;
	dns_db_newversion(db, &version);

	node = NULL;
	result = dns_db_findnode(db, domain, ISC_TRUE, &node);
	check_result(result, "dns_db_findnode()");

	dns_db_addrdataset(db, node, version, 0, &rdataset, 0, NULL);
	if (!ISC_LIST_EMPTY(keylist))
		dns_db_addrdataset(db, node, version, 0, &sigrdataset, 0,
				   NULL);

	dns_db_detachnode(db, &node);
	dns_db_closeversion(db, &version, ISC_TRUE);
	result = dns_db_dump(db, version, output);
	if (result != ISC_R_SUCCESS) {
		char domainstr[DNS_NAME_FORMATSIZE];
		dns_name_format(domain, domainstr, sizeof domainstr);
		fatal("failed to write database for %s to %s",
		      domainstr, output);
	}

	printf("%s\n", output);

	dns_db_detach(&db);

	dns_rdataset_disassociate(&rdataset);
	while (!ISC_LIST_EMPTY(rdatalist.rdata)) {
		rdata = ISC_LIST_HEAD(rdatalist.rdata);
		ISC_LIST_UNLINK(rdatalist.rdata, rdata, link);
		isc_mem_put(mctx, rdata->data, BUFSIZE);
		isc_mem_put(mctx, rdata, sizeof *rdata);
	}
	while (!ISC_LIST_EMPTY(sigrdatalist.rdata)) {
		rdata = ISC_LIST_HEAD(sigrdatalist.rdata);
		ISC_LIST_UNLINK(sigrdatalist.rdata, rdata, link);
		isc_mem_put(mctx, rdata->data, BUFSIZE);
		isc_mem_put(mctx, rdata, sizeof *rdata);
	}

	while (!ISC_LIST_EMPTY(keylist)) {
		keynode = ISC_LIST_HEAD(keylist);
		ISC_LIST_UNLINK(keylist, keynode, link);
		dst_key_free(&keynode->key);
		isc_mem_put(mctx, keynode, sizeof(keynode_t));
	}

	if (savedname != NULL) {
		dns_name_free(savedname, mctx);
		isc_mem_put(mctx, savedname, sizeof(dns_name_t));
	}

	cleanup_logging(&log);
	cleanup_entropy(&ectx);

	isc_mem_free(mctx, output);
	dst_lib_destroy();
	if (verbose > 10)
		isc_mem_stats(mctx, stdout);
	isc_mem_destroy(&mctx);
	return (0);
}
