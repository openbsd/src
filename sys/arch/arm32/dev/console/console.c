/* $NetBSD: console.c,v 1.6 1996/04/19 20:03:37 mark Exp $ */

/*
 * Copyright (c) 1994-1995 Melvyn Tang-Richardson
 * Copyright (c) 1994-1995 RiscBSD kernel team
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
 *	This product includes software developed by the RiscBSD kernel team
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE RISCBSD TEAM ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * console.c
 *
 * Console functions
 *
 * Created      : 17/09/94
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/device.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/tty.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/msgbuf.h>
#include <sys/user.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>

#include <dev/cons.h>

#include <machine/vidc.h>
#include <machine/vconsole.h>
#include <machine/katelib.h>

#include "vt.h"

#define CONSOLE_VERSION "[V203C] "

/*
 * Externals
 */

extern struct tty *constty;
extern int debug_flags;
#define consmap_col(x) (x & 0x3)
#define CONSMAP_BOLD	8
#define CONSMAP_ITALIC	16

/*
 * Local variables (to this file) 
 */

int locked=0;			/* Nut - is this really safe ? */
struct tty *physcon_tty[NVT];
struct vconsole vconsole_master_store;
struct vconsole *vconsole_master = &vconsole_master_store;
struct vconsole *vconsole_current;
struct vconsole *vconsole_head;
struct vconsole *vconsole_default;
struct cfdriver rpc_cd;
extern struct vconsole *debug_vc;	/* rename this to vconsole_debug */
int physcon_major=4;
static char undefined_string[] = "UNDEFINED";
int lastconsole;
static int printing=0;
static int want_switch=-1;

/*
 * Prototypes
 */

int	physcon_switch	__P((u_int /*number*/));
void	physconstart	__P((struct tty */*tp*/));
static	struct vconsole *vconsole_spawn	__P((dev_t , struct vconsole *));
int	physconparam	__P((struct tty */*tp*/, struct termios */*t*/));
void	consinit	__P((void));
int	physcon_switchup __P((void));
int	physcon_switchdown	__P((void));

/*
 * Exported variables
 */

#define BLANKINIT	(10*60*60)
int vconsole_pending=0;
int vconsole_blankinit=BLANKINIT;
int vconsole_blankcounter=BLANKINIT;

/*
 * Now list all my render engines and terminal emulators
 */

extern struct render_engine vidcconsole;
extern struct terminal_emulator vt220;

/*
 * These find functions should move the console it finds to the top of
 * the list to achieve a caching type of operation.  A doubly
 * linked list would be faster methinks. 
 */

struct tty *
find_tp(dev)
	dev_t dev;
{
	struct vconsole *vc;
	struct vconsole *last=0;
	int unit = minor (dev);
	int s;

	s = spltty();
	for (vc=vconsole_head; vc != NULL; vc=vc->next) {
		if (vc->number==unit) {
			if (vc != vconsole_head) {
				last->next = vc->next;
				vc->next = vconsole_head;
				vconsole_head = vc;
			}
			(void)splx(s);
			return vc->tp;
		}
		last = vc;
	}
	(void)splx(s);
	return NULL;
}

struct vconsole *
find_vc(dev)
	dev_t dev;
{
	struct vconsole *vc;
	struct vconsole *last=NULL;
	int unit = minor (dev);
	int s;

	s = spltty();

	for (vc=vconsole_head; vc!=NULL; vc=vc->next) {
		if (vc->number==unit) {
			if (vc!=vconsole_head) {
				last->next = vc->next;
				vc->next = vconsole_head;
				vconsole_head = vc;
			}
			(void)splx(s);
			return vc;
		}
		last=vc;
	}
	(void)splx(s);
	return NULL;
}

struct tty *console_tty = NULL;

/*
 * Place a graphics driver under virtual console switching control.
 */

struct vconsole *
vconsole_spawn_re(dev, vc)
	dev_t dev;
	struct vconsole *vc;
{
	struct vconsole *new;
	register int num = minor(dev);

	MALLOC(new, struct vconsole *, sizeof(struct vconsole),
	    M_DEVBUF,M_NOWAIT );

