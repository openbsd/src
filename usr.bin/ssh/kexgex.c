/*
 * Copyright (c) 2000 Niels Provos.  All rights reserved.
 * Copyright (c) 2001 Markus Friedl.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "includes.h"
RCSID("$OpenBSD: kexgex.c,v 1.1 2001/04/03 19:53:29 markus Exp $");

#include <openssl/bn.h>

#include "xmalloc.h"
#include "buffer.h"
#include "bufaux.h"
#include "key.h"
#include "kex.h"
#include "log.h"
#include "dispatch.h"
#include "packet.h"
#include "dh.h"
#include "ssh2.h"
#include "compat.h"

extern u_char *session_id2;
extern int session_id2_len;

typedef struct State State;
struct State {
	DH *dh;
	int min, nbits, max;
};

dispatch_fn kexgex_input_request;	/* C -> S */
dispatch_fn kexgex_input_group;		/* S -> C */
dispatch_fn kexgex_input_init;		/* C -> S */
dispatch_fn kexgex_input_reply;		/* S -> C */

u_char *
kexgex_hash(
    char *client_version_string,
    char *server_version_string,
    char *ckexinit, int ckexinitlen,
    char *skexinit, int skexinitlen,
    char *serverhostkeyblob, int sbloblen,
    int min, int wantbits, int max, BIGNUM *prime, BIGNUM *gen,
    BIGNUM *client_dh_pub,
    BIGNUM *server_dh_pub,
    BIGNUM *shared_secret)
{
	Buffer b;
	static u_char digest[EVP_MAX_MD_SIZE];
	EVP_MD *evp_md = EVP_sha1();
	EVP_MD_CTX md;

	buffer_init(&b);
	buffer_put_string(&b, client_version_string, strlen(client_version_string));
	buffer_put_string(&b, server_version_string, strlen(server_version_string));

	/* kexinit messages: fake header: len+SSH2_MSG_KEXINIT */
	buffer_put_int(&b, ckexinitlen+1);
	buffer_put_char(&b, SSH2_MSG_KEXINIT);
	buffer_append(&b, ckexinit, ckexinitlen);
	buffer_put_int(&b, skexinitlen+1);
	buffer_put_char(&b, SSH2_MSG_KEXINIT);
	buffer_append(&b, skexinit, skexinitlen);

	buffer_put_string(&b, serverhostkeyblob, sbloblen);
	if (min == -1 || max == -1) 
		buffer_put_int(&b, wantbits);
	else {
		buffer_put_int(&b, min);
		buffer_put_int(&b, wantbits);
		buffer_put_int(&b, max);
	}
	buffer_put_bignum2(&b, prime);
	buffer_put_bignum2(&b, gen);
	buffer_put_bignum2(&b, client_dh_pub);
	buffer_put_bignum2(&b, server_dh_pub);
	buffer_put_bignum2(&b, shared_secret);

#ifdef DEBUG_KEXDH
	buffer_dump(&b);
#endif
	EVP_DigestInit(&md, evp_md);
	EVP_DigestUpdate(&md, buffer_ptr(&b), buffer_len(&b));
	EVP_DigestFinal(&md, digest, NULL);

	buffer_free(&b);

#ifdef DEBUG_KEXDH
	dump_digest("hash", digest, evp_md->md_size);
#endif
	return digest;
}

/* client */

void
kexgex_client(Kex *kex)
{
	State *state;

	dispatch_set(SSH2_MSG_KEX_DH_GEX_GROUP, &kexgex_input_group);

	state = xmalloc(sizeof(*state));
	state->nbits = dh_estimate(kex->we_need * 8);
	kex->state = state;

	if (datafellows & SSH_OLD_DHGEX) {
		debug("SSH2_MSG_KEX_DH_GEX_REQUEST_OLD sent");

		/* Old GEX request */
		packet_start(SSH2_MSG_KEX_DH_GEX_REQUEST_OLD);
		packet_put_int(state->nbits);
		state->min = DH_GRP_MIN;
		state->max = DH_GRP_MAX;
	} else {
		debug("SSH2_MSG_KEX_DH_GEX_REQUEST sent");

		/* New GEX request */
		state->min = DH_GRP_MIN;
		state->max = DH_GRP_MAX;
		packet_start(SSH2_MSG_KEX_DH_GEX_REQUEST);
		packet_put_int(state->min);
		packet_put_int(state->nbits);
		packet_put_int(state->max);
	}
#ifdef DEBUG_KEXDH
	fprintf(stderr, "\nmin = %d, nbits = %d, max = %d\n",
	    state->min, state->nbits, state->max);
#endif
	packet_send();
}

