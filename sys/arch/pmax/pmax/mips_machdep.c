/*	$NetBSD: mips_machdep.c,v 1.6 1996/10/13 21:37:51 jonathan Exp $	*/

/*
 * Copyright 1996 The Board of Trustees of The Leland Stanford
 * Junior University. All Rights Reserved.
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  Stanford University
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <machine/cpu.h>		/* declaration of of cpu_id */
#include <machine/locore.h>

mips_locore_jumpvec_t mips_locore_jumpvec = {
  NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL,
  NULL, NULL
};

/*
 * Forward declarations
 * XXX should be in a header file so each mips port can include it.
 */
extern void cpu_identify __P((void));
extern void mips_vector_init __P((void));

void mips1_vector_init __P((void));
void mips3_vector_init __P((void));


#ifdef MIPS1	/*  r2000 family  (mips-I cpu) */
/*
 * MIPS-I (r2000) locore-function vector.
 */
mips_locore_jumpvec_t R2000_locore_vec =
{
	mips1_ConfigCache,
	mips1_FlushCache,
	mips1_FlushDCache,
	mips1_FlushICache,
	/*mips1_FlushICache*/ mips1_FlushCache,
	mips1_SetPID,
	mips1_TLBFlush,
	mips1_TLBFlushAddr,
	mips1_TLBUpdate,
	mips1_TLBWriteIndexed
};

void
mips1_vector_init()
{
	extern char mips1_UTLBMiss[], mips1_UTLBMissEnd[];
	extern char mips1_exception[], mips1_exceptionEnd[];

	/*
	 * Copy down exception vector code.
	 */
	if (mips1_UTLBMissEnd - mips1_UTLBMiss > 0x80)
		panic("startup: UTLB code too large");
	bcopy(mips1_UTLBMiss, (char *)MIPS_UTLB_MISS_EXC_VEC,
		mips1_UTLBMissEnd - mips1_UTLBMiss);
	bcopy(mips1_exception, (char *)MIPS1_GEN_EXC_VEC,
	      mips1_exceptionEnd - mips1_exception);

	/*
	 * Copy locore-function vector.
	 */
	bcopy(&R2000_locore_vec, &mips_locore_jumpvec,
	      sizeof(mips_locore_jumpvec_t));

	/*
	 * Clear out the I and D caches.
	 */
	mips1_ConfigCache();
	mips1_FlushCache();
}
#endif /* MIPS1 */


#ifdef MIPS3		/* r4000 family (mips-III cpu) */
/*
 * MIPS-III (r4000) locore-function vector.
 */
mips_locore_jumpvec_t R4000_locore_vec =
{
	mips3_ConfigCache,
	mips3_FlushCache,
	mips3_FlushDCache,
	mips3_FlushICache,
#if 0
	 /*
	  * No such vector exists, perhaps it was meant to be HitFlushDCache?
	  */
	mips3_ForceCacheUpdate,
#else
	mips3_FlushCache,
#endif
	mips3_SetPID,
	mips3_TLBFlush,
	mips3_TLBFlushAddr,
	mips3_TLBUpdate,
	mips3_TLBWriteIndexed
};

void
mips3_vector_init()
{

	/* TLB miss handler address and end */
	extern char mips3_exception[], mips3_exceptionEnd[];

	/* r4000 exception handler address and end */
	extern char mips3_TLBMiss[], mips3_TLBMissEnd[];

	/*
	 * Copy down exception vector code.
	 */
	if (mips3_TLBMissEnd - mips3_TLBMiss > 0x80)
		panic("startup: UTLB code too large");
	bcopy(mips3_TLBMiss, (char *)MIPS_UTLB_MISS_EXC_VEC,
	      mips3_TLBMissEnd - mips3_TLBMiss);

	bcopy(mips3_exception, (char *)MIPS_GEN_EXC_VEC,
	      mips3_exceptionEnd - mips3_exception);

	/*
	 * Copy locore-function vector.
	 */
	bcopy(&R4000_locore_vec, &mips_locore_jumpvec,
	      sizeof(mips_locore_jumpvec_t));

	/*
	 * Clear out the I and D caches.
	 */
	mips3_ConfigCache();
	mips3_FlushCache();
}
#endif	/* MIPS3 */


/*
 * Do all the stuff that locore normally does before calling main(),
 * that is common to all mips-CPU NetBSD ports.
 *
 * The principal purpose of this function is to examine the
 * variable cpu_id, into which the kernel locore start code
 * writes the cpu ID register, and to then copy appropriate
 * cod into the CPU exception-vector entries and the jump tables
 * used to  hide the differences in cache and TLB handling in
 * different MIPS  CPUs.
 * 
 * This should be the very first thing called by each port's
 * init_main() function.
 */

/*
 * Initialize the hardware exception vectors, and the jump table used to
 * call locore cache and TLB management functions, based on the kind
 * of CPU the kernel is running on.
 */
