/*	$OpenBSD: vx.c,v 1.2 2000/03/26 23:32:00 deraadt Exp $ */
/*
 * Copyright (c) 1999 Steve Murphree, Jr. 
 * All rights reserved.
 *
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
 *	This product includes software developed by Dale Rahn.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */  

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/tty.h>
#include <sys/uio.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/device.h>
#include <machine/cpu.h>
#include <machine/autoconf.h>
#include <dev/cons.h>
#include <mvme88k/dev/vxreg.h>
#include <sys/syslog.h>
#include "pcctwo.h"
#if NPCCTWO > 0
   #include <mvme88k/dev/pcctworeg.h>
   #include <mvme88k/dev/vme.h>
#endif

#include <machine/psl.h>
#define splvx()	spltty()

#ifdef DEBUG
   #undef DEBUG
#endif
#define DEBUG_KERN 1

struct vx_info {
   struct   tty *tty;
   u_char   vx_swflags;
   int      vx_linestatus;
   int      open;
   int      waiting;
   u_char   vx_consio;
   u_char   vx_speed;
   u_char   read_pending;
   struct   wring  *wringp;
   struct   rring  *rringp;
};

struct vxsoftc {
   struct device     sc_dev;
   struct evcnt      sc_intrcnt;
   struct evcnt      sc_sintrcnt;
   struct vx_info  sc_info[9];
   struct vxreg    *vx_reg;
   unsigned int      board_addr;
   struct channel    *channel;
   char              channel_number;
   struct packet     sc_bppwait_pkt;
   void              *sc_bppwait_pktp;
   struct intrhand   sc_ih_c;
   struct intrhand   sc_ih_s;
   struct vme2reg    *sc_vme2;
   int               sc_ipl;
   int               sc_vec;
   int               sc_flags;
   struct envelope   *elist_head, *elist_tail;
   struct packet     *plist_head, *plist_tail;
};

extern int cold;  /* var in autoconf.c that is set in machdep.c when booting */

/* prototypes */

void *get_next_envelope __P((struct envelope *thisenv));
struct envelope *get_status_head __P((struct vxsoftc *sc));
void set_status_head __P((struct vxsoftc *sc, void *envp));
struct packet *get_packet __P((struct vxsoftc *sc, struct envelope *thisenv));
struct envelope *find_status_packet __P((struct vxsoftc *sc, struct packet * pktp));

void read_wakeup __P((struct vxsoftc *sc, int port));
int  bpp_send __P((struct vxsoftc *sc, void *pkt, int wait_flag));

int  create_channels __P((struct vxsoftc *sc));
void *memcpy2 __P((void *dest, const void *src, size_t size));
void *get_free_envelope __P((struct vxsoftc *sc));
void put_free_envelope __P((struct vxsoftc *sc, void *envp));
void *get_free_packet __P((struct vxsoftc *sc));
void put_free_packet __P((struct vxsoftc *sc, void *pktp));

int  vx_init __P((struct vxsoftc *sc));
int  vx_event __P((struct vxsoftc *sc, struct packet *evntp));

void vx_unblock __P((struct tty *tp));
int  vx_ccparam __P((struct vxsoftc *sc, struct termios *par, int port));

int  vx_param __P((struct tty *tp, struct termios *t));
int  vx_intr __P((struct vxsoftc *sc));
int  vx_sintr __P((struct vxsoftc *sc));
int  vx_poll __P((struct vxsoftc *sc, struct packet *wpktp));
void vx_overflow __P((struct vxsoftc *sc, int port, long *ptime, u_char *msg));
void vx_frame __P((struct vxsoftc *sc, int port));
void vx_break __P(( struct vxsoftc *sc, int port));
int  vx_mctl __P((dev_t dev, int bits, int how));

int  vxmatch __P((struct device *parent, void *self, void *aux));
void vxattach __P((struct device *parent, struct device *self, void *aux));

int  vxopen  __P((dev_t dev, int flag, int mode, struct proc *p));
int  vxclose __P((dev_t dev, int flag, int mode, struct proc *p));
int  vxread  __P((dev_t dev, struct uio *uio, int flag));
int  vxwrite __P((dev_t dev, struct uio *uio, int flag));
int  vxioctl __P((dev_t dev, int cmd, caddr_t data, int flag, struct proc *p));
void vxstart __P((struct tty *tp));
int  vxstop  __P((struct tty *tp, int flag));

static void   vxputc __P((struct vxsoftc *sc, int port, u_char c));
static u_char vxgetc __P((struct vxsoftc *sc, int *port));

struct cfattach vx_ca = {       
   sizeof(struct vxsoftc), vxmatch, vxattach
};      

struct cfdriver vx_cd = {
   NULL, "vx", DV_TTY, 0
}; 
      
#define VX_UNIT(x) (int)(minor(x) / 9)
#define VX_PORT(x) (int)(minor(x) % 9)

extern int cputyp;
struct envelope *bpp_wait;
unsigned int board_addr;

struct tty * vxtty(dev)
dev_t dev;
{
   int unit, port;
   struct vxsoftc *sc;
   unit = VX_UNIT(dev);
   if (unit >= vx_cd.cd_ndevs || 
       (sc = (struct vxsoftc *) vx_cd.cd_devs[unit]) == NULL) {
      return (NULL);
   }
   port = VX_PORT(dev);
   return sc->sc_info[port].tty;
}

int   
vxmatch(parent, self, aux)
struct device *parent;
void *self;
void *aux;
{
   struct vxreg *vx_reg;
   struct vxsoftc *sc = self;
   struct confargs *ca = aux;
   int ret;
   if (cputyp != CPU_187)
      return 0;
#ifdef OLD_MAPPINGS
   ca->ca_vaddr = ca->ca_paddr;
#endif
   ca->ca_len = 0x10000; /* we know this. */
	ca->ca_ipl = 3; /* we need interrupts for this board to work */
   
   vx_reg = (struct vxreg *)ca->ca_vaddr;
   board_addr = (unsigned int)ca->ca_vaddr;
   if (!badvaddr(&vx_reg->ipc_cr, 1)){
      if (ca->ca_vec & 0x03) {
         printf("xvt: bad vector 0x%x\n", ca->ca_vec);
         return (0);
      }
      return (1);
   } else {
      return (0);
   }      
}