	bzero ( (char *)new, sizeof(struct vconsole) );
	*new = *vc;
	new->number = num;
	new->next = vconsole_head;
	new->tp = vc->tp;	/* Implied */
	new->data=0;
	new->t_scrolledback=0;
	new->r_scrolledback=0;
	new->r_data=0;
	new->flags=LOSSY;
	new->vtty = 0;
	vconsole_head = new;
	new->R_INIT ( new );
	new->SPAWN ( new );
	new->data = 0;
	/*new->charmap = 0;*/
	new->flags=LOSSY;
	new->proc = curproc;
	new->vtty = 0;
	return new;
}

static struct vconsole *
vconsole_spawn(dev, vc)
	dev_t dev;
	struct vconsole *vc;
{
	struct vconsole *new;
	register int num = minor(dev);

	if ( find_vc ( dev ) != 0 )
		return 0;

	MALLOC(new, struct vconsole *, sizeof(struct vconsole),
	    M_DEVBUF, M_NOWAIT );

	bzero ( (char *)new, sizeof(struct vconsole) );
	*new = *vc;
	new->number = num;
	new->next = vconsole_head;
	new->tp=NULL;
	new->opened=0;
	new->data=0;
	new->t_scrolledback=0;
	new->r_scrolledback=0;
	new->r_data=0;
	new->vtty = 1;
	vconsole_head = new;
	new->R_INIT ( new );
	new->FLASH ( new, 1 );
	new->CURSOR_FLASH ( new, 1 );
	new->SPAWN ( new );
	new->vtty = 1;

	MALLOC (new->charmap, int *, sizeof(int)*((new->xchars)*(new->ychars)), 
	    M_DEVBUF, M_NOWAIT );

	if (new->charmap==0)
		return 0;
	{
  	    int counter=0;
	    for ( counter=0; counter<((new->xchars)*(new->ychars)); counter++ )
		    (new->charmap)[counter]=' ';
	}
	new->TERM_INIT ( new );
	new->proc = curproc;
	return new;
}

void
vconsole_addcharmap(vc)
	struct vconsole *vc;
{
	int counter=0;

	MALLOC (vc->charmap, int *, sizeof(int)*((vc->xchars)*(vc->ychars)),
	    M_DEVBUF, M_NOWAIT );
	for ( counter=0; counter<((vc->xchars)*(vc->ychars)); counter++ )
		(vc->charmap)[counter]=' ';
}

int
physconopen(dev, flag, mode, p)
	dev_t dev;
	int flag;
	int mode;
	struct proc *p;
{
	struct vconsole *new;

	struct vconsole *vc;
	int unit = minor(dev);
	int found=0;
	int majorhack=0;
	int ret;

	/*
	 * To allow the raw consoles a permanat hook for ioctls
	 * Spawning another virtual console will actuall configure it
	 */

	if ( unit >= NVT ) {
		if ( find_vc(dev)==0 )
			return ENXIO;
	}
/*
	if (unit >= rpc_cd.cd_ndevs || !rpc_cd.cd_devs[unit])
		return ENXIO;
*/

    /* If this virtual console is the real virtual console and hasn't got  */
    /* a charmap then to allocate it one.  We can be sure addcharmap works */
    /* here since this is the open routine.  This is incase the console    */
    /* was initialised before the system was brought up                    */

	if ((unit==0)&&(vconsole_master->charmap==0)) {
		if (vconsole_master==vconsole_current)
			majorhack=1;
		vconsole_addcharmap ( vconsole_master );
		vconsole_master->flags &= ~LOSSY;
	}

    /* Check to see if this console has already been spawned */

	for ( vc = vconsole_head; vc!=NULL; vc=vc->next ) {
		if ( vc->number == unit ) {
			found=1;
			break;
		}
	}

    /* Sanity check.  If we have no default console, set it to the master one */

	if ( vconsole_default==0 )
		vconsole_default = vconsole_master;

    /* Ensure we have a vconsole structure.  Allocate one if we dont already */

	if (found==0)
		new = vconsole_spawn ( dev, vconsole_default );
	else
		new = vc;

	new->proc = p;

    /* Initialise the terminal subsystem for this device */

#define TP (new->tp)

