/* $OpenBSD: wp_locl.h,v 1.2 2014/06/12 15:49:31 deraadt Exp $ */

#include <openssl/whrlpool.h>

void whirlpool_block(WHIRLPOOL_CTX *,const void *,size_t);
