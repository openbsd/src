/*	$OpenBSD: db_interface.c,v 1.4 1999/02/09 06:36:24 smurph Exp $	*/
/*
 * Mach Operating System
 * Copyright (c) 1993-1991 Carnegie Mellon University
 * Copyright (c) 1991 OMRON Corporation
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON AND OMRON ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON AND OMRON DISCLAIM ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 * m88k interface to ddb debugger
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/systm.h> /* just for boothowto --eichin */

#include <vm/vm.h>

#include <machine/m882xx.h>		 /* CMMU defs		        */
#include <machine/trap.h>		 /* current_thread()            */
#include <machine/db_machdep.h>		 /* local ddb stuff             */
#include <machine/bug.h>		 /* bug routines 		*/
#include <machine/mmu.h>

#include <ddb/db_command.h>
#include <ddb/db_sym.h>

extern label_t *db_recover;
extern unsigned int db_maxoff;

int 	db_active = 0;
int 	db_noisy = 0;
int	quiet_db_read_bytes = 0;

/*
 * Received keyboard interrupt sequence.
 */
kdb_kintr(regs)
        register struct m88100_saved_state *regs;
{
        if (db_active == 0 && (boothowto & RB_KDB)) {
                printf("\n\nkernel: keyboard interrupt\n");
                m88k_db_trap(-1, regs);
        }
}

/************************/
/* PRINTING *************/
/************************/

static void
m88k_db_str(char *str)
{
    db_printf(str);
}

static void
m88k_db_str1(char *str, int arg1)
{
    db_printf(str, arg1);
}

static void
m88k_db_str2(char *str, int arg1, int arg2)
{
    db_printf(str, arg1, arg2);
}

/************************/
/* DB_REGISTERS ****/
/************************/

/*
 *
 * If you really feel like understanding the following procedure and
 * macros, see pages 6-22 to 6-30 (Section 6.7.3) of
 *
 * MC881000 RISC Microprocessor User's Manual Second Edition
 * (Motorola Order: MC88100UM/AD REV 1)
 *
 * and ERRATA-5 (6-23, 6-24, 6-24) of
 *
 * Errata to MC88100 User's Manual Second Edition MC88100UM/AD Rev 1
 * (Oct 2, 1990)
 * (Motorola Order: MC88100UMAD/AD)
 *
 */

/* macros for decoding dmt registers */

#define XMEM(x)  ((x) & (1<<12))
#define XMEM_MODE(x) ((((x)>>2 & 0xf) == 0xf) ? "" : ".bu")
#define MODE(x) ((x)>>2 & 0xf)
#define DOUB(x) ((x) & (1<<13))
#define SIGN(x) ((x) & (1<<6))
#define DAS(x) (((x) & (1<<14)) ? "" : ".usr")
#define REG(x) (((x)>>7) & 0x1f)
#define STORE(x) ((x) & 0x2)

/*
 * return 1 if the printing of the next stage should be surpressed
 */
static int
m88k_dmx_print(unsigned t, unsigned d, unsigned a, unsigned no)
{
    static unsigned addr_mod[16] = { 0, 3, 2, 2, 1, 0, 0, 0,
				     0, 0, 0, 0, 0, 0, 0, 0};
    static char *mode[16]  = { "?", ".b", ".b", ".h", ".b", "?", "?", "?",
			      ".b", ".h", "?" , "?" , "?" , "?", "?", ""};
    static unsigned mask[16] = { 0,           0xff,        0xff00,     0xffff,
				 0xff0000,    0,           0,          0,
				 0xff000000U, 0xffff0000U, 0,          0,
				 0,           0,           0,    0xffffffffU};
    static unsigned shift[16] = { 0,  0, 8, 0, 16, 0, 0, 0,
				 24, 16, 0, 0,  0, 0, 0, 0};
    int reg = REG(t);

    if (XMEM(t))
    {
	db_printf("xmem%s%s r%d(0x%x) <-> mem(0x%x),",
	    XMEM_MODE(t), DAS(t), reg,
	    (((t)>>2 & 0xf) == 0xf) ? d : (d & 0xff), a );
	return 1;
    }
    else
    {
	if (MODE(t) == 0xf)
	{
	    /* full or double word */
	    if (STORE(t))
		if (DOUB(t) && no == 2)
		    db_printf("st.d%s -> mem(0x%x) (** restart sxip **)",
			DAS(t), a);
		else
		    db_printf("st%s (0x%x) -> mem(0x%x)", DAS(t), d, a);
	    else /* load */
		if (DOUB(t) && no == 2)
		    db_printf("ld.d%s r%d <- mem(0x%x), r%d <- mem(0x%x)",
			DAS(t), reg, a, reg+1, a+4);
		else
		    db_printf("ld%s r%d <- mem(0x%x)",  DAS(t), reg, a);
	}
	else
	{
	    /* fractional word - check if load or store */
	    a += addr_mod[MODE(t)];
	    if (STORE(t))
		db_printf("st%s%s (0x%x) -> mem(0x%x)", mode[MODE(t)], DAS(t),
		(d & mask[MODE(t)]) >> shift[MODE(t)], a);
	    else
		db_printf("ld%s%s%s r%d <- mem(0x%x)",
		mode[MODE(t)], SIGN(t) ? "" : "u", DAS(t), reg, a);
	}
    }
    return 0;
}

