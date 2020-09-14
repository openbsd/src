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

/*
 * $Id: tsig.c,v 1.14 2020/09/14 08:40:43 florian Exp $
 */
/*! \file */

#include <stdlib.h>
#include <string.h>		/* Required for HP/UX (and others?) */
#include <time.h>

#include <isc/util.h>
#include <isc/buffer.h>
#include <isc/refcount.h>

#include <dns/keyvalues.h>
#include <dns/log.h>
#include <dns/message.h>
#include <dns/rdata.h>
#include <dns/rdatalist.h>
#include <dns/rdataset.h>
#include <dns/result.h>
#include <dns/tsig.h>

#include <dst/result.h>

#define is_response(msg) (msg->flags & DNS_MESSAGEFLAG_QR)
#define algname_is_allocated(algname) \
	((algname) != dns_tsig_hmacsha1_name && \
	 (algname) != dns_tsig_hmacsha224_name && \
	 (algname) != dns_tsig_hmacsha256_name && \
	 (algname) != dns_tsig_hmacsha384_name && \
	 (algname) != dns_tsig_hmacsha512_name)

#define BADTIMELEN 6

static unsigned char hmacsha1_ndata[] = "\011hmac-sha1";
static unsigned char hmacsha1_offsets[] = { 0, 10 };
static dns_name_t hmacsha1 =
	DNS_NAME_INITABSOLUTE(hmacsha1_ndata, hmacsha1_offsets);
dns_name_t *dns_tsig_hmacsha1_name = &hmacsha1;

static unsigned char hmacsha224_ndata[] = "\013hmac-sha224";
static unsigned char hmacsha224_offsets[] = { 0, 12 };
static dns_name_t hmacsha224 =
	DNS_NAME_INITABSOLUTE(hmacsha224_ndata, hmacsha224_offsets);
dns_name_t *dns_tsig_hmacsha224_name = &hmacsha224;

static unsigned char hmacsha256_ndata[] = "\013hmac-sha256";
static unsigned char hmacsha256_offsets[] = { 0, 12 };
static dns_name_t hmacsha256 =
	DNS_NAME_INITABSOLUTE(hmacsha256_ndata, hmacsha256_offsets);
dns_name_t *dns_tsig_hmacsha256_name = &hmacsha256;

static unsigned char hmacsha384_ndata[] = "\013hmac-sha384";
static unsigned char hmacsha384_offsets[] = { 0, 12 };
static dns_name_t hmacsha384 =
	DNS_NAME_INITABSOLUTE(hmacsha384_ndata, hmacsha384_offsets);
dns_name_t *dns_tsig_hmacsha384_name = &hmacsha384;

static unsigned char hmacsha512_ndata[] = "\013hmac-sha512";
static unsigned char hmacsha512_offsets[] = { 0, 12 };
static dns_name_t hmacsha512 =
	DNS_NAME_INITABSOLUTE(hmacsha512_ndata, hmacsha512_offsets);
dns_name_t *dns_tsig_hmacsha512_name = &hmacsha512;

static isc_result_t
tsig_verify_tcp(isc_buffer_t *source, dns_message_t *msg);

static void
tsig_log(dns_tsigkey_t *key, int level, const char *fmt, ...)
     __attribute__((__format__(__printf__, 3, 4)));

static void
tsigkey_free(dns_tsigkey_t *key);

static void
tsig_log(dns_tsigkey_t *key, int level, const char *fmt, ...) {
	va_list ap;
	char message[4096];
	char namestr[DNS_NAME_FORMATSIZE];
	char creatorstr[DNS_NAME_FORMATSIZE];

	if (!isc_log_wouldlog(dns_lctx, level))
		return;
	if (key != NULL) {
		dns_name_format(&key->name, namestr, sizeof(namestr));
	} else {
		strlcpy(namestr, "<null>", sizeof(namestr));
	}

	if (key != NULL && key->generated && key->creator) {
		dns_name_format(key->creator, creatorstr, sizeof(creatorstr));
	} else {
		strlcpy(creatorstr, "<null>", sizeof(creatorstr));
	}

	va_start(ap, fmt);
	vsnprintf(message, sizeof(message), fmt, ap);
	va_end(ap);
	if (key != NULL && key->generated) {
		isc_log_write(dns_lctx,
			      DNS_LOGCATEGORY_DNSSEC, DNS_LOGMODULE_TSIG,
			      level, "tsig key '%s' (%s): %s",
			      namestr, creatorstr, message);
	} else {
		isc_log_write(dns_lctx,
			      DNS_LOGCATEGORY_DNSSEC, DNS_LOGMODULE_TSIG,
			      level, "tsig key '%s': %s", namestr, message);
	}
}

