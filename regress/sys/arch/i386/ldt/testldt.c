/*	$OpenBSD: testldt.c,v 1.8 2003/09/02 23:52:17 david Exp $	*/
/*	$NetBSD: testldt.c,v 1.4 1995/04/20 22:42:38 cgd Exp $	*/

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <machine/segments.h>

extern int i386_get_ldt(int, union descriptor *, int);
extern int i386_set_ldt(int, union descriptor *, int);

int verbose = 0;
struct sigaction segv_act;

static inline void
set_fs(unsigned long val)
{
	__asm__ __volatile__("mov %0,%%fs"::"r" ((unsigned short) val));
}

static inline unsigned char
get_fs_byte(const char * addr)
{
	unsigned register char _v;

	__asm__ ("movb %%fs:%1,%0":"=q" (_v):"m" (*addr));
	return _v;
}

static inline unsigned short
get_cs(void)
{
	unsigned register short _v;

	__asm__ ("movw %%cs,%0"::"r" ((unsigned short) _v));
	return _v;
}

static int
check_desc(unsigned int desc)
{
	desc = LSEL(desc, SEL_UPL);
	set_fs(desc);
	return(get_fs_byte((char *) 0));
}

static void
gated_call(void)
{
	printf("Called from call gate...");
	__asm__ __volatile__("popl %ebp");
	__asm__ __volatile__(".byte 0xcb");
}

static struct segment_descriptor *
make_sd(unsigned base, unsigned limit, int type, int dpl, int seg32, int inpgs)
{
	static struct segment_descriptor d;

	d.sd_lolimit = limit & 0x0000ffff;
	d.sd_lobase  = base & 0x00ffffff;
	d.sd_type    = type & 0x01f;
	d.sd_dpl     = dpl & 0x3;
	d.sd_p	     = 1;
	d.sd_hilimit = (limit & 0x00ff0000) >> 16;
	d.sd_xx	     = 0;
	d.sd_def32   = seg32?1:0;
	d.sd_gran    = inpgs?1:0;
	d.sd_hibase  = (base & 0xff000000) >> 24;

	return (&d);
}

static struct gate_descriptor *
make_gd(unsigned offset, unsigned int sel, unsigned stkcpy, int type, int dpl)
{
	static struct gate_descriptor d;

	d.gd_looffset = offset & 0x0000ffff;
	d.gd_selector = sel & 0xffff;
	d.gd_stkcpy   = stkcpy & 0x1ff;
	d.gd_type     = type & 0x1ff;
	d.gd_dpl      = dpl & 0x3;
	d.gd_p	      = 1;
	d.gd_hioffset = (offset & 0xffff0000) >> 16;

	return(&d);
}

static void
print_ldt(union descriptor *dp)
{
	unsigned long base_addr, limit, offset, selector, stack_copy;
	int type, dpl, i;
	unsigned long *lp = (unsigned long *)dp;
    
	/* First 32 bits of descriptor */
	selector = base_addr = (*lp >> 16) & 0x0000FFFF;
	offset = limit = *lp & 0x0000FFFF;
	lp++;
	
	/* First 32 bits of descriptor */
	base_addr |= (*lp & 0xFF000000) | ((*lp << 16) & 0x00FF0000);
	limit |= (*lp & 0x000F0000);
	type = dp->sd.sd_type;
	dpl = dp->sd.sd_dpl;
	stack_copy = dp->gd.gd_stkcpy;
	offset |= (*lp >> 16) & 0x0000FFFF;
    
	if (type == SDT_SYS386CGT || type == SDT_SYS286CGT)
		printf("LDT: Gate Off %08.8x, Sel   %05.5x, Stkcpy %d DPL %d, Type %d\n",
			offset, selector, stack_copy, dpl, type);
	else
		printf("LDT: Seg Base %08.8x, Limit %05.5x, DPL %d, Type %d\n",
			base_addr, limit, dpl, type);
	printf("	  ");
	if (*lp & 0x100)
		printf("Accessed, ");
	if (*lp & 8000)
		printf("Present, ");
	if (type != SDT_SYS386CGT && type != SDT_SYS286CGT) {
		if (*lp & 0x100000)
			printf("User, ");
		if (*lp & 0x200000)
			printf("X, ");
		if (*lp & 0x400000)
			printf("32, ");
		else
			printf("16, ");
		if (*lp & 0x800000)
			printf("page limit, ");
		else
			printf("byte limit, ");
	}
	printf("\n");
	printf("	  %08.8x %08.8x\n", *(lp), *(lp-1));
}

