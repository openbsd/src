/*	$OpenBSD: pctrctl.c,v 1.1 1996/08/08 18:47:03 dm Exp $	*/
/*
 * Pentium performance counter driver for OpenBSD.
 * Author: David Mazieres <dm@lcs.mit.edu>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <machine/pctr.h>

char *progname;

char *pctr_name[] = {
  "Data read", /* 0 */
  "Data write",
  "Data TLB miss",
  "Data read miss",
  "Data write miss",
  "Write (hit) to M or E state lines",
  "Data cache lines written back",
  "Data cache snoops",
  "Data cache snoop hits", /* 8 */
  "Memory accesses in both pipes",
  "Bank conflicts",
  "Misaligned data memory references",
  "Code read",
  "Code TLB miss",
  "Code cache miss",
  "Any segment register load",
  NULL, /* 0x10 */
  NULL,
  "Branches",
  "BTB hits",
  "Taken branch or BTB hit",
  "Pipeline flushes",
  "Instructions executed",
  "Instructions executed in the V-pipe",
  "Bus utilization (clocks)", /* 0x18 */
  "Pipeline stalled by write backup",
  "Pipeline stalled by data memory read",
  "Pipeline stalled by write to E or M line",
  "Locked bus cycle",
  "I/O read or write cycle",
  "Noncacheable memory references",
  "AGI (Address Generation Interlock)",
  NULL, /* 0x20 */
  NULL,
  "Floating-point operations",
  "Breakpoint 0 match",
  "Breakpoint 1 match",
  "Breakpoint 2 match",
  "Breakpoint 3 match",
  "Hardware interupts",
  "Data read or data write", /* 0x28 */
  "Data read miss or data write miss",
};

static const int pctr_name_size =  (sizeof (pctr_name) / sizeof (char *));

/* Print all possible counter functions */
static void
list (void)
{
  int i;

  printf ("Hardware counter event types:\n");
  for (i = 0; i < pctr_name_size; i++)
    printf ("  %02x  %s\n", i, pctr_name[i] ? pctr_name[i] : "invalid");
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
    printf (" ctr%d = %16qd  [%c%c%c %02x (%s)]\n", i, st.pctr_hwc[i],
	    (st.pctr_fn[i] & PCTR_C) ? 'c' : 'e',
	    (st.pctr_fn[i] & PCTR_U) ? 'u' : '-',
	    (st.pctr_fn[i] & PCTR_K) ? 'k' : '-',
	    (st.pctr_fn[i] & 0x3f),
	    (((st.pctr_fn[i] & 0x3f) < pctr_name_size
	      && pctr_name[st.pctr_fn[i] & 0x3f])
	     ? pctr_name[st.pctr_fn[i] & 0x3f] : "invalid"));
  printf ("  tsc = %16qd\n  idl = %16qd\n", st.pctr_tsc, st.pctr_idl);
}

static void
setctr (int ctr, u_short val)
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
	   "usage: %s [-l | -s ctr [selstr] evtype]\n"
	   "     -l  list event types\n"
	   "     -s  set counter <ctr> to monitor events of type <evtype>\n"
	   "           <selstr> = [e|c][u][k]  (default euk)\n"
	   "              e - count number of events\n"
	   "              c - count number of cycles\n"
	   "              u - count events in user mode (ring 3)\n"
	   "              k - count events in kernel mode (rings 0-2)\n",
	   progname);
  exit (1);
}


int
main (int argc, char **argv)
{
  int fd;
  u_int ctr;
  char *cp;
  u_short fn;

  if (progname = strrchr (argv[0], '/'))
    progname++;
  else
    progname = argv[0];

  if (argc <= 1)
    readst ();
  else if (argc == 2 && !strcmp (argv[1], "-l"))
    list ();
  else if (!strcmp (argv[1], "-s") && argc >= 4) {
    if (argc > 5)
      usage ();
    ctr = atoi (argv[2]);
    if (ctr >= PCTR_NUM)
      usage ();
    if (argc == 5) {
      fn = strtoul (argv[4], NULL, 16);
      if (fn & ~0x3f)
	usage ();
      for (cp = argv[3]; *cp; cp++) {
	switch (*cp) {
	case 'c':
	  fn |= PCTR_C;
	  break;
	case 'e':
	  fn &= ~PCTR_C;
	  break;
	case 'k':
	  fn |= PCTR_K;
	  break;
	case 'u':
	  fn |= PCTR_U;
	  break;
	default:
	  usage ();
	}
      }
    }
    else {
      fn = strtoul (argv[3], NULL, 16);
      if (fn & ~0x3f)
	usage ();
      fn |= PCTR_K | PCTR_U;
    }
    setctr (ctr, fn);
  }
  else
    usage ();

  return 0;
}
