/* $OpenBSD: ppccap.c,v 1.7 2023/01/17 15:04:27 miod Exp $ */

#include <sys/types.h>
#include <sys/sysctl.h>
#include <machine/cpu.h>
#include <unistd.h>

#include <crypto.h>
#include <openssl/bn.h>

#ifdef OPENSSL_BN_ASM_MONT
extern int bn_mul_mont_int(BN_ULONG *, const BN_ULONG *, const BN_ULONG *,
	    const BN_ULONG *, const BN_ULONG *, int);
int
bn_mul_mont(BN_ULONG *rp, const BN_ULONG *ap, const BN_ULONG *bp,
    const BN_ULONG *np, const BN_ULONG *n0, int num)
{
	return bn_mul_mont_int(rp, ap, bp, np, n0, num);
}
#endif
