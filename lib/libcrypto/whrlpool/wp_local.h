/* $OpenBSD: wp_local.h,v 1.2 2023/09/04 08:43:41 tb Exp $ */

#include <sys/types.h>

#include <openssl/whrlpool.h>

__BEGIN_HIDDEN_DECLS

void whirlpool_block(WHIRLPOOL_CTX *,const void *,size_t);

__END_HIDDEN_DECLS