static void
m88k_db_print_frame(db_expr_t addr, int have_addr, int count, char *modif)
{
    struct m88100_saved_state *s = (struct m88100_saved_state *)addr;
    char *name;
    db_expr_t offset;
    int surpress1 = 0, surpress2 = 0;
    int c, force = 0, help = 0;

    if (!have_addr) {
	db_printf("requires address of frame\n");
	help = 1;
    }

    while (modif && *modif) {
   	switch (c = *modif++, c) {
	case 'f': force = 1; break;
	case 'h': help = 1; break;
        default:
	    db_printf("unknown modifier [%c]\n", c);
	    help = 1;
	    break;
    	}
    }

    if (help) {
	db_printf("usage: mach frame/[f] ADDRESS\n");
	db_printf("  /f force printing of insane frames.\n");
	return;
    }

    if (badwordaddr((vm_offset_t)s) ||
	badwordaddr((vm_offset_t)(&((db_regs_t*)s)->mode))) {
	    db_printf("frame at 0x%08x is unreadable\n", s);
	    return;
    }

    if (!frame_is_sane(s))  /* see db_trace.c */
    {
	db_printf("frame seems insane (");

	if (force)
	    db_printf("forging ahead anyway...)\n");
	else {
	    db_printf("use /f to force)\n");
	    return;
	}
    }

#define R(i) s->r[i]
#define IPMASK(x) ((x) &  ~(3))
    db_printf("R00-05: 0x%08x  0x%08x  0x%08x  0x%08x  0x%08x  0x%08x\n",
	R(0),R(1),R(2),R(3),R(4),R(5));
    db_printf("R06-11: 0x%08x  0x%08x  0x%08x  0x%08x  0x%08x  0x%08x\n",
	R(6),R(7),R(8),R(9),R(10),R(11));
    db_printf("R12-17: 0x%08x  0x%08x  0x%08x  0x%08x  0x%08x  0x%08x\n",
	R(12),R(13),R(14),R(15),R(16),R(17));
    db_printf("R18-23: 0x%08x  0x%08x  0x%08x  0x%08x  0x%08x  0x%08x\n",
	R(18),R(19),R(20),R(21),R(22),R(23));
    db_printf("R24-29: 0x%08x  0x%08x  0x%08x  0x%08x  0x%08x  0x%08x\n",
	R(24),R(25),R(26),R(27),R(28),R(29));
    db_printf("R30-31: 0x%08x  0x%08x\n",R(30),R(31));

    db_printf("sxip: 0x%08x ",s->sxip & ~3);
    db_find_xtrn_sym_and_offset((db_addr_t) IPMASK(s->sxip),&name,&offset);
    if (name!= 0 && (unsigned)offset <= db_maxoff)
	db_printf("%s+0x%08x",name,(unsigned)offset);
    db_printf("\n");
    if (s->snip != s->sxip+4)
    {
	db_printf("snip: 0x%08x ",s->snip);
	db_find_xtrn_sym_and_offset((db_addr_t) IPMASK(s->snip),&name,&offset);
	if (name!= 0 && (unsigned)offset <= db_maxoff)
	    db_printf("%s+0x%08x",name,(unsigned)offset);
	db_printf("\n");
    }
    if (s->sfip != s->snip+4)
    {
	db_printf("sfip: 0x%08x ",s->sfip);
	db_find_xtrn_sym_and_offset((db_addr_t) IPMASK(s->sfip),&name,&offset);
	if (name!= 0 && (unsigned)offset <= db_maxoff)
	    db_printf("%s+0x%08x",name,(unsigned)offset);
	db_printf("\n");
    }

    db_printf("vector: 0x%02x                    interrupt mask: 0x%08x\n",
	s->vector, s->mask);
    db_printf("epsr: 0x%08x                current process: 0x%x\n",
	s->epsr, curproc);

    /*
     * If the vector indicates trap, instead of an exception or
     * interrupt, skip the check of dmt and fp regs.
     *
     * Interrupt and exceptions are vectored at 0-10 and 114-127.
     */

    if (!(s->vector <= 10 || (114 <= s->vector && s->vector <= 127)))
    {
	db_printf("\n\n");
	return;
    }

    if (s->vector == /*data*/3 || s->dmt0 & 1)
    {
	db_printf("dmt,d,a0: 0x%08x  0x%08x  0x%08x ",s->dmt0,s->dmd0,s->dma0);
	db_find_xtrn_sym_and_offset((db_addr_t) s->dma0,&name,&offset);
	if (name!= 0 && (unsigned)offset <= db_maxoff)
	    db_printf("%s+0x%08x",name,(unsigned)offset);
	db_printf("\n          ");
	surpress1 = m88k_dmx_print(s->dmt0|0x01, s->dmd0, s->dma0, 0);
	db_printf("\n");

	if ((s->dmt1 & 1) && (!surpress1))
	{
	    db_printf("dmt,d,a1: 0x%08x  0x%08x  0x%08x ",s->dmt1, s->dmd1,s->dma1);
	    db_find_xtrn_sym_and_offset((db_addr_t) s->dma1,&name,&offset);
	    if (name!= 0 && (unsigned)offset <= db_maxoff)
		db_printf("%s+0x%08x",name,(unsigned)offset);
	    db_printf("\n          ");
	    surpress2 = m88k_dmx_print(s->dmt1, s->dmd1, s->dma1, 1);
	    db_printf("\n");

	    if ((s->dmt2 & 1) && (!surpress2))
	    {
		db_printf("dmt,d,a2: 0x%08x  0x%08x  0x%08x ",s->dmt2, s->dmd2,s->dma2);
		db_find_xtrn_sym_and_offset((db_addr_t) s->dma2,&name,&offset);
		if (name!= 0 && (unsigned)offset <= db_maxoff)
		    db_printf("%s+0x%08x",name,(unsigned)offset);
		db_printf("\n          ");
		(void) m88k_dmx_print(s->dmt2, s->dmd2, s->dma2, 2);
		db_printf("\n");
	    }
	}
    }

    if (s->fpecr & 255) /* floating point error occured */
    {
	db_printf("fpecr: 0x%08x fpsr: 0x%08x fpcr: 0x%08x\n",
	    s->fpecr,s->fpsr,s->fpcr);
	db_printf("fcr1-4: 0x%08x  0x%08x  0x%08x  0x%08x\n",
	    s->fphs1, s->fpls1, s->fphs2, s->fpls2);
	db_printf("fcr5-8: 0x%08x  0x%08x  0x%08x  0x%08x\n",
	    s->fppt,  s->fprh,  s->fprl,  s->fpit);
    }
    db_printf("\n\n");
}

