/*	$OpenBSD: pctr.c,v 1.3 1998/05/27 02:26:07 downsj Exp $	*/

/*
 * Pentium performance counter control program for OpenBSD.
 * Copyright 1996 David Mazieres <dm@lcs.mit.edu>.
 *
 * Modification and redistribution in source and binary forms is
 * permitted provided that due credit is given to the author and the
 * OpenBSD project (for instance by leaving this copyright notice
 * intact).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <machine/cpu.h>
#include <machine/pctr.h>

char *progname;
int cpufamily;

#define CFL_MESI 0x1   /* Unit mask accepts MESI encoding */
#define CFL_SA   0x2   /* Unit mask accepts Self/Any bit */
#define CFL_C0   0x4   /* Counter 0 only */
#define CFL_C1   0x8   /* Counter 1 only */

struct ctrfn {
  u_int fn;
  int flags;
  char *name;
  char *desc;
};

struct ctrfn p5fn[] = {
  {0x00, 0, "Data read", NULL},
  {0x01, 0, "Data write", NULL},
  {0x02, 0, "Data TLB miss", NULL},
  {0x03, 0, "Data read miss", NULL},
  {0x04, 0, "Data write miss", NULL},
  {0x05, 0, "Write (hit) to M or E state lines", NULL},
  {0x06, 0, "Data cache lines written back", NULL},
  {0x07, 0, "Data cache snoops", NULL},
  {0x08, 0, "Data cache snoop hits", NULL},
  {0x09, 0, "Memory accesses in both pipes", NULL},
  {0x0a, 0, "Bank conflicts", NULL},
  {0x0b, 0, "Misaligned data memory references", NULL},
  {0x0c, 0, "Code read", NULL},
  {0x0d, 0, "Code TLB miss", NULL},
  {0x0e, 0, "Code cache miss", NULL},
  {0x0f, 0, "Any segment register load", NULL},
  {0x12, 0, "Branches", NULL},
  {0x13, 0, "BTB hits", NULL},
  {0x14, 0, "Taken branch or BTB hit", NULL},
  {0x15, 0, "Pipeline flushes", NULL},
  {0x16, 0, "Instructions executed", NULL},
  {0x17, 0, "Instructions executed in the V-pipe", NULL},
  {0x18, 0, "Bus utilization (clocks)", NULL},
  {0x19, 0, "Pipeline stalled by write backup", NULL},
  {0x1a, 0, "Pipeline stalled by data memory read", NULL},
  {0x1b, 0, "Pipeline stalled by write to E or M line", NULL},
  {0x1c, 0, "Locked bus cycle", NULL},
  {0x1d, 0, "I/O read or write cycle", NULL},
  {0x1e, 0, "Noncacheable memory references", NULL},
  {0x1f, 0, "AGI (Address Generation Interlock)", NULL},
  {0x22, 0, "Floating-point operations", NULL},
  {0x23, 0, "Breakpoint 0 match", NULL},
  {0x24, 0, "Breakpoint 1 match", NULL},
  {0x25, 0, "Breakpoint 2 match", NULL},
  {0x26, 0, "Breakpoint 3 match", NULL},
  {0x27, 0, "Hardware interupts", NULL},
  {0x28, 0, "Data read or data write", NULL},
  {0x29, 0, "Data read miss or data write miss", NULL},
  {0x0, 0, NULL, NULL},
};

