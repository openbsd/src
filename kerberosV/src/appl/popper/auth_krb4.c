/*
 * Copyright (c) 2004 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 *
 * 3. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */

#include <popper.h>
#include <base64.h>
#include <pop_auth.h>
RCSID("$KTH: auth_krb4.c,v 1.2 2004/06/15 11:24:21 joda Exp $");


#if defined(SASL) && defined(KRB4)
#include <krb.h>
#include <des.h>

struct krb4_state {
    int stage;
    u_int32_t nonce;
};

static int
krb4_loop(POP *p, void *state, 
	 /* const */ void *input, size_t input_length,
	 void **output, size_t *output_length)
{
    struct krb4_state *ks = state;

    int ret;
    des_cblock key;
    unsigned char *data;
    char instance[INST_SZ];  
    des_key_schedule schedule;

    if(ks->stage == 0) {
	if(input_length > 0)
	    return POP_AUTH_FAILURE;
	/* S -> C: 32 bit nonce in MSB base64 */
#ifdef HAVE_OPENSSL
#define des_new_random_key des_random_key
#endif
	des_new_random_key(key);
	ks->nonce = (key[0] | (key[1] << 8) | (key[2] << 16) | (key[3] << 24)
		     | key[4] | (key[5] << 8) | (key[6] << 16) | (key[7] << 24));
	*output = malloc(4);
	if(*output == NULL) {
	    pop_auth_set_error("out of memory");
	    return POP_AUTH_FAILURE;
	}
	krb_put_int(ks->nonce, *output, 4, 4);
	*output_length = 4;
	ks->stage++;
	return POP_AUTH_CONTINUE;
    }

    if(ks->stage == 1) {
	KTEXT_ST authent;
	/* C -> S: ticket and authenticator */

	if (input_length > sizeof(authent.dat)) {
	    pop_auth_set_error("data packet too long");
	    return POP_AUTH_FAILURE;
	}
	memcpy(authent.dat, input, input_length);
	authent.length = input_length;

	k_getsockinst (0, instance, sizeof(instance));
	ret = krb_rd_req(&authent, "pop", instance,
			 0 /* XXX p->in_addr.sin_addr.s_addr */,
			 &p->kdata, NULL);
	if (ret != 0) {
	    pop_auth_set_error(krb_get_err_text(ret));
	    return POP_AUTH_FAILURE;
	}
	if (p->kdata.checksum != ks->nonce) {
	    pop_auth_set_error("data stream modified");
	    return POP_AUTH_FAILURE;
	}
	/* S -> C: nonce + 1 | bit | max segment */

	*output = malloc(8);
	if(*output == NULL) {
	    pop_auth_set_error("out of memory");
	    return POP_AUTH_FAILURE;
	}
	data = *output;
	krb_put_int(ks->nonce + 1, data, 8, 4);
	data[4] = 1;
	data[5] = 0;
	data[6] = 0;
	data[7] = 0;
	des_key_sched(&p->kdata.session, schedule);
	des_pcbc_encrypt((des_cblock*)data,
			 (des_cblock*)data, 8,
			 schedule,
			 &p->kdata.session,
			 DES_ENCRYPT);
	*output_length = 8;
	ks->stage++;
	return POP_AUTH_CONTINUE;
    }

    if(ks->stage == 2) {
	u_int32_t nonce_reply;
	/* C -> S: nonce | bit | max segment | username */

	if (input_length % 8 != 0) {
	    pop_auth_set_error("reply is not a multiple of 8 bytes");
	    return POP_AUTH_FAILURE;
	}

	des_key_sched(&p->kdata.session, schedule);
	des_pcbc_encrypt((des_cblock*)input,
			 (des_cblock*)input,
			 input_length,
			 schedule,
			 &p->kdata.session,
			 DES_DECRYPT);

	data = input;
	krb_get_int(data, &nonce_reply, 4, 0);
	if (nonce_reply != ks->nonce) {
	    pop_auth_set_error("data stream modified");
	    return POP_AUTH_FAILURE;
	}
	if(data[4] != 1) {

	}
	if(data[input_length - 1] != '\0') {
	    pop_auth_set_error("bad format of username");
	    return POP_AUTH_FAILURE;
	}
	strlcpy(p->user, data + 8, sizeof(p->user));
	if (kuserok(&p->kdata, p->user)) {
	    pop_log(p, POP_PRIORITY,
		    "%s: (%s.%s@%s) tried to retrieve mail for %s.",
		    p->client, p->kdata.pname, p->kdata.pinst,
		    p->kdata.prealm, p->user);
	    pop_auth_set_error("Permission denied");
	    return POP_AUTH_FAILURE;
	}
	pop_log(p, POP_INFO, "%s: %s.%s@%s -> %s",
		p->ipaddr,
		p->kdata.pname, p->kdata.pinst, p->kdata.prealm,
		p->user);
	return POP_AUTH_COMPLETE;
    }
    return POP_AUTH_FAILURE;
}


static int
krb4_init(POP *p, void **state)
{
    struct krb4_state *ks = malloc(sizeof(*ks));
    if(ks == NULL) {
	pop_auth_set_error("out of memory");
	return POP_AUTH_FAILURE;
    }
    ks->stage = 0;
    *state = ks;
    return POP_AUTH_CONTINUE;
}

static int
krb4_cleanup(POP *p, void *state)
{
    free(state);
    return POP_AUTH_CONTINUE;
}

struct auth_mech krb4_mech = {
    "KERBEROS_V4", krb4_init, krb4_loop, krb4_cleanup
};

#endif /* KRB5 */