	if (TP == NULL)
		TP = ttymalloc();

	physcon_tty[unit] = TP;

	TP->t_oproc = physconstart;
	TP->t_param = physconparam;
	TP->t_dev = dev;
	if ((TP->t_state & TS_ISOPEN) == 0) {
		TP->t_state |= TS_WOPEN;
		ttychars(TP);
		TP->t_iflag = TTYDEF_IFLAG;
		TP->t_oflag = TTYDEF_OFLAG;
		TP->t_cflag = TTYDEF_CFLAG;
		TP->t_lflag = TTYDEF_LFLAG;
		TP->t_ispeed = TP->t_ospeed = TTYDEF_SPEED;
		physconparam(TP, &TP->t_termios);
		ttsetwater(TP);
	} else if (TP->t_state&TS_XCLUDE && p->p_ucred->cr_uid != 0)
		return EBUSY;
	TP->t_state |= TS_CARR_ON;

	new->opened=1;
   
	TP->t_winsize.ws_col = new->xchars;
	TP->t_winsize.ws_row = new->ychars;
	ret = ((*linesw[TP->t_line].l_open)(dev, TP));
 
	if ( majorhack==1 ) {
		struct vconsole *vc_store;
		int counter;
		int end;
		int lines;
		int xs;

		end = msgbufp->msg_bufx-1;
		if (end>=MSG_BSIZE) end-=MSG_BSIZE;

	/*
	 * Try some cute things.  Count the number of lines in the msgbuf
	 * then scroll the real screen up, just to fit the msgbuf on the
	 * screen, then sink all output, and spew the msgbuf to the
	 * new consoles charmap!
	 */ 

		lines = 0; xs=0; 

 		for (counter=0;counter<end;counter++) {
			xs++;
			if (*((msgbufp->msg_bufc)+counter)==0x0a) {
				if (xs>vc->xchars) lines++;	
				lines++;
				xs=0;
			}
		}

		if ( lines < vc->ychars ) {
			counter=vc->ycur;
			while ( (counter--) > lines )
				new->PUTSTRING ( "\x0a", 1, new );
		}

		new->SLEEP(new);

		vc_store = vconsole_current;
		vconsole_current = 0; /* !!! */

		/* HAHA, cant do this */
		/* new->CLS ( new ); */

		new->PUTSTRING ( "\x0c", 1, new );

	/*
	 * Hmmm, I could really pass the whole damn thing to putstring
	 * since it doesn't have zeros, but I need to do the crlf
	 * conversion
	 */

		xs=0;
	
		if ( end < 0 )
			panic ( "msgbuf trashed reboot and try again" );

		for (counter=0;counter<end;counter++) {
			if (*((msgbufp->msg_bufc)+counter)==0x0a) {
				new->PUTSTRING ( "\x0d", 1, new );
				xs=0;
			}
			if ( (xs++)<new->xchars )
				new->PUTSTRING ((msgbufp->msg_bufc)+counter, 1, new );
		}
		vconsole_master->ycur = lines;
		vconsole_current = vc_store;
		new->WAKE(new);
	        vconsole_current->xcur = 0;

	        printf ( "\x0a" );
	    }	 

	return(ret);
}

/*
 * int physconclose(dev_t dev, int flag, int mode, struct proc *p)
 *
 * Close the physical console
 */

int
physconclose(dev, flag, mode, p)
	dev_t dev;
	int flag;
	int mode;
	struct proc *p;
{
	register struct tty *tp;

	tp = find_tp ( dev );
	if (tp == NULL) {
		printf("physconclose: tp=0 dev=%04x\n", dev);
		return(ENXIO);
	}
	(*linesw[tp->t_line].l_close)(tp, flag);
	ttyclose(tp);

	return(0);
}

int
physconread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	register struct tty *tp = find_tp ( dev );
	if (tp == NULL) {
		printf("physconread: tp=0 dev=%04x\n", dev);
		return(ENXIO);
	}
	return ((*linesw[tp->t_line].l_read)(tp, uio, flag));
}