struct ctrfn p6fn[] = {
  {0x03, 0, "LD_BLOCKS",
   "Number of store buffer blocks."},
  {0x04, 0, "SB_DRAINS",
   "Number of store buffer drain cycles."},
  {0x05, 0, "MISALIGN_MEM_REF",
   "Number of misaligned data memory references."},
  {0x06, 0, "SEGMENT_REG_LOADS",
   "Number of segment register loads."},
  {0x10, CFL_C0, "FP_COMP_OPS_EXE",
   "Number of computational floating-point operations executed."},
  {0x11, CFL_C1, "FP_ASSIST",
   "Number of floating-point exception cases handled by microcode."},
  {0x12, CFL_C1, "MUL",
   "Number of multiplies."},
  {0x13, CFL_C1, "DIV",
   "Number of divides."},
  {0x14, CFL_C0, "CYCLES_DIV_BUSY",
   "Number of cycles during which the divider is busy."},
  {0x21, 0, "L2_ADS",
   "Number of L2 address strobes."},
  {0x22, 0, "L2_DBUS_BUSY",
   "Number of cycles durring which the data bus was busy."},
  {0x23, 0, "L2_DBUS_BUSY_RD",
   "Number of cycles during which the data bus was busy transferring "
   "data from L2 to the processor."},
  {0x24, 0, "L2_LINES_IN",
   "Number of lines allocated in the L2."},
  {0x25, 0, "L2_M_LINES_INM",
   "Number of modified lines allocated in the L2."},
  {0x26, 0, "L2_LINES_OUT",
   "Number of lines removed from the L2 for any reason."},
  {0x27, 0, "L2_M_LINES_OUTM",
   "Number of modified lines removed from the L2 for any reason."},
  {0x28, CFL_MESI, "L2_IFETCH",
   "Number of L2 instruction fetches."},
  {0x29, CFL_MESI, "L2_LD", 
   "Number of L2 data loads."},
  {0x2a, CFL_MESI, "L2_ST",
   "Number of L2 data stores."},
  {0x2e, CFL_MESI, "L2_RQSTS",
   "Number of L2 requests."},
  {0x43, 0, "DATA_MEM_REFS",
   "All memory references, both cacheable and non-cacheable."},
  {0x45, 0, "DCU_LINES_IN",
   "Total lines allocated in the DCU."},
  {0x46, 0, "DCU_M_LINES_IN",
   "Number of M state lines allocated in the DCU."},
  {0x47, 0, "DCU_M_LINES_OUT",
   "Number of M state lines evicted from the DCU.  "
   "This includes evictions via snoop HITM, intervention or replacement"},
  {0x48, 0, "DCU_MISS_OUTSTANDING",
   "Weighted number of cycles while a DCU miss is outstanding."},
  {0x60, 0, "BUS_REQ_OUTSTANDING",
   "Number of bus requests outstanding."},
  {0x61, 0, "BUS_BNR_DRV",
   "Number of bus clock cycles during which the processor is "
   "driving the BNR pin."},
  {0x62, CFL_SA, "BUS_DRDY_CLOCKS",
   "Number of clocks during which DRDY is asserted."},
  {0x63, CFL_SA, "BUS_LOCK_CLOCKS",
   "Number of clocks during which LOCK is asserted."},
  {0x64, 0, "BUS_DATA_RCV",
   "Number of bus clock cycles during which the processor is "
   "receiving data."},
  {0x65, CFL_SA, "BUS_TRAN_BRD",
   "Number of burst read transactions."},
  {0x66, CFL_SA, "BUS_TRAN_RFO",
   "Number of read for ownership transactions."},
  {0x67, CFL_SA, "BUS_TRANS_WB", 
   "Number of write back transactions."},
  {0x68, CFL_SA, "BUS_TRAN_IFETCH",
   "Number of instruction fetch transactions."},
  {0x69, CFL_SA, "BUS_TRAN_INVAL",
   "Number of invalidate transactions."},
  {0x6a, CFL_SA, "BUS_TRAN_PWR",
   "Number of partial write transactions."},
  {0x6b, CFL_SA, "BUS_TRANS_P",
   "Number of partial transactions."},
  {0x6c, CFL_SA, "BUS_TRANS_IO",
   "Number of I/O transactions."},
  {0x6d, CFL_SA, "BUS_TRAN_DEF",
   "Number of deferred transactions."},
  {0x6e, CFL_SA, "BUS_TRAN_BURST",
   "Number of burst transactions."},
  {0x6f, CFL_SA, "BUS_TRAN_MEM",
   "Number of memory transactions."},
  {0x70, CFL_SA, "BUS_TRAN_ANY",
   "Number of all transactions."},
  {0x79, 0, "CPU_CLK_UNHALTED",
   "Number of cycles during which the processor is not halted."},
  {0x7a, 0, "BUS_HIT_DRV",
   "Number of bus clock cycles during which the processor is "
   "driving the HIT pin."},
  {0x7b, 0, "BUS_HITM_DRV",
   "Number of bus clock cycles during which the processor is "
   "driving the HITM pin."},
  {0x7e, 0, "BUS_SNOOP_STALL",
   "Number of clock cycles during which the bus is snoop stalled."},
  {0x80, 0, "IFU_IFETCH",
   "Number of instruction fetches, both cacheable and non-cacheable."},
  {0x81, 0, "IFU_IFETCH_MISS",
   "Number of instruction fetch misses."},
  {0x85, 0, "ITLB_MISS",
   "Number of ITLB misses."},
  {0x86, 0, "IFU_MEM_STALL",
   "Number of cycles that the instruction fetch pipe stage is stalled, "
   "including cache mises, ITLB misses, ITLB faults, "
   "and victim cache evictions"},
  {0x87, 0, "ILD_STALL",
   "Number of cycles that the instruction length decoder is stalled"},
  {0xa2, 0, "RESOURCE_STALLS",
   "Number of cycles during which there are resource-related stalls."},
  {0xc0, 0, "INST_RETIRED",
   "Number of instructions retired."},
  {0xc1, CFL_C0, "FLOPS",
   "Number of computational floating-point operations retired."},
  {0xc2, 0, "UOPS_RETIRED",
   "Number of UOPs retired."},
  {0xc4, 0, "BR_INST_RETIRED",
   "Number of branch instructions retired."},
  {0xc5, 0, "BR_MISS_PRED_RETIRED",
   "Number of mispredicted branches retired."},
  {0xc6, 0, "CYCLES_INT_MASKED",
   "Number of processor cycles for which interrupts are disabled."},
  {0xc7, 0, "CYCLES_INT_PENDING_AND_MASKED",
   "Number of processor cycles for which interrupts are disabled "
   "and interrupts are pending."},
  {0xc8, 0, "HW_INT_RX",
   "Number of hardware interrupts received."},
  {0xc9, 0, "BR_TAKEN_RETIRED",
   "Number of taken branches retired."},
  {0xca, 0, "BR_MISS_PRED_TAKEN_RET",
   "Number of taken mispredictioned branches retired."},
  {0xd0, 0, "INST_DECODER",
   "Number of instructions decoded."},
  {0xd2, 0, "PARTIAL_RAT_STALLS",
   "Number of cycles or events for partial stalls."},
  {0xe0, 0, "BR_INST_DECODED",
   "Number of branch instructions decoded."},
  {0xe2, 0, "BTB_MISSES",
   "Number of branches that miss the BTB."},
  {0xe4, 0, "BR_BOGUS",
   "Number of bogus branches."},
  {0xe6, 0, "BACLEARS",
   "Number of times BACLEAR is asserted."},
  {0x0, 0, NULL, NULL},
};