static void
m88k_db_registers(db_expr_t addr, int have_addr, int count, char *modif)
{
    if (modif && *modif) {
	db_printf("usage: mach regs\n");
	return;
    }

    m88k_db_print_frame((db_expr_t)DDB_REGS, TRUE,0,0);
    return;
}

/************************/
/* PAUSE ****************/
/************************/

/*
 * pause for 2*ticks many cycles
 */
static void
m88k_db_pause(unsigned volatile ticks)
{
    while (ticks)
	ticks -= 1;
    return;
}

/*
 *  m88k_db_trap - field a TRACE or BPT trap
 */

m88k_db_trap(
    int type,
    register struct m88100_saved_state *regs)
{

    int i;

#if 0
    if ((i = db_spl()) != 7)
	m88k_db_str1("WARNING: spl is not high in m88k_db_trap (spl=%x)\n", i);
#endif /* 0 */

    if (db_are_interrupts_disabled())
	m88k_db_str("WARNING: entered debugger with interrupts disabled\n");

    switch(type) {
    
    case T_KDB_BREAK:
    case T_KDB_TRACE:
    case T_KDB_ENTRY:
    	break;
    case -1:
    	break;
    default:
    	kdbprinttrap(type, 0);
    	if (db_recover != 0) {
    		db_error("Caught exception in ddb.\n");
    		/*NOTREACHED*/
    	}
    }
    
    ddb_regs = *regs;
    
    db_active++;
    cnpollc(TRUE);
    db_trap(type, 0);
    cnpollc(FALSE);
    db_active--;

    *regs = ddb_regs;

#if 0
    (void) spl7();
#endif
    return(1);
}

