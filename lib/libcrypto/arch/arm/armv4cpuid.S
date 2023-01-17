#include "arm_arch.h"

.text
#if defined(__thumb2__) && !defined(__APPLE__)
.syntax	unified
.thumb
#else
.code	32
#undef	__thumb2__
#endif

#if __ARM_ARCH__>=7
.arch	armv7-a
.fpu	neon

.align	5
.globl	_armv7_neon_probe
.type	_armv7_neon_probe,%function
_armv7_neon_probe:
	vorr	q0,q0,q0
	bx	lr
.size	_armv7_neon_probe,.-_armv7_neon_probe

.globl	_armv8_aes_probe
.type	_armv8_aes_probe,%function
_armv8_aes_probe:
#if defined(__thumb2__) && !defined(__APPLE__)
.byte	0xb0,0xff,0x00,0x03	@ aese.8	q0,q0
#else
.byte	0x00,0x03,0xb0,0xf3	@ aese.8	q0,q0
#endif
	bx	lr
.size	_armv8_aes_probe,.-_armv8_aes_probe

.globl	_armv8_sha1_probe
.type	_armv8_sha1_probe,%function
_armv8_sha1_probe:
#if defined(__thumb2__) && !defined(__APPLE__)
.byte	0x00,0xef,0x40,0x0c	@ sha1c.32	q0,q0,q0
#else
.byte	0x40,0x0c,0x00,0xf2	@ sha1c.32	q0,q0,q0
#endif
	bx	lr
.size	_armv8_sha1_probe,.-_armv8_sha1_probe

.globl	_armv8_sha256_probe
.type	_armv8_sha256_probe,%function
_armv8_sha256_probe:
#if defined(__thumb2__) && !defined(__APPLE__)
.byte	0x00,0xff,0x40,0x0c	@ sha256h.32	q0,q0,q0
#else
.byte	0x40,0x0c,0x00,0xf3	@ sha256h.32	q0,q0,q0
#endif
	bx	lr
.size	_armv8_sha256_probe,.-_armv8_sha256_probe
.globl	_armv8_pmull_probe
.type	_armv8_pmull_probe,%function
_armv8_pmull_probe:
#if defined(__thumb2__) && !defined(__APPLE__)
.byte	0xa0,0xef,0x00,0x0e	@ vmull.p64	q0,d0,d0
#else
.byte	0x00,0x0e,0xa0,0xf2	@ vmull.p64	q0,d0,d0
#endif
	bx	lr
.size	_armv8_pmull_probe,.-_armv8_pmull_probe
#endif

.comm	OPENSSL_armcap_P,4,4
.hidden	OPENSSL_armcap_P
