/*	$OpenBSD: db_interface.c,v 1.43 2004/09/30 21:48:56 miod Exp $	*/
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
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/reboot.h>

#include <uvm/uvm_extern.h>

#include <machine/asm_macro.h>		/* flush_pipeline()	*/
#include <machine/cmmu.h>		/* CMMU defs		*/
#include <machine/trap.h>		/* current_thread()	*/
#include <machine/db_machdep.h>		/* local ddb stuff	*/
#include <machine/bugio.h>		/* bug routines		*/
#include <machine/locore.h>
#include <machine/cpu_number.h>
#ifdef M88100
#include <machine/m88100.h>
#include <machine/m8820x.h>
#endif

#include <ddb/db_access.h>
#include <ddb/db_command.h>
#include <ddb/db_extern.h>
#include <ddb/db_interface.h>
#include <ddb/db_output.h>
#include <ddb/db_sym.h>

extern label_t *db_recover;
extern unsigned db_trace_get_val(vaddr_t, unsigned *);
extern int frame_is_sane(db_regs_t *, int);
extern void cnpollc(int);
void kdbprinttrap(int, int);

void m88k_db_trap(int, struct trapframe *);
int ddb_nmi_trap(int, db_regs_t *);
void ddb_error_trap(char *, db_regs_t *);
void db_putc(int);
int db_getc(void);
int m88k_dmx_print(unsigned, unsigned, unsigned, unsigned);
void m88k_db_pause(unsigned);
void m88k_db_print_frame(db_expr_t, int, db_expr_t, char *);
void m88k_db_registers(db_expr_t, int, db_expr_t, char *);
void m88k_db_where(db_expr_t, int, db_expr_t, char *);
void m88k_db_frame_search(db_expr_t, int, db_expr_t, char *);
void m88k_db_iflush(db_expr_t, int, db_expr_t, char *);
void m88k_db_dflush(db_expr_t, int, db_expr_t, char *);
void m88k_db_peek(db_expr_t, int, db_expr_t, char *);
void m88k_db_noise(db_expr_t, int, db_expr_t, char *);
void m88k_db_translate(db_expr_t, int, db_expr_t, char *);
void m88k_db_cmmucfg(db_expr_t, int, db_expr_t, char *);
void m88k_db_prom_cmd(db_expr_t, int, db_expr_t, char *);

int 	db_active;
int 	db_noisy;

db_regs_t	ddb_regs;

/*
 *
 * If you really feel like understanding the following procedure and
 * macros, see pages 6-22 to 6-30 (Section 6.7.3) of
 *
 * MC88100 RISC Microprocessor User's Manual Second Edition
 * (Motorola Order: MC88100UM/AD REV 1)
 *
 * and ERRATA-5 (6-23, 6-24, 6-24) of
 *
 * Errata to MC88100 User's Manual Second Edition MC88100UM/AD Rev 1
 * (Oct 2, 1990)
 * (Motorola Order: MC88100UMAD/AD)
 *
 */

#ifdef M88100
/* macros for decoding dmt registers */

/*
 * return 1 if the printing of the next stage should be suppressed
 */
