/*	$OpenBSD: keyboard.c,v 1.9 2001/12/07 07:57:35 pvalchev Exp $	*/
/*	$NetBSD: keyboard.c,v 1.2 1995/01/20 08:51:59 jtc Exp $	*/

/*-
 * Copyright (c) 1980, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 */

#ifndef lint
#if 0
static char sccsid[] = "@(#)keyboard.c	8.1 (Berkeley) 6/6/93";
#endif
static char rcsid[] = "$OpenBSD: keyboard.c,v 1.9 2001/12/07 07:57:35 pvalchev Exp $";
#endif /* not lint */

#include <ctype.h>
#include <signal.h>
#include <termios.h>

#include "systat.h"
#include "extern.h"

void
keyboard()
{
        char ch, line[80];
	sigset_t mask, omask;

        for (;;) {
                col = 0;
                move(CMDLINE, 0);
                do {
                        refresh();
                        if ((ch = getch()) == ERR)
				exit(1);
                        ch &= 0177;
                        if (ch == 0177 && ferror(stdin)) {
                                clearerr(stdin);
                                continue;
                        }
                        if (ch >= 'A' && ch <= 'Z')
                                ch += 'a' - 'A';
                        if (col == 0) {
				switch (ch) {
				case CTRL('l'):
				case CTRL('g'):
					sigemptyset(&mask);
					sigaddset(&mask, SIGALRM);
					sigprocmask(SIG_BLOCK, &mask, &omask);
					if (ch == CTRL('l'))
						wrefresh(curscr);
					else
						status();
					sigprocmask(SIG_SETMASK, &omask, NULL);
                                        continue;
				case ':':
					break;
				default:
					continue;
				}
                                move(CMDLINE, 0);
                                clrtoeol();
                        }
                        if (ch == erasechar() && col > 0) {
                                if (col == 1 && line[0] == ':')
                                        continue;
                                col--;
                                goto doerase;
                        }
                        if (ch == CTRL('w') && col > 0) {
                                while (--col >= 0 && isspace(line[col]))
                                        ;
                                col++;
                                while (--col >= 0 && !isspace(line[col]))
                                        if (col == 0 && line[0] == ':')
                                                break;
                                col++;
                                goto doerase;
                        }
                        if (ch == killchar() && col > 0) {
                                col = 0;
                                if (line[0] == ':')
                                        col++;
                doerase:
                                move(CMDLINE, col);
                                clrtoeol();
                                continue;
                        }
			if (col >= sizeof(line) - 1) {
				/* line too long */
				beep();
				continue;
			}
                        if (isprint(ch) || ch == ' ') {
                                line[col] = ch;
                                mvaddch(CMDLINE, col, ch);
                                col++;
                        }
                } while (col == 0 || (ch != '\r' && ch != '\n'));
                line[col] = '\0';
		sigemptyset(&mask);
		sigaddset(&mask, SIGALRM);
		sigprocmask(SIG_BLOCK, &mask, &omask);
                command(line + 1);
		sigprocmask(SIG_SETMASK, &omask, NULL);
        }
	/*NOTREACHED*/
}
