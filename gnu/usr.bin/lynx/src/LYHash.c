/* A hash table for the (fake) CSS support in Lynx-rp
** (c) 1996 Rob Partington
*/

#include "LYStructs.h"
#include "LYCurses.h"
#include "AttrList.h"
#include "SGML.h"
#include "HTMLDTD.h"

#include "LYHash.h"

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
 *	This is the same function as the private HASH_FUNCTION() in HTAnchor.c,
 *      but with a different value for HASH_SIZE.
 */ 

#ifdef NOT_USED
#define HASH_SIZE 8193		/* Arbitrary prime. Memory/speed tradeoff */
#else
#define HASH_SIZE CSHASHSIZE
#endif

PUBLIC int hash_code ARGS1 (char*, string)
{
    int hash;
    unsigned char *p;

    for (p = (unsigned char *)string, hash = 0; *p; p++)
    	hash = (int) (hash * 3 + (*(unsigned char *)p)) % HASH_SIZE;

    return hash;
}
