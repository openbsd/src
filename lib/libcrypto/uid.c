/*
 * Written by Theo de Raadt.  Public domain.
 */

#include <unistd.h>

int
OPENSSL_issetugid(void)
{
	return issetugid();
}