int
m88k_dmx_print(t, d, a, no)
	unsigned t, d, a, no;
{
	static const unsigned addr_mod[16] = {
		0, 3, 2, 2, 1, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0
	};
	static const char *mode[16]  = {
		"?", ".b", ".b", ".h", ".b", "?", "?", "?",
		".b", "?", "?" , "?" , ".h" , "?", "?", ""
	};
	static const unsigned mask[16] = {
		0, 0xff, 0xff00, 0xffff,
		0xff0000, 0, 0, 0,
		0xff000000, 0, 0, 0,
		0xffff0000, 0, 0, 0xffffffff
	};
	static const unsigned shift[16] = {
		0,  0, 8, 0, 16, 0, 0, 0,
		24, 0, 0, 0, 16, 0, 0, 0
	};
	int reg = DMT_DREGBITS(t);

	if (ISSET(t, DMT_LOCKBAR)) {
		db_printf("xmem%s%s r%d(0x%x) <-> mem(0x%x),",
			  DMT_ENBITS(t) == 0x0f ? "" : ".bu", ISSET(t, DMT_DAS) ? "" : ".usr", reg,
			  (((t)>>2 & 0xf) == 0xf) ? d : (d & 0xff), a);
		return 1;
	} else {
		if (DMT_ENBITS(t) == 0xf) {
			/* full or double word */
			if (ISSET(t, DMT_WRITE)) {
				if (ISSET(t, DMT_DOUB1) && no == 2)
					db_printf("st.d%s -> mem(0x%x) (** restart sxip **)",
						  ISSET(t, DMT_DAS) ? "" : ".usr", a);
				else
					db_printf("st%s (0x%x) -> mem(0x%x)",
						  ISSET(t, DMT_DAS) ? "" : ".usr", d, a);
			} else {
				/* load */
				if (ISSET(t, DMT_DOUB1) && no == 2)
					db_printf("ld.d%s r%d <- mem(0x%x), r%d <- mem(0x%x)",
						  ISSET(t, DMT_DAS) ? "" : ".usr", reg, a, reg+1, a+4);
				else
					db_printf("ld%s r%d <- mem(0x%x)",
						  ISSET(t, DMT_DAS) ? "" : ".usr", reg, a);
			}
		} else {
			/* fractional word - check if load or store */
			a += addr_mod[DMT_ENBITS(t)];
			if (ISSET(t, DMT_WRITE))
				db_printf("st%s%s (0x%x) -> mem(0x%x)",
					  mode[DMT_ENBITS(t)], ISSET(t, DMT_DAS) ? "" : ".usr",
					  (d & mask[DMT_ENBITS(t)]) >> shift[DMT_ENBITS(t)], a);
			else
				db_printf("ld%s%s%s r%d <- mem(0x%x)",
				    mode[DMT_ENBITS(t)],
				    ISSET(t, DMT_SIGNED) ? "" : "u",
				    ISSET(t, DMT_DAS) ? "" : ".usr", reg, a);
		}
	}
	return 0;
}
#endif	/* M88100 */

