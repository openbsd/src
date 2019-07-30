/*	$OpenBSD: extern.h,v 1.28 2015/01/20 16:59:07 millert Exp $	*/
/*	$NetBSD: extern.h,v 1.7 1997/07/09 05:22:00 mikel Exp $	*/

/*-
 * Copyright (c) 1992, 1993
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
 * 3. Neither the name of the University nor the names of its contributors
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
 *	@(#)extern.h	8.2 (Berkeley) 4/20/95
 *	$OpenBSD: extern.h,v 1.28 2015/01/20 16:59:07 millert Exp $
 */

struct name;
struct name *cat(struct name *, struct name *);
struct name *delname(struct name *, char *);
struct name *elide(struct name *);
struct name *extract(char *, int);
struct grouphead;
struct name *gexpand(struct name *, struct grouphead *, int, int);
struct name *nalloc(char *, int);
struct header;
struct name *outof(struct name *, FILE *, struct header *);
struct name *put(struct name *, struct name *);
struct name *tailof(struct name *);
struct name *usermap(struct name *);
FILE	*Fdopen(int, char *);
FILE	*Fopen(char *, char *);
FILE	*Popen(char *, char *);
FILE	*collect(struct header *, int);
char	*copy(char *, char *);
char	*copyin(char *, char **);
char	*detract(struct name *, int);
char	*expand(char *);
char	*getdeadletter(void);
char	*getname(uid_t);
struct message;
char	*hfield(char *, struct message *);
FILE	*infix(struct header *, FILE *);
char	*ishfield(char *, char *, char *);
char	*name1(struct message *, int);
char	*nameof(struct message *, int);
char	*nextword(char *, char *);
char	*readtty(char *, char *);
char 	*reedit(char *);
FILE	*run_editor(FILE *, off_t, int, int);
char	*salloc(int);
char	*savestr(char *);
FILE	*setinput(struct message *);
char	*skin(char *);
char	*skip_comment(char *);
char	*snarf(char *, int *);
char	*username(void);
char	*value(char *);
char	*vcopy(char *);
char	*yankword(char *, char *);
int	 Fclose(FILE *);
int	 More(void *);
int	 Pclose(FILE *);
int	 Respond(void *);
int	 Type(void *);
int	 _Respond(int *);
int	 _respond(int *);
void	 alter(char *);
int	 alternates(void *);
void	 announce(void);
int	 append(struct message *, FILE *);
int	 argcount(char **);
void	 assign(char *, char *);
int	 bangexp(char *, size_t);
int	 blankline(char *);
int	 charcount(char *, int);
int	 check(int, int);
void	 clearnew(void);
void	 close_all_files(void);
int	 cmatch(char *, char *);
int	 collabort(void);
void	 commands(void);
int	 copycmd(void *);
int	 count(struct name *);
int	 deletecmd(void *);
int	 delm(int *);
int	 deltype(void *);
void	 demail(void);
void	 dointr(void);
int	 dosh(void *);
int	 echo(void *);
int	 edit1(int *, int);
int	 editor(void *);
int	 edstop(void);
int	 elsecmd(void *);
int	 endifcmd(void *);
int	 evalcol(int);
int	 execute(char *, int);
int	 exwrite(char *, FILE *, int);
void	 fail(char *, char *);
int	 file(void *);
struct grouphead *
	 findgroup(char *);
void	 findmail(char *, char *, int);
void	 fioint(int);
int	 first(int, int);
void	 fixhead(struct header *, struct name *);
void	 fmt(char *, struct name *, FILE *, int);
int	 folders(void *);
int	 forward(char *, FILE *, char *, int);
void	 free_child(pid_t);
int	 from(void *);
off_t	 fsize(FILE *);
int	 getfold(char *, int);
int	 gethfield(FILE *, char *, int, char **);
int	 gethfromtty(struct header *, int);
int	 getmsglist(char *, int *, int);
int	 getrawlist(char *, char **, int);
uid_t	 getuserid(char *);
int	 grabh(struct header *, int);
int	 group(void *);
int	 hash(char *);
void	 hdrint(int);
int	 headers(void *);
int	 help(void *);
void	 holdsigs(void);
int	 ifcmd(void *);
int	 igfield(void *);
struct ignoretab;
int	 ignore1(char **, struct ignoretab *, char *);
int	 ignoresig(int, struct sigaction *, sigset_t *);
int	 igshow(struct ignoretab *, char *);
void	 intr(int);
int	 inc(void *);
int	 incfile(void);
int	 isdate(char *);
int	 isdir(char *);
int	 isfileaddr(char *);
int	 ishead(char *);
int	 isign(char *, struct ignoretab *);
int	 isprefix(char *, char *);
size_t	 istrlcpy(char *, const char *, size_t);
const struct cmd *
	 lex(char *);
