
/* Various assmbly language/system dependent hacks that are required
   so that we can minimize the amount of platform specific code. */

/* Define this if the system uses RELOCA.  */
#define ELF_USES_RELOCA

/* Get a pointer to the argv array.  On many platforms this can be
   just the address if the first argument, on other platforms we need
   to do something a little more subtle here.  */
#define GET_ARGV(ARGVP, ARGS) ((ARGVP) = ((unsigned int *) &(ARGS)))

/* Get the address of the Global offset table.  This must be absolute,
   not relative.  */
#define GET_GOT(X)     __asm__ ("movel %%a5,%0" : "=g" (X))

/* Initialization sequence for a GOT.  */
#define INIT_GOT(GOT_BASE,MODULE)		\
{						\
  GOT_BASE[2] = (int) _dl_linux_resolve;	\
  GOT_BASE[1] = (int) (MODULE);			\
}

/* Here is a macro to perform a relocation.  This is only used when
   bootstrapping the dynamic loader.  RELP is the relocation that we
   are performing, REL is the pointer to the address we are
   relocating.  SYMBOL is the symbol involved in the relocation, and
   LOAD is the load address. */
#define PERFORM_BOOTSTRAP_RELOC(RELP,REL,SYMBOL,LOAD)		\
  switch (ELF32_R_TYPE ((RELP)->r_info))			\
    {								\
    case R_68K_8:						\
      *(char *) (REL) = (SYMBOL) + (RELP)->r_addend;		\
      break;							\
    case R_68K_16:						\
      *(short *) (REL) = (SYMBOL) + (RELP)->r_addend;		\
      break;							\
    case R_68K_32:						\
      *(REL) = (SYMBOL) + (RELP)->r_addend;			\
      break;							\
    case R_68K_PC8:						\
      *(char *) (REL) += ((SYMBOL) + (RELP)->r_addend		\
			  - (unsigned int) (REL));		\
      break;							\
    case R_68K_PC16:						\
      *(short *) (REL) += ((SYMBOL) + (RELP)->r_addend		\
			   - (unsigned int) (REL));		\
      break;							\
    case R_68K_PC32:						\
      *(REL) += ((SYMBOL) + (RELP)->r_addend			\
		 - (unsigned int) (REL));			\
      break;							\
    case R_68K_GLOB_DAT:					\
    case R_68K_JMP_SLOT:					\
      *(REL) = (SYMBOL) + (RELP)->r_addend;			\
      break;							\
    case R_68K_RELATIVE:					\
      *(REL) += (unsigned int) (LOAD) + (RELP)->r_addend;	\
      break;							\
    default:							\
      _dl_exit (1);						\
    }


/* Transfer control to the user's application, once the dynamic loader
   is done.  */

#define START()					\
  __asm__ volatile ("unlk %%a6\n\t"		\
		    "jmp %0@"			\
		    : : "a" (_dl_elf_main));



/* Here we define the magic numbers that this dynamic loader should accept */

#define MAGIC1 EM_68K
#undef MAGIC2
/* Used for error messages */
#define ELF_TARGET "m68k"

struct elf_resolve;
extern unsigned int _dl_linux_resolver (int, int, struct elf_resolve *, int);

/* Define this because we do not want to call .udiv in the library.
   Not needed for m68k.  */
#define do_rem(result, n, base)  ((result) = (n) % (base))