isc_result_t
dns_tsigkey_createfromkey(dns_name_t *name, dns_name_t *algorithm,
			  dst_key_t *dstkey, int generated,
			  dns_name_t *creator, time_t inception,
			  time_t expire,
			  dns_tsigkey_t **key)
{
	dns_tsigkey_t *tkey;
	isc_result_t ret;
	unsigned int refs = 0;

	REQUIRE(key == NULL || *key == NULL);
	REQUIRE(name != NULL);
	REQUIRE(algorithm != NULL);
	REQUIRE(key != NULL);

	tkey = (dns_tsigkey_t *) malloc(sizeof(dns_tsigkey_t));
	if (tkey == NULL)
		return (ISC_R_NOMEMORY);

	dns_name_init(&tkey->name, NULL);
	ret = dns_name_dup(name, &tkey->name);
	if (ret != ISC_R_SUCCESS)
		goto cleanup_key;
	(void)dns_name_downcase(&tkey->name, &tkey->name, NULL);

	if (dns_name_equal(algorithm, DNS_TSIG_HMACSHA1_NAME)) {
		tkey->algorithm = DNS_TSIG_HMACSHA1_NAME;
		if (dstkey != NULL && dst_key_alg(dstkey) != DST_ALG_HMACSHA1) {
			ret = DNS_R_BADALG;
			goto cleanup_name;
		}
	} else if (dns_name_equal(algorithm, DNS_TSIG_HMACSHA224_NAME)) {
		tkey->algorithm = DNS_TSIG_HMACSHA224_NAME;
		if (dstkey != NULL &&
		    dst_key_alg(dstkey) != DST_ALG_HMACSHA224) {
			ret = DNS_R_BADALG;
			goto cleanup_name;
		}
	} else if (dns_name_equal(algorithm, DNS_TSIG_HMACSHA256_NAME)) {
		tkey->algorithm = DNS_TSIG_HMACSHA256_NAME;
		if (dstkey != NULL &&
		    dst_key_alg(dstkey) != DST_ALG_HMACSHA256) {
			ret = DNS_R_BADALG;
			goto cleanup_name;
		}
	} else if (dns_name_equal(algorithm, DNS_TSIG_HMACSHA384_NAME)) {
		tkey->algorithm = DNS_TSIG_HMACSHA384_NAME;
		if (dstkey != NULL &&
		    dst_key_alg(dstkey) != DST_ALG_HMACSHA384) {
			ret = DNS_R_BADALG;
			goto cleanup_name;
		}
	} else if (dns_name_equal(algorithm, DNS_TSIG_HMACSHA512_NAME)) {
		tkey->algorithm = DNS_TSIG_HMACSHA512_NAME;
		if (dstkey != NULL &&
		    dst_key_alg(dstkey) != DST_ALG_HMACSHA512) {
			ret = DNS_R_BADALG;
			goto cleanup_name;
		}
	} else {
		if (dstkey != NULL) {
			ret = DNS_R_BADALG;
			goto cleanup_name;
		}
		tkey->algorithm = malloc(sizeof(dns_name_t));
		if (tkey->algorithm == NULL) {
			ret = ISC_R_NOMEMORY;
			goto cleanup_name;
		}
		dns_name_init(tkey->algorithm, NULL);
		ret = dns_name_dup(algorithm, tkey->algorithm);
		if (ret != ISC_R_SUCCESS)
			goto cleanup_algorithm;
		(void)dns_name_downcase(tkey->algorithm, tkey->algorithm,
					NULL);
	}

	if (creator != NULL) {
		tkey->creator = malloc(sizeof(dns_name_t));
		if (tkey->creator == NULL) {
			ret = ISC_R_NOMEMORY;
			goto cleanup_algorithm;
		}
		dns_name_init(tkey->creator, NULL);
		ret = dns_name_dup(creator, tkey->creator);
		if (ret != ISC_R_SUCCESS) {
			free(tkey->creator);
			goto cleanup_algorithm;
		}
	} else
		tkey->creator = NULL;

	tkey->key = NULL;
	if (dstkey != NULL)
		dst_key_attach(dstkey, &tkey->key);

	if (key != NULL)
		refs = 1;

	ret = isc_refcount_init(&tkey->refs, refs);
	if (ret != ISC_R_SUCCESS)
		goto cleanup_creator;

	tkey->generated = generated;
	tkey->inception = inception;
	tkey->expire = expire;
	ISC_LINK_INIT(tkey, link);

	/*
	 * Ignore this if it's a GSS key, since the key size is meaningless.
	 */
	if (dstkey != NULL && dst_key_size(dstkey) < 64) {
		char namestr[DNS_NAME_FORMATSIZE];
		dns_name_format(name, namestr, sizeof(namestr));
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DNSSEC,
			      DNS_LOGMODULE_TSIG, ISC_LOG_INFO,
			      "the key '%s' is too short to be secure",
			      namestr);
	}

	if (key != NULL)
		*key = tkey;

	return (ISC_R_SUCCESS);

 cleanup_creator:
	if (tkey->key != NULL)
		dst_key_free(&tkey->key);
	if (tkey->creator != NULL) {
		dns_name_free(tkey->creator);
		free(tkey->creator);
	}
 cleanup_algorithm:
	if (algname_is_allocated(tkey->algorithm)) {
		if (dns_name_dynamic(tkey->algorithm))
			dns_name_free(tkey->algorithm);
		free(tkey->algorithm);
	}
 cleanup_name:
	dns_name_free(&tkey->name);
 cleanup_key:
	free(tkey);

	return (ret);
}

isc_result_t
dns_tsigkey_create(dns_name_t *name, dns_name_t *algorithm,
		   unsigned char *secret, int length, int generated,
		   dns_name_t *creator, time_t inception,
		   time_t expire,
		   dns_tsigkey_t **key)
{
	dst_key_t *dstkey = NULL;
	isc_result_t result;

	REQUIRE(length >= 0);
	if (length > 0)
		REQUIRE(secret != NULL);

	if (dns_name_equal(algorithm, DNS_TSIG_HMACSHA1_NAME)) {
		if (secret != NULL) {
			isc_buffer_t b;

			isc_buffer_init(&b, secret, length);
			isc_buffer_add(&b, length);
			result = dst_key_frombuffer(DST_ALG_HMACSHA1,
						    DNS_KEYOWNER_ENTITY,
						    DNS_KEYPROTO_DNSSEC,
						    &b, &dstkey);
				if (result != ISC_R_SUCCESS)
					return (result);
		}
	} else if (dns_name_equal(algorithm, DNS_TSIG_HMACSHA224_NAME)) {
		if (secret != NULL) {
			isc_buffer_t b;

			isc_buffer_init(&b, secret, length);
			isc_buffer_add(&b, length);
			result = dst_key_frombuffer(DST_ALG_HMACSHA224,
						    DNS_KEYOWNER_ENTITY,
						    DNS_KEYPROTO_DNSSEC,
						    &b, &dstkey);
				if (result != ISC_R_SUCCESS)
					return (result);
		}
	} else if (dns_name_equal(algorithm, DNS_TSIG_HMACSHA256_NAME)) {
		if (secret != NULL) {
			isc_buffer_t b;

			isc_buffer_init(&b, secret, length);
			isc_buffer_add(&b, length);
			result = dst_key_frombuffer(DST_ALG_HMACSHA256,
						    DNS_KEYOWNER_ENTITY,
						    DNS_KEYPROTO_DNSSEC,
						    &b, &dstkey);
				if (result != ISC_R_SUCCESS)
					return (result);
		}
	} else if (dns_name_equal(algorithm, DNS_TSIG_HMACSHA384_NAME)) {
		if (secret != NULL) {
			isc_buffer_t b;

			isc_buffer_init(&b, secret, length);
			isc_buffer_add(&b, length);
			result = dst_key_frombuffer(DST_ALG_HMACSHA384,
						    DNS_KEYOWNER_ENTITY,
						    DNS_KEYPROTO_DNSSEC,
						    &b, &dstkey);
				if (result != ISC_R_SUCCESS)
					return (result);
		}
	} else if (dns_name_equal(algorithm, DNS_TSIG_HMACSHA512_NAME)) {
		if (secret != NULL) {
			isc_buffer_t b;

			isc_buffer_init(&b, secret, length);
			isc_buffer_add(&b, length);
			result = dst_key_frombuffer(DST_ALG_HMACSHA512,
						    DNS_KEYOWNER_ENTITY,
						    DNS_KEYPROTO_DNSSEC,
						    &b, &dstkey);
				if (result != ISC_R_SUCCESS)
					return (result);
		}
	} else if (length > 0)
		return (DNS_R_BADALG);

	result = dns_tsigkey_createfromkey(name, algorithm, dstkey,
					   generated, creator,
					   inception, expire, key);
	if (dstkey != NULL)
		dst_key_free(&dstkey);
	return (result);
}