void
vxattach(parent, self, aux)
struct device *parent;
struct device *self;
void *aux;
{
   struct vxsoftc *sc = (struct vxsoftc *)self;
   struct confargs *ca = aux;
   int i;

   /* set up dual port memory and registers and init*/
   sc->vx_reg = (struct vxreg *)ca->ca_vaddr;
   sc->channel = (struct channel *)(ca->ca_vaddr + 0x0100);
   sc->sc_vme2 = ca->ca_master;
   sc->sc_ipl = ca->ca_ipl; 
   sc->sc_vec = ca->ca_vec; 
   sc->board_addr = (unsigned int)ca->ca_vaddr;
   
   printf("\n");

   if (create_channels(sc)) {
      printf("%s: failed to create channel %d\n", sc->sc_dev.dv_xname, 
             sc->channel->channel_number);
      return;
   }
   if (vx_init(sc)){
      printf("%s: failed to initialize\n", sc->sc_dev.dv_xname);
      return;
   }
   
   /* enable interrupts */
   sc->sc_ih_c.ih_fn = vx_intr;
   sc->sc_ih_c.ih_arg = sc;
   sc->sc_ih_c.ih_ipl = ca->ca_ipl;
   sc->sc_ih_c.ih_wantframe = 0;
   
   vmeintr_establish(ca->ca_vec, &sc->sc_ih_c);
   evcnt_attach(&sc->sc_dev, "intr", &sc->sc_intrcnt);
}

int vxtdefaultrate = TTYDEF_SPEED;

dtr_ctl(sc, port, on)
   struct vxsoftc *sc;
   int port;
   int on;
{
   struct packet pkt;
   bzero(&pkt, sizeof(struct packet));
   pkt.command = CMD_IOCTL;
   pkt.ioctl_cmd_l = IOCTL_TCXONC;
   pkt.command_pipe_number = sc->channel_number;
   pkt.status_pipe_number = sc->channel_number;
   pkt.device_number = port;
   if (on) {
      pkt.ioctl_arg_l = 6;  /* assert DTR */
   } else {
      pkt.ioctl_arg_l = 7;  /* negate DTR */
   }
   bpp_send(sc, &pkt, NOWAIT);
   return (pkt.error_l);
}

rts_ctl(sc, port, on)
   struct vxsoftc *sc;
   int port;
   int on;
{
   struct packet pkt;
   bzero(&pkt, sizeof(struct packet));
   pkt.command = CMD_IOCTL;
   pkt.ioctl_cmd_l = IOCTL_TCXONC;
   pkt.command_pipe_number = sc->channel_number;
   pkt.status_pipe_number = sc->channel_number;
   pkt.device_number = port;
   if (on) {
      pkt.ioctl_arg_l = 4;  /* assert RTS */
   } else {
      pkt.ioctl_arg_l = 5;  /* negate RTS */
   }
   bpp_send(sc, &pkt, NOWAIT);
   return (pkt.error_l);
}

flush_ctl(sc, port, which)
   struct vxsoftc *sc;
   int port;
   int which;
{
   struct packet pkt;
   bzero(&pkt, sizeof(struct packet));
   pkt.command = CMD_IOCTL;
   pkt.ioctl_cmd_l = IOCTL_TCFLSH;
   pkt.command_pipe_number = sc->channel_number;
   pkt.status_pipe_number = sc->channel_number;
   pkt.device_number = port;
   pkt.ioctl_arg_l = which; /* 0=input, 1=output, 2=both */
   bpp_send(sc, &pkt, NOWAIT);
   return (pkt.error_l);
}

int vx_mctl (dev, bits, how)
dev_t dev;
int bits;
int how;
{
   int s, unit, port;
   int vxbits;
   struct vxsoftc *sc;
   struct vx_info *vxt;
   u_char msvr;
   
   unit = VX_UNIT(dev);
   port = VX_PORT(dev);
   sc = (struct vxsoftc *) vx_cd.cd_devs[unit];
   vxt = &sc->sc_info[port];
	
   s = splvx();
   switch (how) {
   case DMSET:
		if( bits & TIOCM_RTS) {
			rts_ctl(sc, port, 1);
         vxt->vx_linestatus |= TIOCM_RTS;
		} else {
			rts_ctl(sc, port, 0);
         
         vxt->vx_linestatus &= ~TIOCM_RTS;
		}
		if( bits & TIOCM_DTR) {
			dtr_ctl(sc, port, 1);
         vxt->vx_linestatus |= TIOCM_DTR;
		} else {
			dtr_ctl(sc, port, 0);
         vxt->vx_linestatus &= ~TIOCM_DTR;
		}
		break;
   case DMBIC:
      if ( bits & TIOCM_RTS) {
			rts_ctl(sc, port, 0);
         vxt->vx_linestatus &= ~TIOCM_RTS;
      }
      if ( bits & TIOCM_DTR) {
			dtr_ctl(sc, port, 0);
         vxt->vx_linestatus &= ~TIOCM_DTR;
      }
      break;

   case DMBIS:
      if ( bits & TIOCM_RTS) {
			rts_ctl(sc, port, 1);
         vxt->vx_linestatus |= TIOCM_RTS;
      }
      if ( bits & TIOCM_DTR) {
			dtr_ctl(sc, port, 1);
         vxt->vx_linestatus |= TIOCM_DTR;
      }
      break;

   case DMGET:
		bits = 0;
		msvr = vxt->vx_linestatus;
		if( msvr & TIOCM_DSR) {
			bits |= TIOCM_DSR;
		}
		if( msvr & TIOCM_CD) {
			bits |= TIOCM_CD;
		}
		if( msvr & TIOCM_CTS) {
			bits |= TIOCM_CTS;
		}
		if( msvr & TIOCM_DTR) {
			bits |= TIOCM_DTR;
		}
		if( msvr & TIOCM_RTS) {
			bits |= TIOCM_RTS;
		}
      break;
   }
   
   splx(s);
   bits = 0;
	bits |= TIOCM_DTR;
	bits |= TIOCM_RTS;
	bits |= TIOCM_CTS;
	bits |= TIOCM_CD;
	bits |= TIOCM_DSR;
   return (bits);
}

int vxopen (dev, flag, mode, p)
dev_t dev;
int flag;
int mode;
struct proc *p;
{
   int s, unit, port;
   struct vx_info *vxt;
   struct vxsoftc *sc;
   struct tty *tp;
   struct open_packet opkt;
   u_short code;

   unit = VX_UNIT(dev);
   port = VX_PORT(dev);
   
   if (unit >= vx_cd.cd_ndevs || 
       (sc = (struct vxsoftc *) vx_cd.cd_devs[unit]) == NULL) {
      return (ENODEV);
   }
   
   /*flush_ctl(sc, port, 2);*/
   
