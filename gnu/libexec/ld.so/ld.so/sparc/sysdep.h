
/*
 * Various assmbly language/system dependent  hacks that are required
 * so that we can minimize the amount of platform specific code.
 */

/*
 * Define this if the system uses RELOCA.
 */
#define ELF_USES_RELOCA

/*
 * Get the address of the Global offset table.  This must be absolute, not
 * relative.
 */
#define GET_GOT(X)     __asm__("\tmov %%l7,%0\n\t" : "=r" (X))

/*
 * Get a pointer to the argv array.  On many platforms this can be just
 * the address if the first argument, on other platforms we need to
 * do something a little more subtle here.  We assume that argc is stored
 * at the word just below the argvp that we return here.
 */
#define GET_ARGV(ARGVP, ARGS) __asm__("\tadd %%fp,68,%0\n" : "=r" (ARGVP));

/*
 * Initialization sequence for a GOT.  For the Sparc, this points to the
 * PLT, and we need to initialize a couple of the slots.  The PLT should
 * look like:
 *
 *		save %sp, -64, %sp
 *		call _dl_linux_resolve
 *		nop
 *		.word implementation_dependent
 */
#define INIT_GOT(GOT_BASE,MODULE) \
{				\
   GOT_BASE[0] = 0x9de3bfc0;  /* save %sp, -64, %sp */	\
   GOT_BASE[1] = 0x40000000 | (((unsigned int) _dl_linux_resolve - (unsigned int) GOT_BASE - 4) >> 2);	\
   GOT_BASE[2] = 0x01000000; /* nop */ 			\
   GOT_BASE[3] = (int) MODULE;					\
}

/*
 * Here is a macro to perform a relocation.  This is only used when
 * bootstrapping the dynamic loader.
 */
#define PERFORM_BOOTSTRAP_RELOC(RELP,REL,SYMBOL,LOAD) \
	switch(ELF32_R_TYPE((RELP)->r_info)) {		\
	case R_SPARC_32:				\
	  *REL = SYMBOL + (RELP)->r_addend;		\
	  break;					\
	case R_SPARC_GLOB_DAT:				\
	  *REL = SYMBOL + (RELP)->r_addend;		\
	  break;					\
	case R_SPARC_JMP_SLOT:				\
	  REL[1] = 0x03000000 | ((SYMBOL >> 10) & 0x3fffff);	\
	  REL[2] = 0x81c06000 | (SYMBOL & 0x3ff);	\
	  break;					\
	case R_SPARC_NONE:				\
	  break;					\
	case R_SPARC_RELATIVE:				\
	  *REL += (unsigned int) LOAD + (RELP)->r_addend; \
	  break;					\
	default:					\
	  _dl_exit(1);					\
	}


/*
 * Transfer control to the user's application, once the dynamic loader
 * is done.  The crt calls atexit with $g1 if not null, so we need to
 * ensure that it contains NULL.
 */

#define START()		\
	__asm__ volatile ("add %%g0,%%g0,%%g1\n\t" \
			   "jmpl %0, %%o7\n\t"	\
			   "restore %%g0,%%g0,%%g0\n\t" \
		    	: "=r" (status) :	\
		    	  "r" (_dl_elf_main))



/* Here we define the magic numbers that this dynamic loader should accept */

#define MAGIC1 EM_SPARC
#undef  MAGIC2
/* Used for error messages */
#define ELF_TARGET "Sparc"

#ifndef COMPILE_ASM
extern unsigned int _dl_linux_resolver(unsigned int reloc_entry,
					unsigned int * i);
#endif

/*
 * Define this if you want a dynamic loader that works on Solaris.
 */
#define SOLARIS_COMPATIBLE

/*
 * Define this because we do not want to call .udiv in the library.
 */
#define do_div(n,base) ({ \
volatile int __res; \
__asm__("mov %%g0,%%Y\n\t" \
	"sdiv %2,%3,%0\n\t" \
	 :"=r" (n),"=r" (__res):"r" (n),"r"(base)); __res; })


#define do_rem(result,n,base) ({ \
volatile int __res; \
__asm__("mov %%g0,%%Y\n\t" \
	"sdiv %2,%3,%%l6\n\t" \
	 "smul %%l6,%3,%%l6\n\t" \
	 "sub  %2,%%l6,%0\n\t" \
	 :"=r" (result),"=r" (__res):"r" (n),"r"(base) : "l6" ); __res; })

/*
 * dbx wants the binder to have a specific name.  Mustn't disappoint it.
 */
#ifdef SOLARIS_COMPATIBLE
#define _dl_linux_resolve _elf_rtbndr
#endif

