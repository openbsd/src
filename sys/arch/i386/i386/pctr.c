/*	$OpenBSD: pctr.c,v 1.1 1996/08/08 18:47:07 dm Exp $	*/

/*
 * Pentium performance counter driver for OpenBSD.
 * Author: David Mazieres <dm@lcs.mit.edu>
 */

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/ioccom.h>

#include <machine/cputypes.h>
#include <machine/psl.h>
#include <machine/pctr.h>
#include <machine/cpu.h>

pctrval pctr_idlcnt;  /* Gets incremented in locore.s */

#define rdtsc()						\
({							\
  pctrval v;						\
  __asm __volatile (".byte 0xf, 0x31" : "=A" (v));	\
  v;							\
})

#define rdmsr(msr)						\
({								\
  pctrval v;							\
  __asm __volatile (".byte 0xf, 0x32" : "=A" (v) : "c" (msr));	\
  v;								\
})

#define wrmsr(msr, v) \
     __asm __volatile (".byte 0xf, 0x30" :: "A" (v), "c" (msr));

void
pctrattach (int num)
{
  if (num > 1)
    panic ("no more than one pctr device");
}

int
pctropen (dev_t dev, int oflags, int devtype, struct proc *p)
{
  if (minor (dev) || cpu_class < CPUCLASS_586)
    return ENXIO;
  return 0;
}

int
pctrclose (dev_t dev, int oflags, int devtype, struct proc *p)
{
  return 0;
}

static int
pctrset (int fflag, int cmd, u_short fn)
{
  pctrval msr11;
  int msr;
  int shift;

  switch (cmd) {
  case PCIOCS0:
    msr = 0x12;
    shift = 0;
    break;
  case PCIOCS1:
    msr = 0x13;
    shift = 16;
    break;
  default:
    return EINVAL;
  }

  if (! (fflag & FWRITE))
    return EPERM;
  if (fn >= 0x200)
    return EINVAL;
  msr11 = rdmsr (0x11);
  msr11 &= ~(0x1ffLL << shift);
  msr11 |= fn << shift;
  wrmsr (0x11, msr11);
  wrmsr (msr, 0LL);

  return 0;
}

int
pctrioctl (dev_t dev, int cmd, caddr_t data, int fflag, struct proc *p)
{

  if (minor (dev) || cpu_class < CPUCLASS_586)
    panic ("pctr: bad device %d should never have been opened.\n", dev);

  switch (cmd) {
  case PCIOCRD:
    {
      u_int msr11;
      struct pctrst *st;

      st = (void *) data;
      msr11 = rdmsr (0x11);
      st->pctr_fn[0] = msr11 & 0xffff;
      st->pctr_fn[1] = msr11 >> 16;
      __asm __volatile ("cli");
      st->pctr_tsc = rdtsc ();
      st->pctr_hwc[0] = rdmsr (0x12);
      st->pctr_hwc[1] = rdmsr (0x13);
      __asm __volatile ("sti");
      st->pctr_idl = pctr_idlcnt;
      return 0;
      break;
    }
  case PCIOCS0:
  case PCIOCS1:
    return pctrset (fflag, cmd, *(u_short *) data);
  default:
    return EINVAL;
  }
}
