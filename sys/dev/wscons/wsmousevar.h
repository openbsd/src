/* $OpenBSD: wsmousevar.h,v 1.8 2014/12/21 18:16:07 shadchin Exp $ */
/* $NetBSD: wsmousevar.h,v 1.4 2000/01/08 02:57:24 takemura Exp $ */

/*
 * Copyright (c) 1996, 1997 Christopher G. Demetriou.  All rights reserved.
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
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

/*
 * WSMOUSE interfaces.
 */

/*
 * Mouse access functions (must be provided by all mice).
 *
 * There is a "void *" cookie provided by the mouse driver associated
 * with these functions, which is passed to them when they are invoked.
 */
struct wsmouse_accessops {
	int	(*enable)(void *);
	int	(*ioctl)(void *v, u_long cmd, caddr_t data, int flag,
		    struct proc *p);
	void	(*disable)(void *);
};

/*
 * Attachment information provided by wsmousedev devices when attaching
 * wsmouse units.
 */
struct wsmousedev_attach_args {
	const struct wsmouse_accessops *accessops;	/* access ops */
	void	*accesscookie;				/* access cookie */
};

#define	wsmousedevcf_mux	cf_loc[WSMOUSEDEVCF_MUX]

/*
 * Autoconfiguration helper functions.
 */
int	wsmousedevprint(void *, const char *);

/*
 * Callbacks from the mouse driver to the wsmouse interface driver.
 */
#define WSMOUSE_INPUT_DELTA		0
#define WSMOUSE_INPUT_ABSOLUTE_X	(1<<0)
#define WSMOUSE_INPUT_ABSOLUTE_Y	(1<<1)
#define WSMOUSE_INPUT_ABSOLUTE_Z	(1<<2)
#define WSMOUSE_INPUT_ABSOLUTE_W	(1<<3)

void	wsmouse_input(struct device *kbddev, u_int btns,
			   int x, int y, int z, int w, u_int flags);
