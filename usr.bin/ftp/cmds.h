/*	$OpenBSD: cmds.h,v 1.1 2009/05/05 19:35:30 martynas Exp $	*/

/*
 * Copyright (c) 2009 Martynas Venckus <martynas@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

void	setascii(int, char **);
void	settenex(int, char **);
void	setftmode(int, char **);
void	setform(int, char **);
void	setstruct(int, char **);
void	reput(int, char **);
void	put(int, char **);
void	putit(int, char **, int);
void	mput(int, char **);
void	reget(int, char **);
char   *onoff(int);
void	status(int, char **);
int	togglevar(int, char **, int *, const char *);
void	setbell(int, char **);
void	setedit(int, char **);
void	setepsv4(int, char **);
void	settrace(int, char **);
void	sethash(int, char **);
void	setverbose(int, char **);
void	setport(int, char **);
void	setprogress(int, char **);
void	setprompt(int, char **);
void	setgate(int, char **);
void	setglob(int, char **);
void	setpreserve(int, char **);
void	setdebug(int, char **);
void	lcd(int, char **);
void	deletecmd(int, char **);
void	mdelete(int, char **);
void	renamefile(int, char **);
void	ls(int, char **);
void	mls(int, char **);
void	shell(int, char **);
void	user(int, char **);
void	pwd(int, char **);
void	lpwd(int, char **);
void	makedir(int, char **);
void	removedir(int, char **);
void	quote(int, char **);
void	site(int, char **);
void	quote1(const char *, int, char **);
void	do_chmod(int, char **);
void	do_umask(int, char **);
void	idle(int, char **);
void	rmthelp(int, char **);
void	quit(int, char **);
void	account(int, char **);
void	proxabort(int);
void	doproxy(int, char **);
void	setcase(int, char **);
void	setcr(int, char **);
void	setntrans(int, char **);
void	setnmap(int, char **);
void	setpassive(int, char **);
void	setsunique(int, char **);
void	setrunique(int, char **);
void	cdup(int, char **);
void	restart(int, char **);
void	syst(int, char **);
void	macdef(int, char **);
void	sizecmd(int, char **);
void	modtime(int, char **);
void	rmtstatus(int, char **);
void	newer(int, char **);
void	page(int, char **);

