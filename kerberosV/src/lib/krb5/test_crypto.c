/*
 * Copyright (c) 2003 Kungliga Tekniska Högskolan
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
 * 3. Neither the name of KTH nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY KTH AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL KTH OR ITS CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

#include "krb5_locl.h"
#include <err.h>
#include <getarg.h>

RCSID("$KTH: test_crypto.c,v 1.3 2005/04/04 17:54:50 lha Exp $");

static void
time_encryption(krb5_context context, size_t size,
		krb5_enctype etype, int iterations)
{
    struct timeval tv1, tv2;
    krb5_error_code ret;
    krb5_keyblock key;
    krb5_crypto crypto;
    krb5_data data;
    char *etype_name;
    void *buf;
    int i;

    ret = krb5_generate_random_keyblock(context, etype, &key);
    if (ret)
	krb5_err(context, 1, ret, "krb5_generate_random_keyblock");

    ret = krb5_enctype_to_string(context, etype, &etype_name);
    if (ret)
	krb5_err(context, 1, ret, "krb5_enctype_to_string");

    buf = malloc(size);
    if (buf == NULL)
	krb5_errx(context, 1, "out of memory");
    memset(buf, 0, size);

    ret = krb5_crypto_init(context, &key, 0, &crypto);
    if (ret)
	krb5_err(context, 1, ret, "krb5_crypto_init");

    gettimeofday(&tv1, NULL);

    for (i = 0; i < iterations; i++) {
	ret = krb5_encrypt(context, crypto, 0, buf, size, &data);
	if (ret)
	    krb5_err(context, 1, ret, "encrypt: %d", i);
	krb5_data_free(&data);
    }

    gettimeofday(&tv2, NULL);

    timevalsub(&tv2, &tv1);

    printf("%s size: %7lu iterations: %d time: %3ld.%06ld\n", 
	   etype_name, (unsigned long)size, iterations,
	   (long)tv2.tv_sec, (long)tv2.tv_usec);

    krb5_crypto_destroy(context, crypto);
    krb5_free_keyblock_contents(context, &key);
}



static int version_flag = 0;
static int help_flag	= 0;

static struct getargs args[] = {
    {"version",	0,	arg_flag,	&version_flag,
     "print version", NULL },
    {"help",	0,	arg_flag,	&help_flag,
     NULL, NULL }
};

static void
usage (int ret)
{
    arg_printusage (args,
		    sizeof(args)/sizeof(*args),
		    NULL,
		    "");
    exit (ret);
}

int
main(int argc, char **argv)
{
    krb5_context context;
    krb5_error_code ret;
    int i, iterations;
    int optind = 0;

    krb5_enctype enctypes[] = { 
	ETYPE_DES_CBC_CRC,
	ETYPE_DES3_CBC_SHA1,
	ETYPE_ARCFOUR_HMAC_MD5,
	ETYPE_AES128_CTS_HMAC_SHA1_96,
	ETYPE_AES256_CTS_HMAC_SHA1_96
    };

    setprogname(argv[0]);

    if(getarg(args, sizeof(args) / sizeof(args[0]), argc, argv, &optind))
	usage(1);
    
    if (help_flag)
	usage (0);

    if(version_flag){
	print_version(NULL);
	exit(0);
    }

    argc -= optind;
    argv += optind;


    ret = krb5_init_context(&context);
    if (ret)
	errx (1, "krb5_init_context failed: %d", ret);

    iterations = 1000;

    for (i = 0; i < sizeof(enctypes)/sizeof(enctypes[0]); i++) {
	time_encryption(context, 16, enctypes[i], iterations);
	time_encryption(context, 32, enctypes[i], iterations);
	time_encryption(context, 512, enctypes[i], iterations);
	time_encryption(context, 1024, enctypes[i], iterations);
	time_encryption(context, 2048, enctypes[i], iterations);
	time_encryption(context, 4096, enctypes[i], iterations);
	time_encryption(context, 8192, enctypes[i], iterations);
	time_encryption(context, 16384, enctypes[i], iterations);
	time_encryption(context, 32768, enctypes[i], iterations);
    }

    krb5_free_context(context);

    return 0;
}
