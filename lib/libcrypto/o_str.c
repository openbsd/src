/*
 * Written by Theo de Raadt.  Public domain.
 */

#include <ctype.h>
#include <e_os.h>
#include "o_str.h"
#include <string.h>

int
OPENSSL_strncasecmp(const char *str1, const char *str2, size_t n)
{
	return strncasecmp(str1, str2, n);
}

int
OPENSSL_strcasecmp(const char *str1, const char *str2)
{
	return strcasecmp(str1, str2);
}

int
OPENSSL_memcmp(const void *v1, const void *v2, size_t n)
{
	return memcmp(v1, v2, n);
}
