/* A hash table for the (fake) CSS support in Lynx-rp
** (c) 1996 Rob Partington
*/
#include <LYHash.h>

#ifdef USE_COLOR_STYLE

#ifdef NOT_USED

PUBLIC int hash_table[CSHASHSIZE]; /* 32K should be big enough */

PUBLIC int hash_code_rp ARGS1(char*,string)
{
    char* hash_ptr = string;
    int hash_tmp = 0xC00A | ((*hash_ptr) << 4);

    while (*hash_ptr++)
    {
	hash_tmp ^= (((*hash_ptr)<<4) ^ ((*hash_ptr)<<12));
	hash_tmp >>= 1;
    }
    return (hash_tmp % CSHASHSIZE);
}
#endif

/*
 *  This is the same function as the private HASH_FUNCTION() in HTAnchor.c,
 *  but with a different value for HASH_SIZE.
 */

#ifdef NOT_USED
#define HASH_SIZE 8193		/* Arbitrary prime.  Memory/speed tradeoff */
#else
#define HASH_SIZE CSHASHSIZE
#endif

#define HASH_OF(h, v) ((int)((h) * 3 + (unsigned char)(v)) % HASH_SIZE)

PUBLIC int hash_code ARGS1 (char*, string)
{
    int hash;
    unsigned char *p;

    for (p = (unsigned char *)string, hash = 0; *p; p++)
	hash = HASH_OF(hash,*p);

    return hash;
}

PUBLIC int hash_code_lowercase_on_fly ARGS1 (char*, string)
{
    int hash;
    unsigned char *p;

    for (p = (unsigned char *)string, hash = 0; *p; p++)
	hash = HASH_OF(hash,tolower(*(char *)p));

    return hash;
}

PUBLIC int hash_code_aggregate_char ARGS2 (char, c,int,hash)
{
    return HASH_OF(hash,c);
}

PUBLIC int hash_code_aggregate_lower_str ARGS2 (char*, string,int,hash_was)
{
    int hash;
    unsigned char *p;

    for (p = (unsigned char *)string, hash = hash_was ; *p; p++)
	hash = HASH_OF(hash,tolower(*(char *)p));

    return hash;
}

#endif /* USE_COLOR_STYLE */