   bzero(&opkt, sizeof(struct packet));
   opkt.eye_catcher[0] = 0x33;
   opkt.eye_catcher[1] = 0x33;
   opkt.eye_catcher[2] = 0x33;
   opkt.eye_catcher[3] = 0x33;
   opkt.command_pipe_number = sc->channel_number;
   opkt.status_pipe_number = sc->channel_number;
   opkt.command = CMD_OPEN;
   opkt.device_number = port;
   
   bpp_send(sc, &opkt, WAIT_POLL);
   
   if (opkt.error_l) {
#ifdef DEBUG_VXT
      printf("unit %d, port %d, ", unit, port);
      printf("error = %d\n", opkt.error_l);
#endif 
      return (ENODEV);
   }
   
   code = opkt.event_code;
	s = splvx();
   
   vxt = &sc->sc_info[port];
   if (vxt->tty) {
      tp = vxt->tty;
   } else {
      tp = vxt->tty = ttymalloc();  
   }
   
   /* set line status */
   tp->t_state |= TS_CARR_ON;
   if (code & E_DCD) {
		tp->t_state |= TS_CARR_ON;
      vxt->vx_linestatus |= TIOCM_CD;
   }
   if (code & E_DSR) {
      vxt->vx_linestatus |= TIOCM_DSR;
   }
   if (code & E_CTS) {
      vxt->vx_linestatus |= TIOCM_CTS;
   }
   
   tp->t_oproc = vxstart;
   tp->t_param = vx_param;
   tp->t_dev = dev;
   
   if ((tp->t_state & TS_ISOPEN) == 0) {
      tp->t_state |= TS_WOPEN;
      ttychars(tp);
      if (tp->t_ispeed == 0) {
         /*
          * only when cleared do we reset to defaults.
          */
         tp->t_iflag = TTYDEF_IFLAG;
         tp->t_oflag = TTYDEF_OFLAG;
         tp->t_lflag = TTYDEF_LFLAG;
         tp->t_ispeed = tp->t_ospeed = vxtdefaultrate;
         tp->t_cflag = TTYDEF_CFLAG;
      }
      /*
       * do these all the time
       */
      if (vxt->vx_swflags & TIOCFLAG_CLOCAL)
         tp->t_cflag |= CLOCAL;
      if (vxt->vx_swflags & TIOCFLAG_CRTSCTS)
         tp->t_cflag |= CRTSCTS;
      if (vxt->vx_swflags & TIOCFLAG_MDMBUF)
         tp->t_cflag |= MDMBUF;
      vx_param(tp, &tp->t_termios);
      ttsetwater(tp);

      (void)vx_mctl(dev, TIOCM_DTR | TIOCM_RTS, DMSET);
      
      tp->t_state |= TS_CARR_ON;
   } else if (tp->t_state & TS_XCLUDE && p->p_ucred->cr_uid != 0) {
      splx(s);
      return (EBUSY);
   }

   /*
    * Reset the tty pointer, as there could have been a dialout
    * use of the tty with a dialin open waiting.
    */
   tp->t_dev = dev;
   sc->sc_info[port].open = 1;
   read_wakeup(sc, port);
   splx(s);
   return ((*linesw[tp->t_line].l_open)(dev, tp));
}

int 
vx_param(tp, t)
struct tty *tp;
struct termios *t;
{
   int unit, port;
   struct vxsoftc *sc;
   int s;
   dev_t dev;

   dev = tp->t_dev;
   unit = VX_UNIT(dev);
   if (unit >= vx_cd.cd_ndevs || 
       (sc = (struct vxsoftc *) vx_cd.cd_devs[unit]) == NULL) {
      return (ENODEV);
   }
   port = VX_PORT(dev);
   tp->t_ispeed = t->c_ispeed;
   tp->t_ospeed = t->c_ospeed;
   tp->t_cflag = t->c_cflag;
   vx_ccparam(sc, t, port);
   vx_unblock(tp);
   return 0;
}

int 
vxclose (dev, flag, mode, p)
dev_t dev;
int flag;
int mode;
struct proc *p;
{
   int unit, port;
   struct tty *tp;
   struct vx_info *vxt;
   struct vxsoftc *sc;
   int s;
   struct close_packet cpkt;
   unit = VX_UNIT(dev);
   if (unit >= vx_cd.cd_ndevs || 
       (sc = (struct vxsoftc *) vx_cd.cd_devs[unit]) == NULL) {
      return (ENODEV);
   }
   port = VX_PORT(dev);
/*   flush_ctl(sc, port, 2);   flush both input and output */
   
   vxt = &sc->sc_info[port];
   tp = vxt->tty;
   (*linesw[tp->t_line].l_close)(tp, flag);
   
   if((tp->t_cflag & HUPCL) != 0) {
		rts_ctl(sc, port, 0);
		dtr_ctl(sc, port, 0);
	}
	
   s = splvx();
   
   bzero(&cpkt, sizeof(struct packet));
   cpkt.eye_catcher[0] = 0x55;
   cpkt.eye_catcher[1] = 0x55;
   cpkt.eye_catcher[2] = 0x55;
   cpkt.eye_catcher[3] = 0x55;
   cpkt.command_pipe_number = sc->channel_number;
   cpkt.status_pipe_number = sc->channel_number;
   cpkt.command = CMD_CLOSE;
   cpkt.device_number = port;
   
   bpp_send(sc, &cpkt, NOWAIT);
   splx(s);
   ttyclose(tp);
   sc->sc_info[port].open = 0;
   return (0);
}

void 
read_wakeup(sc, port)
struct vxsoftc *sc;
int port;
{
   struct rring *rp;
   struct read_wakeup_packet rwp;
   volatile struct vx_info *vxt;
   vxt = &sc->sc_info[port];
   /* 
    * If we already have a read_wakeup paket 
    * for this port, do nothing.
    */
   if (vxt->read_pending) {
      return;
   } else {
      vxt->read_pending = 1;
   }

   bzero(&rwp, sizeof(struct packet));
   rwp.eye_catcher[0] = 0x11;
   rwp.eye_catcher[1] = 0x11;
   rwp.eye_catcher[2] = 0x11;
   rwp.eye_catcher[3] = 0x11;
   rwp.command_pipe_number = sc->channel_number;
   rwp.status_pipe_number = sc->channel_number;
   rwp.command = CMD_READW;
   rwp.device_number = port;
   
   /*
    * Do not wait.  Characters will be transfered
    * to (*linesw[tp->t_line].l_rint)(c,tp); by 
    * vx_intr()  (IPC will notify via interrupt)
    */
   bpp_send(sc, &rwp, NOWAIT);
}

