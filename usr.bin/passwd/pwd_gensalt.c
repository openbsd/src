/*
 * Copyright 1997 Niels Provos <provos@physnet.uni-hamburg.de>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Theo de Raadt.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#include <stdio.h>
#include <string.h>
#include <err.h>
#include <pwd.h>


#define NUM_OPTIONS	2	/* Number of hardcoded defaults */
#define LINE_MAX	100	/* Max. length of one config file */

static const char options[NUM_OPTIONS][2][80] =
{
	{"local_cipher", "blowfish,4"},
	{"yp_cipher", "old"}
};
/* Read lines and removes trailers. */

static int
read_line(fp, line, max)
	FILE   *fp;
	char   *line;
	int     max;
{
	char   *p, *c;
	/* Read one line of config */
	if (fgets(line, max, fp) == 0)
		return 0;
	if (!(p = strchr(line, '\n'))) {
		warnx("line too long");
		return 0;
	}
	*p = '\0';

	/* Remove comments */
	if ((p = strchr(line, '#')))
		*p = '\0';

	/* Remove trailing spaces */
	p = line;
	while (isspace(*p))
		p++;
	memcpy(line, p, strlen(p) + 1);

	p = line + strlen(line) - 1;
	while (isspace(*p))
		p--;
	*(p + 1) = '\0';
	return 1;
}


static const char *
pwd_default(option)
	char   *option;
{
	int     i;
	for (i = 0; i < NUM_OPTIONS; i++)
		if (!strcasecmp(options[i][0], option))
			return options[i][1];
	return NULL;
}

void
pwd_gettype(data, max, key, option)
	char   *data;
	int     max;
	char   *key;
	char   *option;
{
	FILE   *fp;
	char    line[LINE_MAX];
	static char result[LINE_MAX];
	int     defaultw;
	int     keyw;
	int     got;
	result[0] = '\0';
	if ((fp = fopen(_PATH_PASSWDCONF, "r")) == NULL) {
		strncpy(data, pwd_default(option), max - 1);
		data[max - 1] = '\0';
		return;
	}
	defaultw = 0;
	keyw = 0;
	got = 0;
	while (!keyw && (got || read_line(fp, line, LINE_MAX))) {
		got = 0;
		if (!strcmp("default:", line))
			defaultw = 1;
		if (!strncmp(key, line, strlen(key)) &&
		    line[strlen(key)] == ':')
			keyw = 1;

		/* Now we found default or specified key */
		if (defaultw || keyw) {
			while (read_line(fp, line, LINE_MAX)) {
				/* Leaving key field */
				if (strchr(line, ':')) {
					got = 1;
					break;
				}
				if (!strncmp(line, option, strlen(option)) &&
				    line[strlen(option)] == '=') {
					char   *p;
					p = line + strlen(option) + 1;
					while (isspace(*p))
						p++;
					strcpy(result, p);
					break;
				}
			}
			if (keyw)
				break;
			defaultw = 0;
		}
	}
	fclose(fp);
	if (!strlen(result)) {
		strncpy(data, pwd_default(option), max - 1);
		data[max - 1] = '\0';
		return;
	}
	strncpy(data, result, max - 1);
	data[max - 1] = '\0';
}

void
pwd_gensalt(salt, max, pwd, type)
	char   *salt;
	int     max;
	struct passwd *pwd;
	char    type;
{
	char   *bcrypt_gensalt __P((u_int8_t));
	char    option[LINE_MAX];
	char   *next, *now;
	*salt = '\0';
	if (max < 10)
		return;

	switch (type) {
	case 'y':
		pwd_gettype(option, LINE_MAX, pwd->pw_name, "yp_cipher");
		break;
	case 'l':
	default:
		pwd_gettype(option, LINE_MAX, pwd->pw_name, "local_cipher");
		break;
	}

	next = option;
	now = strsep(&next, ",");
	if (!strcmp(now, "old")) {
		(void) srandom((int) time((time_t *) NULL));
		to64(&salt[0], random(), 2);
	} else
		if (!strcmp(now, "newsalt")) {
			(void) srandom((int) time((time_t *) NULL));
			salt[0] = _PASSWORD_EFMT1;
			to64(&salt[1], (long) (29 * 25), 4);
			to64(&salt[5], random(), 4);
		} else
			if (!strcmp(now, "blowfish")) {
				int     rounds = atoi(next);
				if (rounds < 4)
					rounds = 4;
				strncpy(salt, bcrypt_gensalt(rounds), max - 1);
				salt[max - 1] = 0;
			} else {
				strcpy(salt, ":");
				warnx("Unkown option %s.", now);
			}
}
