/*
 * Copyright (c) 2001 Kungliga Tekniska Högskolan
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

#include "krb_locl.h"

RCSID("$KTH: random_block.c,v 1.1 2001/08/26 01:46:51 assar Exp $");

#ifdef HAVE_OPENSSL
#include <openssl/rand.h>

/* From openssl/crypto/rand/rand_lcl.h */
#define ENTROPY_NEEDED 20
static int
seed_something(void)
{
    int fd = -1;
    char buf[1024], seedfile[256];

    /* If there is a seed file, load it. But such a file cannot be trusted,
       so use 0 for the entropy estimate */
    if (RAND_file_name(seedfile, sizeof(seedfile))) {
	fd = open(seedfile, O_RDONLY);
	if (fd >= 0) {
	    read(fd, buf, sizeof(buf));
	    /* Use the full buffer anyway */
	    RAND_add(buf, sizeof(buf), 0.0);
	} else
	    seedfile[0] = '\0';
    } else
	seedfile[0] = '\0';

    /* Calling RAND_status() will try to use /dev/urandom if it exists so
       we do not have to deal with it. */
    if (RAND_status() != 1) {

	const char *p;

	/* Try using egd */
	p = krb_get_config_string("egd_socket");
	if (p != NULL)
	    RAND_egd_bytes(p, ENTROPY_NEEDED);
    }
    
    if (RAND_status() == 1)	{
	/* Update the seed file */
	if (seedfile[0])
	    RAND_write_file(seedfile);

	return 0;
    } else
	return -1;
}

void
krb_generate_random_block(void *buf, size_t len)
{
    static int rng_initialized = 0;
    
    if (!rng_initialized) {
	if (seed_something()) {
	    fprintf(stderr, "Could not initialize openssl rng\n");
	    exit(1);
	}
	
	rng_initialized = 1;
    }
    RAND_bytes(buf, len);
}

#else /* !HAVE_OPENSSL */

void
krb_generate_random_block(void *buf, size_t len)
{
    des_cblock key, out;
    static des_cblock counter;
    static des_key_schedule schedule;
    int i;
    static int initialized = 0;

    if(!initialized) {
	des_new_random_key(&key);
	des_set_key(&key, schedule);
	memset(&key, 0, sizeof(key));
	des_new_random_key(&counter);
    }
    while(len > 0) {
	des_ecb_encrypt(&counter, &out, schedule, DES_ENCRYPT);
	for(i = 7; i >=0; i--)
	    if(counter[i]++)
		break;
	memcpy(buf, out, min(len, sizeof(out)));
	len -= min(len, sizeof(out));
	buf = (char*)buf + sizeof(out);
    }
}

#endif /* !HAVE_OPENSSL */
