/*	$OpenBSD: talk.h,v 1.5 1999/03/03 20:43:31 millert Exp $	*/
/*	$NetBSD: talk.h,v 1.3 1994/12/09 02:14:27 jtc Exp $	*/

/*
 * Copyright (c) 1983, 1993
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
 *
 *	@(#)talk.h	8.1 (Berkeley) 6/6/93
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <protocols/talkd.h>
#include <curses.h>
#include <string.h>
#include <err.h>

extern	int sockt;
extern	int curses_initialized;
extern	int invitation_waiting;

extern	char *current_state;
extern	int current_line;

typedef struct xwin {
	WINDOW	*x_win;
	int	x_nlines;
	int	x_ncols;
	int	x_line;
	int	x_col;
	char	kill;
	char	cerase;
	char	werase;
} xwin_t;

extern	xwin_t my_win;
extern	xwin_t his_win;
extern	WINDOW *line_win;

void	announce_invite __P((void));
int	check_local __P((void));
void	ctl_transact __P((struct in_addr, CTL_MSG, int, CTL_RESPONSE *));
void	display __P((xwin_t *, char *, int));
void	disp_msg __P((int));
void	end_msgs __P((void));
void	get_addrs __P((char *, char *));
void	get_names __P((int, char **));
void	init_display __P((void));
void	invite_remote __P((void));
int	look_for_invite __P((CTL_RESPONSE *));
int	max __P((int, int));
void	message __P((char *));
void	open_ctl __P((void));
void	open_sockt __P((void));
void	print_addr __P((struct sockaddr_in));
void	quit __P((char *, int));
int	readwin __P((WINDOW *, int, int));
void	re_invite __P((int));
void	send_delete __P((void));
void	set_edit_chars __P((void));
void	sig_sent __P((int));
void	start_msgs __P((void));
void	talk __P((void));
void	xscroll __P((xwin_t *, int));
