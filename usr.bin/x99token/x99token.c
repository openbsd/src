/*
 * X9.9 calculator
 * This software is provided AS IS with no express or implied warranty
 * October 1995, Paul Borman <prb@krystal.com>
 */
#if	defined(KRBDES) && !defined(__unix__)
#define	__unix__
#endif
#ifdef	__unix__
#ifdef	LITTLE_ENDIAN
#undef	LITTLE_ENDIAN
#endif
#include <pwd.h>
#else
#include <dos.h>
#endif
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
extern char *optarg;


#ifdef	__unix__
#define	KEYFILE	".keyfile.des"
#define	MPL	1024
#else
#define	KEYFILE	"keyfile.des"
#define	MPL	256
#endif

#define	HEXDIGITS	"0123456789abcdef"
#define	DECDIGITS	"0123456789012345"

char *digits = HEXDIGITS;

#ifdef	KRBDES
#include <des.h>
#define setkey	dessetkey

void desinit(int i) { ; }
void dessetkey(char ks[16][8], char key[8])
{
	des_key_schedule *k = (des_key_schedule *)ks;
	des_fixup_key_parity((des_cblock *)key);
	des_key_sched((des_cblock *)key, *k);
}
void endes(char ks[16][8], char key[8])
{
	des_cblock cb;
	des_key_schedule *k = (des_key_schedule *)ks;

	des_ecb_encrypt((des_cblock *)key, &cb, *k, DES_ENCRYPT);
	memcpy(key, &cb, 8);
}
#endif

void predict(char ks[16][8], char *chal, int cnt);

int
main(int ac, char **av)
{
	int i;
	char ks[16][8];
	char buf[256];
	char key[8];
	char _keyfile[MPL];
	char *keyfile = 0;
	FILE *fp;
	int init = 0;
	int hex = 1;
	int cnt = 1;
	unsigned long pin;
#ifdef	__unix__
	struct passwd *pwd;
#endif

	while ((i = getopt(ac, av, "dk:in:")) != EOF)
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
			if (cnt <= 0) {
				fprintf(stderr, "%s: invalid count\n", optarg);
				exit(1);
			}
			break;
		default:
			fprintf(stderr, "Usage: x99token [-n cnt] [-h] [-k keyfile]\n"
					"       x99token -i [-k keyfile]\n");
			exit(1);
		}

	desinit(0);

	if (!keyfile) {
#ifdef	__unix__
		if ((pwd = getpwuid(getuid())) == 0) {
			fprintf(stderr, "Say, just who are you, anyhow?\n");
			exit(1);
		}
		sprintf(_keyfile, "%s/%s", pwd->pw_dir, KEYFILE);
		keyfile = _keyfile;
#else
		keyfile = KEYFILE;
#endif
	}

	if (init) {
#ifdef	__unix__
		strcpy(buf, (char *)getpass("Enter Key: "));
#else
		printf("Enter key: ");
		if (fgets(buf, sizeof(buf), stdin) == NULL)
			exit(0);
#endif
	} else if ((fp = fopen(keyfile, "r")) == NULL) {
		fprintf(stderr, "Failed to open %s\n", keyfile);
		exit(1);
	} else {
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
	} else
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

#ifdef	__unix__
	strcpy(buf, (char *)getpass("Enter Pin: "));
#else
	printf("Enter Pin: ");
	if (fgets(buf, sizeof(buf), stdin) == NULL)
		exit(0);
#endif

	for (i = 0; buf[i] && buf[i] != '\n'; ++i)
		if (isdigit(buf[i]))
			pin = pin * 16 + buf[i] - '0' + 1;

	if ((pin & 0xffff0000L) == 0)
		pin |= pin << 16;

	for (i = 0; i < 8; ++i)
		key[0] ^= (pin >> ((i * 7) % 26)) & 0x7f;

	if (init) {
		if ((fp = fopen(keyfile, "w")) == NULL) {
			fprintf(stderr, "could not open %s for writing\n",
				keyfile);
			exit(1);
		}
		for (i = 0; i < 8; ++i) {
			fprintf(fp, "%c", digits[(key[i]>>4)&0xf]);
			fprintf(fp, "%c", digits[(key[i]>>0)&0xf]);
		}
		fprintf(fp, "\n");
		fclose(fp);
#ifdef	__unix__
		chmod(keyfile, 0600);
#else
		dos_setfileattr(keyfile, FA_HIDDEN | FA_SYSTEM);
#endif
		exit(0);
	}

	setkey(ks, key);

	printf("Enter challange: ");
	memset(buf, 0, sizeof(buf));
	if (fgets(buf, sizeof(buf), stdin) == NULL)
		exit(0);

	for (i = 0; i < 8; ++i)
		if (buf[i] == '\n')
			buf[i] = '\0';

	if (!hex)
		digits = DECDIGITS;

	predict(ks, buf, cnt);

	exit(0);
}

void
predict(char ks[16][8], char *chal, int cnt)
{
	int i;

	while (cnt-- > 0) {
		printf("%.8s: ", chal);
		endes(ks, chal);
		for (i = 0; i < 4; ++i) {
			printf("%c", digits[(chal[i]>>4) & 0xf]);
			printf("%c", digits[(chal[i]>>0) & 0xf]);
		}
		printf("\n");
		for (i = 0; i < 8; ++i) {
			if ((chal[i] &= 0xf) > 9)
				chal[i] -= 10;
			chal[i] |= 0x30;
		}
	}
}