extern char *trap_type[];
extern int trap_types;

/*
 * Print trap reason.
 */
kdbprinttrap(type, code)
	int	type, code;
{
	printf("kernel: ");
	if (type >= trap_types || type < 0)
		printf("type %d", type);
	else
		printf("%s", trap_type[type]);
	printf(" trap\n");
}

void
Debugger(void)
{
    asm (ENTRY_ASM); /* entry trap */
    /* ends up at ddb_entry_trap below */
}

/* gimmeabreak - drop execute the ENTRY trap */
void
gimmeabreak(void)
{
    asm (ENTRY_ASM); /* entry trap */
    /* ends up at ddb_entry_trap below */
}

/* fielded a non maskable interrupt */
int
ddb_nmi_trap(int level, db_regs_t *eframe)
{
    NOISY(m88k_db_str("kernel: nmi interrupt\n");)
    m88k_db_trap(T_KDB_ENTRY, eframe);

    return 0;
}

/*
 * When the below routine is entered interrupts should be on
 * but spl should be high
 *
 * The following routine is for breakpoint and watchpoint entry.
 */

/* breakpoint/watchpoint entry */
int
ddb_break_trap(int type, db_regs_t *eframe)
{
    m88k_db_trap(type, eframe);

    if (type == T_KDB_BREAK) {
	/* back up an instruction and retry the instruction at the
	   breakpoint address */
	eframe->sfip = eframe->snip;
	eframe->snip = eframe->sxip;
    }

    return 0;
}

/* enter at splhigh */
int
ddb_entry_trap(int level, db_regs_t *eframe)
{
    m88k_db_trap(T_KDB_ENTRY, eframe);

    return 0;
}

/*
 * When the below routine is entered interrupts should be on
 * but spl should be high
 */
/* error trap - unreturnable */
void
ddb_error_trap(char *error, db_regs_t *eframe)
{

    m88k_db_str1("KERNEL:  terminal error [%s]\n",(int)error);
    m88k_db_str ("KERNEL:  Exiting debugger will cause abort to rom\n");
    m88k_db_str1("at 0x%x ", eframe->sxip & ~3);
    m88k_db_str2("dmt0 0x%x dma0 0x%x", eframe->dmt0, eframe->dma0);
    m88k_db_pause(1000000);
    m88k_db_trap(T_KDB_BREAK, eframe);
}

/*
 * Read bytes from kernel address space for debugger.
 */
void
db_read_bytes(addr, size, data)
	vm_offset_t     addr;
	register int    size;
	register char   *data;
{
    register char	*src;

    src = (char *)addr;

    while(--size >= 0) {
	*data++ = *src++;
    }
}

/*
 * Write bytes to kernel address space for debugger.
 * This should make a text page writable to be able
 * to plant a break point (right now text is mapped with
 * write access in pmap_bootstrap()). XXX nivas
 */
void
db_write_bytes(char *addr, int size, char *data)
{

    register char	*dst;
    int i = size;
    vm_offset_t physaddr;
    pte_template_t 	*pte;

    dst = (char *)addr;
	
    while(--size >= 0) {
#if 0
    	db_printf("byte %x\n", *data);
#endif /* 0 */
	*dst++ = *data++;    
    }
    physaddr = pmap_extract(kernel_pmap, (vm_offset_t)addr);
    cmmu_flush_cache(physaddr, i); 
}

/* to print a character to the console */
void
db_putc(int c)
{
    bugoutchr(c & 0xff);
}

/* to peek at the console; returns -1 if no character is there */
int
db_getc(void)
{
    if (buginstat())
	return (buginchr());
    else
	return -1;
}