int
physconwrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	register struct tty *tp;

	tp = find_tp(dev);

	if (tp == NULL) {
		printf("physconwrite: tp=0 dev=%04x\n", dev);
		return(ENXIO);
	}

	return((*linesw[tp->t_line].l_write)(tp, uio, flag));
}

struct tty *
physcontty(dev)
	dev_t dev;
{
	return(find_tp(dev));
}

int ioctlconsolebug;

int
physconioctl(dev, cmd, data, flag, p)
	dev_t dev;
	int cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct vconsole vconsole_new;
	struct tty *tp=(struct tty *)0xDEADDEAD ;
	int error;
	int s;
	struct vconsole *vc = find_vc(dev);

	ioctlconsolebug = cmd;

	tp = find_tp(dev);

	if ((vc==0)||(tp==0))
		return ENXIO;

	switch ( cmd ) {
	case CONSOLE_SWITCHUP:
		physcon_switchup ();
		return 0;
	case CONSOLE_SWITCHDOWN:
		physcon_switchdown ();
		return 0;
	case CONSOLE_SWITCHTO:
		if (!physcon_switch ( *(int *)data ))
			return 0;
		else
			return EINVAL;
	case CONSOLE_SWITCHPREV:
		physcon_switch ( lastconsole );
		return 0;
	case CONSOLE_CREATE:
		if ( vconsole_spawn ( makedev ( physcon_major, *(int *)data ),
		    vconsole_default ) == 0 )
			return ENOMEM;
		else
			return 0;
		break;

	case CONSOLE_RENDERTYPE:
		strncpy ( (char *)data, vc->T_NAME, 20 );
		return 0;

	case CONSOLE_TERMTYPE:
		strncpy ( (char *)data, vc->T_NAME, 20 );
		return 0;

	case CONSOLE_LOCK:
		s = spltty();
		locked++;
		(void)splx(s);
		return 0;
	case CONSOLE_UNLOCK:
		s = spltty();
		locked--;
		if ( locked<0 )
			locked=0;
		(void)splx(s);
		return 0;
	case CONSOLE_SPAWN_VIDC:
/*
		vconsole_new = *vconsole_default;
*/
		vconsole_new = *vc;
		vconsole_new.render_engine = &vidcconsole;
		if ( vconsole_spawn_re ( 
		    makedev ( physcon_major, *(int *)data ),
		    &vconsole_new ) == 0 )
			return ENOMEM;
		else
			return 0;

 	case CONSOLE_GETVC:
	    {
/*	   	struct vconsole *vc_p;	
	   	vc_p = find_vc(dev);
		*(int *)data = vc_p->number;*/
		*(int *)data = vconsole_current->number;
		return 0;
	    }

	case CONSOLE_CURSORFLASHRATE:
		vc->CURSORFLASHRATE ( vc, *(int *)data );		
		return 0;

	case CONSOLE_BLANKTIME:
		vconsole_blankinit = *(int *)data;		
		return 0;

	case CONSOLE_DEBUGPRINT:
		{
		    struct vconsole *vc_p;	

		    vc_p = find_vc(makedev(physcon_major,*(int*)data));
		    if (vc_p==0) return EINVAL;
		    printf ( "DEBUGPRINT for console %d\n", *(int*)data );
		    printf ( "flags %08x vtty %01x\n", vc_p->flags, vc_p->vtty );
		    printf ( "TTY INFO - winsize (%d, %d)\n",
				vc_p->tp->t_winsize.ws_col,
				vc_p->tp->t_winsize.ws_row);
		    vc_p->R_DEBUGPRINT ( vc_p );
		    vc_p->T_DEBUGPRINT ( vc_p );
		    return 0;
		}
		
	default: 
		error = vc->IOCTL ( vc, dev, cmd, data, flag, p );
		if ( error >=0 )
			return error;
		error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
		if (error >= 0)
			return error;
		error = ttioctl(tp, cmd, data, flag, p);
		if (error >= 0)
			return error;
	} 
	return(ENOTTY);
}

