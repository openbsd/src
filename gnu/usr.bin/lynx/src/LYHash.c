/*
 * $LynxId: LYHash.c,v 1.16 2008/12/31 22:10:38 tom Exp $
 *
 * A hash table for the (fake) CSS support in Lynx-rp
 * (c) 1996 Rob Partington
 * rewritten 1997 by Klaus Weide.
 */
#include <LYHash.h>
#include <LYUtils.h>

#ifdef USE_COLOR_STYLE

/*
 * This is the same function as the private HASH_FUNCTION() in HTAnchor.c, but
 * with a different value for HASH_SIZE.
 */

#define HASH_SIZE CSHASHSIZE
#define HASH_OF(h, v) ((int)((h) * 3 + UCH(v)) % HASH_SIZE)

int hash_code(const char *string)
{
    int hash;
    const char *p;

    for (p = string, hash = 0; *p; p++)
	hash = HASH_OF(hash, *p);

    return hash;
}

int hash_code_lowercase_on_fly(const char *string)
{
    int hash;
    const char *p;

    for (p = string, hash = 0; *p; p++)
	hash = HASH_OF(hash, TOLOWER(*p));

    return hash;
}

int hash_code_aggregate_char(char c, int hash)
{
    return HASH_OF(hash, c);
}

int hash_code_aggregate_lower_str(const char *string, int hash_was)
{
    int hash;
    const char *p;

    for (p = string, hash = hash_was; *p; p++)
	hash = HASH_OF(hash, TOLOWER(*p));

    return hash;
}

#endif /* USE_COLOR_STYLE */
