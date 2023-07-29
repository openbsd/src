/* $OpenBSD: enc.c,v 1.31 2023/07/29 17:15:45 tb Exp $ */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "apps.h"

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/objects.h>

int set_hex(char *in, unsigned char *out, int size);

#define SIZE	(512)
#define BSIZE	(8*1024)

static struct {
	int base64;
	char *bufsize;
	const EVP_CIPHER *cipher;
	int debug;
	int enc;
	char *hiv;
	char *hkey;
	char *hsalt;
	char *inf;
	int iter;
	char *keyfile;
	char *keystr;
	char *md;
	int nopad;
	int nosalt;
	int olb64;
	char *outf;
	char *passarg;
	int pbkdf2;
	int printkey;
	int verbose;
} cfg;

static int
enc_opt_cipher(int argc, char **argv, int *argsused)
{
	char *name = argv[0];

	if (*name++ != '-')
		return (1);

	if (strcmp(name, "none") == 0) {
		cfg.cipher = NULL;
		*argsused = 1;
		return (0);
	}

	if ((cfg.cipher = EVP_get_cipherbyname(name)) != NULL) {
		*argsused = 1;
		return (0);
	}

	return (1);
}

static const struct option enc_options[] = {
	{
		.name = "A",
		.desc = "Process base64 data on one line (requires -a)",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.olb64,
	},
	{
		.name = "a",
		.desc = "Perform base64 encoding/decoding (alias -base64)",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.base64,
	},
	{
		.name = "base64",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.base64,
	},
	{
		.name = "bufsize",
		.argname = "size",
		.desc = "Specify the buffer size to use for I/O",
		.type = OPTION_ARG,
		.opt.arg = &cfg.bufsize,
	},
	{
		.name = "d",
		.desc = "Decrypt the input data",
		.type = OPTION_VALUE,
		.opt.value = &cfg.enc,
		.value = 0,
	},
	{
		.name = "debug",
		.desc = "Print debugging information",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.debug,
	},
	{
		.name = "e",
		.desc = "Encrypt the input data (default)",
		.type = OPTION_VALUE,
		.opt.value = &cfg.enc,
		.value = 1,
	},
	{
		.name = "in",
		.argname = "file",
		.desc = "Input file to read from (default stdin)",
		.type = OPTION_ARG,
		.opt.arg = &cfg.inf,
	},
	{
		.name = "iter",
		.argname = "iterations",
		.desc = "Specify iteration count and force use of PBKDF2",
		.type = OPTION_ARG_INT,
		.opt.value = &cfg.iter,
	},
	{
		.name = "iv",
		.argname = "IV",
		.desc = "IV to use, specified as a hexadecimal string",
		.type = OPTION_ARG,
		.opt.arg = &cfg.hiv,
	},
	{
		.name = "K",
		.argname = "key",
		.desc = "Key to use, specified as a hexadecimal string",
		.type = OPTION_ARG,
		.opt.arg = &cfg.hkey,
	},
	{
		.name = "k",		/* Superseded by -pass. */
		.type = OPTION_ARG,
		.opt.arg = &cfg.keystr,
	},
	{
		.name = "kfile",	/* Superseded by -pass. */
		.type = OPTION_ARG,
		.opt.arg = &cfg.keyfile,
	},
	{
		.name = "md",
		.argname = "digest",
		.desc = "Digest to use to create a key from the passphrase",
		.type = OPTION_ARG,
		.opt.arg = &cfg.md,
	},
	{
		.name = "none",
		.desc = "Use NULL cipher (no encryption or decryption)",
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = enc_opt_cipher,
	},
	{
		.name = "nopad",
		.desc = "Disable standard block padding",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.nopad,
	},
	{
		.name = "nosalt",
		.type = OPTION_VALUE,
		.opt.value = &cfg.nosalt,
		.value = 1,
	},
	{
		.name = "out",
		.argname = "file",
		.desc = "Output file to write to (default stdout)",
		.type = OPTION_ARG,
		.opt.arg = &cfg.outf,
	},
	{
		.name = "P",
		.desc = "Print out the salt, key and IV used, then exit\n"
		    "  (no encryption or decryption is performed)",
		.type = OPTION_VALUE,
		.opt.value = &cfg.printkey,
		.value = 2,
	},
	{
		.name = "p",
		.desc = "Print out the salt, key and IV used",
		.type = OPTION_VALUE,
		.opt.value = &cfg.printkey,
		.value = 1,
	},
	{
		.name = "pass",
		.argname = "source",
		.desc = "Password source",
		.type = OPTION_ARG,
		.opt.arg = &cfg.passarg,
	},
	{
		.name = "pbkdf2",
		.desc = "Use the pbkdf2 key derivation function",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.pbkdf2,
	},
	{
		.name = "S",
		.argname = "salt",
		.desc = "Salt to use, specified as a hexadecimal string",
		.type = OPTION_ARG,
		.opt.arg = &cfg.hsalt,
	},
	{
		.name = "salt",
		.desc = "Use a salt in the key derivation routines (default)",
		.type = OPTION_VALUE,
		.opt.value = &cfg.nosalt,
		.value = 0,
	},
	{
		.name = "v",
		.desc = "Verbose",
		.type = OPTION_FLAG,
		.opt.flag = &cfg.verbose,
	},
	{
		.name = NULL,
		.type = OPTION_ARGV_FUNC,
		.opt.argvfunc = enc_opt_cipher,
	},
	{ NULL },
};

