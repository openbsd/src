/* $OpenBSD: cu.h,v 1.2 2012/07/10 08:16:27 nicm Exp $ */

/*
 * Copyright (c) 2012 Nicholas Marriott <nicm@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef CU_H
#define CU_H

/* command.c */
void				 do_command(char);

/* cu.c */
extern struct bufferevent	*input_ev;
extern struct bufferevent	*output_ev;
extern int			 line_fd;
extern struct bufferevent	*line_ev;
int				 set_line(int);
void				 set_termios(void);
void				 restore_termios(void);
char				*tilde_expand(const char *);

/* input.c */
const char			*get_input(const char *);

#endif
