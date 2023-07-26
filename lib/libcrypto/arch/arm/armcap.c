/* $OpenBSD: armcap.c,v 1.2 2023/07/26 09:57:34 jsing Exp $ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <signal.h>
#include <openssl/crypto.h>

#include "arm_arch.h"

unsigned int OPENSSL_armcap_P;

#if __ARM_ARCH__ >= 7
static sigset_t all_masked;

static sigjmp_buf ill_jmp;
	static void ill_handler (int sig) { siglongjmp(ill_jmp, sig);
}

/*
 * Following subroutines could have been inlined, but it's not all
 * ARM compilers support inline assembler...
 */
void _armv7_neon_probe(void);
void _armv8_aes_probe(void);
void _armv8_sha1_probe(void);
void _armv8_sha256_probe(void);
void _armv8_pmull_probe(void);
#endif

void
OPENSSL_cpuid_setup(void)
{
#if __ARM_ARCH__ >= 7
	struct sigaction	ill_oact, ill_act;
	sigset_t		oset;
#endif
	static int trigger = 0;

	if (trigger)
		return;
	trigger = 1;

	OPENSSL_armcap_P = 0;

#if __ARM_ARCH__ >= 7
	sigfillset(&all_masked);
	sigdelset(&all_masked, SIGILL);
	sigdelset(&all_masked, SIGTRAP);
	sigdelset(&all_masked, SIGFPE);
	sigdelset(&all_masked, SIGBUS);
	sigdelset(&all_masked, SIGSEGV);

	memset(&ill_act, 0, sizeof(ill_act));
	ill_act.sa_handler = ill_handler;
	ill_act.sa_mask = all_masked;

	sigprocmask(SIG_SETMASK, &ill_act.sa_mask, &oset);
	sigaction(SIGILL, &ill_act, &ill_oact);

	if (sigsetjmp(ill_jmp, 1) == 0) {
		_armv7_neon_probe();
		OPENSSL_armcap_P |= ARMV7_NEON;
		if (sigsetjmp(ill_jmp, 1) == 0) {
			_armv8_pmull_probe();
			OPENSSL_armcap_P |= ARMV8_PMULL | ARMV8_AES;
		} else if (sigsetjmp(ill_jmp, 1) == 0) {
			_armv8_aes_probe();
			OPENSSL_armcap_P |= ARMV8_AES;
		}
		if (sigsetjmp(ill_jmp, 1) == 0) {
			_armv8_sha1_probe();
			OPENSSL_armcap_P |= ARMV8_SHA1;
		}
		if (sigsetjmp(ill_jmp, 1) == 0) {
			_armv8_sha256_probe();
			OPENSSL_armcap_P |= ARMV8_SHA256;
		}
	}

	sigaction (SIGILL, &ill_oact, NULL);
	sigprocmask(SIG_SETMASK, &oset, NULL);
#endif
}