/* display where all the cpus are stopped at */
static void
m88k_db_where(db_expr_t addr, int have_addr, db_expr_t count, char *modif)
{
    struct m88100_saved_state *s;
    char *name;
    int *offset;
    int i;
    int l;

    s = DDB_REGS;

    l = m88k_pc(s); /* clear low bits */

    db_find_xtrn_sym_and_offset((db_addr_t) l,&name, (db_expr_t*)&offset);
    if (name && (unsigned)offset <= db_maxoff)
	db_printf("stopped at 0x%x  (%s+0x%x)\n",
	    l, name, offset);
    else
	db_printf("stopped at 0x%x\n", l);
}

/*
 * Walk back a stack, looking for exception frames.
 * These frames are recognized by the routine frame_is_sane. Frames
 * only start with zero, so we only call frame_is_sane if the
 * current address contains zero.
 *
 * If addr is given, it is assumed to an address on the stack to be
 * searched. Otherwise, r31 of the current cpu is used.
 */
static void
m88k_db_frame_search(db_expr_t addr, int have_addr, db_expr_t count, char *modif)
{
#if 1
    db_printf("sorry, frame search currently disabled.\n");
#else
    if (have_addr)
	addr &= ~3; /* round to word */
    else
	addr = (DDB_REGS -> r[31]);

    /* walk back up stack until 8k boundry, looking for 0 */
    while (addr & ((8*1024)-1))
    {
	int i;
	db_read_bytes(addr, 4, &i);
	if (i == 0 && frame_is_sane(i))
	    db_printf("frame found at 0x%x\n", i);
	addr += 4;
    }

    db_printf("(Walked back until 0x%x)\n",addr);
#endif
}

/* flush icache */
static void
m88k_db_iflush(db_expr_t addr, int have_addr, db_expr_t count, char *modif)
{
    addr = 0;
    cmmu_remote_set(addr, CMMU_SCR, 0, CMMU_FLUSH_CACHE_CBI_ALL);
}

/* flush dcache */

static void
m88k_db_dflush(db_expr_t addr, int have_addr, db_expr_t count, char *modif)
{
    addr = 0;

    cmmu_remote_set(addr, CMMU_SCR, 1, CMMU_FLUSH_CACHE_CBI_ALL);
}

/* probe my cache */
static void
m88k_db_peek(db_expr_t addr, int have_addr, int count, char *modif)
{
    int pa12;
    int valmask;

    pa12 = addr & ~((1<<12) -1);

    /* probe dcache */
    cmmu_remote_set(0, CMMU_SAR, 1, addr);

    valmask = cmmu_remote_get(0, CMMU_CSSP, 1);
    db_printf("dcache valmask 0x%x\n", (unsigned)valmask);
    db_printf("dcache tag ports 0x%x 0x%x 0x%x 0x%x\n",
    (unsigned)cmmu_remote_get(0, CMMU_CTP0, 1),
    (unsigned)cmmu_remote_get(0, CMMU_CTP1, 1),
    (unsigned)cmmu_remote_get(0, CMMU_CTP2, 1),
    (unsigned)cmmu_remote_get(0, CMMU_CTP3, 1));

    /* probe icache */
    cmmu_remote_set(0, CMMU_SAR, 0, addr);

    valmask = cmmu_remote_get(0, CMMU_CSSP, 0);
    db_printf("icache valmask 0x%x\n", (unsigned)valmask);
    db_printf("icache tag ports 0x%x 0x%x 0x%x 0x%x\n",
    (unsigned)cmmu_remote_get(0, CMMU_CTP0, 0),
    (unsigned)cmmu_remote_get(0, CMMU_CTP1, 0),
    (unsigned)cmmu_remote_get(0, CMMU_CTP2, 0),
    (unsigned)cmmu_remote_get(0, CMMU_CTP3, 0));

}


/*
 * control how much info the debugger prints about itself
 */
static void
m88k_db_noise(db_expr_t addr, int have_addr, db_expr_t count, char *modif)
{
    if (!have_addr)
    {
	/* if off make noisy; if noisy or very noisy turn off */
	if (db_noisy)
	{
	    db_printf("changing debugger status from %s to quiet\n",
		db_noisy == 1 ? "noisy" :
		db_noisy == 2 ? "very noisy" : "violent");
	    db_noisy = 0;
	}
	else
	{
	    db_printf("changing debugger status from quiet to noisy\n");
	    db_noisy = 1;
	}
    }
    else
	if (addr < 0 || addr > 3)
	    db_printf("invalid noise level to m88k_db_noisy; should be 0, 1, 2, or 3\n");
	else
	{
	    db_noisy = addr;
	    db_printf("debugger noise level set to %s\n",
		db_noisy == 0 ? "quiet" :
		(db_noisy == 1 ? "noisy" :
		db_noisy==2 ? "very noisy" : "violent"));
	}
}