int 
vxread (dev, uio, flag)
dev_t dev;
struct uio *uio;
int flag;
{
   int unit, port;
   struct tty *tp;
   volatile struct vx_info *vxt;
   volatile struct vxsoftc *sc;

   unit = VX_UNIT(dev);
   if (unit >= vx_cd.cd_ndevs || 
       (sc = (struct vxsoftc *) vx_cd.cd_devs[unit]) == NULL) {
      return (ENODEV);
   }
   port = VX_PORT(dev);
   vxt = &sc->sc_info[port];
   tp = vxt->tty;
   if (!tp) return ENXIO;
   return ((*linesw[tp->t_line].l_read)(tp, uio, flag));
}

int 
vxwrite (dev, uio, flag)
dev_t dev;
struct uio *uio;
int flag;
{
   int unit, port;
   struct tty *tp;
   struct vx_info *vxt;
   struct vxsoftc *sc;
   struct wring *wp;
   struct write_wakeup_packet wwp;
   u_short get, put;
   int i, cnt, s;
   
   unit = VX_UNIT(dev);
   if (unit >= vx_cd.cd_ndevs || 
       (sc = (struct vxsoftc *) vx_cd.cd_devs[unit]) == NULL) {
      return (ENODEV);
   }
   
   port = VX_PORT(dev);
   vxt = &sc->sc_info[port];
   tp = vxt->tty;
   if (!tp) return ENXIO;
   
   wp = sc->sc_info[port].wringp;
   get = wp->get;
   put = wp->put;
   
   if ((put + 1) == get) {
      bzero(&wwp, sizeof(struct packet));
      wwp.eye_catcher[0] = 0x22;
      wwp.eye_catcher[1] = 0x22;
      wwp.eye_catcher[2] = 0x22;
      wwp.eye_catcher[3] = 0x22;
      wwp.command_pipe_number = sc->channel_number;
      wwp.status_pipe_number = sc->channel_number;
      wwp.command = CMD_WRITEW;
      wwp.device_number = port;

      bpp_send(sc, &wwp, WAIT_POLL);
      
      if (wwp.error_l) {
         return (ENXIO);
      }
   }
   return ((*linesw[tp->t_line].l_write)(tp, uio, flag));
}  

int
vxioctl (dev, cmd, data, flag, p)
dev_t dev;
int cmd;
caddr_t data;
int flag;
struct proc *p;
{
   int error;
   int unit, port;
   struct tty *tp;
   struct vx_info *vxt;
   struct vxsoftc *sc;
   unit = VX_UNIT(dev);
   if (unit >= vx_cd.cd_ndevs || 
       (sc = (struct vxsoftc *) vx_cd.cd_devs[unit]) == NULL) {
      return (ENODEV);
   }
   port = VX_PORT(dev);
   vxt = &sc->sc_info[port];
   tp = vxt->tty;
   if (!tp)
      return ENXIO;

   error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
   if (error >= 0)
      return (error);

   error = ttioctl(tp, cmd, data, flag, p);
   if (error >= 0)
      return (error);

   switch (cmd) {
   case TIOCSBRK:
      /* */
      break;

   case TIOCCBRK:
      /* */
      break;

   case TIOCSDTR:
		(void) vx_mctl(dev, TIOCM_DTR | TIOCM_RTS, DMBIS);
      break;

   case TIOCCDTR:
		(void) vx_mctl(dev, TIOCM_DTR | TIOCM_RTS, DMBIC);
      break;

   case TIOCMSET:
		(void) vx_mctl(dev, *(int *) data, DMSET);
      break;

   case TIOCMBIS:
		(void) vx_mctl(dev, *(int *) data, DMBIS);
      break;

   case TIOCMBIC:
		(void) vx_mctl(dev, *(int *) data, DMBIC);
      break;

   case TIOCMGET:
		*(int *)data = vx_mctl(dev, 0, DMGET);
      break;

   case TIOCGFLAGS:
		*(int *)data = vxt->vx_swflags;
		break;

   case TIOCSFLAGS:
		error = suser(p->p_ucred, &p->p_acflag); 
		if (error != 0)
			return(EPERM); 

		vxt->vx_swflags = *(int *)data;
		vxt->vx_swflags &= /* only allow valid flags */
			(TIOCFLAG_SOFTCAR | TIOCFLAG_CLOCAL | TIOCFLAG_CRTSCTS);
      break;

   default:
      return (ENOTTY);
   }
   return 0;
}

int
vxstop(tp, flag)
struct tty *tp;
int flag;
{
   int s;

	s = splvx();
   if (tp->t_state & TS_BUSY) {
      if ((tp->t_state & TS_TTSTOP) == 0)
         tp->t_state |= TS_FLUSH;
   }
	splx(s);
   return 0;
}

static u_char 
vxgetc(sc, port)
struct vxsoftc *sc;
int *port;
{
   return 0;
}

static void
vxputc(sc, port, c)
struct vxsoftc *sc;
int port;
u_char c;
{
   struct wring *wp;
   
   wp = sc->sc_info[port].wringp;
   wp->data[wp->put++ & (WRING_BUF_SIZE-1)] = c;
   wp->put &= (WRING_BUF_SIZE-1);
   return;
}

u_short vxtspeed(speed)
int speed;
{
   switch (speed) {
   case B0:
      return VB0;
      break;
   case B50:
      return VB50;
      break;
   case B75:
      return VB75;
      break;
   case B110:
      return VB110;
      break;
   case B134:
      return VB134;
      break;
   case B150:
      return VB150;
      break;
   case B200:
      return VB200;
      break;
   case B300:
      return VB300;
      break;
   case B600:
      return VB600;
      break;
   case B1200:
      return VB1200;
      break;
   case B1800:
      return VB1800;
      break;
   case B2400:
      return VB2400;
      break;
   case B4800:
      return VB4800;
      break;
   case B9600:
      return VB9600;
      break;
   case B19200:
      return VB19200;
      break;
   case B38400:
      return VB38400;
      break;
   default:
      return VB9600;
      break;
   }
}