int
physconmmap(dev, offset, nprot)
	dev_t dev;
	int offset;
	int nprot;
{
	struct vconsole *vc = find_vc(dev);
	u_int physaddr;

	if (minor(dev) < 64) {
		log(LOG_WARNING, "You should no longer use ttyv to mmap a frame buffer\n");
		log(LOG_WARNING, "For vidc use /dev/vidcvideo0\n");
	}
	physaddr = vc->MMAP(vc, offset, nprot);
	return(physaddr);
}

/*
 * Perform output on specified tty stream
 */

void
physconstart(tp)
	struct tty *tp;
{
	int s, len;
	struct clist *cl;
	struct vconsole *vc;
	u_char buf[128];

	s = spltty();

	vc = find_vc ( tp->t_dev );

	/* Are we ready to perform output */

	if (tp->t_state & (TS_TIMEOUT | TS_BUSY | TS_TTSTOP)) {
		(void)splx(s);
		return;
	}

	tp->t_state |= TS_BUSY;

	/* Fill our buffer with the data to print */

	cl = &tp->t_outq;

	if ( vc->r_scrolledback ) vc->R_SCROLLBACKEND ( vc );
	if ( vc->t_scrolledback ) vc->T_SCROLLBACKEND ( vc );

	(void)splx(s);

    /* Apparently we do this out of spl since it _IS_ fairly expensive */
    /* and it stops the serial ports overflowing 		       */

	while ( (len = q_to_b(cl, buf, 128)) ) {
		if ( vc!=NULL )
			vc->PUTSTRING(buf, len, vc);
	}

	s = spltty ();

	tp->t_state &= ~TS_BUSY;

	if (cl->c_cc) {
		tp->t_state |= TS_TIMEOUT;
		timeout(ttrstrt, tp, 1);
	}

	if (cl->c_cc <= tp->t_lowat) {
		if (tp->t_state & TS_ASLEEP) {
			tp->t_state &= ~TS_ASLEEP;
			wakeup(cl);
		}
		selwakeup(&tp->t_wsel);
	}

	if ( want_switch != -1 ) {
		physcon_switch ( want_switch );
		want_switch=-1;
	}

	(void)splx(s);
}

int
physconparam(tp, t)
	struct tty *tp;
	struct termios *t;
{
	tp->t_ispeed = t->c_ispeed;
	tp->t_ospeed = t->c_ospeed;
	tp->t_cflag = t->c_cflag;
	return(0);
}

void
physconstop(tp, flag)
	struct tty *tp;
	int flag;
{
	/* Nothing necessary */
}

int
physconkbd(key)
	int key;
{
	char *string;
	register struct tty *tp;
	int s;

	s = spltty();

	tp = vconsole_current->tp;

	if (tp == NULL) return(1);

	if ((tp->t_state & TS_ISOPEN) == 0) {
		(void)splx(s);
		return(1);
	}

	if (key < 0x100)
		(*linesw[tp->t_line].l_rint)(key, tp);
	else {
	        switch (key) {
		case 0x100:
			string = "\x1b[A";
			break;
		case 0x101:
			string = "\x1b[B";
			break;
		case 0x102:
			string = "\x1b[D";
			break;
		case 0x103:
			string = "\x1b[C";
			break;
		case 0x104:
			string = "\x1b[6~";
			break;
		case 0x105:
			string = "\x1b[5~";
			break;
		case 0x108:
			string = "\x1b[2~";
			break;
		case 0x109:
			string = "\x7f";
			break;
		default:
			string = "";
			break;
		}
		while (*string != 0) {
			(*linesw[tp->t_line].l_rint)(*string, tp);
			++string;
		}
	}
	(void)splx(s);
	return(0);
}

static int physconinit_called = 0;

void
physconinit(cp)
	struct consdev *cp;
{
	int *test;
	int counter;

    /*
     * Incase we're called more than once.  All routines below here
     * undergo once time initialisation
     */

	if ( physconinit_called )
		return;

	physconinit_called=1;

	locked=0;

	physcon_major = major ( cp->cn_dev );

	/*
	 * Create the master console
	 */

	vconsole_master->next = NULL;
	vconsole_master->number = 0;
	vconsole_master->opened = 1;
	vconsole_master->tp = NULL;

	/*
	 * Right, here I can choose some render routines
	 */

	vconsole_master->render_engine = &vidcconsole;
	vconsole_master->terminal_emulator = &vt220;

