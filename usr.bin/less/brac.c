/*
 * Copyright (c) 1984,1985,1989,1994,1995  Mark Nudelman
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice in the documentation and/or other materials provided with 
 *    the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR 
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR 
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE 
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN 
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/*
 * Routines to perform bracket matching functions.
 */

#include "less.h"
#include "position.h"

/*
 * Try to match the n-th open bracket 
 *  which appears in the top displayed line (forwdir),
 * or the n-th close bracket 
 *  which appears in the bottom displayed line (!forwdir).
 * The characters which serve as "open bracket" and 
 * "close bracket" are given.
 */
	public void
match_brac(obrac, cbrac, forwdir, n)
	register int obrac;
	register int cbrac;
	int forwdir;
	int n;
{
	register int c;
	register int nest;
	POSITION pos;
	int (*chget)();

	extern int ch_forw_get(), ch_back_get();

	/*
	 * Seek to the line containing the open bracket.
	 * This is either the top or bottom line on the screen,
	 * depending on the type of bracket.
	 */
	pos = position((forwdir) ? TOP : BOTTOM);
	if (pos == NULL_POSITION || ch_seek(pos))
	{
		if (forwdir)
			error("Nothing in top line", NULL_PARG);
		else
			error("Nothing in bottom line", NULL_PARG);
		return;
	}

	/*
	 * Look thru the line to find the open bracket to match.
	 */
	do
	{
		if ((c = ch_forw_get()) == '\n' || c == EOI)
		{
			if (forwdir)
				error("No bracket in top line", NULL_PARG);
			else
				error("No bracket in bottom line", NULL_PARG);
			return;
		}
	} while (c != obrac || --n > 0);

	/*
	 * Position the file just "after" the open bracket
	 * (in the direction in which we will be searching).
	 * If searching forward, we are already after the bracket.
	 * If searching backward, skip back over the open bracket.
	 */
	if (!forwdir)
		(void) ch_back_get();

	/*
	 * Search the file for the matching bracket.
	 */
	chget = (forwdir) ? ch_forw_get : ch_back_get;
	nest = 0;
	while ((c = (*chget)()) != EOI)
	{
		if (c == obrac)
			nest++;
		else if (c == cbrac && --nest < 0)
		{
			/*
			 * Found the matching bracket.
			 * If searching backward, put it on the top line.
			 * If searching forward, put it on the bottom line.
			 */
			jump_line_loc(ch_tell(), forwdir ? -1 : 1);
			return;
		}
	}
	error("No matching bracket", NULL_PARG);
}