static void busfault(int signal, int code, struct sigcontext *sc)
{
	fprintf(stderr, "\nbus fault - investigate.\n");
	_exit(1);
}

static void usage(int status)
{
	fprintf(stderr, "Usage: testldt [-v]\n");
        exit(status);
}

#define MAX_USER_LDT 1024
main(int argc, char *argv[])
{
	union descriptor ldt[MAX_USER_LDT];
	int num, n, ch;
	unsigned int cs = get_cs();
	char *data;
	struct segment_descriptor *sd;
	struct gate_descriptor *gd;
	
	segv_act.sa_handler = (sig_t) busfault;
	if (sigaction(SIGBUS, &segv_act, NULL) < 0) {
		perror("sigaction");
		exit(1);
	}

	while ((ch = getopt(argc, argv, "v")) != -1) {
		switch (ch) {
		case 'v':
		    verbose++;
		    break;
		default:
		    usage(1);
		    break;
		}
	}

	printf("Testing i386_get_ldt...\n");
	if ((num = i386_get_ldt(0, ldt, MAX_USER_LDT)) < 0) {
		perror("get_ldt");
		exit(2);
	}
	if (num == 0) {
	    fprintf(stderr, "ERROR: i386_get_ldt() return 0 default LDT entries.\n");
	    exit(1);
	}

	if (verbose) {
	    printf("Got %d (default) LDTs\n", num);
	    for (n=0; n < num; n++) {
		printf("Entry %d: ", n);
		print_ldt(&ldt[n]);
	    }
	}
	
	/*
	 * mmap a data area and assign an LDT to it
	 */
	printf("Testing i386_set_ldt...\n");
	data = (void *) mmap( (char *)0x005f0000, 0x0fff,
			     PROT_EXEC | PROT_READ | PROT_WRITE,
			     MAP_FIXED | MAP_PRIVATE | MAP_ANON, -1, 0);
	if (data == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}
	if (verbose) printf("data address: %8.8x\n", data);

	*data = 0x97;

	/* Get the next free LDT and set it to the allocated data. */
	sd = make_sd((unsigned)data, 4096, SDT_MEMRW, SEL_UPL, 0, 0);
	if ((num = i386_set_ldt(6, (union descriptor *)sd, 1)) < 0) {
		perror("set_ldt");
		exit(1);
	}
	if (verbose) printf("setldt returned: %d\n", num);
	if ((n = i386_get_ldt(num, ldt, 1)) < 0) {
		perror("get_ldt");
		exit(1);
	}
	if (verbose) {
		printf("Entry %d: ", num);
		print_ldt(&ldt[0]);
	}

	if (verbose) printf("Checking desc (should be 0x97): 0x%x\n", check_desc(num));
	if (check_desc(num) != 0x97) {
		fprintf(stderr, "ERROR: descriptor check failed: (should be 0x97): 0x%x\n", check_desc(num));
		exit(1);
	}
	
	/*
	 * Test a Call Gate
	 */
	printf("Testing Call Gate...");
	gd = make_gd((unsigned)gated_call, cs, 0, SDT_SYS386CGT, SEL_UPL);
	if ((num = i386_set_ldt(5, (union descriptor *)gd, 1)) < 0) {
		perror("set_ldt: call gate");
		exit(1);
	}
	if (verbose) printf("setldt returned: %d\n", num);
	if (verbose) printf("Call gate sel = 0x%x\n", LSEL(num, SEL_UPL));
	if ((n = i386_get_ldt(num, ldt, 1)) < 0) {
		perror("get_ldt");
		exit(1);
	}
	if (verbose) printf("Entry %d: ", num);
	if (verbose) print_ldt(&ldt[0]);

#if 0
	err = setldt(5,
		     gated_call,	/* Offset */
		     0x0001,		/* This selector is for the executable segment descriptor.  It
					   is the standard linux text descriptor. */
		     0x00008c00);	/* Descriptor flags (you can't set all, the OS protects some) */
	printf("setldt returned: %d\n", err);
#endif

	__asm__ __volatile__(".byte 0x9a"); /* This is a call to a call gate. */
	__asm__ __volatile__(".byte 0x00"); /* Value is ignored in a call gate but can be used. */
	__asm__ __volatile__(".byte 0x00"); /* by the called procedure. */
	__asm__ __volatile__(".byte 0x00");
	__asm__ __volatile__(".byte 0x00");
	__asm__ __volatile__(".byte 0x2f"); /* Selector 0x002f.	 This is index = 5 (the call gate), */
	__asm__ __volatile__(".byte 0x00"); /* and a requestor priveledge level of 3. */

	printf("Gated call returned\n");
	exit (0);
}