int
vx_ccparam(sc, par, port)
struct vxsoftc *sc;
struct termios *par;
int port;
{
   struct termio tio;
   int imask=0, ints, s;
   int cflag, iflag, oflag, lflag;
   struct ioctl_a_packet pkt;
   bzero(&pkt, sizeof(struct packet));
   
   if (par->c_ospeed == 0) { 
      s = splvx();
      /* dont kill the console */
      if(sc->sc_info[port].vx_consio == 0) {
         /* disconnect, drop RTS DTR stop reciever */
         rts_ctl(sc, port, 0);
         dtr_ctl(sc, port, 0);
      }
      splx(s);
      return (0xff);
   }
   
   pkt.command = CMD_IOCTL;
   pkt.ioctl_cmd_l = IOCTL_TCGETA;
   pkt.command_pipe_number = sc->channel_number;
   pkt.status_pipe_number = sc->channel_number;
   pkt.device_number = port;
   bpp_send(sc, &pkt, WAIT_POLL);
   
   cflag = pkt.c_cflag;
   cflag |= vxtspeed(par->c_ospeed);
   
   switch (par->c_cflag & CSIZE) {
	case CS5:
		cflag |= VCS5;
		imask = 0x1F;
		break;
	case CS6:
		cflag |= VCS6;
		imask = 0x3F;
		break;
	case CS7:
		cflag |= VCS7;
		imask = 0x7F;
		break;
	default:
		cflag |= VCS8;
		imask = 0xFF;
   }
   
   if (par->c_cflag & PARENB) cflag |= VPARENB; else cflag &= ~VPARENB;
   if (par->c_cflag & PARODD) cflag |= VPARODD; else cflag &= ~VPARODD;
   if (par->c_cflag & CREAD) cflag |= VCREAD; else cflag &= ~VCREAD;
   if (par->c_cflag & CLOCAL) cflag |= VCLOCAL; else cflag &= ~VCLOCAL;
   if (par->c_cflag & HUPCL) cflag |= VHUPCL; else cflag &= ~VHUPCL;
   /*
   if (par->c_iflag & BRKINT) iflag |= VBRKINT; else iflag &= ~VBRKINT;
   if (par->c_iflag & ISTRIP) iflag |= VISTRIP; else iflag &= ~VISTRIP;
   if (par->c_iflag & ICRNL) iflag |= VICRNL; else iflag &= ~VICRNL;
   if (par->c_iflag & IXON) iflag |= VIXON; else iflag &= ~VIXON;
   if (par->c_iflag & IXANY) iflag |= VIXANY; else iflag &= ~VIXANY;
   if (par->c_oflag & OPOST) oflag |= VOPOST; else oflag &= ~VOPOST;
   if (par->c_oflag & ONLCR) oflag |= VONLCR; else oflag &= ~VONLCR;
   if (par->c_oflag & OXTABS) oflag |= VOXTABS; else oflag &= ~VOXTABS;
   if (par->c_lflag & ECHO) lflag |= VECHO; else lflag &= ~VECHO;
   if (par->c_lflag & ECHOE) lflag |= VECHOE; else lflag &= ~VECHOE;
   if (par->c_lflag & ICANON) lflag |= VICANON; else lflag &= ~VICANON;
   if (par->c_lflag & ISIG) lflag |= VISIG; else lflag &= ~VISIG;
   */
   pkt.command = CMD_IOCTL;
   pkt.ioctl_cmd_l = IOCTL_TCSETA;
   pkt.command_pipe_number = sc->channel_number;
   pkt.status_pipe_number = sc->channel_number;
   pkt.device_number = port;
   pkt.c_cflag = cflag;
/*
   pkt.c_iflag = iflag;
   pkt.c_oflag = oflag;
   pkt.c_lflag = lflag;
 */

   bpp_send(sc, &pkt, WAIT_POLL);
   return imask;
}

void
vx_unblock(tp)
struct tty *tp;
{
   tp->t_state &= ~TS_FLUSH;
   if (tp->t_outq.c_cc != 0)
      vxstart(tp);
}

void
vxstart(tp)
struct tty *tp;
{
   dev_t dev;
   u_char cbuf;
   struct vxsoftc *sc;
   struct wring *wp;
   int cc, port, unit, s, cnt, i;
   u_short get, put;
   char buffer[WRING_BUF_SIZE];
   char *wrdp;

   dev = tp->t_dev;
   port = VX_PORT(dev);
   unit = VX_UNIT(dev);
   if (unit >= vx_cd.cd_ndevs || 
       (sc = (struct vxsoftc *) vx_cd.cd_devs[unit]) == NULL) {
      return;
   }
   
   if ((tp->t_state & TS_ISOPEN) == 0)
      return;
	
   s = splvx();
   if ((tp->t_state & (TS_TIMEOUT | TS_BUSY | TS_TTSTOP | TS_FLUSH)) == 0) {
      tp->t_state |= TS_BUSY;
      wp = sc->sc_info[port].wringp;
      get = wp->get;
      put = wp->put;
      cc = tp->t_outq.c_cc;
      while (cc > 0) {
         cnt = min(WRING_BUF_SIZE, cc);
         cnt = q_to_b(&tp->t_outq, buffer, cnt);
         buffer[cnt] = 0;
         for (i=0; i<cnt; i++) {
            vxputc(sc, port, buffer[i]);
         }
         cc -= cnt;
      }
      tp->t_state &= ~TS_BUSY;
   }
   splx(s);
   return;
}

void 
read_chars(sc, port)
struct vxsoftc *sc;
int port;
{
   /* 
    * This routine is called by vx_intr() when there are
    * characters in the read ring.  It will process one 
    * cooked line, put the chars in the line disipline ring,
    * and then return.  The characters may then 
    * be read by vxread.
    */
   struct vx_info *vxt;
   struct rring *rp;
   struct read_wakeup_packet rwp;
   struct tty *tp;
   u_short get, put;
   int frame_count, i, pc = 0, open;
   char c;
   
   vxt = &sc->sc_info[port];
   tp = vxt->tty;
   rp = vxt->rringp;
   open = vxt->open;
   get = rp->get;
   put = rp->put;
#ifdef DEBUG_VXT
   printf("read_chars() get=%d, put=%d ", get, put);
   printf("open = %d ring at 0x%x\n", open, rp);
#endif 
   while (get != put) {
      frame_count = rp->data[rp->get++ & (RRING_BUF_SIZE - 1)];
      rp->get &= (RRING_BUF_SIZE - 1);
      for (i=0; i<frame_count; i++) {
         c = rp->data[rp->get++ & (RRING_BUF_SIZE - 1)];
         rp->get &= (RRING_BUF_SIZE - 1);
         if (open) 
            (*linesw[tp->t_line].l_rint)(c,tp);
      }
      c = rp->data[rp->get++ & (RRING_BUF_SIZE - 1)];
      rp->get &= (RRING_BUF_SIZE - 1);
      if (!(c & DELIMITER)) {
         vx_frame (sc, port);
         break;
      } else {
         break;
      }
      get = rp->get;
      put = rp->put;
   }
   vxt->read_pending = 0;
   read_wakeup(sc, port);
   return;
}

