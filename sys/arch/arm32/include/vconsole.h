/* $OpenBSD: vconsole.h,v 1.2 2000/03/03 00:54:48 todd Exp $ */
/* $NetBSD: vconsole.h,v 1.1 1996/01/31 23:23:29 mark Exp $ */

/*
 * Copyright (c) 1994,1995 Melvyn Tang-Richardson
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
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
 *	This product includes software developed by the RiscBSD team
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * vconsole.h
 *
 * Virtual console header
 *
 * Created      : 18/09/94
 * Last updated : 10/01/96
 *
 */

#ifdef _KERNEL
#define LOSSY           1
#define FIXEDRES        2

#define BOLD		(1<<15)
#define UNDERLINE	(1<<16)
#define REVERSE		(1<<17)
#define BLINKING	(1<<18)

#include <machine/vidc.h>

struct vconsole;

/*
 * Render routines and terminal drivers which conform to version 1.00
 * of the spec should always be present.  This is the lowest common
 * denominator, which enables the driver to always find something that
 * will work.
 *
 * Prefered drivers can be added required in the tables.
 */

struct render_engine {
	char * name;
        int  ( *init		) __P(( struct vconsole *vc ));
        void ( *putchar		) __P(( dev_t dev, char c, struct vconsole *vc ));
        int  ( *spawn		) __P(( struct vconsole *vc ));
        int  ( *swapin		) __P(( struct vconsole *vc ));
        int  ( *mmap		) __P(( struct vconsole *vc, int offset, int nprot ));
	void ( *render		) __P(( struct vconsole *vc, char c));
	void ( *scrollup	) __P(( struct vconsole *vc, int low, int high ));
	void ( *scrolldown	) __P(( struct vconsole *vc, int low, int high ));
	void ( *cls		) __P(( struct vconsole *vc ));
	void ( *update		) __P(( struct vconsole *vc ));
	int ( *scrollback	) __P(( struct vconsole *vc ));
	int ( *scrollforward	) __P(( struct vconsole *vc ));
	int ( *scrollbackend	) __P(( struct vconsole *vc ));
	int ( *clreos		) __P(( struct vconsole *vc, int code ));
	int ( *debugprint	) __P(( struct vconsole *vc ));
	int ( *cursorupdate	) __P(( struct vconsole *vc ));
	int ( *cursorflashrate	) __P(( struct vconsole *vc, int rate ));
	int ( *setfgcol		) __P(( struct vconsole *vc, int col ));
	int ( *setbgcol		) __P(( struct vconsole *vc, int col ));
	int ( *textpalette	) __P(( struct vconsole *vc ));
	int ( *sgr		) __P(( struct vconsole *vc, int type ));
	int ( *blank		) __P(( struct vconsole *vc, int type ));
        int ( *ioctl		) __P(( struct vconsole *vc, dev_t dev, int cmd,
				    caddr_t data, int flag, struct proc *p));
        int ( *redraw		) __P(( struct vconsole *vc, int x, int y, int a, int b ));
	int ( *attach		) __P(( struct vconsole *vc, struct device *dev, struct device *dev1, void * arg));
	int ( *flash		) __P(( struct vconsole *vc, int flash ));
	int ( *cursor_flash	) __P(( struct vconsole *vc, int flash ));
};

/* Blank types.  VESA defined */

/* Blank type 3 is suported by default */

#define BLANK_NONE	0	/* Not really blanked */
#define BLANK_IDLE	1	/* Vsync dropped for fast reactivation */ 
#define BLANK_UNUSED	2	/* Hsync dropped for semi fast reactivation */
#define BLANK_OFF	3	/* All signals removed slowest reactivation */

#define R_NAME		render_engine->name
#define	SPAWN		render_engine->spawn
#define SCROLLUP	render_engine->scrollup
#define SCROLLDOWN	render_engine->scrolldown
#define RENDER		render_engine->render
#define R_SWAPIN	render_engine->swapin
#define CLS		render_engine->cls
#define R_INIT		render_engine->init
#define PUTCHAR		render_engine->putchar
#define R_SWAPIN	render_engine->swapin
#define MMAP		render_engine->mmap
#define R_SCROLLBACK	render_engine->scrollback
#define R_SCROLLFORWARD	render_engine->scrollforward
#define R_SCROLLBACKEND	render_engine->scrollbackend
#define R_CLREOS	render_engine->clreos
#define R_DEBUGPRINT	render_engine->debugprint
#define CURSORUPDATE	render_engine->cursorupdate
#define CURSORFLASHRATE	render_engine->cursorflashrate
#define SETFGCOL	render_engine->setfgcol
#define SETBGCOL	render_engine->setbgcol
#define	TEXTPALETTE	render_engine->textpalette
#define	SGR		render_engine->sgr
#define BLANK		render_engine->blank
#define IOCTL		render_engine->ioctl
#define REDRAW		render_engine->redraw
#define R_ATTACH	render_engine->attach
#define FLASH		render_engine->flash
#define CURSOR_FLASH	render_engine->cursor_flash

/*
 * The terminal emulator's scroll back is only used as a last resort for
 * cases when a render engine can't scrollback.  In most cases though, the
 * terminal emulator won't allocate enough chapmap to perform scrollback.
 */

