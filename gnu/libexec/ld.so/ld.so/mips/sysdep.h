
/* Various assmbly language/system dependent hacks that are required
   so that we can minimize the amount of platform specific code. */

/* Define this if the system uses RELOCA.  */
/*#define ELF_USES_RELOCA*/

/* Get a pointer to the argv array.  On many platforms this can be
   just the address if the first argument, on other platforms we need
   to do something a little more subtle here.  */
#define GET_ARGV(ARGVP, ARGS) \
	asm("   addiu   %0,$29,328" : "=r" (ARGVP))

/* Get the address of the Global offset table.  This must be absolute,
   not relative. This is already set up on mips  */
#define GET_GOT(X)

/*                      
 * Calculate how much off the link address we are loaded.
 */                             
static inline Elf32_Addr
elf_machine_load_offset (void)     
{ 
  Elf32_Addr addr;                  
  asm ("        .set noreorder\n"
       "        la %0, here\n"          
       "        bltzal $0, here\n"
       "        nop\n"          
       "here:   subu %0, $31, %0\n"     
       "        .set reorder\n"
       : "=r" (addr));          
  return addr;
}                               

/* Here is a macro to perform a relocation.  This is only used when
   bootstrapping the dynamic loader.  RELP is the relocation that we
   are performing, REL is the pointer to the address we are
   relocating.  SYMBOL is the symbol involved in the relocation, and
   LOAD is the load address. */
#define PERFORM_BOOTSTRAP_RELOC(RELP,REL,SYMBOL,LOAD) \
	if(ELF32_R_TYPE((RELP)->r_info) == R_MIPS_REL32) { \
		if (ELF32_ST_BIND ((SYMBOL)->st_info) == STB_LOCAL \
		   && (ELF32_ST_TYPE ((SYMBOL)->st_info) == STT_SECTION \
	            || ELF32_ST_TYPE ((SYMBOL)->st_info) == STT_NOTYPE)) { \
			*(REL) += (LOAD); \
		} \
		else { \
			*(REL) = (LOAD) + (SYMBOL)->st_value; \
		} \
	}

/* Transfer control to the user's application, once the dynamic loader
   is done.  t9 has to contain the entry (link) address */

#define START()					\
  __asm__ volatile ("move  $25, %0\n\t"		\
		    "move  $29, %1\n\t"		\
		    "jr  $25"			\
		    : : "r" (_dl_elf_main), "r" (stack));



/* Here we define the magic numbers that this dynamic loader should accept */

#define MAGIC1 EM_MIPS
#undef MAGIC2
/* Used for error messages */
#define ELF_TARGET "mips"

struct elf_resolve;
extern unsigned int _dl_linux_resolver (int, int, struct elf_resolve *, int);

/* Define this because we do not want to call .udiv in the library.
   Not needed for mips.  */
#define do_rem(result, n, base)  ((result) = (n) % (base))