ccode(sc, port, c)
struct vxsoftc *sc;
int port;
char c;
{
   struct vx_info *vxt;
   struct tty *tp;
   tp = vxt->tty;
   vxt = &sc->sc_info[port];
   tp = vxt->tty;
   (*linesw[tp->t_line].l_rint)(c,tp);
}

int
vx_intr(sc)
struct vxsoftc *sc;
{
   struct envelope *envp, *next_envp;
   struct envelope env;
   struct packet *pktp, pkt;
   int valid, i;
	short  cmd;
	u_char  port;
   struct vme2reg *vme2 = (struct vme2reg *)sc->sc_vme2;

   if (vme2->vme2_vbr & VME2_SYSFAIL){
      /* do something... print_dump(sc); */
   }
   if (!cold) sc->sc_intrcnt.ev_count++;
   
   while (env_isvalid(get_status_head(sc))) {
      pktp = get_packet(sc, get_status_head(sc));
      valid = env_isvalid(get_status_head(sc));
      cmd = pktp->command;
      port = pktp->device_number;
      /* if we are waiting on this packet, strore the info so bpp_send 
         can process the packet  */
      if (sc->sc_bppwait_pktp == pktp)
         memcpy2(&sc->sc_bppwait_pkt, pktp, sizeof(struct packet));
      
      memcpy2(&pkt, pktp, sizeof(struct packet));
      next_envp = get_next_envelope(get_status_head(sc));
      envp = get_status_head(sc);
      /* return envelope and packet to the free queues */
      put_free_envelope(sc, envp);
      put_free_packet(sc, pktp);
      /* mark new status pipe head pointer */
      set_status_head(sc, next_envp);
      /* if it was valid, process packet */
      switch (cmd) {
      case CMD_READW:
#ifdef DEBUG_VXT
         printf("READW Packet\n");  
#endif 
         read_chars(sc, port);          
         return 1;
         break;
      case CMD_WRITEW:
#ifdef DEBUG_VXT
         printf("WRITEW Packet\n");  /* Still don't know XXXsmurph */
#endif 
         return 1;
         break;
      case CMD_EVENT:
#ifdef DEBUG_VXT
         printf("EVENT Packet\n");  
#endif 
         vx_event(sc, &pkt);
         return 1;
         break;
      case CMD_PROCCESED:
#ifdef DEBUG_VXT
         printf("CMD_PROCCESED Packet\n");
#endif 
         return 1;
         break;
      default:
#ifdef DEBUG_VXT
         printf("Other packet 0x%x\n", cmd);  
#endif 
         return 1;
         break;
      }
   }
   return 1;
}

int 
vx_event(sc, evntp)
struct vxsoftc *sc;
struct packet *evntp;
{
   u_short code = evntp->event_code;
   struct event_packet evnt;
   struct vx_info *vxt;
   
   vxt = &sc->sc_info[evntp->device_number];
   
   if (code & E_INTR) {
      ccode(sc, evntp->device_number, CINTR);
   }
   if (code & E_QUIT) {
      ccode(sc, evntp->device_number, CQUIT);
   }
   if (code & E_HUP) {
      rts_ctl(sc, evntp->device_number, 0);
      dtr_ctl(sc, evntp->device_number, 0);
   }
   if (code & E_DCD) {
      vxt->vx_linestatus |= TIOCM_CD;
   }
   if (code & E_DSR) {
      vxt->vx_linestatus |= TIOCM_DSR;
   }
   if (code & E_CTS) {
      vxt->vx_linestatus |= TIOCM_CTS;
   }
   if (code & E_LOST_DCD) {
      vxt->vx_linestatus &= ~TIOCM_CD;
   }
   if (code & E_LOST_DSR) {
      vxt->vx_linestatus &= ~TIOCM_DSR;
   }
   if (code & E_LOST_CTS) {
      vxt->vx_linestatus &= ~TIOCM_CTS;
   }
   if (code & E_PR_FAULT) {
      /* do something... */
   }
   if (code & E_PR_POUT) {
      /* do something... */
   }
   if (code & E_PR_SELECT) {
      /* do something... */
   }
   if (code & E_SWITCH) {
      /* do something... */
   }
   if (code & E_BREAK) {
      vx_break (sc, evntp->device_number);
   }
   
   /* send and event packet backe to the device */
   bzero(&evnt, sizeof(struct event_packet));
   evnt.command = CMD_EVENT;
   evnt.device_number = evntp->device_number;
   evnt.command_pipe_number = sc->channel_number;
   /* return status on same channel */
   evnt.status_pipe_number = sc->channel_number;
   /* send packet to the firmware */
   bpp_send(sc, &evnt, NOWAIT);
   return 1;
}

void
vx_overflow (sc, port, ptime, msg)
struct vxsoftc *sc;
int port;
long *ptime;
u_char *msg;
{
   log(LOG_WARNING, "%s port %d: overrun\n", sc->sc_dev.dv_xname, port);
   return;
}

void
vx_frame (sc, port)
struct vxsoftc *sc;
int port;
{
   log(LOG_WARNING, "%s port %d: frame error\n", sc->sc_dev.dv_xname, port);
   return;
}

void
vx_break (sc, port)
struct vxsoftc *sc;
int port;
{
#ifdef DEBUG_KERN
   Debugger();
#else
   log(LOG_WARNING, "%s port %d: break detected\n", sc->sc_dev.dv_xname, port);
#endif
   return;
}

/*
 *	Initialization and Buffered Pipe Protocol (BPP) code
 */

/* special function for 16 bit data transfer */
/* Not needed now that I figured out VME bus */
/* mappings and address modifiers, but I don't */
/* want to change them :) */
void *
memcpy2(void *dest, const void *src, size_t size)
{
   int i;
   short *d, *s;
   d = (short*) dest;
   s = (short*) src;
   for (i=0; i<(size/2); i++) {
      *d = *s;
      d++;
      s++;
   }
}

void
wzero(void *addr, size_t size)
{
   int i;
   short *d;
   d = (short*) addr;
   for (i=0; i<(size/2); i++) {
      *d = 0;
      d++;
   }
}

int
create_free_queue(sc)
struct vxsoftc *sc;
{
   int i;
   struct envelope *envp;
   struct envelope env;
   struct packet   *pktp;
   struct packet   pkt;

