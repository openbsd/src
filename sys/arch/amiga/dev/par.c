/*	$OpenBSD: par.c,v 1.5 1998/08/04 23:18:20 millert Exp $	*/
/*	$NetBSD: par.c,v 1.16 1996/12/23 09:10:28 veego Exp $	*/

/*
 * Copyright (c) 1982, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)ppi.c	7.3 (Berkeley) 12/16/90
 */

/*
 * parallel port interface
 */

#include "par.h"
#if NPAR > 0

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/uio.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/file.h>
#include <sys/systm.h>
#include <sys/proc.h>

#include <amiga/amiga/device.h>
#include <amiga/amiga/cia.h>
#include <amiga/dev/parioctl.h>

#include <sys/conf.h>
#include <machine/conf.h>

struct	par_softc {
	int	sc_flags;
	struct	parparam sc_param;
#define sc_burst sc_param.burst
#define sc_timo  sc_param.timo
#define sc_delay sc_param.delay
} *par_softcp;

#define getparsp(x)	(x > 0 ? NULL : par_softcp)

/* sc_flags values */
#define	PARF_ALIVE	0x01	
#define	PARF_OPEN	0x02	
#define PARF_UIO	0x04
#define PARF_TIMO	0x08
#define PARF_DELAY	0x10
#define PARF_OREAD	0x40
#define PARF_OWRITE	0x80

#define UNIT(x)		minor(x)

#ifdef DEBUG
int	pardebug = 0;
#define PDB_FOLLOW	0x01
#define PDB_IO		0x02
#define PDB_INTERRUPT   0x04
#define PDB_NOCHECK	0x80
#endif

int parrw __P((dev_t, struct uio *));
int parhztoms __P((int));
int parmstohz __P((int));
int parsend __P((u_char *, int));
int parreceive __P((u_char *, int));
int parsendch __P((u_char));

void partimo __P((void *));
void parstart __P((void *));
void parintr __P((void *));

void parattach __P((struct device *, struct device *, void *));
int parmatch __P((struct device *, void *, void *));

struct cfattach par_ca = {
	sizeof(struct device), parmatch, parattach
};

struct cfdriver par_cd = {
	NULL, "par", DV_DULL, NULL, 0
};

/*ARGSUSED*/
int
parmatch(pdp, match, auxp)
	struct device *pdp;
	void *match, *auxp;
{
	struct cfdata *cfp = match;

	if (matchname((char *)auxp, "par") && cfp->cf_unit == 0)
		return(1);
	return(0);
}

void
parattach(pdp, dp, auxp)
	struct device *pdp, *dp;
	void *auxp;
{
	par_softcp = (struct par_softc *)dp;

#ifdef DEBUG
	if ((pardebug & PDB_NOCHECK) == 0)
#endif
		par_softcp->sc_flags = PARF_ALIVE;
	printf("\n");
}

int
paropen(dev, flags, mode, p)
	dev_t dev;
	int flags;
	int mode;
	struct proc *p;
{
	int unit = UNIT(dev);
	struct par_softc *sc = getparsp(unit);
  
	if (unit >= NPAR || (sc->sc_flags & PARF_ALIVE) == 0)
		return(ENXIO);
#ifdef DEBUG
	if (pardebug & PDB_FOLLOW) {
		printf("paropen(%x, %x): flags %x, ",
		    dev, flags, sc->sc_flags);
		printf ("port = $%x\n", ((ciab.pra ^ CIAB_PRA_SEL)
		    & (CIAB_PRA_SEL|CIAB_PRA_BUSY|CIAB_PRA_POUT)));
	}
#endif
	if (sc->sc_flags & PARF_OPEN)
		return(EBUSY);
	/* can either read or write, but not both */
	if ((flags & (FREAD|FWRITE)) == (FREAD|FWRITE))
		return EINVAL;
  
	sc->sc_flags |= PARF_OPEN;

	if (flags & FREAD)
		sc->sc_flags |= PARF_OREAD;
	else
		sc->sc_flags |= PARF_OWRITE;