void   
kexgex_input_group(int type, int plen, void *ctxt)
{
	DH *dh;
	int dlen;
	BIGNUM *p = 0, *g = 0;
	Kex *kex = (Kex*) ctxt;
	State *state = (State *) kex->state;

	debug("SSH2_MSG_KEX_DH_GEX_GROUP receivied");
	dispatch_set(SSH2_MSG_KEX_DH_GEX_GROUP, &kex_protocol_error);
	dispatch_set(SSH2_MSG_KEX_DH_GEX_REPLY, &kexgex_input_reply);

	if ((p = BN_new()) == NULL)
		fatal("BN_new");
	packet_get_bignum2(p, &dlen);
	if ((g = BN_new()) == NULL)
		fatal("BN_new");
	packet_get_bignum2(g, &dlen);

	if (BN_num_bits(p) < state->min || BN_num_bits(p) > state->max)
		fatal("DH_GEX group out of range: %d !< %d !< %d",
		    state->min, BN_num_bits(p), state->max);

	dh = dh_new_group(g, p);
	dh_gen_key(dh, kex->we_need * 8);

	state->dh = dh;

#ifdef DEBUG_KEXDH
	DHparams_print_fp(stderr, dh);
	fprintf(stderr, "pub= ");
	BN_print_fp(stderr, dh->pub_key);
	fprintf(stderr, "\n");
#endif

	debug("SSH2_MSG_KEX_DH_GEX_INIT sent");
	/* generate and send 'e', client DH public key */
	packet_start(SSH2_MSG_KEX_DH_GEX_INIT);
	packet_put_bignum2(dh->pub_key);
	packet_send();
}

void   
kexgex_input_reply(int type, int plen, void *ctxt)
{
	BIGNUM *dh_server_pub = NULL, *shared_secret = NULL;
	Key *server_host_key;
	Kex *kex = (Kex*) ctxt;
	State *state = (State *) kex->state;
	DH *dh = state->dh;
	u_char *kbuf, *hash, *signature = NULL, *server_host_key_blob = NULL;
	u_int klen, kout, slen, sbloblen;
	int dlen, min, max;

	debug("SSH2_MSG_KEX_DH_GEX_REPLY received");
	dispatch_set(SSH2_MSG_KEX_DH_GEX_REPLY, &kex_protocol_error);

	/* key, cert */
	server_host_key_blob = packet_get_string(&sbloblen);
	server_host_key = key_from_blob(server_host_key_blob, sbloblen);
	if (server_host_key == NULL)
		fatal("cannot decode server_host_key_blob");

	if (kex->check_host_key == NULL)
		fatal("cannot check server_host_key");
	kex->check_host_key(server_host_key);

	/* DH paramter f, server public DH key */
	dh_server_pub = BN_new();
	if (dh_server_pub == NULL)
		fatal("dh_server_pub == NULL");
	packet_get_bignum2(dh_server_pub, &dlen);

#ifdef DEBUG_KEXDH
	fprintf(stderr, "dh_server_pub= ");
	BN_print_fp(stderr, dh_server_pub);
	fprintf(stderr, "\n");
	debug("bits %d", BN_num_bits(dh_server_pub));
#endif

	/* signed H */
	signature = packet_get_string(&slen);
	packet_done();

	if (!dh_pub_is_valid(dh, dh_server_pub))
		packet_disconnect("bad server public DH value");

	klen = DH_size(dh);
	kbuf = xmalloc(klen);
	kout = DH_compute_key(kbuf, dh_server_pub, dh);
#ifdef DEBUG_KEXDH
        dump_digest("shared secret", kbuf, kout);
#endif
	shared_secret = BN_new();
	BN_bin2bn(kbuf, kout, shared_secret);
	memset(kbuf, 0, klen);
	xfree(kbuf);

	if (datafellows & SSH_OLD_DHGEX) {
		min = max = -1;
	} else {
		min = state->min;
		max = state->max;
	}

	/* calc and verify H */
	hash = kexgex_hash(
	    kex->client_version_string,
	    kex->server_version_string,
	    buffer_ptr(&kex->my), buffer_len(&kex->my),
	    buffer_ptr(&kex->peer), buffer_len(&kex->peer),
	    server_host_key_blob, sbloblen,
	    min, state->nbits, max,
	    dh->p, dh->g,
	    dh->pub_key,
	    dh_server_pub,
	    shared_secret
	);
	xfree(server_host_key_blob);
	BN_free(dh_server_pub);

	if (key_verify(server_host_key, (u_char *)signature, slen, hash, 20) != 1)
		fatal("key_verify failed for server_host_key");
	key_free(server_host_key);
	xfree(signature);

	kex_derive_keys(kex, hash, shared_secret);
	BN_clear_free(shared_secret);
	packet_set_kex(kex);

	/* save session id */
	session_id2_len = 20;
	session_id2 = xmalloc(session_id2_len);
	memcpy(session_id2, hash, session_id2_len);

	kex_send_newkeys();

	/* have keys, free DH */
	DH_free(dh);
	xfree(state);
	kex->state = NULL;
}