   envp = (struct envelope *)ENVELOPE_AREA;
   sc->elist_head = envp;
   for (i=0; i < NENVELOPES; i++) {
      bzero(envp, sizeof(struct envelope));
      if (i==(NENVELOPES - 1)) {
         envp->link = NULL;
      } else {
         envp->link = (u_long)envp + sizeof(struct envelope);
      }
      envp->packet_ptr = NULL;
      envp->valid_flag = 0;
      envp++;
   }
   sc->elist_tail = --envp;

   pktp = (struct packet *)PACKET_AREA;
   sc->plist_head = pktp;
   for (i=0; i < NPACKETS; i++) {
      bzero(pktp, sizeof(struct packet));
      if (i==(NPACKETS - 1)) {
         pktp->link = NULL;
      } else {
         pktp->link = (u_long)pktp + sizeof(struct packet);
      }
      pktp++;
   }
   sc->plist_tail = --pktp;
   return 0; /* no error */
}

void *
get_free_envelope(sc) 
struct vxsoftc *sc;
{
   void *envp;

   envp = sc->elist_head;
   sc->elist_head = (struct envelope *)sc->elist_head->link;
   bzero(envp, sizeof(struct envelope));
   return envp;
}

void 
put_free_envelope(sc, ep)
struct vxsoftc *sc;
void * ep;
{
   struct envelope *envp = (struct envelope *)ep;
   bzero(envp, sizeof(struct envelope));
   sc->elist_tail->link = (ulong)envp;
   envp->link = NULL;
   sc->elist_tail = envp;
}

void* 
get_free_packet(sc)
struct vxsoftc *sc;
{
   struct packet *pktp;

   pktp = sc->plist_head;
   sc->plist_head = (struct packet *)sc->plist_head->link;
   bzero(pktp, sizeof(struct packet));
   return pktp;
}

void 
put_free_packet(sc, pp)
struct vxsoftc *sc;
void *pp;
{
   struct packet *pktp = (struct packet *)pp;
   /*bzero(pktp, sizeof(struct packet));*/
   pktp->command = CMD_PROCCESED;
   sc->plist_tail->link = (u_long)pktp;
   pktp->link = NULL;
   sc->plist_tail = pktp;
}

/* 
 * This is the nitty gritty.  All the rest if this code
 * was hell to come by.  Getting this right from the 
 * Moto manual took *time*!  
 */
int 
create_channels(sc)
struct vxsoftc *sc;
{
   struct envelope *envp;
   struct envelope env;
   struct packet *pktp;
   u_char valid;
   u_short status;
   u_short tas, csr;
   struct vxreg *ipc_csr;
   
   ipc_csr = sc->vx_reg;
   /* wait for busy bit to clear */
   while ((ipc_csr->ipc_cr & IPC_CR_BUSY));
   create_free_queue(sc);
   /* set up channel header.  we only want one */
   tas = ipc_csr->ipc_tas;
   while (!(tas & IPC_TAS_VALID_STATUS)) {
      envp = get_free_envelope(sc);
      sc->channel->command_pipe_head_ptr_h = HI(envp);
      sc->channel->command_pipe_head_ptr_l = LO(envp);
      sc->channel->command_pipe_tail_ptr_h = sc->channel->command_pipe_head_ptr_h;
      sc->channel->command_pipe_tail_ptr_l = sc->channel->command_pipe_head_ptr_l;
      envp = get_free_envelope(sc);
      sc->channel->status_pipe_head_ptr_h = HI(envp);
      sc->channel->status_pipe_head_ptr_l = LO(envp);
      sc->channel->status_pipe_tail_ptr_h = sc->channel->status_pipe_head_ptr_h;
      sc->channel->status_pipe_tail_ptr_l = sc->channel->status_pipe_head_ptr_l;
      sc->channel->interrupt_level =  sc->sc_ipl;
      sc->channel->interrupt_vec = sc->sc_vec;
      sc->channel->channel_priority = 0;
      sc->channel->channel_number = 0;
      sc->channel->valid = 1;
      sc->channel->address_modifier = 0x8D; /* A32/D16 supervisor data access */
      sc->channel->datasize = 0; /* 32 bit data mode */

      /* loop until TAS bit is zero */
      while ((ipc_csr->ipc_tas & IPC_TAS_TAS)); 
      ipc_csr->ipc_tas |= IPC_TAS_TAS;
      /* load address of channel header */
      ipc_csr->ipc_addrh = HI(sc->channel);
      ipc_csr->ipc_addrl = LO(sc->channel);
      /* load address modifier reg (supervisor data access) */
      ipc_csr->ipc_amr = 0x8D;
      /* load tas with create channel command */
      ipc_csr->ipc_tas |= IPC_CSR_CREATE;
      /* set vaild command bit */
      ipc_csr->ipc_tas |= IPC_TAS_VALID_CMD;
      /* notify IPC of the CSR command */
      ipc_csr->ipc_cr |= IPC_CR_ATTEN;
      /* loop until IPC sets vaild status bit */
      delay(5000);
      tas = ipc_csr->ipc_tas;
   }

   /* save the status */
   status = ipc_csr->ipc_sr;
   /* set COMMAND COMPLETE bit */
   ipc_csr->ipc_tas |= IPC_TAS_COMPLETE;
   /* notify IPC that we are through */
   ipc_csr->ipc_cr |= IPC_CR_ATTEN;
   /* check and see if the channel was created */
   if (!status && sc->channel->valid) {
      sc->channel_number = sc->channel->channel_number;
      printf("%s: created channel %d\n", sc->sc_dev.dv_xname, 
             sc->channel->channel_number);
      return 0;
   } else {
      switch (status) {
      case 0x0000:
         printf("%s: channel not valid\n", 
                sc->sc_dev.dv_xname);
         break;
      case 0xFFFF:
         printf("%s: invalid CSR command\n", 
                sc->sc_dev.dv_xname);
         break;
      case 0xC000:
         printf("%s: could not read channel structure\n", 
                sc->sc_dev.dv_xname);
         break;
      case 0x8000:
         printf("%s: could not write channel structure\n", 
                sc->sc_dev.dv_xname);
         break;
      default:
         printf("%s: unknown IPC CSR command error 0x%x\n", 
                sc->sc_dev.dv_xname, status);
         break;
      }
      return status; /* error */
   }
}

void
print_dump(sc)
struct vxsoftc *sc;
{
   char *dump_area, *end_dump, *dumpp;
   char dump[209];
   char dump2[209];
   bzero(&dump, 209);

   dump_area = (char *)0xff780030;
   memcpy2(&dump, dump_area, 208);
   
   printf("%s", dump);
}

void *
get_next_envelope(thisenv)
struct envelope *thisenv;
{
   return ((void *)thisenv->link);
}

