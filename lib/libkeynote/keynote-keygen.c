/* $OpenBSD: keynote-keygen.c,v 1.6 1999/10/09 06:59:37 angelos Exp $ */
/*
 * The author of this code is Angelos D. Keromytis (angelos@dsl.cis.upenn.edu)
 *
 * This code was written by Angelos D. Keromytis in Philadelphia, PA, USA,
 * in April-May 1998
 *
 * Copyright (C) 1998, 1999 by Angelos D. Keromytis.
 *	
 * Permission to use, copy, and modify this software without fee
 * is hereby granted, provided that this entire notice is included in
 * all copies of any software which is or includes a copy or
 * modification of this software. 
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTY. IN PARTICULAR, THE AUTHORS MAKES NO
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE
 * MERCHANTABILITY OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR
 * PURPOSE.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#if STDC_HEADERS
#include <string.h>
#endif /* STDC_HEADERS */

#if HAVE_FCNTL_H
#include <fcntl.h>
#endif /* HAVE_FCNTL_H */

#if HAVE_IO_H
#include <io.h>
#elif HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_IO_H */

#include "header.h"
#include "keynote.h"
#include "assertion.h"
#include "signature.h"

void
keygenusage(void)
{
    fprintf(stderr, "Arguments:\n");
    fprintf(stderr, "\t<AlgorithmName> <keysize> "
	    "<PublicKeyFile> <PrivateKeyFile> [<printf-offset> "
	    "<print-length>]\n");
}

/*
 * Print the specified number of spaces.
 */
void
print_space(FILE *fp, int n)
{
    while (n--)
      fprintf(fp, " ");
}

/*
 * Output a key, properly formatted.
 */
void
print_key(FILE *fp, char *algname, char *key, int start, int length)
{
    int i, k;

    print_space(fp, start);
    fprintf(fp, "\"%s", algname);

    for (i = 0, k = strlen(algname) + 2; i < strlen(key); i++, k++)
    {
	if (k == length)
	{
	    if (i == strlen(key))
	    {
		fprintf(fp, "\"\n");
		return;
	    }

	    fprintf(fp, "\\\n");
	    print_space(fp, start);
	    i--;
	    k = 0;
	}
	else
	  fprintf(fp, "%c", key[i]);
    }

    fprintf(fp, "\"\n");
}