/* server */

void
kexgex_server(Kex *kex)
{
	State *state;

	state = xmalloc(sizeof(*state));
	kex->state = state;
	
	dispatch_set(SSH2_MSG_KEX_DH_GEX_REQUEST, &kexgex_input_request);
	dispatch_set(SSH2_MSG_KEX_DH_GEX_REQUEST_OLD, &kexgex_input_request);
}

void
kexgex_input_request(int type, int plen, void *ctxt)
{
	Kex *kex = (Kex*) ctxt;
	State *state = (State *) kex->state;
	int min = -1, max = -1;

	dispatch_set(SSH2_MSG_KEX_DH_GEX_REQUEST, &kex_protocol_error);
	dispatch_set(SSH2_MSG_KEX_DH_GEX_REQUEST_OLD, &kex_protocol_error);
	dispatch_set(SSH2_MSG_KEX_DH_GEX_INIT, &kexgex_input_init);

	switch(type){
	case SSH2_MSG_KEX_DH_GEX_REQUEST:
		debug("SSH2_MSG_KEX_DH_GEX_REQUEST received");
		min = packet_get_int();
		state->nbits = packet_get_int();
		max = packet_get_int();
		min = MAX(DH_GRP_MIN, min);
		max = MIN(DH_GRP_MAX, max);
		state->min = min;
		state->max = max;
		break;
	case SSH2_MSG_KEX_DH_GEX_REQUEST_OLD:
		debug("SSH2_MSG_KEX_DH_GEX_REQUEST_OLD received");
		state->nbits = packet_get_int();
		min = DH_GRP_MIN;
		max = DH_GRP_MAX;
		/* unused for old GEX */
		state->min = -1;
		state->max = -1;
		break;
	}
	packet_done();

	if (max < min || state->nbits < min || max < state->nbits)
		fatal("DH_GEX_REQUEST, bad parameters: %d !< %d !< %d",
		    min, state->nbits, max);

	state->dh = choose_dh(min, state->nbits, max);
	if (state->dh == NULL)
		packet_disconnect("Protocol error: no matching DH grp found");

	debug("SSH2_MSG_KEX_DH_GEX_GROUP sent");
	packet_start(SSH2_MSG_KEX_DH_GEX_GROUP);
	packet_put_bignum2(state->dh->p);
	packet_put_bignum2(state->dh->g);
	packet_send();

	/* flush */
	packet_write_wait();

	/* Compute our exchange value in parallel with the client */
	dh_gen_key(state->dh, kex->we_need * 8);
}