#define __cpuid()				\
({						\
  pctrval id;					\
  __asm __volatile ("pushfl\n"			\
		    "\tpopl %%eax\n"		\
		    "\tmovl %%eax,%%ecx\n"	\
		    "\txorl %1,%%eax\n"		\
		    "\tpushl %%eax\n"		\
		    "\tpopfl\n"			\
		    "\tpushfl\n"		\
		    "\tpopl %%eax\n"		\
		    "\tpushl %%ecx\n"		\
		    "\tpopfl\n"			\
		    "\tcmpl %%eax,%%ecx\n"	\
		    "\tmovl $0,%%eax\n"		\
		    "\tje 1f\n"			\
		    "\tcpuid\n"			\
		    "\ttestl %%eax,%%eax\n"	\
		    "\tje 1f\n"			\
		    "\tmovl $1,%%eax\n"		\
		    "\tcpuid\n"			\
		    "\tjmp 2f\n"		\
		    "1:\t"			\
		    "\txorl %%eax,%%eax\n"	\
		    "\txorl %%edx,%%edx\n"	\
		    "2:\t"			\
		    : "=A" (id) : "i" (PSL_ID)	\
		    : "edx", "ecx", "ebx");	\
  id;						\
})

static void
printdesc (char *desc)
{
  char *p;

  for (;;) {
    while (*desc == ' ')
      desc++;
    if (strlen (desc) < 70) {
      if (*desc)
	printf ("      %s\n", desc);
      return;
    }
    p = desc + 72;
    while (*--p != ' ')
      ;
    while (*--p == ' ')
      ;
    p++;
    printf ("      %.*s\n", p - desc, desc);
    desc = p;
  }

}