	sc->sc_burst = PAR_BURST;
	sc->sc_timo = parmstohz(PAR_TIMO);
	sc->sc_delay = parmstohz(PAR_DELAY);
	/* enable interrupts for CIAA-FLG */
	ciaa.icr = CIA_ICR_IR_SC | CIA_ICR_FLG;
	return(0);
}

int
parclose(dev, flags, mode, p)
	dev_t dev;
	int flags;
	int mode;
	struct proc *p;
{
  int unit = UNIT(dev);
  struct par_softc *sc = getparsp(unit);

#ifdef DEBUG
  if (pardebug & PDB_FOLLOW)
    printf("parclose(%x, %x): flags %x\n",
	   dev, flags, sc->sc_flags);
#endif
  sc->sc_flags &= ~(PARF_OPEN|PARF_OREAD|PARF_OWRITE);
  /* don't allow interrupts for CIAA-FLG any longer */
  ciaa.icr = CIA_ICR_FLG;
  return(0);
}

void
parstart(arg)
	void *arg;
{
	struct par_softc *sc;
	int unit;

	unit = (int)arg;
	sc = getparsp(unit);
#ifdef DEBUG
	if (pardebug & PDB_FOLLOW)
		printf("parstart(%x)\n", unit);
#endif
	sc->sc_flags &= ~PARF_DELAY;
	wakeup(sc);
}

void
partimo(arg)
	void *arg;
{
	struct par_softc *sc;
	int unit;

	unit = (int) arg;
	sc = getparsp(unit);
#ifdef DEBUG
	if (pardebug & PDB_FOLLOW)
		printf("partimo(%x)\n", unit);
#endif
	sc->sc_flags &= ~(PARF_UIO|PARF_TIMO);
	wakeup(sc);
}