void
kexgex_input_init(int type, int plen, void *ctxt)
{
	BIGNUM *shared_secret = NULL, *dh_client_pub = NULL;
	Key *server_host_key;
	Kex *kex = (Kex*) ctxt;
	State *state = (State *) kex->state;
	DH *dh = state->dh;
	u_char *kbuf, *hash, *signature = NULL, *server_host_key_blob = NULL;
	u_int sbloblen, klen, kout;
	int dlen, slen;

	if (kex->load_host_key == NULL)
		fatal("Cannot load hostkey");
	server_host_key = kex->load_host_key(kex->hostkey_type);
	if (server_host_key == NULL)
		fatal("Unsupported hostkey type %d", kex->hostkey_type);

	dispatch_set(SSH2_MSG_KEX_DH_GEX_INIT, &kex_protocol_error);
	debug("SSH2_MSG_KEX_DH_GEX_INIT received");

	/* key, cert */
	dh_client_pub = BN_new();
	if (dh_client_pub == NULL)
		fatal("dh_client_pub == NULL");
	packet_get_bignum2(dh_client_pub, &dlen);

#ifdef DEBUG_KEXDH
	fprintf(stderr, "dh_client_pub= ");
	BN_print_fp(stderr, dh_client_pub);
	fprintf(stderr, "\n");
	debug("bits %d", BN_num_bits(dh_client_pub));
#endif

#ifdef DEBUG_KEXDH
	DHparams_print_fp(stderr, dh);
	fprintf(stderr, "pub= ");
	BN_print_fp(stderr, dh->pub_key);
	fprintf(stderr, "\n");
#endif
	if (!dh_pub_is_valid(dh, dh_client_pub))
		packet_disconnect("bad client public DH value");

	klen = DH_size(dh);
	kbuf = xmalloc(klen);
	kout = DH_compute_key(kbuf, dh_client_pub, dh);
#ifdef DEBUG_KEXDH
        dump_digest("shared secret", kbuf, kout);
#endif
	shared_secret = BN_new();
	BN_bin2bn(kbuf, kout, shared_secret);
	memset(kbuf, 0, klen);
	xfree(kbuf);

	key_to_blob(server_host_key, &server_host_key_blob, &sbloblen);

	/* calc H */			/* XXX depends on 'kex' */
	hash = kexgex_hash(
	    kex->client_version_string,
	    kex->server_version_string,
	    buffer_ptr(&kex->peer), buffer_len(&kex->peer),
	    buffer_ptr(&kex->my), buffer_len(&kex->my),
	    (char *)server_host_key_blob, sbloblen,
	    state->min, state->nbits, state->max,
	    dh->p, dh->g,
	    dh_client_pub,
	    dh->pub_key,
	    shared_secret
	);
	BN_free(dh_client_pub);

	/* save session id := H */
	/* XXX hashlen depends on KEX */
	session_id2_len = 20;
	session_id2 = xmalloc(session_id2_len);
	memcpy(session_id2, hash, session_id2_len);

	/* sign H */
	/* XXX hashlen depends on KEX */
	key_sign(server_host_key, &signature, &slen, hash, 20);

	/* destroy_sensitive_data(); */

	/* send server hostkey, DH pubkey 'f' and singed H */
	debug("SSH2_MSG_KEX_DH_GEX_REPLY sent");
	packet_start(SSH2_MSG_KEX_DH_GEX_REPLY);
	packet_put_string((char *)server_host_key_blob, sbloblen);
	packet_put_bignum2(dh->pub_key);	/* f */
	packet_put_string((char *)signature, slen);
	packet_send();
	xfree(signature);
	xfree(server_host_key_blob);
	/* packet_write_wait(); */

	kex_derive_keys(kex, hash, shared_secret);
	BN_clear_free(shared_secret);
	packet_set_kex(kex);

	kex_send_newkeys();

	/* have keys, free DH */
	DH_free(dh);
	xfree(state);
	kex->state = NULL;
}

void
kexgex(Kex *kex)
{
	if (kex->server)
		kexgex_server(kex);
	else
		kexgex_client(kex);
}