/* Print all possible counter functions */
static void
list (int fam)
{
  struct ctrfn *cfnp;

  if (fam == 5)
    cfnp = p5fn;
  else if (fam == 6)
    cfnp = p6fn;
  else {
    fprintf (stderr, "Unknown CPU family %d\n", fam);
    exit (1);
  }
  printf ("Hardware counter functions for the %s:\n\n",
	  fam == 5 ? "Pentium" : "Pentium Pro");
  for (; cfnp->name; cfnp++) {
    printf ("%02x  %s", cfnp->fn, cfnp->name);
    if (cfnp->flags & CFL_MESI)
      printf ("/mesi");
    else if (cfnp->flags & CFL_SA)
      printf ("/a");
    if (cfnp->flags & CFL_C0)
      printf ("  (ctr0 only)");
    if (cfnp->flags & CFL_C1)
      printf ("  (ctr1 only)");
    printf ("\n");
    if (cfnp->desc)
      printdesc (cfnp->desc);
  }
}

struct ctrfn *
fn2cfnp (u_int family, u_int sel)
{
  struct ctrfn *cfnp;

  if (family == 6) {
    cfnp = p6fn;
    sel &= 0xff;
  }
  else {
    cfnp = p5fn;
    sel &= 0x3f;
  }
  for (; cfnp->name; cfnp++)
    if (cfnp->fn == sel)
      return (cfnp);
  return (NULL);
}

static char *
fn2str (int family, u_int sel)
{
  static char buf[128];
  char um[9] = "";
  char cm[6] = "";
  struct ctrfn *cfnp;
  u_int fn;

  if (family == 5) {
    fn = sel & 0x3f;
    cfnp = fn2cfnp (family, fn);
    sprintf (buf, "%c%c%c %02x %s",
	     sel & P5CTR_C ? 'c' : '-',
	     sel & P5CTR_U ? 'u' : '-',
	     sel & P5CTR_K ? 'k' : '-',
	     fn, cfnp ? cfnp->name : "unknown function");
  }
  else if (family == 6) {
    fn = sel & 0xff;
    cfnp = fn2cfnp (family, fn);
    if (cfnp && cfnp->flags & CFL_MESI)
      sprintf (um, "/%c%c%c%c",
	       sel & P6CTR_UM_M ? 'm' : '-',
	       sel & P6CTR_UM_E ? 'e' : '-',
	       sel & P6CTR_UM_S ? 's' : '-',
	       sel & P6CTR_UM_I ? 'i' : '-');
    else if (cfnp && cfnp->flags & CFL_SA)
      sprintf (um, "/%c", sel & P6CTR_UM_A ? 'a' : '-');
    if (sel >> 24)
      sprintf (cm, "+%d", sel >> 24);
    sprintf (buf, "%c%c%c%c %02x%s%s%*s %s",
	     sel & P6CTR_I ? 'i' : '-',
	     sel & P6CTR_E ? 'e' : '-',
	     sel & P6CTR_K ? 'k' : '-',
	     sel & P6CTR_U ? 'u' : '-',
	     fn, cm, um, 7 - (strlen (cm) + strlen (um)), "",
	     cfnp ? cfnp->name : "unknown function");
  }
  else
    return (NULL);
  return (buf);
}

/* Print status of counters */
static void
readst (void)
{
  int fd, i;
  struct pctrst st;

  fd = open (_PATH_PCTR, O_RDONLY);
  if (fd < 0) {
    perror (_PATH_PCTR);
    exit (1);
  }
  if (ioctl (fd, PCIOCRD, &st) < 0) {
    perror ("PCIOCRD");
    exit (1);
  }
  close (fd);

  for (i = 0; i < PCTR_NUM; i++)
    printf (" ctr%d = %16qd  [%s]\n", i, st.pctr_hwc[i],
	    fn2str (cpufamily, st.pctr_fn[i]));
  printf ("  tsc = %16qd\n  idl = %16qd\n", st.pctr_tsc, st.pctr_idl);
}

static void
setctr (int ctr, u_int val)
{
  int fd;

  fd = open (_PATH_PCTR, O_WRONLY);
  if (fd < 0) {
    perror (_PATH_PCTR);
    exit (1);
  }
  if (ioctl (fd, PCIOCS0 + ctr, &val) < 0) {
    perror ("PCIOCSn");
    exit (1);
  }
  close (fd);
}