void
m88k_db_print_frame(addr, have_addr, count, modif)
	db_expr_t addr;
	int have_addr;
	db_expr_t count;
	char *modif;
{
	struct trapframe *s = (struct trapframe *)addr;
	char *name;
	db_expr_t offset;
#ifdef M88100
	int suppress1 = 0, suppress2 = 0;
#endif
	int c, force = 0, help = 0;

	if (!have_addr) {
		db_printf("requires address of frame\n");
		help = 1;
	}

	while (modif && *modif) {
		switch (c = *modif++, c) {
		case 'f':
			force = 1;
			break;
		case 'h':
			help = 1;
			break;
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

	if (badwordaddr((vaddr_t)s) ||
	    badwordaddr((vaddr_t)(&((db_regs_t*)s)->fpit))) {
		db_printf("frame at %8p is unreadable\n", s);
		return;
	}

	if (frame_is_sane((db_regs_t *)s, 0) == 0) {	/* see db_trace.c */
		if (force == 0)
			return;
	}

#define R(i) s->tf_r[i]
#define IPMASK(x) ((x) &  ~(3))
	db_printf("R00-05: 0x%08x  0x%08x  0x%08x  0x%08x  0x%08x  0x%08x\n",
		  R(0), R(1), R(2), R(3), R(4), R(5));
	db_printf("R06-11: 0x%08x  0x%08x  0x%08x  0x%08x  0x%08x  0x%08x\n",
		  R(6), R(7), R(8), R(9), R(10), R(11));
	db_printf("R12-17: 0x%08x  0x%08x  0x%08x  0x%08x  0x%08x  0x%08x\n",
		  R(12), R(13), R(14), R(15), R(16), R(17));
	db_printf("R18-23: 0x%08x  0x%08x  0x%08x  0x%08x  0x%08x  0x%08x\n",
		  R(18), R(19), R(20), R(21), R(22), R(23));
	db_printf("R24-29: 0x%08x  0x%08x  0x%08x  0x%08x  0x%08x  0x%08x\n",
		  R(24), R(25), R(26), R(27), R(28), R(29));
	db_printf("R30-31: 0x%08x  0x%08x\n", R(30), R(31));

	db_printf("%cxip: 0x%08x ",
	    CPU_IS88110 ? 'e' : 's', s->tf_sxip & ~3);
	db_find_xtrn_sym_and_offset((db_addr_t)IPMASK(s->tf_sxip),
	    &name, &offset);
	if (name != NULL && (unsigned)offset <= db_maxoff)
		db_printf("%s+0x%08x", name, (unsigned)offset);
	db_printf("\n");

	if (s->tf_snip != s->tf_sxip + 4) {
		db_printf("%cnip: 0x%08x ",
		    CPU_IS88110 ? 'e' : 's', s->tf_snip);
		db_find_xtrn_sym_and_offset((db_addr_t)IPMASK(s->tf_snip),
		    &name, &offset);
		if (name != NULL && (unsigned)offset <= db_maxoff)
			db_printf("%s+0x%08x", name, (unsigned)offset);
		db_printf("\n");
	}

#ifdef M88100
	if (CPU_IS88100) {
		if (s->tf_sfip != s->tf_snip + 4) {
			db_printf("sfip: 0x%08x ", s->tf_sfip);
			db_find_xtrn_sym_and_offset((db_addr_t)IPMASK(s->tf_sfip),
			    &name, &offset);
			if (name != NULL && (unsigned)offset <= db_maxoff)
				db_printf("%s+0x%08x", name, (unsigned)offset);
			db_printf("\n");
		}
	}
#endif
#ifdef M88110
	if (CPU_IS88110) {
		db_printf("fpsr: 0x%08x fpcr: 0x%08x fpecr: 0x%08x\n",
			  s->tf_fpsr, s->tf_fpcr, s->tf_fpecr);
		db_printf("dsap 0x%08x duap 0x%08x dsr 0x%08x dlar 0x%08x dpar 0x%08x\n",
			  s->tf_dsap, s->tf_duap, s->tf_dsr, s->tf_dlar, s->tf_dpar);
		db_printf("isap 0x%08x iuap 0x%08x isr 0x%08x ilar 0x%08x ipar 0x%08x\n",
			  s->tf_isap, s->tf_iuap, s->tf_isr, s->tf_ilar, s->tf_ipar);
	}
#endif

	db_printf("epsr: 0x%08x                current process: %p\n",
		  s->tf_epsr, curproc);
	db_printf("vector: 0x%02x                    interrupt mask: 0x%08x\n",
		  s->tf_vector, s->tf_mask);

	/*
	 * If the vector indicates trap, instead of an exception or
	 * interrupt, skip the check of dmt and fp regs.
	 *
	 * Interrupt and exceptions are vectored at 0-10 and 114-127.
	 */

	if (!(s->tf_vector <= 10 || (114 <= s->tf_vector && s->tf_vector <= 127))) {
		db_printf("\n");
		return;
	}

#ifdef M88100
	if (CPU_IS88100) {
		if (s->tf_vector == /*data*/3 || s->tf_dmt0 & DMT_VALID) {
			db_printf("dmt,d,a0: 0x%08x  0x%08x  0x%08x ",
			    s->tf_dmt0, s->tf_dmd0, s->tf_dma0);
			db_find_xtrn_sym_and_offset((db_addr_t)s->tf_dma0, &name, &offset);
			if (name != NULL && (unsigned)offset <= db_maxoff)
				db_printf("%s+0x%08x", name, (unsigned)offset);
			db_printf("\n          ");

			suppress1 = m88k_dmx_print(s->tf_dmt0, s->tf_dmd0, s->tf_dma0, 0);
			db_printf("\n");

			if ((s->tf_dmt1 & DMT_VALID) && (!suppress1)) {
				db_printf("dmt,d,a1: 0x%08x  0x%08x  0x%08x ",
				    s->tf_dmt1, s->tf_dmd1, s->tf_dma1);
				db_find_xtrn_sym_and_offset((db_addr_t)s->tf_dma1,
				    &name, &offset);
				if (name != NULL &&
				    (unsigned)offset <= db_maxoff)
					db_printf("%s+0x%08x", name, (unsigned)offset);
				db_printf("\n          ");
				suppress2 = m88k_dmx_print(s->tf_dmt1, s->tf_dmd1, s->tf_dma1, 1);
				db_printf("\n");

				if ((s->tf_dmt2 & DMT_VALID) && (!suppress2)) {
					db_printf("dmt,d,a2: 0x%08x  0x%08x  0x%08x ",
						  s->tf_dmt2, s->tf_dmd2, s->tf_dma2);
					db_find_xtrn_sym_and_offset((db_addr_t)s->tf_dma2,
								    &name, &offset);
					if (name != 0 && (unsigned)offset <= db_maxoff)
						db_printf("%s+0x%08x", name, (unsigned)offset);
					db_printf("\n          ");
					m88k_dmx_print(s->tf_dmt2, s->tf_dmd2, s->tf_dma2, 2);
					db_printf("\n");
				}
			}

			db_printf("fault code %d\n",
			    CMMU_PFSR_FAULT(s->tf_dpfsr));
		}
	}
#endif	/* M88100 */

	if (s->tf_fpecr & 255) { /* floating point error occurred */
		db_printf("fpecr: 0x%08x fpsr: 0x%08x fpcr: 0x%08x\n",
			  s->tf_fpecr, s->tf_fpsr, s->tf_fpcr);
#ifdef M88100
		if (CPU_IS88100) {
			db_printf("fcr1-4: 0x%08x  0x%08x  0x%08x  0x%08x\n",
				  s->tf_fphs1, s->tf_fpls1, s->tf_fphs2, s->tf_fpls2);
			db_printf("fcr5-8: 0x%08x  0x%08x  0x%08x  0x%08x\n",
				  s->tf_fppt, s->tf_fprh, s->tf_fprl, s->tf_fpit);
		}
#endif
	}
	db_printf("\n");
}

void
m88k_db_registers(addr, have_addr, count, modif)
	db_expr_t addr;
	int have_addr;
	db_expr_t count;
	char *modif;
{
	m88k_db_print_frame((db_expr_t)DDB_REGS, TRUE, 0, modif);
}

/*
 * pause for 2*ticks many cycles
 */
void
m88k_db_pause(ticks)
	unsigned volatile ticks;
{
	while (ticks)
		ticks -= 1;
}

/*
 * m88k_db_trap - field a TRACE or BPT trap
 * Note that only the tf_regs part of the frame is valid - some ddb routines
 * invoke this function with a promoted struct reg!
 */
void
m88k_db_trap(type, frame)
	int type;
	struct trapframe *frame;
{

	if (get_psr() & (1 << PSR_INTERRUPT_DISABLE_BIT))
		db_printf("WARNING: entered debugger with interrupts disabled\n");

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

	ddb_regs = frame->tf_regs;

	db_active++;
	cnpollc(TRUE);
	db_trap(type, 0);
	cnpollc(FALSE);
	db_active--;

	frame->tf_regs = ddb_regs;
}

extern const char *trap_type[];
extern const int trap_types;

/*
 * Print trap reason.
 */
void
kdbprinttrap(type, code)
	int type, code;
{
	printf("kernel: ");
	if (type >= trap_types || type < 0)
		printf("type %d", type);
	else
		printf("%s", trap_type[type]);
	printf(" trap\n");
}

void
Debugger()
{
	asm (ENTRY_ASM); /* entry trap */
	/* ends up at ddb_entry_trap below */
}

/* fielded a non maskable interrupt */
int
ddb_nmi_trap(level, eframe)
	int level;
	db_regs_t *eframe;
{
	if (db_noisy)
		db_printf("kernel: nmi interrupt\n");
	m88k_db_trap(T_KDB_ENTRY, (struct trapframe *)eframe);

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
ddb_break_trap(type, eframe)
	int type;
	db_regs_t *eframe;
{
	m88k_db_trap(type, (struct trapframe *)eframe);

	if (type == T_KDB_BREAK) {
		/*
		 * back up an instruction and retry the instruction
		 * at the breakpoint address.  mc88110's exip reg
		 * already has the address of the exception instruction.
		 */
		if (CPU_IS88100) {
			eframe->sfip = eframe->snip;
			eframe->snip = eframe->sxip;
		}
	}

	return 0;
}

/* enter at splhigh */
int
ddb_entry_trap(level, eframe)
	int level;
	db_regs_t *eframe;
{
	m88k_db_trap(T_KDB_ENTRY, (struct trapframe *)eframe);

	return 0;
}

/*
 * When the below routine is entered interrupts should be on
 * but spl should be high
 */
/* error trap - unreturnable */
void
ddb_error_trap(error, regs)
	char *error;
	db_regs_t *regs;
{
	db_printf("KERNEL:  terminal error [%s]\n", error);
	db_printf("KERNEL:  Exiting debugger will cause abort to rom\n");
	db_printf("at 0x%x ", regs->sxip & XIP_ADDR);
	db_printf("dmt0 0x%x dma0 0x%x", regs->dmt0, regs->dma0);
	m88k_db_pause(1000000);
	m88k_db_trap(T_KDB_BREAK, (struct trapframe *)regs);
}

/*
 * Read bytes from kernel address space for debugger.
 */
void
db_read_bytes(db_addr_t addr, size_t size, char *data)
{
	char *src;

	src = (char *)addr;

	while(size-- > 0) {
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
db_write_bytes(db_addr_t addr, size_t size, char *data)
{
	char *dst;
	paddr_t physaddr;
	psize_t psize = size;

	dst = (char *)addr;

	while (size-- > 0) {
		*dst++ = *data++;
	}
	/* XXX test return value */
	pmap_extract(pmap_kernel(), (vaddr_t)addr, &physaddr);
	cmmu_flush_cache(cpu_number(), physaddr, psize);
}

/* to print a character to the console */
void
db_putc(c)
	int c;
{
	bugoutchr(c & 0xff);
}

/* to peek at the console; returns -1 if no character is there */
int
db_getc()
{
	if (buginstat())
		return (buginchr());
	else
		return -1;
}

/* display where all the cpus are stopped at */
void
m88k_db_where(addr, have_addr, count, modif)
	db_expr_t addr;
	int have_addr;
	db_expr_t count;
	char *modif;
{
	char *name;
	db_expr_t offset;
	db_addr_t l;

	l = PC_REGS(DDB_REGS); /* clear low bits */

	db_find_xtrn_sym_and_offset(l, &name, &offset);
	if (name && (unsigned)offset <= db_maxoff)
		db_printf("stopped at 0x%lx  (%s+0x%x)\n", l, name, offset);
	else
		db_printf("stopped at 0x%lx\n", l);
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
void
m88k_db_frame_search(addr, have_addr, count, modif)
	db_expr_t addr;
	int have_addr;
	db_expr_t count;
	char *modif;
{
	if (have_addr)
		addr &= ~3; /* round to word */
	else
		addr = (DDB_REGS->r[31]);

	/* walk back up stack until 8k boundry, looking for 0 */
	while (addr & ((8 * 1024) - 1)) {
		if (frame_is_sane((db_regs_t *)addr, 1) != 0)
			db_printf("frame found at 0x%x\n", addr);
		addr += 4;
	}

	db_printf("(Walked back until 0x%x)\n",addr);
}

/* flush icache */
void
m88k_db_iflush(addr, have_addr, count, modif)
	db_expr_t addr;
	int have_addr;
	db_expr_t count;
	char *modif;
{
	addr = 0;
#ifdef may_be_removed
	cmmu_remote_set(addr, CMMU_SCR, 0, CMMU_FLUSH_CACHE_CBI_ALL);
#endif
}

/* flush dcache */

void
m88k_db_dflush(addr, have_addr, count, modif)
	db_expr_t addr;
	int have_addr;
	db_expr_t count;
	char *modif;
{
	addr = 0;
#ifdef may_be_removed
	cmmu_remote_set(addr, CMMU_SCR, 1, CMMU_FLUSH_CACHE_CBI_ALL);
#endif
}

/* probe my cache */
void
m88k_db_peek(addr, have_addr, count, modif)
	db_expr_t addr;
	int have_addr;
	db_expr_t count;
	char *modif;
{
#ifdef may_be_removed
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
#endif
}


/*
 * control how much info the debugger prints about itself
 */
void
m88k_db_noise(addr, have_addr, count, modif)
	db_expr_t addr;
	int have_addr;
	db_expr_t count;
	char *modif;
{
	if (!have_addr) {
		/* if off make noisy; if noisy or very noisy turn off */
		if (db_noisy) {
			db_printf("changing debugger status from %s to quiet\n",
				  db_noisy == 1 ? "noisy" :
				  db_noisy == 2 ? "very noisy" : "violent");
			db_noisy = 0;
		} else {
			db_printf("changing debugger status from quiet to noisy\n");
			db_noisy = 1;
		}
	} else if (addr < 0 || addr > 3)
		db_printf("invalid noise level to m88k_db_noisy; should be 0, 1, 2, or 3\n");
	else {
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
void
m88k_db_translate(addr, have_addr, count, modif)
	db_expr_t addr;
	int have_addr;
	db_expr_t count;
	char *modif;
{
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
	cmmu_show_translation(addr, supervisor_flag, verbose_flag, -1);
}

void
m88k_db_cmmucfg(addr, have_addr, count, modif)
	db_expr_t addr;
	int have_addr;
	db_expr_t count;
	char *modif;
{
	if (modif != NULL && *modif != 0) {
		db_printf("usage: mach cmmucfg\n");
	return;
	}

	cmmu_dump_config();
}

void
m88k_db_prom_cmd(addr, have_addr, count, modif)
	db_expr_t addr;
	int have_addr;
	db_expr_t count;
	char *modif;
{
	doboot();
}

/************************/
/* COMMAND TABLE / INIT */
/************************/

struct db_command m88k_cache_cmds[] = {
	{ "iflush",	m88k_db_iflush,	0,	NULL },
	{ "dflush",	m88k_db_dflush,	0,	NULL },
	{ "peek",	m88k_db_peek,	0,	NULL },
	{ NULL,		NULL,		0,	NULL }
};

struct db_command db_machine_cmds[] = {
	{ "cache",	NULL,			0,	m88k_cache_cmds },
	{ "frame",	m88k_db_print_frame,	0,	NULL },
	{ "regs",	m88k_db_registers,	0,	NULL },
	{ "noise",	m88k_db_noise,		0,	NULL },
	{ "searchframe",m88k_db_frame_search,	0,	NULL },
	{ "translate",	m88k_db_translate,	0,	NULL },
	{ "cmmucfg",	m88k_db_cmmucfg,	0,	NULL },
	{ "where",	m88k_db_where,		0,	NULL },
	{ "prom",	m88k_db_prom_cmd,	0,	NULL },
	{ NULL,		NULL,			0,	NULL }
};

void
db_machine_init()
{
	db_machine_commands_install(db_machine_cmds);
}