void
dns_tsigkey_attach(dns_tsigkey_t *source, dns_tsigkey_t **targetp) {
	REQUIRE(targetp != NULL && *targetp == NULL);

	isc_refcount_increment(&source->refs, NULL);
	*targetp = source;
}

static void
tsigkey_free(dns_tsigkey_t *key) {
	dns_name_free(&key->name);
	if (algname_is_allocated(key->algorithm)) {
		dns_name_free(key->algorithm);
		free(key->algorithm);
	}
	if (key->key != NULL)
		dst_key_free(&key->key);
	if (key->creator != NULL) {
		dns_name_free(key->creator);
		free(key->creator);
	}
	isc_refcount_destroy(&key->refs);
	free(key);
}

void
dns_tsigkey_detach(dns_tsigkey_t **keyp) {
	dns_tsigkey_t *key;
	unsigned int refs;

	REQUIRE(keyp != NULL);

	key = *keyp;
	isc_refcount_decrement(&key->refs, &refs);

	if (refs == 0)
		tsigkey_free(key);

	*keyp = NULL;
}

isc_result_t
dns_tsig_sign(dns_message_t *msg) {
	dns_tsigkey_t *key;
	dns_rdata_any_tsig_t tsig, querytsig;
	unsigned char data[128];
	isc_buffer_t databuf, sigbuf;
	isc_buffer_t *dynbuf;
	dns_name_t *owner;
	dns_rdata_t *rdata = NULL;
	dns_rdatalist_t *datalist;
	dns_rdataset_t *dataset;
	isc_region_t r;
	time_t now;
	dst_context_t *ctx = NULL;
	isc_result_t ret;
	unsigned char badtimedata[BADTIMELEN];
	unsigned int sigsize = 0;
	int response;

	REQUIRE(msg != NULL);
	key = dns_message_gettsigkey(msg);

	/*
	 * If this is a response, there should be a query tsig.
	 */
	response = is_response(msg);
	if (response && msg->querytsig == NULL)
		return (DNS_R_EXPECTEDTSIG);

	dynbuf = NULL;

	tsig.common.rdclass = dns_rdataclass_any;
	tsig.common.rdtype = dns_rdatatype_tsig;
	ISC_LINK_INIT(&tsig.common, link);
	dns_name_init(&tsig.algorithm, NULL);
	dns_name_clone(key->algorithm, &tsig.algorithm);

	time(&now);
	tsig.timesigned = now + msg->timeadjust;
	tsig.fudge = DNS_TSIG_FUDGE;

	tsig.originalid = msg->id;

	isc_buffer_init(&databuf, data, sizeof(data));

	if (response)
		tsig.error = msg->querytsigstatus;
	else
		tsig.error = dns_rcode_noerror;

	if (tsig.error != dns_tsigerror_badtime) {
		tsig.otherlen = 0;
		tsig.other = NULL;
	} else {
		isc_buffer_t otherbuf;

		tsig.otherlen = BADTIMELEN;
		tsig.other = badtimedata;
		isc_buffer_init(&otherbuf, tsig.other, tsig.otherlen);
		isc_buffer_putuint48(&otherbuf, tsig.timesigned);
	}

	if ((key->key != NULL) &&
	    (tsig.error != dns_tsigerror_badsig) &&
	    (tsig.error != dns_tsigerror_badkey))
	{
		unsigned char header[DNS_MESSAGE_HEADERLEN];
		isc_buffer_t headerbuf;
		uint16_t digestbits;

		/*
		 * If it is a response, we assume that the request MAC
		 * has validated at this point. This is why we include a
		 * MAC length > 0 in the reply.
		 */
		ret = dst_context_create3(key->key,
					  DNS_LOGCATEGORY_DNSSEC,
					  1, &ctx);
		if (ret != ISC_R_SUCCESS)
			return (ret);

		/*
		 * If this is a response, digest the request's MAC.
		 */
		if (response) {
			dns_rdata_t querytsigrdata = DNS_RDATA_INIT;

			INSIST(msg->verified_sig);

			ret = dns_rdataset_first(msg->querytsig);
			if (ret != ISC_R_SUCCESS)
				goto cleanup_context;
			dns_rdataset_current(msg->querytsig, &querytsigrdata);
			ret = dns_rdata_tostruct_tsig(&querytsigrdata,
						      &querytsig);
			if (ret != ISC_R_SUCCESS)
				goto cleanup_context;
			isc_buffer_putuint16(&databuf, querytsig.siglen);
			if (isc_buffer_availablelength(&databuf) <
			    querytsig.siglen) {
				ret = ISC_R_NOSPACE;
				goto cleanup_context;
			}
			isc_buffer_putmem(&databuf, querytsig.signature,
					  querytsig.siglen);
			isc_buffer_usedregion(&databuf, &r);
			ret = dst_context_adddata(ctx, &r);
			if (ret != ISC_R_SUCCESS)
				goto cleanup_context;
		}

		/*
		 * Digest the header.
		 */
		isc_buffer_init(&headerbuf, header, sizeof(header));
		dns_message_renderheader(msg, &headerbuf);
		isc_buffer_usedregion(&headerbuf, &r);
		ret = dst_context_adddata(ctx, &r);
		if (ret != ISC_R_SUCCESS)
			goto cleanup_context;

		/*
		 * Digest the remainder of the message.
		 */
		isc_buffer_usedregion(msg->buffer, &r);
		isc_region_consume(&r, DNS_MESSAGE_HEADERLEN);
		ret = dst_context_adddata(ctx, &r);
		if (ret != ISC_R_SUCCESS)
			goto cleanup_context;

		if (msg->tcp_continuation == 0) {
			/*
			 * Digest the name, class, ttl, alg.
			 */
			dns_name_toregion(&key->name, &r);
			ret = dst_context_adddata(ctx, &r);
			if (ret != ISC_R_SUCCESS)
				goto cleanup_context;

			isc_buffer_clear(&databuf);
			isc_buffer_putuint16(&databuf, dns_rdataclass_any);
			isc_buffer_putuint32(&databuf, 0); /* ttl */
			isc_buffer_usedregion(&databuf, &r);
			ret = dst_context_adddata(ctx, &r);
			if (ret != ISC_R_SUCCESS)
				goto cleanup_context;

			dns_name_toregion(&tsig.algorithm, &r);
			ret = dst_context_adddata(ctx, &r);
			if (ret != ISC_R_SUCCESS)
				goto cleanup_context;

		}
		/* Digest the timesigned and fudge */
		isc_buffer_clear(&databuf);
		if (tsig.error == dns_tsigerror_badtime) {
			INSIST(response);
			tsig.timesigned = querytsig.timesigned;
		}
		isc_buffer_putuint48(&databuf, tsig.timesigned);
		isc_buffer_putuint16(&databuf, tsig.fudge);
		isc_buffer_usedregion(&databuf, &r);
		ret = dst_context_adddata(ctx, &r);
		if (ret != ISC_R_SUCCESS)
			goto cleanup_context;

		if (msg->tcp_continuation == 0) {
			/*
			 * Digest the error and other data length.
			 */
			isc_buffer_clear(&databuf);
			isc_buffer_putuint16(&databuf, tsig.error);
			isc_buffer_putuint16(&databuf, tsig.otherlen);

			isc_buffer_usedregion(&databuf, &r);
			ret = dst_context_adddata(ctx, &r);
			if (ret != ISC_R_SUCCESS)
				goto cleanup_context;

			/*
			 * Digest other data.
			 */
			if (tsig.otherlen > 0) {
				r.length = tsig.otherlen;
				r.base = tsig.other;
				ret = dst_context_adddata(ctx, &r);
				if (ret != ISC_R_SUCCESS)
					goto cleanup_context;
			}
		}

		ret = dst_key_sigsize(key->key, &sigsize);
		if (ret != ISC_R_SUCCESS)
			goto cleanup_context;
		tsig.signature = (unsigned char *) malloc(sigsize);
		if (tsig.signature == NULL) {
			ret = ISC_R_NOMEMORY;
			goto cleanup_context;
		}

		isc_buffer_init(&sigbuf, tsig.signature, sigsize);
		ret = dst_context_sign(ctx, &sigbuf);
		if (ret != ISC_R_SUCCESS)
			goto cleanup_signature;
		dst_context_destroy(&ctx);
		digestbits = dst_key_getbits(key->key);
		if (digestbits != 0) {
			/*
			 * XXXRAY: Is this correct? What is the
			 * expected behavior when digestbits is not an
			 * integral multiple of 8? It looks like bytes
			 * should either be (digestbits/8) or
			 * (digestbits+7)/8.
			 *
			 * In any case, for current algorithms,
			 * digestbits are an integral multiple of 8, so
			 * it has the same effect as (digestbits/8).
			 */
			unsigned int bytes = (digestbits + 1) / 8;
			if (response && bytes < querytsig.siglen)
				bytes = querytsig.siglen;
			if (bytes > isc_buffer_usedlength(&sigbuf))
				bytes = isc_buffer_usedlength(&sigbuf);
			tsig.siglen = bytes;
		} else
			tsig.siglen = isc_buffer_usedlength(&sigbuf);
	} else {
		tsig.siglen = 0;
		tsig.signature = NULL;
	}

	ret = dns_message_gettemprdata(msg, &rdata);
	if (ret != ISC_R_SUCCESS)
		goto cleanup_signature;
	ret = isc_buffer_allocate(&dynbuf, 512);
	if (ret != ISC_R_SUCCESS)
		goto cleanup_rdata;
	ret = dns_rdata_fromstruct_tsig(rdata, dns_rdataclass_any,
				        dns_rdatatype_tsig, &tsig, dynbuf);
	if (ret != ISC_R_SUCCESS)
		goto cleanup_dynbuf;

	dns_message_takebuffer(msg, &dynbuf);

	if (tsig.signature != NULL) {
		free(tsig.signature);
		tsig.signature = NULL;
	}

	owner = NULL;
	ret = dns_message_gettempname(msg, &owner);
	if (ret != ISC_R_SUCCESS)
		goto cleanup_rdata;
	dns_name_init(owner, NULL);
	ret = dns_name_dup(&key->name, owner);
	if (ret != ISC_R_SUCCESS)
		goto cleanup_owner;

	datalist = NULL;
	ret = dns_message_gettemprdatalist(msg, &datalist);
	if (ret != ISC_R_SUCCESS)
		goto cleanup_owner;
	dataset = NULL;
	ret = dns_message_gettemprdataset(msg, &dataset);
	if (ret != ISC_R_SUCCESS)
		goto cleanup_rdatalist;
	datalist->rdclass = dns_rdataclass_any;
	datalist->type = dns_rdatatype_tsig;
	ISC_LIST_APPEND(datalist->rdata, rdata, link);
	RUNTIME_CHECK(dns_rdatalist_tordataset(datalist, dataset)
		      == ISC_R_SUCCESS);
	msg->tsig = dataset;
	msg->tsigname = owner;

	/* Windows does not like the tsig name being compressed. */
	msg->tsigname->attributes |= DNS_NAMEATTR_NOCOMPRESS;

	return (ISC_R_SUCCESS);

 cleanup_rdatalist:
	dns_message_puttemprdatalist(msg, &datalist);
 cleanup_owner:
	dns_message_puttempname(msg, &owner);
	goto cleanup_rdata;
 cleanup_dynbuf:
	isc_buffer_free(&dynbuf);
 cleanup_rdata:
	dns_message_puttemprdata(msg, &rdata);
 cleanup_signature:
	if (tsig.signature != NULL)
		free(tsig.signature);
 cleanup_context:
	if (ctx != NULL)
		dst_context_destroy(&ctx);
	return (ret);
}

