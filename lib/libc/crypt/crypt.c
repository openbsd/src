/*	$OpenBSD: crypt.c,v 1.29 2015/07/18 00:58:19 tedu Exp $	*/

#include <pwd.h>

char *
crypt(const char *key, const char *setting)
{
	if (setting[0] == '$') {
		switch (setting[1]) {
		case '2':
			return bcrypt(key, setting);
		default:
			errno = EINVAL;
			return (NULL);
		}
	}
	errno = EINVAL;
	return (NULL);
}