static void
skip_aead_and_xts(const OBJ_NAME *name, void *arg)
{
	const EVP_CIPHER *cipher;

	if ((cipher = EVP_get_cipherbyname(name->name)) == NULL)
		return;

	if ((EVP_CIPHER_flags(cipher) & EVP_CIPH_FLAG_AEAD_CIPHER) != 0)
		return;
	if (EVP_CIPHER_mode(cipher) == EVP_CIPH_XTS_MODE)
		return;

	show_cipher(name, arg);
}

static void
enc_usage(void)
{
	int n = 0;

	fprintf(stderr, "usage: enc -ciphername [-AadePp] [-base64] "
	    "[-bufsize number] [-debug]\n"
	    "    [-in file] [-iter iterations] [-iv IV] [-K key] "
            "[-k password]\n"
	    "    [-kfile file] [-md digest] [-none] [-nopad] [-nosalt]\n"
	    "    [-out file] [-pass source] [-pbkdf2] [-S salt] [-salt]\n\n");
	options_usage(enc_options);
	fprintf(stderr, "\n");

	fprintf(stderr, "Valid ciphername values:\n\n");
	OBJ_NAME_do_all_sorted(OBJ_NAME_TYPE_CIPHER_METH, skip_aead_and_xts, &n);
	fprintf(stderr, "\n");
}

