/*
 * $OpenBSD: console.c,v 1.2 1997/05/13 16:16:38 niklas Exp $
 * $NetBSD: console.c,v 1.1.1.1 1996/11/29 23:36:29 is Exp $
 *
 * Copyright (c) 1996 Ignatios Souvatzis
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
 *      This product includes software developed by Ignatios Souvatzis
 *      for the NetBSD project.
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
 *
 */

/*
 * Bootblock support routines for Intuition console support.
 */

#include <sys/types.h>

#include <stand.h>
#include "samachdep.h"

#include "amigatypes.h"
#include "amigagraph.h"
#include "amigaio.h"
#include "libstubs.h"

const u_int32_t screentags[] = {
	SA_Type, CUSTOMSCREEN,
	SA_DisplayID, 0x8000,
	SA_ShowTitle, 0,
	SA_Quiet, 1,
	0
};

u_int32_t windowtags[] = {
	WA_CustomScreen, 0L,
	WA_Borderless, 1L,
	WA_Backdrop, 1L,
	WA_Activate, 1L,
	0
};

struct AmigaIO *cnior;
struct TimerIO *tmior;
struct MsgPort *cnmp;

u_int16_t timelimit;

int
consinit() {
	struct Screen *s = 0;
	struct Window *w = 0;

	IntuitionBase = OpenLibrary("intuition.library", 36L);
	if (IntuitionBase == 0)
		goto err;

	s = OpenScreenTagList(0, screentags);
	if (!s)
		goto err;

	windowtags[1] = (u_int32_t)s;
	w = OpenWindowTagList(0, windowtags);
	if (!w)
		goto err;

	cnmp = CreateMsgPort();

	if (!cnmp)
		goto err;

	cnior = (struct AmigaIO *)CreateIORequest(cnmp,
	    sizeof(struct AmigaIO));
	if (!cnior)
		goto err;

	cnior->buf = (void *)w;
	if (OpenDevice("console.device", 0, cnior, 0))
		goto err;

	tmior = (struct TimerIO *)CreateIORequest(cnmp,
	    sizeof(struct TimerIO));
	if (!tmior)
		goto err;

	if (OpenDevice("timer.device", 0, (struct AmigaIO*)tmior, 0))
		goto err;

	return 0;

err:
#ifdef notyet
	if (tmior)
		DeleteIORequest(tmior);

	if (cnior)
		DeleteIORequest(cnior);

	if (cnmp)
		DeleteMsgPort(cnmp);

	if (w)
		CloseWindow(w);

	if (s)
		CloseScreen(s);
	if (IntuitionBase)
		CloseLibrary(IntuitionBase);
#endif

	return 1;
}

void
putchar(c)
	char c;
{
	cnior->length = 1;
	cnior->buf = &c;
	cnior->cmd = Cmd_Wr;
	(void)DoIO(cnior);
}

void
puts(s)
	char *s;
{
	cnior->length = -1;
	cnior->buf = s;
	cnior->cmd = Cmd_Wr;
	(void)DoIO(cnior);
}

int
getchar()
{
	struct AmigaIO *ior;
	char c = -1;

	cnior->length = 1;
	cnior->buf = &c;
	cnior->cmd = Cmd_Rd;

	SendIO(cnior);

	if (timelimit) {
		tmior->cmd = Cmd_Addtimereq;
		tmior->secs = timelimit;
		tmior->usec = 2; /* Paranoid */
		SendIO((struct AmigaIO *)tmior);

		ior = WaitPort(cnmp);
		if (ior == cnior)
			AbortIO((struct AmigaIO *)tmior);
		else /* if (ior == tmior) */ {
			AbortIO(cnior);
			c = '\n';
		}
		WaitIO((struct AmigaIO *)tmior);
		timelimit = 0;
	} 
	(void)WaitIO(cnior);
	return c;
}