	/*
	 * We will very soon loose the master console, and number 0 will
	 * become the current console so as not to waste a struct vconsole
	 */
	vconsole_current = vconsole_head = vconsole_master;
	vconsole_master->data = NULL;
	vconsole_master->vtty = 1;

	/*
	 * Perform initial checking
	 */

	if ( vconsole_master->terminal_emulator->name == 0 )
		vconsole_master->terminal_emulator->name = undefined_string;
	if ( vconsole_master->render_engine->name == 0 )
		vconsole_master->render_engine->name = undefined_string;

	/*
	 * Right, I have to assume the init and print procedures are ok
	 * or there's nothing else that can be done
	 */

	vconsole_master->R_INIT ( vconsole_master);
	vconsole_master->SPAWN ( vconsole_master );
	vconsole_master->TERM_INIT (vconsole_master);
	vconsole_master->flags = LOSSY;

	/*
	 * Now I can do some productive verification
	 */

	/* Ensure there are no zeros in the termulation and render_engine */

	test = (int *) vconsole_master->render_engine;
	for ( counter=0; counter<(sizeof(struct render_engine)/4)-1; counter++ )
		if (test[counter]==0)
			panic ( "Render engine %i is missins a routine",
			    vconsole_master->render_engine->name );
  
	test = (int *) vconsole_master->terminal_emulator;
	for ( counter=0; counter<(sizeof(struct terminal_emulator)/4)-1; counter++ )
		if (test[counter]==0)
			panic ( "Render engine %i is missing a routine",
			    vconsole_master->terminal_emulator->name );
}

/*
 * void physconputstring(char *string, int length)
 *
 * Render a string on the physical console
 */

void
physconputstring(string, length)
	char *string;
	int length;
{
	vconsole_current->PUTSTRING(string, length, vconsole_current);
}

/*
 * void physcongetchar(void)
 *
 * Get a character from the physical console
 */

int getkey_polled __P(());

char
physcongetchar(void)
{
	return(getkey_polled());
}

void
consinit(void)
{
	static int consinit_called = 0;

	if (consinit_called != 0)
		return;

	consinit_called = 1;
	cninit();

/* Ok get our start up message in early ! */

	printf("\x0cRiscBSD booting...\n%s\n", version);
}

void
rpcconsolecnprobe(cp)
	struct consdev *cp;
{
	int major;

/*	printf("rpcconsoleprobe: pc=%08x\n", cp);*/

/*
 * Locate the major number for the physical console device
 * We do this by searching the character device list until we find
 * the device with the open function for the physical console
 */

	for (major = 0; major < nchrdev; ++major) {
		if (cdevsw[major].d_open == physconopen)
			break;
	}

/* Initialise the required fields */

	cp->cn_dev = makedev(major, 0);
	cp->cn_pri = CN_INTERNAL;
}

#define RPC_BUF_LEN	(64)
char rpc_buf[RPC_BUF_LEN];
int rpc_buf_ptr = 0;

#define RPC_BUF_FLUSH	\
{			\
	vconsole_current->PUTSTRING ( rpc_buf, rpc_buf_ptr, vconsole_current );	\
	rpc_buf_ptr=0;	\
}

void
rpcconsolecninit(cp)
	struct consdev *cp;
{
	physconinit(cp);	/* woo Woo WOO!!!, woo, woo, yes ok bye */
}

void
rpcconsolecnputc(dev, character)
	dev_t dev;
	char character;
{
	extern int cold;

	if (( rpc_buf_ptr==RPC_BUF_LEN ) || (cold==0) )
		RPC_BUF_FLUSH

	rpc_buf[rpc_buf_ptr++] = character;

	if ((character == 0x0a )||(character==0x0d)||(character=='.'))
		RPC_BUF_FLUSH
}

int
console_switchdown()
{
	physcon_switchdown ();
	return 0;
}

int
console_switchup()
{
	physcon_switchup ();
	return 0;
}

int
console_unblank()
{
	vconsole_blankcounter = vconsole_blankinit;
	vconsole_current->BLANK ( vconsole_current, BLANK_NONE );
	return 0;
}