void
keynote_keygen(int argc, char *argv[])
{
    int begin = KEY_PRINT_OFFSET, prlen = KEY_PRINT_LENGTH;
#if defined(CRYPTO) || defined(PGPLIB)
    char *foo, *privalgname, seed[SEED_LEN];
    int alg, enc, ienc, len = 0, counter;
    struct keynote_deckey dc;
    unsigned long h;
    DSA *dsa;
    RSA *rsa;
    FILE *fp;
    int fd, cnt = RND_BYTES;
#endif /* CRYPTO || PGPLIB */
    char *algname;

    if ((argc != 5) && (argc != 6) && (argc != 7))
    {
	keygenusage();
	exit(0);
    }

    /* Fix algorithm name */
    if (argv[1][strlen(argv[1]) - 1] != ':')
    {
        fprintf(stderr, "Algorithm name [%s] should be terminated with a "
		"colon, fixing.\n", argv[1]);
	algname = (char *) calloc(strlen(argv[1]) + 2, sizeof(char));
	if (algname == (char *) NULL)
	{
	    perror("calloc()");
	    exit(-1);
	}

	strcpy(algname, argv[1]);
	algname[strlen(algname)] = ':';
    }
    else
	algname = argv[1];

    if (argc > 5)
    {
	begin = atoi(argv[5]);
	if (begin <= -1)
	{
	    fprintf(stderr, "Erroneous value for print-offset parameter.\n");
	    exit(-1);
	}
    }

    if (argc > 6)
    {
	prlen = atoi(argv[6]);
	if (prlen <= 0)
	{
	    fprintf(stderr, "Erroneous value for print-length parameter.\n");
	    exit(-1);
	}
    }

    if (strlen(algname) + 2 > prlen)
    {
	fprintf(stderr, "Parameter ``print-length'' should be larger "
		"than the length of AlgorithmName (%d)\n", strlen(algname));
	exit(-1);
    }

#if defined(CRYPTO) || defined(PGPLIB)
    alg = keynote_get_key_algorithm(algname, &enc, &ienc);
    len = atoi(argv[2]);

    if (len <= 0)
    {
	fprintf(stderr, "Invalid specified keysize %d\n", len);
	exit(-1);
    }

    fd = open(KEYNOTERNDFILENAME, O_RDONLY, 0);
    if (fd < 0)
    {
	perror(KEYNOTERNDFILENAME);
	exit(-1);
    }

    for (h = 0; h < 5; h++)
    {
	if (read(fd, seed, SEED_LEN) <= 0)
	{
	    perror("read()");
	    exit(-1);
	}

	RAND_seed(seed, SEED_LEN);
    }

    if (read(fd, seed, SEED_LEN) < SEED_LEN)
    {
	perror("read()");
	exit(-1);
    }

    close(fd);

    /* Make sure we read RND_BYTES bytes */
    do
    {
        if ((fd = RAND_load_file(KEYNOTERNDFILENAME, cnt)) <= 0)
        {
	    perror(KEYNOTERNDFILENAME);
	    exit(-1);
        }

	cnt -= fd;
    } while (cnt > 0);


    if ((alg == KEYNOTE_ALGORITHM_DSA) &&
	(ienc == INTERNAL_ENC_ASN1) &&
	((enc == ENCODING_HEX) || (enc == ENCODING_BASE64)))
    {
	dsa = DSA_generate_parameters(len, seed, SEED_LEN, &counter, &h, NULL
#if SSLEAY_VERSION_NUMBER >= 0x0900
				      , NULL
#endif /* SSLEAY_VERSION_NUMBER */
				     );

	if (dsa == (DSA *) NULL)
	{
	    ERR_print_errors_fp(stderr);
	    exit(-1);
	}

	if (DSA_generate_key(dsa) != 1)
	{
	    ERR_print_errors_fp(stderr);
	    exit(-1);
	}

	dc.dec_algorithm = KEYNOTE_ALGORITHM_DSA;
	dc.dec_key = (void *) dsa;

	foo = kn_encode_key(&dc, ienc, enc, KEYNOTE_PUBLIC_KEY);
	if (foo == (char *) NULL)
	{
	    fprintf(stderr, "Error encoding key (errno %d)\n", keynote_errno);
	    exit(-1);
	}

	if (!strcmp(argv[3], "-"))
	  fp = stdout;
	else
	{
	    fp = fopen(argv[3], "w");
	    if (fp == (FILE *) NULL)
	    {
		perror(argv[3]);
		exit(-1);
	    }
	}

	print_key(fp, algname, foo, begin, prlen);
	free(foo);

	if (strcmp(argv[3], "-"))
	  fclose(fp);

	foo = kn_encode_key(&dc, ienc, enc, KEYNOTE_PRIVATE_KEY);
	if (foo == (char *) NULL)
	{
	    fprintf(stderr, "Error encoding key (errno %d)\n", keynote_errno);
	    exit(-1);
	}

	if (!strcmp(argv[4], "-"))
	{
	    fp = stdout;
	    if (!strcmp(argv[3], "-"))
	      printf("===========================\n");
	}
	else
	{
	    fp = fopen(argv[4], "w");
	    if (fp == (FILE *) NULL)
	    {
		perror(argv[4]);
		exit(-1);
	    }
	}

	privalgname = (char *) calloc(strlen(KEYNOTE_PRIVATE_KEY_PREFIX) +
				      strlen(foo) + 1, sizeof(char));
	if (privalgname == (char *) NULL)
	{
	    perror("calloc()");
	    exit(-1);
	}
	sprintf(privalgname, "%s%s", KEYNOTE_PRIVATE_KEY_PREFIX, algname);
	print_key(fp, privalgname, foo, begin, prlen);
	free(privalgname);
	free(foo);

	if (strcmp(argv[4], "-"))
	  fclose(fp);

	exit(0);
    }

    if ((alg == KEYNOTE_ALGORITHM_RSA) &&
	(ienc == INTERNAL_ENC_PKCS1) &&
	((enc == ENCODING_HEX) || (enc == ENCODING_BASE64)))
    {
	rsa = RSA_generate_key(len, DEFAULT_PUBLIC, NULL
#if SSLEAY_VERSION_NUMBER >= 0x0900
			       , NULL
#endif /* SSLEAY_VERSION_NUMBER */
				     );

	if (rsa == (RSA *) NULL)
	{
	    ERR_print_errors_fp(stderr);
	    exit(-1);
	}

	dc.dec_algorithm = KEYNOTE_ALGORITHM_RSA;
	dc.dec_key = (void *) rsa;

	foo = kn_encode_key(&dc, ienc, enc, KEYNOTE_PUBLIC_KEY);
	if (foo == (char *) NULL)
	{
	    fprintf(stderr, "Error encoding key (errno %d)\n", keynote_errno);
	    exit(-1);
	}

	if (!strcmp(argv[3], "-"))
	  fp = stdout;
	else
	{
	    fp = fopen(argv[3], "w");
	    if (fp == (FILE *) NULL)
	    {
		perror(argv[3]);
		exit(-1);
	    }
	}

	print_key(fp, algname, foo, begin, prlen);
	free(foo);

	if (strcmp(argv[3], "-"))
	  fclose(fp);

	foo = kn_encode_key(&dc, ienc, enc, KEYNOTE_PRIVATE_KEY);
	if (foo == (char *) NULL)
	{
	    fprintf(stderr, "Error encoding key (errno %d)\n", keynote_errno);
	    exit(-1);
	}

	if (!strcmp(argv[4], "-"))
	{
	    fp = stdout;
	    if (!strcmp(argv[3], "-"))
	      printf("===========================\n");
	}
	else
	{
	    fp = fopen(argv[4], "w");
	    if (fp == (FILE *) NULL)
	    {
		perror(argv[4]);
		exit(-1);
	    }
	}

	privalgname = (char *) calloc(strlen(KEYNOTE_PRIVATE_KEY_PREFIX) +
				      strlen(foo) + 1, sizeof(char));
	if (privalgname == (char *) NULL)
	{
	    perror("calloc()");
	    exit(-1);
	}
	sprintf(privalgname, "%s%s", KEYNOTE_PRIVATE_KEY_PREFIX, algname);
	print_key(fp, privalgname, foo, begin, prlen);
	free(privalgname);
	free(foo);

	if (strcmp(argv[4], "-"))
	  fclose(fp);

	exit(0);
    }

    /* More algorithms here */
#endif /* CRYPTO */

    fprintf(stderr, "Unknown/unsupported algorithm [%s]\n", algname);
    exit(-1);
}
