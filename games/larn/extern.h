/*	$OpenBSD: extern.h,v 1.2 2002/02/16 21:27:10 millert Exp $	*/
/*	$NetBSD: extern.h,v 1.1 1997/10/18 20:03:17 christos Exp $	*/

/*
 * Copyright (c) 1997 Christos Zoulas.  All rights reserved.
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
 *	This product includes software developed by Christos Zoulas.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

/* bill.c */
void mailbill(void);

/* config.c */

/* create.c */
void makeplayer(void);
void newcavelevel(int);
void makemaze(int);
void eat(int, int);
int cannedlevel(int);
void treasureroom(int);
void troom(int, int, int, int, int, int);
void makeobject(int);
void fillmroom(int, int, int);
void froom(int, int, int);
int fillmonst(int);
void sethp(int);
void checkgen(void);

/* data.c */

/* diag.c */
void diag(void);
int dcount(int);
void diagdrawscreen(void);
int savegame(char *);
void restoregame(char *);
void greedy(void);
void fsorry(void);
void fcheat(void);

/* display.c */
void bottomline(void);
void bottomhp(void);
void bottomspell(void);
void bottomdo(void);
void bot_linex(void);
void bottomgold(void);
void bot_hpx(void);
void bot_spellx(void);
void botside(void);
void draws(int, int, int, int);
void drawscreen(void);
void showcell(int, int);
void show1cell(int, int);
void showplayer(void);
int moveplayer(int);
void seemagic(int);
void seepage(void);

/* fortune.c */
char *fortune(void);

/* global.c */
void raiselevel(void);
void loselevel(void);
void raiseexperience(long);
void loseexperience(long);
void losehp(int);
void losemhp(int);
void raisehp(int);
void raisemhp(int);
void raisespells(int);
void raisemspells(int);
void losespells(int);
void losemspells(int);
int makemonst(int);
void positionplayer(void);
void recalc(void);
void quit(void);
void more(void);
int take(int, int);
int drop_object(int);
void enchantarmor(void);
void enchweapon(void);
int pocketfull(void);
int nearbymonst(void);
int stealsomething(void);
int emptyhanded(void);
void creategem(void);
void adjustcvalues(int, int);
void gettokstr(char *);
int getpassword(void);
int getyn(void);
int packweight(void);
int rnd(int);
int rund(int);

/* help.c */
void help(void);
void welcome(void);
void retcont(void);
int openhelp(void);

/* io.c */
void setupvt100(void);
void clearvt100(void);
int lgetchar(void);
void scbr(void);
void sncbr(void);
void newgame(void);
void lprintf(const char *, ...);
void lprint(long);
void lwrite(char *, int);
long lgetc(void);
long lrint(void);
void lrfill(char *, int);
char *lgetw(void);
char *lgetl(void);
int lcreat(char *);
int lopen(char *);
int lappend(char *);
void lrclose(void);
void lwclose(void);
void lprcat(char *);
void cursor(int, int);
void cursors(void);
void init_term(void);
void cl_line(int, int);
void cl_up(int, int);
void cl_dn(int, int);
void lstandout(char *);
void set_score_output(void);
void lflush(void);
int xputchar(int);
void flush_buf(void);
char *tmcapcnv(char *, char *);
void lbeep(void);

/* main.c */
int main(int, char **);
void showstr(void);
void qshowstr(void);
void t_setup(int);
void t_endup(int);
void showwear(void);
void showwield(void);
void showread(void);
void showeat(void);
void showquaff(void);
void show1(int, char *[]);
void show3(int);
void randmonst(void);
void parse(void);
void parse2(void);
void run(int);
void wield(void);
void ydhi(int);
void ycwi(int);
void wear(void);
void dropobj(void);
void readscr(void);
void eatcookie(void);
void quaff(void);
int whatitem(char *);
unsigned long readnum(long);
void szero(char *);

/* monster.c */
void createmonster(int);
int cgood(int, int, int, int);
void createitem(int, int);
void cast(void);
void speldamage(int);
void loseint(void);
int isconfuse(void);
int nospell(int, int);
int fullhit(int);
void direct(int, int, char *, int);
void godirect(int, int, char *, int, int);
void ifblind(int, int);
void tdirect(int);
void omnidirect(int, int, char *);
int vxy(int *, int *);
void dirpoly(int);
void hitmonster(int, int);
int hitm(int, int, int);
void hitplayer(int, int);
void dropsomething(int);
void dropgold(int);
void something(int);
int newobject(int, int *);
int spattack(int, int, int);
void checkloss(int);
int annihilate(void);
int newsphere(int, int, int, int);
int rmsphere(int, int);
void sphboom(int, int);
void genmonst(void);

/* moreobj.c */
void oaltar(void);
void othrone(int);
void odeadthrone(void);
void ochest(void);
void ofountain(void);
void fntchange(int);

/* movem.c */
void movemonst(void);
void movemt(int, int);
void mmove(int, int, int, int);
void movsphere(void);

/* nap.c */
void nap(int);

/* object.c */
void lookforobject(void);
void finditem(int);
void ostairs(int);
void oteleport(int);
void opotion(int);
void quaffpotion(int);
void oscroll(int);
void adjusttime(long);
void read_scroll(int);
void oorb(void);
void opit(void);
void obottomless(void);
void oelevator(int);
void ostatue(void);
void omirror(void);
void obook(void);
void readbook(int);
void ocookie(void);
void ogold(int);
void ohome(void);
void iopts(void);
void ignore(void);

/* regen.c */
void regen(void);

/* savelev.c */
void savelevel(void);
void getlevel(void);

/* scores.c */
int readboard(void);
int writeboard(void);
int makeboard(void);
int hashewon(void);
long paytaxes(long);
int winshou(void);
int shou(int);
void showscores(void);
void showallscores(void);
int sortboard(void);
void newscore(long, char *, int, int);
void new1sub(long, int, char *, long);
void new2sub(long, int, char *, int);
void died(int);
void diedsub(int);
void diedlog(void);
int getplid(char *);

/* signal.c */
void sigsetup(void);

/* store.c */
void dnd_2hed(void);
void dnd_hed(void);
void dndstore(void);
void sch_hed(void);
void oschool(void);
void obank(void);
void obank2(void);
void ointerest(void);
void obanksub(void);
void appraise(int);
void otradepost(void);
void cnsitm(void);
void olrs(void);

/* tok.c */
int yylex(void);
void flushall(void);
void sethard(int);
void readopts(void);