isc_result_t
dns_tsig_verify(isc_buffer_t *source, dns_message_t *msg)
{
	dns_rdata_any_tsig_t tsig, querytsig;
	isc_region_t r, source_r, header_r, sig_r;
	isc_buffer_t databuf;
	unsigned char data[32];
	dns_name_t *keyname;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	time_t now;
	isc_result_t ret;
	dns_tsigkey_t *tsigkey;
	dst_key_t *key = NULL;
	unsigned char header[DNS_MESSAGE_HEADERLEN];
	dst_context_t *ctx = NULL;
	uint16_t addcount, id;
	unsigned int siglen;
	unsigned int alg;
	int response;

	REQUIRE(source != NULL);
	tsigkey = dns_message_gettsigkey(msg);
	response = is_response(msg);

	msg->verify_attempted = 1;
	msg->verified_sig = 0;
	msg->tsigstatus = dns_tsigerror_badsig;

	if (msg->tcp_continuation) {
		if (tsigkey == NULL || msg->querytsig == NULL)
			return (DNS_R_UNEXPECTEDTSIG);
		return (tsig_verify_tcp(source, msg));
	}

	/*
	 * There should be a TSIG record...
	 */
	if (msg->tsig == NULL)
		return (DNS_R_EXPECTEDTSIG);

	/*
	 * If this is a response and there's no key or query TSIG, there
	 * shouldn't be one on the response.
	 */
	if (response && (tsigkey == NULL || msg->querytsig == NULL))
		return (DNS_R_UNEXPECTEDTSIG);

	/*
	 * If we're here, we know the message is well formed and contains a
	 * TSIG record.
	 */

	keyname = msg->tsigname;
	ret = dns_rdataset_first(msg->tsig);
	if (ret != ISC_R_SUCCESS)
		return (ret);
	dns_rdataset_current(msg->tsig, &rdata);
	ret = dns_rdata_tostruct_tsig(&rdata, &tsig);
	if (ret != ISC_R_SUCCESS)
		return (ret);
	dns_rdata_reset(&rdata);
	if (response) {
		ret = dns_rdataset_first(msg->querytsig);
		if (ret != ISC_R_SUCCESS)
			return (ret);
		dns_rdataset_current(msg->querytsig, &rdata);
		ret = dns_rdata_tostruct_tsig(&rdata, &querytsig);
		if (ret != ISC_R_SUCCESS)
			return (ret);
	}
	/*
	 * Do the key name and algorithm match that of the query?
	 */
	if (response &&
	    (!dns_name_equal(keyname, &tsigkey->name) ||
	     !dns_name_equal(&tsig.algorithm, &querytsig.algorithm))) {
		msg->tsigstatus = dns_tsigerror_badkey;
		tsig_log(msg->tsigkey, 2,
			 "key name and algorithm do not match");
		return (DNS_R_TSIGVERIFYFAILURE);
	}

	/*
	 * Get the current time.
	 */
	time(&now);

	/*
	 * Find dns_tsigkey_t based on keyname.
	 */
	if (tsigkey == NULL) {
		ret = ISC_R_NOTFOUND;
		if (ret != ISC_R_SUCCESS) {
			msg->tsigstatus = dns_tsigerror_badkey;
			ret = dns_tsigkey_create(keyname, &tsig.algorithm,
						 NULL, 0, 0, NULL,
						 now, now,
						 &msg->tsigkey);
			if (ret != ISC_R_SUCCESS)
				return (ret);
			tsig_log(msg->tsigkey, 2, "unknown key");
			return (DNS_R_TSIGVERIFYFAILURE);
		}
		msg->tsigkey = tsigkey;
	}

	key = tsigkey->key;

	/*
	 * Check digest length.
	 */
	alg = dst_key_alg(key);
	ret = dst_key_sigsize(key, &siglen);
	if (ret != ISC_R_SUCCESS)
		return (ret);
	if (
	    alg == DST_ALG_HMACSHA1 ||
	    alg == DST_ALG_HMACSHA224 || alg == DST_ALG_HMACSHA256 ||
	    alg == DST_ALG_HMACSHA384 || alg == DST_ALG_HMACSHA512)
	{
		if (tsig.siglen > siglen) {
			tsig_log(msg->tsigkey, 2, "signature length too big");
			return (DNS_R_FORMERR);
		}
		if (tsig.siglen > 0 &&
		    (tsig.siglen < 10 || tsig.siglen < ((siglen + 1) / 2)))
		{
			tsig_log(msg->tsigkey, 2,
				 "signature length below minimum");
			return (DNS_R_FORMERR);
		}
	}

	if (tsig.siglen > 0) {
		uint16_t addcount_n;

		sig_r.base = tsig.signature;
		sig_r.length = tsig.siglen;

		ret = dst_context_create3(key,
					  DNS_LOGCATEGORY_DNSSEC,
					  0, &ctx);
		if (ret != ISC_R_SUCCESS)
			return (ret);

		if (response) {
			isc_buffer_init(&databuf, data, sizeof(data));
			isc_buffer_putuint16(&databuf, querytsig.siglen);
			isc_buffer_usedregion(&databuf, &r);
			ret = dst_context_adddata(ctx, &r);
			if (ret != ISC_R_SUCCESS)
				goto cleanup_context;
			if (querytsig.siglen > 0) {
				r.length = querytsig.siglen;
				r.base = querytsig.signature;
				ret = dst_context_adddata(ctx, &r);
				if (ret != ISC_R_SUCCESS)
					goto cleanup_context;
			}
		}

		/*
		 * Extract the header.
		 */
		isc_buffer_usedregion(source, &r);
		memmove(header, r.base, DNS_MESSAGE_HEADERLEN);
		isc_region_consume(&r, DNS_MESSAGE_HEADERLEN);

		/*
		 * Decrement the additional field counter.
		 */
		memmove(&addcount, &header[DNS_MESSAGE_HEADERLEN - 2], 2);
		addcount_n = ntohs(addcount);
		addcount = htons((uint16_t)(addcount_n - 1));
		memmove(&header[DNS_MESSAGE_HEADERLEN - 2], &addcount, 2);

		/*
		 * Put in the original id.
		 */
		id = htons(tsig.originalid);
		memmove(&header[0], &id, 2);

		/*
		 * Digest the modified header.
		 */
		header_r.base = (unsigned char *) header;
		header_r.length = DNS_MESSAGE_HEADERLEN;
		ret = dst_context_adddata(ctx, &header_r);
		if (ret != ISC_R_SUCCESS)
			goto cleanup_context;

		/*
		 * Digest all non-TSIG records.
		 */
		isc_buffer_usedregion(source, &source_r);
		r.base = source_r.base + DNS_MESSAGE_HEADERLEN;
		r.length = msg->sigstart - DNS_MESSAGE_HEADERLEN;
		ret = dst_context_adddata(ctx, &r);
		if (ret != ISC_R_SUCCESS)
			goto cleanup_context;

		/*
		 * Digest the key name.
		 */
		dns_name_toregion(&tsigkey->name, &r);
		ret = dst_context_adddata(ctx, &r);
		if (ret != ISC_R_SUCCESS)
			goto cleanup_context;

		isc_buffer_init(&databuf, data, sizeof(data));
		isc_buffer_putuint16(&databuf, tsig.common.rdclass);
		isc_buffer_putuint32(&databuf, msg->tsig->ttl);
		isc_buffer_usedregion(&databuf, &r);
		ret = dst_context_adddata(ctx, &r);
		if (ret != ISC_R_SUCCESS)
			goto cleanup_context;

		/*
		 * Digest the key algorithm.
		 */
		dns_name_toregion(tsigkey->algorithm, &r);
		ret = dst_context_adddata(ctx, &r);
		if (ret != ISC_R_SUCCESS)
			goto cleanup_context;

		isc_buffer_clear(&databuf);
		isc_buffer_putuint48(&databuf, tsig.timesigned);
		isc_buffer_putuint16(&databuf, tsig.fudge);
		isc_buffer_putuint16(&databuf, tsig.error);
		isc_buffer_putuint16(&databuf, tsig.otherlen);
		isc_buffer_usedregion(&databuf, &r);
		ret = dst_context_adddata(ctx, &r);
		if (ret != ISC_R_SUCCESS)
			goto cleanup_context;

		if (tsig.otherlen > 0) {
			r.base = tsig.other;
			r.length = tsig.otherlen;
			ret = dst_context_adddata(ctx, &r);
			if (ret != ISC_R_SUCCESS)
				goto cleanup_context;
		}

		ret = dst_context_verify(ctx, &sig_r);
		if (ret == DST_R_VERIFYFAILURE) {
			ret = DNS_R_TSIGVERIFYFAILURE;
			tsig_log(msg->tsigkey, 2,
				 "signature failed to verify(1)");
			goto cleanup_context;
		} else if (ret != ISC_R_SUCCESS) {
			goto cleanup_context;
		}
		msg->verified_sig = 1;
	} else if (tsig.error != dns_tsigerror_badsig &&
		   tsig.error != dns_tsigerror_badkey) {
		tsig_log(msg->tsigkey, 2, "signature was empty");
		return (DNS_R_TSIGVERIFYFAILURE);
	}

	/*
	 * Here at this point, the MAC has been verified. Even if any of
	 * the following code returns a TSIG error, the reply will be
	 * signed and WILL always include the request MAC in the digest
	 * computation.
	 */

	/*
	 * Is the time ok?
	 */
	if (now + msg->timeadjust > (time_t)(tsig.timesigned + tsig.fudge)) {
		msg->tsigstatus = dns_tsigerror_badtime;
		tsig_log(msg->tsigkey, 2, "signature has expired");
		ret = DNS_R_CLOCKSKEW;
		goto cleanup_context;
	} else if (now + msg->timeadjust < (time_t)(tsig.timesigned -
	    tsig.fudge)) {
		msg->tsigstatus = dns_tsigerror_badtime;
		tsig_log(msg->tsigkey, 2, "signature is in the future");
		ret = DNS_R_CLOCKSKEW;
		goto cleanup_context;
	}

	if (
	    alg == DST_ALG_HMACSHA1 ||
	    alg == DST_ALG_HMACSHA224 || alg == DST_ALG_HMACSHA256 ||
	    alg == DST_ALG_HMACSHA384 || alg == DST_ALG_HMACSHA512)
	{
		uint16_t digestbits = dst_key_getbits(key);

		/*
		 * XXXRAY: Is this correct? What is the expected
		 * behavior when digestbits is not an integral multiple
		 * of 8? It looks like bytes should either be
		 * (digestbits/8) or (digestbits+7)/8.
		 *
		 * In any case, for current algorithms, digestbits are
		 * an integral multiple of 8, so it has the same effect
		 * as (digestbits/8).
		 */
		if (tsig.siglen > 0 && digestbits != 0 &&
		    tsig.siglen < ((digestbits + 1) / 8))
		{
			msg->tsigstatus = dns_tsigerror_badtrunc;
			tsig_log(msg->tsigkey, 2,
				 "truncated signature length too small");
			ret = DNS_R_TSIGVERIFYFAILURE;
			goto cleanup_context;
		}
		if (tsig.siglen > 0 && digestbits == 0 &&
		    tsig.siglen < siglen)
		{
			msg->tsigstatus = dns_tsigerror_badtrunc;
			tsig_log(msg->tsigkey, 2, "signature length too small");
			ret = DNS_R_TSIGVERIFYFAILURE;
			goto cleanup_context;
		}
	}

	if (tsig.error != dns_rcode_noerror) {
		msg->tsigstatus = tsig.error;
		if (tsig.error == dns_tsigerror_badtime)
			ret = DNS_R_CLOCKSKEW;
		else
			ret = DNS_R_TSIGERRORSET;
		goto cleanup_context;
	}

	msg->tsigstatus = dns_rcode_noerror;
	ret = ISC_R_SUCCESS;

 cleanup_context:
	if (ctx != NULL)
		dst_context_destroy(&ctx);

	return (ret);
}

