/*
 * X9.9 calculator
 * This software is provided AS IS with no express or implied warranty
 * October 1995, Paul Borman <prb@krystal.com>
 */
#include <sys/param.h>
#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <pwd.h>
#include <readpassphrase.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <des.h>

#define	KEYFILE		".keyfile.des"
#define	HEXDIGITS	"0123456789abcdef"
#define	DECDIGITS	"0123456789012345"

void predict(des_key_schedule, char *, int);

char *digits = HEXDIGITS;
extern char *__progname;

int
main(int argc, char **argv)
{
	int i;
	char buf[256];
	des_key_schedule ks;
	des_cblock key;
	char _keyfile[MAXPATHLEN];
	char *keyfile = 0;
	FILE *fp;
	int init = 0;
	int hex = 1;
	int cnt = 1;
	unsigned int pin;
	struct passwd *pwd;

	while ((i = getopt(argc, argv, "dk:in:")) != -1) {
		switch (i) {
		case 'k':
			keyfile = optarg;
			break;
		case 'i':
			init = 1;
			break;
		case 'd':
			hex = 0;
			break;
		case 'n':
			cnt = atoi(optarg);
			if (cnt <= 0)
				err(1, "invalid count: %s", optarg);
			break;
		default:
			fprintf(stderr, "usage: %s [-n cnt] [-h] [-k keyfile]\n"
			    "       %s -i [-k keyfile]\n", __progname,
			    __progname);
			exit(1);
		}
	}

	if (!keyfile) {
		if ((pwd = getpwuid(getuid())) == NULL) {
			fprintf(stderr, "Say, just who are you, anyhow?\n");
			exit(1);
		}
		snprintf(_keyfile, sizeof(_keyfile), "%s/%s", pwd->pw_dir,
		    KEYFILE);
		keyfile = _keyfile;
	}

	if (init)
		readpassphrase("Enter Key: ", buf, sizeof(buf), 0);
	else if ((fp = fopen(keyfile, "r")) == NULL)
		err(1, "unable to open %s", keyfile);
	else {
		if (fgets(buf, sizeof(buf), fp) == NULL) {
			fprintf(stderr, "No key in %s\n", keyfile);
			exit(1);
		}
		fclose(fp);
	}

	memset(key, 0, sizeof(key));
	if (init && buf[3] == ' ') {
		char *b = buf;
		/* Assume octal input */
		for (i = 0; i < 8; ++i) {
			if (!*b) {
				fprintf(stderr, "%s: invalid key\n", buf);
			}
			while (isdigit(*b))
				key[i] = key[i] << 3 | *b++ - '0';
			while (*b && !isdigit(*b))
				++b;
		}
	} else {
		for (i = 0; i < 16; ++i) {
			int d;

			if (islower(buf[i]))
				buf[i] = toupper(buf[i]);
			if (buf[i] >= '0' && buf[i] <= '9')
				d = buf[i] - '0';
			else if (buf[i] >= 'A' && buf[i] <= 'F')
				d = buf[i] - 'A' + 10;
			else {
				fprintf(stderr, "invalid key: %s\n", buf);
				exit(1);
			}
			key[i>>1] |= d << ((i & 1) ? 0 : 4);
		}
	}

	/* XXX - should warn on non-space or non-digit */
	readpassphrase("Enter Pin: ", buf, sizeof(buf), 0);
	for (i = 0, pin = 0; buf[i] && buf[i] != '\n'; ++i)
		if (isdigit(buf[i]))
			pin = pin * 16 + buf[i] - '0' + 1;

	if ((pin & 0xffff0000) == 0)
		pin |= pin << 16;

	for (i = 0; i < 8; ++i)
		key[0] ^= (pin >> ((i * 7) % 26)) & 0x7f;

	if (init) {
		if ((fp = fopen(keyfile, "w")) == NULL)
			err(1, "could not open %s for writing", keyfile);
		fchmod(fileno(fp), 0600);
		for (i = 0; i < 8; ++i) {
			fprintf(fp, "%c", digits[(key[i]>>4)&0xf]);
			fprintf(fp, "%c", digits[(key[i]>>0)&0xf]);
		}
		fputc('\n', fp);
		fclose(fp);
		exit(0);
	}

	des_fixup_key_parity(&key);
	des_key_sched(&key, ks);

	buf[0] = '\0';
	readpassphrase("Enter challenge: ", buf, sizeof(buf), RPP_ECHO_ON);
	if (buf[0] == '\0')
		exit(0);

	for (i = 0; i < 8; ++i)
		if (buf[i] == '\n')
			buf[i] = '\0';

	if (!hex)
		digits = DECDIGITS;

	predict(ks, buf, cnt);

	memset(&ks, 0, sizeof(ks));
	memset(buf, 0, sizeof(buf));

	exit(0);
}

void
predict(des_key_schedule ks, char *chal, int cnt)
{
	int i;
	des_cblock cb;

	while (cnt-- > 0) {
		printf("%.8s: ", chal);
		des_ecb_encrypt((des_cblock *)chal, &cb, ks, DES_ENCRYPT);
		for (i = 0; i < 4; ++i) {
			printf("%c", digits[(cb[i]>>4) & 0xf]);
			printf("%c", digits[(cb[i]>>0) & 0xf]);
		}
		putchar('\n');
		for (i = 0; i < 8; ++i) {
			if ((cb[i] &= 0xf) > 9)
				cb[i] -= 10;
			cb[i] |= 0x30;
		}
	}
	memset(&cb, 0, sizeof(cb));
}