void	 load(char *);
struct var *
	 lookup(char *);
int	 mail(struct name *, struct name *, struct name *, struct name *,
	       char *, char *);
void	 mail1(struct header *, int);
void	 makemessage(FILE *, int);
void	 mark(int);
int	 markall(char *, int);
int	 marknew(void *);
int	 matchsender(char *, int);
int	 matchsubj(char *, int);
int	 mboxit(void *);
int	 member(char *, struct ignoretab *);
void	 mesedit(FILE *, int);
void	 mespipe(FILE *, char *);
int	 messize(void *);
int	 metamess(int, int);
int	 more(void *);
int	 newfileinfo(int);
int	 next(void *);
int	 null(void *);
struct headline;
void	 parse(char *, struct headline *, char *);
int	 pcmdlist(void *);
int	 pdot(void *);
int	 pipeit(void *, void *);
void	 prepare_child(sigset_t *, int, int);
int	 preserve(void *);
void	 prettyprint(struct name *);
void	 printgroup(char *);
void	 printhead(int);
int	 puthead(struct header *, FILE *, int);
int	 putline(FILE *, char *, int);
int	 pversion(void *);
int	 quit(void);
int	 quitcmd(void *);
int	 readline(FILE *, char *, int, int *);
void	 register_file(FILE *, int, pid_t);
void	 regret(int);
void	 relsesigs(void);
int	 respond(void *);
int	 retfield(void *);
int	 rexit(void *);
int	 rm(char *);
int	 run_command(char *cmd, sigset_t *nset, int infd, int outfd, ...);
int	 save(void *);
int	 save1(char *, int, char *, struct ignoretab *);
void	 savedeadletter(FILE *);
int	 saveigfield(void *);
int	 savemail(char *, FILE *);
int	 saveretfield(void *);
int	 scan(char **);
void	 scaninit(void);
int	 schdir(void *);
int	 screensize(void);
int	 scroll(void *);
void	 sendint(int);
int	 sendmessage(struct message *, FILE *, struct ignoretab *, char *);
int	 sendmail(void *);
int	 set(void *);
int	 setfile(char *);
void	 setmsize(int);
void	 setptr(FILE *, off_t);
void	 setscreensize(void);
int	 shell(void *);
void	 sigchild(int);
void	 sort(char **);
int	 source(void *);
int	 spool_lock(void);
int	 spool_unlock(void);
void	 spreserve(void);
void	 sreset(void);
pid_t	 start_command(char *cmd, sigset_t *nset, int infd, int outfd, ...);
pid_t	 start_commandv(char *, sigset_t *, int, int, __va_list);
int	 statusput(struct message *, FILE *, char *);
void	 stop(int);
int	 stouch(void *);
int	 swrite(void *);
void	 tinit(void);
int	 top(void *);
void	 touch(struct message *);
void	 ttyint(int);
void	 ttystop(int);
int	 type(void *);
int	 type1(int *, char *, int, int);
int	 undeletecmd(void *);
void	 unmark(int);
char	**unpack(struct name *, struct name *);
int	 unread(void *);
void	 unregister_file(FILE *);
int	 unset(void *);
int	 unstack(void);
void	 vfree(char *);
int	 visual(void *);
int	 wait_child(pid_t);
int	 wait_command(int);
int	 writeback(FILE *);

extern char *__progname;
extern char *tmpdir;
extern const struct cmd *com; /* command we are running */