int 
env_isvalid(thisenv)
struct envelope *thisenv;
{
   return thisenv->valid_flag;
}

struct envelope *
get_cmd_tail(sc)
struct vxsoftc *sc;
{
   unsigned long retaddr;
   retaddr = (unsigned long)sc->vx_reg;
   retaddr += sc->channel->command_pipe_tail_ptr_l;
   return ((struct envelope *)retaddr);
}

struct envelope *
get_status_head(sc)
struct vxsoftc *sc;
{
   unsigned long retaddr;
   retaddr = (unsigned long)sc->vx_reg;
   retaddr += sc->channel->status_pipe_head_ptr_l;
   return ((struct envelope *)retaddr);
}

void
set_status_head(sc, envp)
struct vxsoftc *sc;
void *envp;
{
   sc->channel->status_pipe_head_ptr_h = HI(envp);
   sc->channel->status_pipe_head_ptr_l = LO(envp);
   return;   
}

struct packet *
get_packet(sc, thisenv)
struct vxsoftc *sc;
struct envelope *thisenv;
{
   struct envelope env;
   unsigned long baseaddr; 

   if (thisenv == NULL) return NULL;
   baseaddr = (unsigned long)sc->vx_reg;
   /* 
    * packet ptr returned on status pipe is only last two bytes
    * so we must supply the full address based on the board address.
    * This also works for all envelopes because every address is an
    * offset to the board address 
    */
   baseaddr |= thisenv->packet_ptr;
   return ((void*)baseaddr);
}

/*
 *	Send a command via BPP
 */
int 
bpp_send(struct vxsoftc *sc, void *pkt, int wait_flag)
{
   struct envelope *envp;
   struct init_packet init, *initp;
   struct packet *wpktp, *pktp, *testpktp;
   struct vme2reg *vme2 = (struct vme2reg *)sc->sc_vme2;
   unsigned long newenv;
   int i, s;


   /* load up packet in dual port mem */
   pktp = get_free_packet(sc);
   memcpy2(pktp, pkt, sizeof(struct packet));
   
   envp = get_cmd_tail(sc);
   newenv = (unsigned long)get_free_envelope(sc); /* put a NULL env on the tail */
   envp->link = newenv;
   sc->channel->command_pipe_tail_ptr_h = HI(newenv);
   sc->channel->command_pipe_tail_ptr_l = LO(newenv);
   envp->packet_ptr = (u_long)pktp;   /* add the command packet */
   envp->valid_flag = 1;              /* set valid command flag */

   sc->vx_reg->ipc_cr |= IPC_CR_ATTEN;
   if (wait_flag) {                    /* wait for a packet to return */
      while (pktp->command != CMD_PROCCESED) {
#ifdef DEBUG_VXT
            printf("Polling for packet 0x%x in envelope 0x%x...\n", pktp, envp);
#endif 
            vx_intr(sc);
            delay(5000);
      }
      memcpy2(pkt, pktp, sizeof(struct packet));
      return 0;
   }
   return 0; /* no error */
}

/*
 *	BPP commands
 */
int 
vx_init(sc)
struct vxsoftc *sc;
{
   int i;
   struct init_info *infp, inf;
   struct wring *wringp;
   struct rring *rringp;
   struct termio def_termio;
   struct init_packet init;
   struct event_packet evnt;

   bzero(&def_termio, sizeof(struct termio));
   /* init wait queue */
   bzero(&sc->sc_bppwait_pkt, sizeof(struct packet));
   sc->sc_bppwait_pktp = NULL;
   /* set up init_info array */
   wringp = (struct wring *)WRING_AREA;
   rringp = (struct rring *)RRING_AREA;
   infp = (struct init_info *)INIT_INFO_AREA;
   for (i=0; i<9; i++) {
      bzero(&inf, sizeof(struct init_info));
      infp->write_ring_ptr_h = HI(wringp);
      infp->write_ring_ptr_l = LO(wringp);
      sc->sc_info[i].wringp = wringp;
      infp->read_ring_ptr_h = HI(rringp);
      infp->read_ring_ptr_l = LO(rringp);
      sc->sc_info[i].rringp = rringp;
#ifdef DEBUG_VXT
      printf("write at 0x%8x, read at 0x%8x\n", wringp, rringp);
#endif 
      infp->write_ring_size = WRING_DATA_SIZE;
      infp->read_ring_size = RRING_DATA_SIZE;
      infp->def_termio.c_iflag = VBRKINT;
      infp->def_termio.c_oflag = 0;
      infp->def_termio.c_cflag = (VB9600 | VCS8);
      
      infp->def_termio.c_lflag = VISIG; /* enable signal processing */
      infp->def_termio.c_line = 1; /* raw line disipline, we want to control it! */
      infp->def_termio.c_cc[0] = CINTR;
      infp->def_termio.c_cc[1] = CQUIT;
      infp->def_termio.c_cc[2] = CERASE;
      infp->def_termio.c_cc[3] = CKILL;
      infp->def_termio.c_cc[4] = 20;
      infp->def_termio.c_cc[5] = 2;
      infp->reserved1 = 0;  /* Must be Zero */
      infp->reserved2 = 0;
      infp->reserved3 = 0;
      infp->reserved4 = 0;
      wringp++; rringp++; infp++;
   }
   /* set up init_packet */
   bzero(&init, sizeof(struct init_packet));
   init.eye_catcher[0] = 0x12;
   init.eye_catcher[1] = 0x34;
   init.eye_catcher[2] = 0x56;
   init.eye_catcher[3] = 0x78;
   init.command = CMD_INIT;
   init.command_pipe_number = sc->channel_number;
   /* return status on the same channel */
   init.status_pipe_number = sc->channel_number;
   init.interrupt_level = sc->sc_ipl;
   init.interrupt_vec = sc->sc_vec;
   init.init_info_ptr_h = HI(INIT_INFO_AREA);
   init.init_info_ptr_l = LO(INIT_INFO_AREA);

   /* send packet to the firmware and wait for completion */
   bpp_send(sc, &init, WAIT_POLL);

   /* check for error */
   if (init.error_l !=0) {
      return init.error_l;
   } else {
      /* send one event packet to each device; */
      for (i=0; i<9; i++) {
         bzero(&evnt, sizeof(struct event_packet));
         evnt.command = CMD_EVENT;
         evnt.device_number = i;
         evnt.command_pipe_number = sc->channel_number;
         /* return status on same channel */
         evnt.status_pipe_number = sc->channel_number;
         /* send packet to the firmware */
         bpp_send(sc, &evnt, NOWAIT);
      }
      return 0;
   }
}