struct terminal_emulator {
	char *name;
	/* Terminal emulation routines */
	int (*term_init)	__P((struct vconsole *vc));
        int (*putstring)	__P((char *string, int length, struct vconsole *vc));
        int (*swapin)		__P((struct vconsole *vc));
	int (*swapout)		__P((struct vconsole *vc));
	int (*sleep)		__P((struct vconsole *vc));
	int (*wake)		__P((struct vconsole *vc));
	int ( *scrollback)	__P(( struct vconsole *vc ));
	int ( *scrollforward)	__P(( struct vconsole *vc ));
	int ( *scrollbackend)	__P(( struct vconsole *vc ));
	int ( *debugprint)	__P(( struct vconsole *vc ));
	int ( *modechange)	__P(( struct vconsole *vc ));
	int ( *attach ) 	__P(( struct vconsole *vc, struct device *dev, struct device *dev1, void *arg ));
};

#define T_NAME		terminal_emulator->name
#define TERM_INIT	terminal_emulator->term_init
#define T_SWAPIN	terminal_emulator->swapin
#define PUTSTRING	terminal_emulator->putstring
#define SLEEP		terminal_emulator->sleep
#define WAKE		terminal_emulator->wake
#define T_SCROLLBACK	terminal_emulator->scrollback
#define T_SCROLLFORWARD	terminal_emulator->scrollforward
#define T_SCROLLBACKEND	terminal_emulator->scrollbackend
#define T_DEBUGPRINT	terminal_emulator->debugprint
#define MODECHANGE	terminal_emulator->modechange
#define T_ATTACH	terminal_emulator->attach

struct vconsole {
        /* Management of consoles */

        struct vconsole *next;
        int number;
        int opened;
        struct tty *tp;
	struct proc *proc;
        int flags;

        /* Data structures */
        char *data;
	char *r_data;

        /* Structures required for the generic character map */
        int xchars, ychars;
        int *charmap;
	int xcur, ycur;

	/* This is the end of the old stuff */

	struct render_engine *render_engine;
	struct terminal_emulator *terminal_emulator;

	int t_scrolledback;
	int r_scrolledback;

	int blanked;

	int vtty;
};

extern int vconsole_pending;
extern int vconsole_blankinit;
extern int vconsole_blankcounter;

extern struct vconsole *vconsole_current;

extern struct render_engine *render_engine_tab[];
extern struct terminal_emulator *terminal_emulator_tab[];

#endif

/* ioctls for switching between vconsoles */

#define CONSOLE_SWITCHUP	_IO( 'n',  0 )
#define CONSOLE_SWITCHDOWN	_IO( 'n',  1 )
#define CONSOLE_SWITCHTO	_IOW( 'n', 2, int )
#define CONSOLE_SWITCHPREV	_IO( 'n',  3 )

/* ioctls for creating new virtual consoles */

#define CONSOLE_CREATE		_IOW( 'n', 10, int )
#define CONSOLE_RENDERTYPE	_IOR( 'n', 11, 20 )
#define CONSOLE_TERMTYPE	_IOR( 'n', 12, 20 )

/* ioctls for locking in the current console.  Kinky eh ? */

#define CONSOLE_LOCK		_IO( 'n', 20 )
#define CONSOLE_UNLOCK		_IO( 'n', 21 )

/* ioctls for animation, multimedia and games */

#define CONSOLE_SWOP		_IO( 'n', 30 ) /* Screen Banking */

#ifdef CONSOLEGFX
struct console_line {
	int len;
	char data[128];
};

struct console_coords {
	int x, y;
};

#define CONSOLE_DRAWGFX		_IOW( 'n', 31, struct console_line ) /* Screen Banking */
#define CONSOLE_MOVE		_IOW( 'n', 32, struct console_coords )
#endif

/* ioctls for configuration and control */

#define CONSOLE_CURSORFLASHRATE	_IOW ( 'n', 40, int )
#define CONSOLE_MODE 		_IOW ( 'n', 41, struct vidc_mode )
#define CONSOLE_MODE_ALL 	_IOW ( 'n', 42, struct vidc_mode )

#define CONSOLE_BLANKTIME	_IOW ( 'n', 44, int )

/* ioctls for developers *DO NOT USE * */

#define CONSOLE_SPAWN_VIDC	_IOW( 'n', 100, int )
#define CONSOLE_DEBUGPRINT	_IOW( 'n', 101, int )


/* structures and ioctls added by mark for the Xserver development */

struct console_info {
	videomemory_t videomemory;
	int width;
	int height;
	int bpp;
};

struct console_palette {
	int entry;
	int red;
	int green;
	int blue;
};

#define CONSOLE_RESETSCREEN	_IO( 'n', 102)
#define CONSOLE_RESTORESCREEN	_IO( 'n', 103)
#define CONSOLE_GETINFO		_IOR( 'n', 104, struct console_info )
#define CONSOLE_PALETTE		_IOW( 'n', 105, struct console_palette )
#define CONSOLE_GETVC		_IOR( 'n', 106, int )

#define CONSOLE_IOCTL_COMPAT_N	_IO( 'n', 107 )
#define CONSOLE_IOCTL_COMPAT_T	_IO( 't', 107 )

/* End of vconsole.h */
