/*	$OpenBSD: crypt.c,v 1.28 2015/05/13 21:01:54 bluhm Exp $	*/

#include <pwd.h>

char *
crypt(const char *key, const char *setting)
{
	if (setting[0] == '$') {
		switch (setting[1]) {
		case '2':
			return bcrypt(key, setting);
		default:
			return (NULL);
		}
	}

	return (NULL);
}