/*
 * See how a virtual address translates.
 * Must have an address.
 */
static void
m88k_db_translate(db_expr_t addr, int have_addr, db_expr_t count, char *modif)
{
#if 0
    char c;
    int verbose_flag = 0;
    int supervisor_flag = 1;
    int wanthelp = 0;

    if (!have_addr)
	wanthelp = 1;
    else {
        while (c = *modif++, c != 0) {
	    switch (c) {
            default:
		db_printf("bad modifier [%c]\n", c); 
		wanthelp = 1;
		break;
            case 'h':
		wanthelp = 1;
		break;
            case 'v':
		verbose_flag++;
		break;
            case 's':
		supervisor_flag = 1;
		break;
            case 'u':
		supervisor_flag = 0;
		break;
    	    }
	}
    }
    if (wanthelp) {
	db_printf("usage: translate[/vvsu] address\n");
	db_printf("flags: v - be verbose (vv - be very verbose)\n");
	db_printf("       s - use cmmu's supervisor area pointer (default)\n");
	db_printf("       u - use cmmu's user area pointer\n");
	return;
    }

    cmmu_show_translation(addr, supervisor_flag, verbose_flag);
#endif /* 0 */
}

void cpu_interrupt_to_db(int cpu_no)
{}


/************************/
/* COMMAND TABLE / INIT */
/************************/

static struct db_command m88k_cache_cmds[] =
{
    { "iflush",    m88k_db_iflush, 0, 0},
    { "dflush",    m88k_db_dflush, 0, 0},
    { "peek",      m88k_db_peek, 0, 0},
    { (char *) 0,}
};

struct db_command db_machine_cmds[] =
{
    {"cache",		0,			0, m88k_cache_cmds},
    {"frame",		m88k_db_print_frame,	0, 0},
    {"noise",		m88k_db_noise,		0, 0},
    {"regs",		m88k_db_registers,	0, 0},
    {"searchframe",	m88k_db_frame_search,	0, 0},
    {"translate",	m88k_db_translate,      0, 0},
    {"where",		m88k_db_where,		0, 0},
    {(char  *) 0,}
};

/*
 * Called from "m88k/m1x7_init.c"
 */
void
kdb_init(void)
{
#ifdef DB_MACHINE_COMMANDS
    db_machine_commands_install(db_machine_cmds);
#endif
    ddb_init();

    db_printf("ddb enabled\n");
}

/*
 * Attempt to figure out the UX name of the task.
 * This is kludgy at best... we can't even be sure the task is a UX task...
 */
#define TOP_OF_USER_STACK USRSTACK
#define MAX_DISTANCE_TO_LOOK (1024 * 10)

#define DB_TASK_NAME_LEN 50

char
*db_task_name()
{
    static unsigned buffer[(DB_TASK_NAME_LEN + 5)/sizeof(unsigned)];
    unsigned ptr = (vm_offset_t)(TOP_OF_USER_STACK - 4);
    unsigned limit = ptr - MAX_DISTANCE_TO_LOOK;
    unsigned word;
    int i;

    /* skip zeros at the end */
    while (ptr > limit &&
	   (i = db_trace_get_val((vm_offset_t)ptr, &word))
	   && (word == 0))
    {
	ptr -= 4; /* continue looking for a non-null word */
    }

    if (ptr <= limit) {
	db_printf("bad name at line %d\n", __LINE__);
	return "<couldn't find 1>";
    } else if (i != 1) {
 	return "<nostack>";
    }

    /* skip looking for null before all the text */
    while (ptr > limit
	&&(i = db_trace_get_val(ptr, &word))
    	&& (word != 0))
    {
	ptr -= 4; /* continue looking for a null word */
    }
    
    if (ptr <= limit) {
	db_printf("bad name at line %d\n", __LINE__);
	return "<couldn't find 2>";
    } else if (i != 1) {
	db_printf("bad name read of %x "
	          "at line %d\n", ptr, __LINE__);
 	return "<bad read 2>";
    }

    ptr += 4; /* go back to the non-null word after this one */

    for (i = 0; i < sizeof(buffer); i++, ptr+=4) {
	buffer[i] = 0; /* just in case it's not read */
	db_trace_get_val((vm_offset_t)ptr, &buffer[i]);
    }
    return (char*)buffer;
}