static isc_result_t
tsig_verify_tcp(isc_buffer_t *source, dns_message_t *msg) {
	dns_rdata_any_tsig_t tsig, querytsig;
	isc_region_t r, source_r, header_r, sig_r;
	isc_buffer_t databuf;
	unsigned char data[32];
	dns_name_t *keyname;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	time_t now;
	isc_result_t ret;
	dns_tsigkey_t *tsigkey;
	dst_key_t *key = NULL;
	unsigned char header[DNS_MESSAGE_HEADERLEN];
	uint16_t addcount, id;
	int has_tsig = 0;
	unsigned int siglen;
	unsigned int alg;

	REQUIRE(source != NULL);
	REQUIRE(msg != NULL);
	REQUIRE(dns_message_gettsigkey(msg) != NULL);
	REQUIRE(msg->tcp_continuation == 1);
	REQUIRE(msg->querytsig != NULL);

	msg->verified_sig = 0;
	msg->tsigstatus = dns_tsigerror_badsig;

	if (!is_response(msg))
		return (DNS_R_EXPECTEDRESPONSE);

	tsigkey = dns_message_gettsigkey(msg);
	key = tsigkey->key;

	/*
	 * Extract and parse the previous TSIG
	 */
	ret = dns_rdataset_first(msg->querytsig);
	if (ret != ISC_R_SUCCESS)
		return (ret);
	dns_rdataset_current(msg->querytsig, &rdata);
	ret = dns_rdata_tostruct_tsig(&rdata, &querytsig);
	if (ret != ISC_R_SUCCESS)
		return (ret);
	dns_rdata_reset(&rdata);

	/*
	 * If there is a TSIG in this message, do some checks.
	 */
	if (msg->tsig != NULL) {
		has_tsig = 1;

		keyname = msg->tsigname;
		ret = dns_rdataset_first(msg->tsig);
		if (ret != ISC_R_SUCCESS)
			goto cleanup_querystruct;
		dns_rdataset_current(msg->tsig, &rdata);
		ret = dns_rdata_tostruct_tsig(&rdata, &tsig);
		if (ret != ISC_R_SUCCESS)
			goto cleanup_querystruct;

		/*
		 * Do the key name and algorithm match that of the query?
		 */
		if (!dns_name_equal(keyname, &tsigkey->name) ||
		    !dns_name_equal(&tsig.algorithm, &querytsig.algorithm))
		{
			msg->tsigstatus = dns_tsigerror_badkey;
			ret = DNS_R_TSIGVERIFYFAILURE;
			tsig_log(msg->tsigkey, 2,
				 "key name and algorithm do not match");
			goto cleanup_querystruct;
		}

		/*
		 * Check digest length.
		 */
		alg = dst_key_alg(key);
		ret = dst_key_sigsize(key, &siglen);
		if (ret != ISC_R_SUCCESS)
			goto cleanup_querystruct;
		if (
			alg == DST_ALG_HMACSHA1 ||
			alg == DST_ALG_HMACSHA224 ||
			alg == DST_ALG_HMACSHA256 ||
			alg == DST_ALG_HMACSHA384 ||
			alg == DST_ALG_HMACSHA512)
		{
			if (tsig.siglen > siglen) {
				tsig_log(tsigkey, 2,
					 "signature length too big");
				ret = DNS_R_FORMERR;
				goto cleanup_querystruct;
			}
			if (tsig.siglen > 0 &&
			    (tsig.siglen < 10 ||
			     tsig.siglen < ((siglen + 1) / 2)))
			{
				tsig_log(tsigkey, 2,
					 "signature length below minimum");
				ret = DNS_R_FORMERR;
				goto cleanup_querystruct;
			}
		}
	}

	if (msg->tsigctx == NULL) {
		ret = dst_context_create3(key,
					  DNS_LOGCATEGORY_DNSSEC,
					  0, &msg->tsigctx);
		if (ret != ISC_R_SUCCESS)
			goto cleanup_querystruct;

		/*
		 * Digest the length of the query signature
		 */
		isc_buffer_init(&databuf, data, sizeof(data));
		isc_buffer_putuint16(&databuf, querytsig.siglen);
		isc_buffer_usedregion(&databuf, &r);
		ret = dst_context_adddata(msg->tsigctx, &r);
		if (ret != ISC_R_SUCCESS)
			goto cleanup_context;

		/*
		 * Digest the data of the query signature
		 */
		if (querytsig.siglen > 0) {
			r.length = querytsig.siglen;
			r.base = querytsig.signature;
			ret = dst_context_adddata(msg->tsigctx, &r);
			if (ret != ISC_R_SUCCESS)
				goto cleanup_context;
		}
	}

	/*
	 * Extract the header.
	 */
	isc_buffer_usedregion(source, &r);
	memmove(header, r.base, DNS_MESSAGE_HEADERLEN);
	isc_region_consume(&r, DNS_MESSAGE_HEADERLEN);

	/*
	 * Decrement the additional field counter if necessary.
	 */
	if (has_tsig) {
		uint16_t addcount_n;

		memmove(&addcount, &header[DNS_MESSAGE_HEADERLEN - 2], 2);
		addcount_n = ntohs(addcount);
		addcount = htons((uint16_t)(addcount_n - 1));
		memmove(&header[DNS_MESSAGE_HEADERLEN - 2], &addcount, 2);

		/*
		 * Put in the original id.
		 *
		 * XXX Can TCP transfers be forwarded?  How would that
		 * work?
		 */
		id = htons(tsig.originalid);
		memmove(&header[0], &id, 2);
	}

	/*
	 * Digest the modified header.
	 */
	header_r.base = (unsigned char *) header;
	header_r.length = DNS_MESSAGE_HEADERLEN;
	ret = dst_context_adddata(msg->tsigctx, &header_r);
	if (ret != ISC_R_SUCCESS)
		goto cleanup_context;

	/*
	 * Digest all non-TSIG records.
	 */
	isc_buffer_usedregion(source, &source_r);
	r.base = source_r.base + DNS_MESSAGE_HEADERLEN;
	if (has_tsig)
		r.length = msg->sigstart - DNS_MESSAGE_HEADERLEN;
	else
		r.length = source_r.length - DNS_MESSAGE_HEADERLEN;
	ret = dst_context_adddata(msg->tsigctx, &r);
	if (ret != ISC_R_SUCCESS)
		goto cleanup_context;

	/*
	 * Digest the time signed and fudge.
	 */
	if (has_tsig) {
		isc_buffer_init(&databuf, data, sizeof(data));
		isc_buffer_putuint48(&databuf, tsig.timesigned);
		isc_buffer_putuint16(&databuf, tsig.fudge);
		isc_buffer_usedregion(&databuf, &r);
		ret = dst_context_adddata(msg->tsigctx, &r);
		if (ret != ISC_R_SUCCESS)
			goto cleanup_context;

		sig_r.base = tsig.signature;
		sig_r.length = tsig.siglen;
		if (tsig.siglen == 0) {
			if (tsig.error != dns_rcode_noerror) {
				msg->tsigstatus = tsig.error;
				if (tsig.error == dns_tsigerror_badtime) {
					ret = DNS_R_CLOCKSKEW;
				} else {
					ret = DNS_R_TSIGERRORSET;
				}
			} else {
				tsig_log(msg->tsigkey, 2,
					 "signature is empty");
				ret = DNS_R_TSIGVERIFYFAILURE;
			}
			goto cleanup_context;
		}

		ret = dst_context_verify(msg->tsigctx, &sig_r);
		if (ret == DST_R_VERIFYFAILURE) {
			tsig_log(msg->tsigkey, 2,
				 "signature failed to verify(2)");
			ret = DNS_R_TSIGVERIFYFAILURE;
			goto cleanup_context;
		} else if (ret != ISC_R_SUCCESS) {
			goto cleanup_context;
		}
		msg->verified_sig = 1;

		/*
		 * Here at this point, the MAC has been verified. Even
		 * if any of the following code returns a TSIG error,
		 * the reply will be signed and WILL always include the
		 * request MAC in the digest computation.
		 */

		/*
		 * Is the time ok?
		 */
		time(&now);

		if (now + msg->timeadjust > (time_t)(tsig.timesigned +
		    tsig.fudge)) {
			msg->tsigstatus = dns_tsigerror_badtime;
			tsig_log(msg->tsigkey, 2, "signature has expired");
			ret = DNS_R_CLOCKSKEW;
			goto cleanup_context;
		} else if (now + msg->timeadjust < (time_t)(tsig.timesigned -
		    tsig.fudge)) {
			msg->tsigstatus = dns_tsigerror_badtime;
			tsig_log(msg->tsigkey, 2,
				 "signature is in the future");
			ret = DNS_R_CLOCKSKEW;
			goto cleanup_context;
		}

		alg = dst_key_alg(key);
		ret = dst_key_sigsize(key, &siglen);
		if (ret != ISC_R_SUCCESS)
			goto cleanup_context;
		if (
			alg == DST_ALG_HMACSHA1 ||
			alg == DST_ALG_HMACSHA224 ||
			alg == DST_ALG_HMACSHA256 ||
			alg == DST_ALG_HMACSHA384 ||
			alg == DST_ALG_HMACSHA512)
		{
			uint16_t digestbits = dst_key_getbits(key);

			/*
			 * XXXRAY: Is this correct? What is the
			 * expected behavior when digestbits is not an
			 * integral multiple of 8? It looks like bytes
			 * should either be (digestbits/8) or
			 * (digestbits+7)/8.
			 *
			 * In any case, for current algorithms,
			 * digestbits are an integral multiple of 8, so
			 * it has the same effect as (digestbits/8).
			 */
			if (tsig.siglen > 0 && digestbits != 0 &&
			    tsig.siglen < ((digestbits + 1) / 8))
			{
				msg->tsigstatus = dns_tsigerror_badtrunc;
				tsig_log(msg->tsigkey, 2,
					 "truncated signature length "
					 "too small");
				ret = DNS_R_TSIGVERIFYFAILURE;
				goto cleanup_context;
			}
			if (tsig.siglen > 0 && digestbits == 0 &&
			    tsig.siglen < siglen)
			{
				msg->tsigstatus = dns_tsigerror_badtrunc;
				tsig_log(msg->tsigkey, 2,
					 "signature length too small");
				ret = DNS_R_TSIGVERIFYFAILURE;
				goto cleanup_context;
			}
		}

		if (tsig.error != dns_rcode_noerror) {
			msg->tsigstatus = tsig.error;
			if (tsig.error == dns_tsigerror_badtime)
				ret = DNS_R_CLOCKSKEW;
			else
				ret = DNS_R_TSIGERRORSET;
			goto cleanup_context;
		}
	}

	msg->tsigstatus = dns_rcode_noerror;
	ret = ISC_R_SUCCESS;

 cleanup_context:
	/*
	 * Except in error conditions, don't destroy the DST context
	 * for unsigned messages; it is a running sum till the next
	 * TSIG signed message.
	 */
	if ((ret != ISC_R_SUCCESS || has_tsig) && msg->tsigctx != NULL) {
		dst_context_destroy(&msg->tsigctx);
	}

 cleanup_querystruct:
	dns_rdata_freestruct_tsig(&querytsig);

	return (ret);
}

