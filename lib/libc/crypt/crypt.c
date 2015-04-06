/*	$OpenBSD: crypt.c,v 1.27 2015/04/06 20:49:41 tedu Exp $	*/

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
}