static void
usage (void)
{
  fprintf (stderr,
	   "usage:\n"
	   "  %s\n"
	   "    Read the counters.\n"
	   "  %s -l [5|6]\n"
	   "    List all possible counter functions for P5/P6.\n",
	   progname, progname);
  if (cpufamily == 5)
    fprintf (stderr,
	     "  %s -s {0|1} [-[c][u][k]] function\n"
	     "    Configure counter.\n"
	     "      0/1 - counter to configure\n"
	     "        c - count cycles not events\n"
	     "        u - count events in user mode (ring 3)\n"
	     "        k - count events in kernel mode (rings 0-2)\n",
	     progname);
  else if (cpufamily == 6)
    fprintf (stderr,
	     "  %s -s {0|1} [-[i][e][k][u]] "
	     "function[+cm][/{[m][e][s][i]|[a]}]\n"
	     "    Configure counter.\n"
	     "       0/1 - counter number to configure\n"
	     "         i - invert cm\n"
	     "         e - edge detect\n"
	     "         k - count events in kernel mode (rings 0-2)\n"
	     "         u - count events in user mode (ring 3)\n"
	     "        cm - # events/cycle required to bump ctr\n"
	     "      mesi - Modified/Exclusive/Shared/Invalid in cache\n"
	     "       s/a - self generated/all events\n", progname);
  exit (1);
}


int
main (int argc, char **argv)
{
  int fd;
  u_int ctr;
  char *cp;
  u_int fn, fl = 0;
  pctrval id = __cpuid ();
  char **ap;
  int ac;
  struct ctrfn *cfnp;

  cpufamily = (id >> 8) & 0xf;

  if (progname = strrchr (argv[0], '/'))
    progname++;
  else
    progname = argv[0];

  if (argc <= 1)
    readst ();
  else if (argc == 2 && !strcmp (argv[1], "-l"))
    list (cpufamily);
  else if (argc == 3 && !strcmp (argv[1], "-l"))
    list (atoi (argv[2]));
  else if (!strcmp (argv[1], "-s") && argc >= 4) {
    ctr = atoi (argv[2]);
    if (ctr >= PCTR_NUM)
      usage ();
    ap = &argv[3];
    ac = argc - 3;

    if (cpufamily == 6)
      fl |= P6CTR_EN;
    if (**ap == '-') {
      cp = *ap;
      if (cpufamily == 6)
	while (*++cp)
	  switch (*cp) {
	  case 'i':
	    fl |= P6CTR_I;
	    break;
	  case 'e':
	    fl |= P6CTR_E;
	    break;
	  case 'k':
	    fl |= P6CTR_K;
	    break;
	  case 'u':
	    fl |= P6CTR_U;
	    break;
	  default:
	    usage ();
	  }
      else
	while (*++cp)
	  switch (*cp) {
	  case 'c':
	    fl |= P5CTR_C;
	    break;
	  case 'k':
	    fl |= P5CTR_K;
	    break;
	  case 'u':
	    fl |= P5CTR_U;
	    break;
	  default:
	    usage ();
	  }
      ap++;
      ac--;
    }
    else {
      if (cpufamily == 6)
	fl |= P6CTR_U|P6CTR_K;
      else
	fl |= P5CTR_U|P5CTR_K;
    }

    if (!ac)
      usage ();

    fn = strtoul (*ap, NULL, 16);
    if (cpufamily == 6 && (fn & ~0xff) || cpufamily != 6 && (fn & ~0x3f))
      usage ();
    fl |= fn;
    if (cpufamily == 6 && (cp = strchr (*ap, '+'))) {
      cp++;
      fn = strtol (cp, NULL, 0);
      if (fn & ~0xff)
	usage ();
      fl |= (fn << 24);
    }
    cfnp = fn2cfnp (6, fl);
    if (cpufamily == 6 && cfnp && (cp = strchr (*ap, '/'))) {
      if (cfnp->flags & CFL_MESI)
	while (*++cp)
	  switch (*cp) {
	  case 'm':
	    fl |= P6CTR_UM_M;
	    break;
	  case 'e':
	    fl |= P6CTR_UM_E;
	    break;
	  case 's':
	    fl |= P6CTR_UM_S;
	    break;
	  case 'i':
	    fl |= P6CTR_UM_I;
	    break;
	  default:
	    usage ();
	  }
      else if (cfnp->flags & CFL_SA)
	while (*++cp)
	  switch (*cp) {
	  case 'a':
	    fl |= P6CTR_UM_A;
	    break;
	  default:
	    usage ();
	  }
      else
	usage ();
    }
    else if (cfnp && (cfnp->flags & CFL_MESI))
      fl |= P6CTR_UM_MESI;
    ap++;
    ac--;

    if (ac)
      usage ();

    if (cpufamily == 6 && ! (fl & 0xff))
      fl = 0;
    setctr (ctr, fl);
  }
  else
    usage ();

  return 0;
}
