/* $OpenBSD: wp_local.h,v 1.1 2022/11/26 16:08:54 tb Exp $ */

#include <openssl/whrlpool.h>

__BEGIN_HIDDEN_DECLS

void whirlpool_block(WHIRLPOOL_CTX *,const void *,size_t);

__END_HIDDEN_DECLS