int
console_scrollback ()
{
	if (vconsole_current==NULL)
		return 0;
	if ( vconsole_current->R_SCROLLBACK(vconsole_current) ==-1 ) {
		if ( vconsole_current->T_SCROLLBACK(vconsole_current)==-1 ) {  
		}
	}
	return 0;
}

int
console_scrollforward ()
{
	if (vconsole_current==NULL)
		return 0;
	if ( vconsole_current->R_SCROLLFORWARD(vconsole_current) ==-1 ) {
		if ( vconsole_current->T_SCROLLFORWARD(vconsole_current)==-1 ) {  
		}
	}
	return 0;
}

int
console_switchlast ()
{
	return (physcon_switch ( lastconsole ));
}

int
physcon_switchdown ()
{
	int start;
	int next = (vconsole_current->number);
	start=next;
	do {	
		next--;
		next = next&0xff;
		if (next==start) return 0;
	} while (physcon_switch ( next ));
        return 0;
}

int
physcon_switchup ()
{
	int start;
	int next = (vconsole_current->number);
	start=next;
	do {	
		next++;
		next = next&0xff;
		if (next==start) return 0;
	} while (physcon_switch ( next ));
	return 0;
}

void
console_switch(number)
	u_int number;
{
	physcon_switch ( number );
}

/* switchto */
int 
physcon_switch(number)
	u_int number;
{
	register struct vconsole *vc;
        int s = spltty ();
        int ret;

	if ( locked!=0 ) {
		ret=0;
		goto out;
        }

	if ( printing ) {
		want_switch = number;
		ret=0;
		goto out;
	}

	vc = find_vc ( makedev ( physcon_major, number ) );

	if ( vc==0 ) {
		ret = 1;
		goto out;
	}

	if ( vc==vconsole_current ) {
		ret = 1;
		goto out;
 	}

	/* Point of no return */

	locked++;		/* We cannot reenter this routine now */

	/* De-activate the render engine functions */
	if ( vconsole_current->vtty==1 ) {
		vconsole_current->SLEEP(vconsole_current);
		vconsole_current->FLASH ( vc, 0 );
		vconsole_current->CURSOR_FLASH ( vc, 0 );
	}

	/* Swap in the new consoles state */

	lastconsole = vconsole_current->number;
	vconsole_current=vc;
	vconsole_current->R_SWAPIN ( vc );

	/* Re-activate the render engine functions */

	if ( vconsole_current->vtty==1 ) {
		vconsole_current->T_SWAPIN ( vc );
		vconsole_current->WAKE(vconsole_current);
		vconsole_current->FLASH ( vc, 1 );
		vconsole_current->CURSOR_FLASH ( vc, 1 );
	}

	locked--;

	/* Tell the process about the switch, like the X server */

	if ( vc->proc )
		psignal ( vc->proc, SIGIO );

	ret = 0;
out:
	(void)splx(s);
	return(ret);
}

char
rpcconsolecngetc(dev)
	dev_t dev;
{
	return( physcongetchar () );
}

void
rpcconsolecnpollc(dev, on)
	dev_t dev;
	int on;
{
	RPC_BUF_FLUSH
}

int
rpcprobe(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	return(1);
}

void
rpcattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	printf(" riscbsd generic console driver %susing %s %s\n",
	    CONSOLE_VERSION, vconsole_master->terminal_emulator->name,
	    vconsole_master->render_engine->name);

	vconsole_master->T_ATTACH(vconsole_master, parent, self, aux);
	vconsole_master->R_ATTACH(vconsole_master, parent, self, aux);
}

/*
struct cfattach rpc_ca = {
	sizeof(struct device), rpcprobe, rpcattach
};

struct cfdriver rpc_cd = {
	NULL, "rpc", DV_TTY
};
*/

struct cfattach vt_ca = {
	sizeof(struct device), rpcprobe, rpcattach
};

struct cfdriver vt_cd = {
	NULL, "rpc", DV_TTY
};

extern struct terminal_emulator vt220;

struct render_engine *render_engine_tab[] = {
        &vidcconsole,
};

struct terminal_emulator *terminal_emulator_tab[] = {
        &vt220,
};