int
enc_main(int argc, char **argv)
{
	static const char magic[] = "Salted__";
	char mbuf[sizeof magic - 1];
	char *strbuf = NULL, *pass = NULL;
	unsigned char *buff = NULL;
	int bsize = BSIZE;
	int ret = 1, inl;
	unsigned char key[EVP_MAX_KEY_LENGTH], iv[EVP_MAX_IV_LENGTH];
	unsigned char salt[PKCS5_SALT_LEN];
	EVP_CIPHER_CTX *ctx = NULL;
	const EVP_MD *dgst = NULL;
	BIO *in = NULL, *out = NULL, *b64 = NULL, *benc = NULL;
	BIO *rbio = NULL, *wbio = NULL;
#define PROG_NAME_SIZE  39
	char pname[PROG_NAME_SIZE + 1];
	int i;

	if (pledge("stdio cpath wpath rpath tty", NULL) == -1) {
		perror("pledge");
		exit(1);
	}

	memset(&cfg, 0, sizeof(cfg));
	cfg.enc = 1;

	/* first check the program name */
	program_name(argv[0], pname, sizeof(pname));

	if (strcmp(pname, "base64") == 0)
		cfg.base64 = 1;

	cfg.cipher = EVP_get_cipherbyname(pname);

	if (!cfg.base64 && cfg.cipher == NULL && strcmp(pname, "enc") != 0) {
		BIO_printf(bio_err, "%s is an unknown cipher\n", pname);
		goto end;
	}

	if (options_parse(argc, argv, enc_options, NULL, NULL) != 0) {
		enc_usage();
		goto end;
	}

	if (cfg.keyfile != NULL) {
		static char buf[128];
		FILE *infile;

		infile = fopen(cfg.keyfile, "r");
		if (infile == NULL) {
			BIO_printf(bio_err, "unable to read key from '%s'\n",
			    cfg.keyfile);
			goto end;
		}
		buf[0] = '\0';
		if (!fgets(buf, sizeof buf, infile)) {
			BIO_printf(bio_err, "unable to read key from '%s'\n",
			    cfg.keyfile);
			fclose(infile);
			goto end;
		}
		fclose(infile);
		i = strlen(buf);
		if (i > 0 && (buf[i - 1] == '\n' || buf[i - 1] == '\r'))
			buf[--i] = '\0';
		if (i > 0 && (buf[i - 1] == '\n' || buf[i - 1] == '\r'))
			buf[--i] = '\0';
		if (i < 1) {
			BIO_printf(bio_err, "zero length password\n");
			goto end;
		}
		cfg.keystr = buf;
	}

	if (cfg.cipher != NULL &&
	    (EVP_CIPHER_flags(cfg.cipher) & EVP_CIPH_FLAG_AEAD_CIPHER) != 0) {
		BIO_printf(bio_err, "enc does not support AEAD ciphers\n");
		goto end;
	}

	if (cfg.cipher != NULL &&
	    EVP_CIPHER_mode(cfg.cipher) == EVP_CIPH_XTS_MODE) {
		BIO_printf(bio_err, "enc does not support XTS mode\n");
		goto end;
	}

	if (cfg.md != NULL &&
	    (dgst = EVP_get_digestbyname(cfg.md)) == NULL) {
		BIO_printf(bio_err,
		    "%s is an unsupported message digest type\n",
		    cfg.md);
		goto end;
	}
	if (dgst == NULL)
		dgst = EVP_sha256();

	if (cfg.bufsize != NULL) {
		char *p = cfg.bufsize;
		unsigned long n;

		/* XXX - provide an OPTION_ARG_DISKUNIT. */
		for (n = 0; *p != '\0'; p++) {
			i = *p;
			if ((i <= '9') && (i >= '0'))
				n = n * 10 + i - '0';
			else if (i == 'k') {
				n *= 1024;
				p++;
				break;
			}
		}
		if (*p != '\0') {
			BIO_printf(bio_err, "invalid 'bufsize' specified.\n");
			goto end;
		}
		/* It must be large enough for a base64 encoded line. */
		if (cfg.base64 && n < 80)
			n = 80;

		bsize = (int)n;
		if (cfg.verbose)
			BIO_printf(bio_err, "bufsize=%d\n", bsize);
	}
	strbuf = malloc(SIZE);
	buff = malloc(EVP_ENCODE_LENGTH(bsize));
	if (buff == NULL || strbuf == NULL) {
		BIO_printf(bio_err, "malloc failure %ld\n", (long) EVP_ENCODE_LENGTH(bsize));
		goto end;
	}
	in = BIO_new(BIO_s_file());
	out = BIO_new(BIO_s_file());
	if (in == NULL || out == NULL) {
		ERR_print_errors(bio_err);
		goto end;
	}
	if (cfg.debug) {
		BIO_set_callback(in, BIO_debug_callback);
		BIO_set_callback(out, BIO_debug_callback);
		BIO_set_callback_arg(in, (char *) bio_err);
		BIO_set_callback_arg(out, (char *) bio_err);
	}
	if (cfg.inf == NULL) {
		if (cfg.bufsize != NULL)
			setvbuf(stdin, (char *) NULL, _IONBF, 0);
		BIO_set_fp(in, stdin, BIO_NOCLOSE);
	} else {
		if (BIO_read_filename(in, cfg.inf) <= 0) {
			perror(cfg.inf);
			goto end;
		}
	}

	if (!cfg.keystr && cfg.passarg) {
		if (!app_passwd(bio_err, cfg.passarg, NULL, &pass, NULL)) {
			BIO_printf(bio_err, "Error getting password\n");
			goto end;
		}
		cfg.keystr = pass;
	}
	if (cfg.keystr == NULL && cfg.cipher != NULL && cfg.hkey == NULL) {
		for (;;) {
			char buf[200];
			int retval;

			retval = snprintf(buf, sizeof buf,
			    "enter %s %s password:",
			    OBJ_nid2ln(EVP_CIPHER_nid(cfg.cipher)),
			    cfg.enc ? "encryption" : "decryption");
			if ((size_t)retval >= sizeof buf) {
				BIO_printf(bio_err,
				    "Password prompt too long\n");
				goto end;
			}
			strbuf[0] = '\0';
			i = EVP_read_pw_string((char *)strbuf, SIZE, buf,
			    cfg.enc);
			if (i == 0) {
				if (strbuf[0] == '\0') {
					ret = 1;
					goto end;
				}
				cfg.keystr = strbuf;
				break;
			}
			if (i < 0) {
				BIO_printf(bio_err, "bad password read\n");
				goto end;
			}
		}
	}
	if (cfg.outf == NULL) {
		BIO_set_fp(out, stdout, BIO_NOCLOSE);
		if (cfg.bufsize != NULL)
			setvbuf(stdout, (char *)NULL, _IONBF, 0);
	} else {
		if (BIO_write_filename(out, cfg.outf) <= 0) {
			perror(cfg.outf);
			goto end;
		}
	}

	rbio = in;
	wbio = out;

	if (cfg.base64) {
		if ((b64 = BIO_new(BIO_f_base64())) == NULL)
			goto end;
		if (cfg.debug) {
			BIO_set_callback(b64, BIO_debug_callback);
			BIO_set_callback_arg(b64, (char *) bio_err);
		}
		if (cfg.olb64)
			BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
		if (cfg.enc)
			wbio = BIO_push(b64, wbio);
		else
			rbio = BIO_push(b64, rbio);
	}
	if (cfg.cipher != NULL) {
		/*
		 * Note that keystr is NULL if a key was passed on the command
		 * line, so we get no salt in that case. Is this a bug?
		 */
		if (cfg.keystr != NULL) {
			/*
			 * Salt handling: if encrypting generate a salt and
			 * write to output BIO. If decrypting read salt from
			 * input BIO.
			 */
			unsigned char *sptr;
			if (cfg.nosalt)
				sptr = NULL;
			else {
				if (cfg.enc) {
					if (cfg.hsalt) {
						if (!set_hex(cfg.hsalt, salt, sizeof salt)) {
							BIO_printf(bio_err,
							    "invalid hex salt value\n");
							goto end;
						}
					} else
						arc4random_buf(salt,
						    sizeof(salt));
					/*
					 * If -P option then don't bother
					 * writing
					 */
					if ((cfg.printkey != 2)
					    && (BIO_write(wbio, magic,
						    sizeof magic - 1) != sizeof magic - 1
						|| BIO_write(wbio,
						    (char *) salt,
						    sizeof salt) != sizeof salt)) {
						BIO_printf(bio_err, "error writing output file\n");
						goto end;
					}
				} else if (BIO_read(rbio, mbuf, sizeof mbuf) != sizeof mbuf
					    || BIO_read(rbio,
						(unsigned char *) salt,
					sizeof salt) != sizeof salt) {
					BIO_printf(bio_err, "error reading input file\n");
					goto end;
				} else if (memcmp(mbuf, magic, sizeof magic - 1)) {
					BIO_printf(bio_err, "bad magic number\n");
					goto end;
				}
				sptr = salt;
			}
			if (cfg.pbkdf2 == 1 || cfg.iter > 0) {
				/*
				 * derive key and default iv
				 * concatenated into a temporary buffer
				 */
				unsigned char tmpkeyiv[EVP_MAX_KEY_LENGTH + EVP_MAX_IV_LENGTH];
				int iklen = EVP_CIPHER_key_length(cfg.cipher);
				int ivlen = EVP_CIPHER_iv_length(cfg.cipher);
				/* not needed if HASH_UPDATE() is fixed : */
				int islen = (sptr != NULL ? sizeof(salt) : 0);

				if (cfg.iter == 0)
					cfg.iter = 10000;

				if (!PKCS5_PBKDF2_HMAC(cfg.keystr,
					strlen(cfg.keystr), sptr, islen,
					cfg.iter, dgst, iklen+ivlen, tmpkeyiv)) {
					BIO_printf(bio_err, "PKCS5_PBKDF2_HMAC failed\n");
					goto end;
				}
				/* split and move data back to global buffer */
				memcpy(key, tmpkeyiv, iklen);
				memcpy(iv, tmpkeyiv + iklen, ivlen);
				explicit_bzero(tmpkeyiv, sizeof tmpkeyiv);
			} else {
				EVP_BytesToKey(cfg.cipher, dgst, sptr,
				    (unsigned char *)cfg.keystr,
				    strlen(cfg.keystr), 1, key, iv);
			}

			/*
			 * zero the complete buffer or the string passed from
			 * the command line bug picked up by Larry J. Hughes
			 * Jr. <hughes@indiana.edu>
			 */
			if (cfg.keystr == strbuf)
				explicit_bzero(cfg.keystr, SIZE);
			else
				explicit_bzero(cfg.keystr,
				    strlen(cfg.keystr));
		}
		if (cfg.hiv != NULL && !set_hex(cfg.hiv, iv, sizeof iv)) {
			BIO_printf(bio_err, "invalid hex iv value\n");
			goto end;
		}
		if (cfg.hiv == NULL && cfg.keystr == NULL &&
		    EVP_CIPHER_iv_length(cfg.cipher) != 0) {
			/*
			 * No IV was explicitly set and no IV was generated
			 * during EVP_BytesToKey. Hence the IV is undefined,
			 * making correct decryption impossible.
			 */
			BIO_printf(bio_err, "iv undefined\n");
			goto end;
		}
		if (cfg.hkey != NULL && !set_hex(cfg.hkey, key, sizeof key)) {
			BIO_printf(bio_err, "invalid hex key value\n");
			goto end;
		}
		if ((benc = BIO_new(BIO_f_cipher())) == NULL)
			goto end;

		/*
		 * Since we may be changing parameters work on the encryption
		 * context rather than calling BIO_set_cipher().
		 */

		BIO_get_cipher_ctx(benc, &ctx);

		if (!EVP_CipherInit_ex(ctx, cfg.cipher, NULL, NULL,
		    NULL, cfg.enc)) {
			BIO_printf(bio_err, "Error setting cipher %s\n",
			    EVP_CIPHER_name(cfg.cipher));
			ERR_print_errors(bio_err);
			goto end;
		}
		if (cfg.nopad)
			EVP_CIPHER_CTX_set_padding(ctx, 0);

		if (!EVP_CipherInit_ex(ctx, NULL, NULL, key, iv, cfg.enc)) {
			BIO_printf(bio_err, "Error setting cipher %s\n",
			    EVP_CIPHER_name(cfg.cipher));
			ERR_print_errors(bio_err);
			goto end;
		}
		if (cfg.debug) {
			BIO_set_callback(benc, BIO_debug_callback);
			BIO_set_callback_arg(benc, (char *) bio_err);
		}
		if (cfg.printkey) {
			int key_len, iv_len;

			if (!cfg.nosalt) {
				printf("salt=");
				for (i = 0; i < (int) sizeof(salt); i++)
					printf("%02X", salt[i]);
				printf("\n");
			}
			key_len = EVP_CIPHER_key_length(cfg.cipher);
			if (key_len > 0) {
				printf("key=");
				for (i = 0; i < key_len; i++)
					printf("%02X", key[i]);
				printf("\n");
			}
			iv_len = EVP_CIPHER_iv_length(cfg.cipher);
			if (iv_len > 0) {
				printf("iv =");
				for (i = 0; i < iv_len; i++)
					printf("%02X", iv[i]);
				printf("\n");
			}
			if (cfg.printkey == 2) {
				ret = 0;
				goto end;
			}
		}
	}
	/* Only encrypt/decrypt as we write the file */
	if (benc != NULL)
		wbio = BIO_push(benc, wbio);

	for (;;) {
		inl = BIO_read(rbio, (char *) buff, bsize);
		if (inl <= 0)
			break;
		if (BIO_write(wbio, (char *) buff, inl) != inl) {
			BIO_printf(bio_err, "error writing output file\n");
			goto end;
		}
	}
	if (!BIO_flush(wbio)) {
		BIO_printf(bio_err, "bad decrypt\n");
		goto end;
	}
	ret = 0;
	if (cfg.verbose) {
		BIO_printf(bio_err, "bytes read   :%8ld\n", BIO_number_read(in));
		BIO_printf(bio_err, "bytes written:%8ld\n", BIO_number_written(out));
	}
 end:
	ERR_print_errors(bio_err);
	free(strbuf);
	free(buff);
	BIO_free(in);
	BIO_free_all(out);
	BIO_free(benc);
	BIO_free(b64);
	free(pass);

	return (ret);
}

int
set_hex(char *in, unsigned char *out, int size)
{
	int i, n;
	unsigned char j;

	n = strlen(in);
	if (n > (size * 2)) {
		BIO_printf(bio_err, "hex string is too long\n");
		return (0);
	}
	memset(out, 0, size);
	for (i = 0; i < n; i++) {
		j = (unsigned char) *in;
		*(in++) = '\0';
		if (j == 0)
			break;
		if (j >= '0' && j <= '9')
			j -= '0';
		else if (j >= 'A' && j <= 'F')
			j = j - 'A' + 10;
		else if (j >= 'a' && j <= 'f')
			j = j - 'a' + 10;
		else {
			BIO_printf(bio_err, "non-hex digit\n");
			return (0);
		}
		if (i & 1)
			out[i / 2] |= j;
		else
			out[i / 2] = (j << 4);
	}
	return (1);
}