void mips_vector_init()
{
	register caddr_t v;
	extern char edata[], end[];

	/* clear the BSS segment */
	v = (caddr_t)mips_round_page(end);
	bzero(edata, v - edata);

	/* Work out what kind of CPU and FPU are present. */
	switch(cpu_id.cpu.cp_imp) {

#ifdef MIPS1	/*  r2000 family  (mips-I cpu) */
	case MIPS_R2000:
	case MIPS_R3000:
	  	mips1_vector_init();
		break;
#endif /* MIPS1 */


#ifdef MIPS3		/* r4000 family (mips-III cpu) */
	case MIPS_R4000:
	  	mips3_vector_init();
		break;
#endif /* MIPS3 */

	default:
		panic("Unconfigured or unsupported MIPS cpu\n");

	}
}


/*
 * Identify cpu and fpu type and revision.
 *
 * XXX Should be moved to mips_cpu.c but that doesn't exist
 */
void
cpu_identify()
{


	/* Work out what kind of CPU and FPU are present. */

	switch(cpu_id.cpu.cp_imp) {

	case MIPS_R2000:
		printf("MIPS R2000 CPU");
		break;
	case MIPS_R3000:

	  	/*
		 * XXX
		 * R2000A silicion has an r3000 core and shows up here.
		 *  The caller should indicate that by setting a flag
		 * indicating the baseboard is  socketed for an r2000.
		 * Needs more thought.
		 */
#ifdef notyet
	  	if (SYSTEM_HAS_R2000_CPU_SOCKET())
			printf("MIPS R2000A CPU");
		else
#endif
		printf("MIPS R3000 CPU");
		break;
	case MIPS_R6000:
		printf("MIPS R6000 CPU");
		break;

	case MIPS_R4000:
#ifdef pica /* XXX*/
		if(machPrimaryInstCacheSize == 16384)
			printf("MIPS R4400 CPU");
		else
#endif /* XXX*/
			printf("MIPS R4000 CPU");
		break;
	case MIPS_R3LSI:
		printf("LSI Logic R3000 derivate");
		break;
	case MIPS_R6000A:
		printf("MIPS R6000A CPU");
		break;
	case MIPS_R3IDT:
		printf("IDT R3000 derivate");
		break;
	case MIPS_R10000:
		printf("MIPS R10000/T5 CPU");
		break;
	case MIPS_R4200:
		printf("MIPS R4200 CPU (ICE)");
		break;
	case MIPS_R8000:
		printf("MIPS R8000 Blackbird/TFP CPU");
		break;
	case MIPS_R4600:
		printf("QED R4600 Orion CPU");
		break;
	case MIPS_R3SONY:
		printf("Sony R3000 based CPU");
		break;
	case MIPS_R3TOSH:
		printf("Toshiba R3000 based CPU");
		break;
	case MIPS_R3NKK:
		printf("NKK R3000 based CPU");
		break;
	case MIPS_UNKC1:
	case MIPS_UNKC2:
	default:
		printf("Unknown CPU type (0x%x)",cpu_id.cpu.cp_imp);
		break;
	}
	printf(" Rev. %d.%d with ", cpu_id.cpu.cp_majrev, cpu_id.cpu.cp_minrev);


	switch(fpu_id.cpu.cp_imp) {

	case MIPS_SOFT:
		printf("Software emulation float");
		break;
	case MIPS_R2360:
		printf("MIPS R2360 FPC");
		break;
	case MIPS_R2010:
		printf("MIPS R2010 FPC");
		break;
	case MIPS_R3010:
	  	/*
		 * XXX FPUs  for R2000A(?) silicion has an r3010 core and
		 *  shows up here.
		 */
#ifdef notyet
	  	if (SYSTEM_HAS_R2000_CPU_SOCKET())
			printf("MIPS R2010A CPU");
		else
#endif
		printf("MIPS R3010 FPC");
		break;
	case MIPS_R6010:
		printf("MIPS R6010 FPC");
		break;
	case MIPS_R4010:
		printf("MIPS R4010 FPC");
		break;
	case MIPS_R31LSI:
		printf("FPC");
		break;
	case MIPS_R10010:
		printf("MIPS R10000/T5 FPU");
		break;
	case MIPS_R4210:
		printf("MIPS R4200 FPC (ICE)");
	case MIPS_R8000:
		printf("MIPS R8000 Blackbird/TFP");
		break;
	case MIPS_R4600:
		printf("QED R4600 Orion FPC");
		break;
	case MIPS_R3SONY:
		printf("Sony R3000 based FPC");
		break;
	case MIPS_R3TOSH:
		printf("Toshiba R3000 based FPC");
		break;
	case MIPS_R3NKK:
		printf("NKK R3000 based FPC");
		break;
	case MIPS_UNKF1:
	default:
		printf("Unknown FPU type (0x%x)", fpu_id.cpu.cp_imp);
		break;
	}
	printf(" Rev. %d.%d", fpu_id.cpu.cp_majrev, fpu_id.cpu.cp_minrev);
	printf("\n");

#ifdef pica
	printf("        Primary cache size: %dkb Instruction, %dkb Data.\n",
		machPrimaryInstCacheSize / 1024,
		machPrimaryDataCacheSize / 1024);
#endif
}