int
parread(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{

#ifdef DEBUG
	if (pardebug & PDB_FOLLOW)
		printf("parread(%x, %p)\n", dev, uio);
#endif
	return (parrw(dev, uio));
}


int
parwrite(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{

#ifdef DEBUG
	if (pardebug & PDB_FOLLOW)
		printf("parwrite(%x, %p)\n", dev, uio);
#endif
	return (parrw(dev, uio));
}


int
parrw(dev, uio)
     dev_t dev;
     register struct uio *uio;
{
  int unit = UNIT(dev);
  register struct par_softc *sc = getparsp(unit);
  register int s, len, cnt;
  register char *cp;
  int error = 0, gotdata = 0;
  int buflen;
  char *buf;

  len = 0;
  cnt = 0;
  if (!!(sc->sc_flags & PARF_OREAD) ^ (uio->uio_rw == UIO_READ))
    return EINVAL;

  if (uio->uio_resid == 0)
    return(0);

#ifdef DEBUG
  if (pardebug & (PDB_FOLLOW|PDB_IO))
    printf("parrw(%x, %p, %c): burst %d, timo %d, resid %x\n",
	   dev, uio, uio->uio_rw == UIO_READ ? 'R' : 'W',
	   sc->sc_burst, sc->sc_timo, uio->uio_resid);
#endif
  buflen = min(sc->sc_burst, uio->uio_resid);
  buf = (char *)malloc(buflen, M_DEVBUF, M_WAITOK);
  sc->sc_flags |= PARF_UIO;
  if (sc->sc_timo > 0) 
    {
      sc->sc_flags |= PARF_TIMO;
      timeout(partimo, (void *)unit, sc->sc_timo);
    }
  while (uio->uio_resid > 0) 
    {
      len = min(buflen, uio->uio_resid);
      cp = buf;
      if (uio->uio_rw == UIO_WRITE) 
	{
	  error = uiomove(cp, len, uio);
	  if (error)
	    break;
	}
again:
      s = splbio();
#if 0
      if ((sc->sc_flags & PARF_UIO) && hpibreq(&sc->sc_dq) == 0)
	sleep(sc, PRIBIO+1);
#endif
      /*
       * Check if we timed out during sleep or uiomove
       */
      (void) splsoftclock();
      if ((sc->sc_flags & PARF_UIO) == 0) 
	{
#ifdef DEBUG
	  if (pardebug & PDB_IO)
	    printf("parrw: uiomove/sleep timo, flags %x\n",
		   sc->sc_flags);
#endif
	  if (sc->sc_flags & PARF_TIMO) 
	    {
	      untimeout(partimo, (void *)unit);
	      sc->sc_flags &= ~PARF_TIMO;
	    }
	  splx(s);
	  break;
	}
      splx(s);
      /*
       * Perform the operation
       */
      if (uio->uio_rw == UIO_WRITE)
	cnt = parsend (cp, len);
      else
	cnt = parreceive (cp, len);
      
      if (cnt < 0)
	{
	  error = -cnt;
	  break;
	}
      
      s = splbio();
#if 0
      hpibfree(&sc->sc_dq);
#endif
#ifdef DEBUG
      if (pardebug & PDB_IO)
	printf("parrw: %s(%p, %d) -> %d\n",
	       uio->uio_rw == UIO_READ ? "recv" : "send", cp, len, cnt);
#endif
      splx(s);
      if (uio->uio_rw == UIO_READ) 
	{
	  if (cnt) 
	    {
	      error = uiomove(cp, cnt, uio);
	      if (error)
		break;
	      gotdata++;
	    }
	  /*
	   * Didn't get anything this time, but did in the past.
	   * Consider us done.
	   */
	  else if (gotdata)
	    break;
	}
      s = splsoftclock();
      /*
       * Operation timeout (or non-blocking), quit now.
       */
      if ((sc->sc_flags & PARF_UIO) == 0) 
	{
#ifdef DEBUG
	  if (pardebug & PDB_IO)
	    printf("parrw: timeout/done\n");
#endif
	  splx(s);
	  break;
	}
      /*
       * Implement inter-read delay
       */
      if (sc->sc_delay > 0) 
	{
	  sc->sc_flags |= PARF_DELAY;
	  timeout(parstart, (void *)unit, sc->sc_delay);
	  error = tsleep(sc, PCATCH | (PZERO - 1), "par-cdelay", 0);
	  if (error) 
	    {
	      splx(s);
	      break;
	    }
	}
      splx(s);
      /*
       * Must not call uiomove again til we've used all data
       * that we already grabbed.
       */
      if (uio->uio_rw == UIO_WRITE && cnt != len) 
	{
	  cp += cnt;
	  len -= cnt;
	  cnt = 0;
	  goto again;
	}
    }
  s = splsoftclock();
  if (sc->sc_flags & PARF_TIMO) 
    {
      untimeout(partimo, (void *)unit);
      sc->sc_flags &= ~PARF_TIMO;
    }
  if (sc->sc_flags & PARF_DELAY) 
    {
      untimeout(parstart, (void *)unit);
      sc->sc_flags &= ~PARF_DELAY;
    }
  splx(s);
  /*
   * Adjust for those chars that we uiomove'ed but never wrote
   */
  if (uio->uio_rw == UIO_WRITE && cnt != len) 
    {
      uio->uio_resid += (len - cnt);
#ifdef DEBUG
      if (pardebug & PDB_IO)
	printf("parrw: short write, adjust by %d\n",
	       len-cnt);
#endif
    }
  free(buf, M_DEVBUF);
#ifdef DEBUG
  if (pardebug & (PDB_FOLLOW|PDB_IO))
    printf("parrw: return %d, resid %u\n", error, uio->uio_resid);
#endif
  return (error);
}

int
parioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
  struct par_softc *sc = getparsp(UNIT(dev));
  struct parparam *pp, *upp;
  int error = 0;

  switch (cmd) 
    {
    case PARIOCGPARAM:
      pp = &sc->sc_param;
      upp = (struct parparam *)data;
      upp->burst = pp->burst;
      upp->timo = parhztoms(pp->timo);
      upp->delay = parhztoms(pp->delay);
      break;

    case PARIOCSPARAM:
      pp = &sc->sc_param;
      upp = (struct parparam *)data;
      if (upp->burst < PAR_BURST_MIN || upp->burst > PAR_BURST_MAX ||
	  upp->delay < PAR_DELAY_MIN || upp->delay > PAR_DELAY_MAX)
	return(EINVAL);
      pp->burst = upp->burst;
      pp->timo = parmstohz(upp->timo);
      pp->delay = parmstohz(upp->delay);
      break;

    default:
      return(EINVAL);
    }
  return (error);
}

int
parhztoms(h)
     int h;
{
  extern int hz;
  register int m = h;

  if (m > 0)
    m = m * 1000 / hz;
  return(m);
}

int
parmstohz(m)
     int m;
{
  extern int hz;
  register int h = m;

  if (h > 0) {
    h = h * hz / 1000;
    if (h == 0)
      h = 1000 / hz;
  }
  return(h);
}

/* stuff below here if for interrupt driven output of data thru
   the parallel port. */

int partimeout_pending;
int parsend_pending;

void
parintr(arg)
	void *arg;
{
	int s, mask;

	mask = (int)arg;
	s = splclock();

#ifdef DEBUG
	if (pardebug & PDB_INTERRUPT)
		printf("parintr %s\n", mask ? "FLG" : "tout");
#endif
	/*
	 * if invoked from timeout handler, mask will be 0,
	 * if from interrupt, it will contain the cia-icr mask,
	 * which is != 0
	 */
	if (mask) {
		if (partimeout_pending)
			untimeout(parintr, 0);
		if (parsend_pending)
			parsend_pending = 0;
	}

	/* either way, there won't be a timeout pending any longer */
	partimeout_pending = 0;
  
	wakeup(parintr);
	splx(s);
}

int
parsendch (ch)
     u_char ch;
{
  int error = 0;
  int s;

  /* if either offline, busy or out of paper, wait for that
     condition to clear */
  s = splclock();
  while (!error 
	 && (parsend_pending 
	     || ((ciab.pra ^ CIAB_PRA_SEL)
		 & (CIAB_PRA_SEL|CIAB_PRA_BUSY|CIAB_PRA_POUT))))
    {
      extern int hz;

#ifdef DEBUG
      if (pardebug & PDB_INTERRUPT)
	printf ("parsendch, port = $%x\n",
		((ciab.pra ^ CIAB_PRA_SEL)
		 & (CIAB_PRA_SEL|CIAB_PRA_BUSY|CIAB_PRA_POUT)));
#endif
      /* wait a second, and try again */
      timeout(parintr, 0, hz);
      partimeout_pending = 1;
      /* this is essentially a flipflop to have us wait for the
	 first character being transmitted when trying to transmit
	 the second, etc. */
      parsend_pending = 0;
      /* it's quite important that a parallel putc can be
	 interrupted, given the possibility to lock a printer
	 in an offline condition.. */
      if ((error = tsleep(parintr, PCATCH | (PZERO - 1), "parsendch", 0)) > 0)
	{
#ifdef DEBUG
	  if (pardebug & PDB_INTERRUPT)
	    printf ("parsendch interrupted, error = %d\n", error);
#endif
	  if (partimeout_pending)
	    untimeout(parintr, 0);

	  partimeout_pending = 0;
	}
    }

  if (! error)
    {
#ifdef DEBUG
      if (pardebug & PDB_INTERRUPT)
	printf ("#%d", ch);
#endif
      ciaa.prb = ch;
      parsend_pending = 1;
    }

  splx (s);

  return error;
}


int
parsend (buf, len)
     u_char *buf;
     int len;
{
  int err, orig_len = len;

  /* make sure I/O lines are setup right for output */

  /* control lines set to input */
  ciab.ddra &= ~(CIAB_PRA_SEL|CIAB_PRA_POUT|CIAB_PRA_BUSY);
  /* data lines to output */
  ciaa.ddrb = 0xff;
  
  for (; len; len--, buf++)
    if ((err = parsendch (*buf)) != 0)
      return err < 0 ? -EINTR : -err;

  /* either all or nothing.. */
  return orig_len;
}



int
parreceive (buf, len)
     u_char *buf;
     int len;
{
  /* oh deary me, something's gotta be left to be implemented
     later... */
  return 0;
}


#endif
