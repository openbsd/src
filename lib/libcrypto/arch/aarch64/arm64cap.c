/* $OpenBSD: arm64cap.c,v 1.3 2023/07/26 09:57:34 jsing Exp $ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <signal.h>
#include <openssl/crypto.h>

#if defined(__OpenBSD__)
#include <sys/sysctl.h>
#include <machine/cpu.h>	/* CPU_ID_AA64ISAR0 */
#endif

#include "arm64_arch.h"

/* ID_AA64ISAR0_EL1 required for OPENSSL_cpuid_setup */
#define	ID_AA64ISAR0_AES_SHIFT		4
#define	ID_AA64ISAR0_AES_MASK		(0xf << ID_AA64ISAR0_AES_SHIFT)
#define	ID_AA64ISAR0_AES(x)		((x) & ID_AA64ISAR0_AES_MASK)
#define	 ID_AA64ISAR0_AES_BASE		(0x1 << ID_AA64ISAR0_AES_SHIFT)
#define	 ID_AA64ISAR0_AES_PMULL		(0x2 << ID_AA64ISAR0_AES_SHIFT)
#define	ID_AA64ISAR0_SHA1_SHIFT		8
#define	ID_AA64ISAR0_SHA1_MASK		(0xf << ID_AA64ISAR0_SHA1_SHIFT)
#define	ID_AA64ISAR0_SHA1(x)		((x) & ID_AA64ISAR0_SHA1_MASK)
#define	 ID_AA64ISAR0_SHA1_BASE		(0x1 << ID_AA64ISAR0_SHA1_SHIFT)
#define	ID_AA64ISAR0_SHA2_SHIFT		12
#define	ID_AA64ISAR0_SHA2_MASK		(0xf << ID_AA64ISAR0_SHA2_SHIFT)
#define	ID_AA64ISAR0_SHA2(x)		((x) & ID_AA64ISAR0_SHA2_MASK)
#define	 ID_AA64ISAR0_SHA2_BASE		(0x1 << ID_AA64ISAR0_SHA2_SHIFT)

unsigned int OPENSSL_armcap_P;

#if defined(CPU_ID_AA64ISAR0)
void
OPENSSL_cpuid_setup(void)
{
	int isar0_mib[] = { CTL_MACHDEP, CPU_ID_AA64ISAR0 };
	size_t len = sizeof(uint64_t);
	uint64_t cpu_id = 0;

	if (OPENSSL_armcap_P != 0)
		return;

	if (sysctl(isar0_mib, 2, &cpu_id, &len, NULL, 0) < 0)
		return;

	OPENSSL_armcap_P |= ARMV7_NEON;

	if (ID_AA64ISAR0_AES(cpu_id) >= ID_AA64ISAR0_AES_BASE)
		OPENSSL_armcap_P |= ARMV8_AES;

	if (ID_AA64ISAR0_AES(cpu_id) >= ID_AA64ISAR0_AES_PMULL)
		OPENSSL_armcap_P |= ARMV8_PMULL;

	if (ID_AA64ISAR0_SHA1(cpu_id) >= ID_AA64ISAR0_SHA1_BASE)
		OPENSSL_armcap_P |= ARMV8_SHA1;

	if (ID_AA64ISAR0_SHA2(cpu_id) >= ID_AA64ISAR0_SHA2_BASE)
		OPENSSL_armcap_P |= ARMV8_SHA256;
}
#else
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
#endif
