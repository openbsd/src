/*	$OpenBSD: extern.h,v 1.1 1998/09/15 05:12:31 pjanzen Exp $	*/
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
void mailbill __P((void));

/* config.c */

/* create.c */
void makeplayer __P((void));
void newcavelevel __P((int));
void makemaze __P((int));
void eat __P((int, int));
int cannedlevel __P((int));
void treasureroom __P((int));
void troom __P((int, int, int, int, int, int));
void makeobject __P((int));
void fillmroom __P((int, int, int));
void froom __P((int, int, int));
int fillmonst __P((int));
void sethp __P((int));
void checkgen __P((void));

/* data.c */

/* diag.c */
void diag __P((void));
int dcount __P((int));
void diagdrawscreen __P((void));
int savegame __P((char *));
void restoregame __P((char *));
void greedy __P((void));
void fsorry __P((void));
void fcheat __P((void));

/* display.c */
void bottomline __P((void));
void bottomhp __P((void));
void bottomspell __P((void));
void bottomdo __P((void));
void bot_linex __P((void));
void bottomgold __P((void));
void bot_hpx __P((void));
void bot_spellx __P((void));
void botside __P((void));
void draws __P((int, int, int, int));
void drawscreen __P((void));
void showcell __P((int, int));
void show1cell __P((int, int));
void showplayer __P((void));
int moveplayer __P((int));
void seemagic __P((int));
void seepage __P((void));

/* fortune.c */
char *fortune __P((void));

/* global.c */
void raiselevel __P((void));
void loselevel __P((void));
void raiseexperience __P((long));
void loseexperience __P((long));
void losehp __P((int));
void losemhp __P((int));
void raisehp __P((int));
void raisemhp __P((int));
void raisespells __P((int));
void raisemspells __P((int));
void losespells __P((int));
void losemspells __P((int));
int makemonst __P((int));
void positionplayer __P((void));
void recalc __P((void));
void quit __P((void));
void more __P((void));
int take __P((int, int));
int drop_object __P((int));
void enchantarmor __P((void));
void enchweapon __P((void));
int pocketfull __P((void));
int nearbymonst __P((void));
int stealsomething __P((void));
int emptyhanded __P((void));
void creategem __P((void));
void adjustcvalues __P((int, int));
void gettokstr __P((char *));
int getpassword __P((void));
int getyn __P((void));
int packweight __P((void));
int rnd __P((int));
int rund __P((int));

/* help.c */
void help __P((void));
void welcome __P((void));
void retcont __P((void));
int openhelp __P((void));

/* io.c */
void setupvt100 __P((void));
void clearvt100 __P((void));
int lgetchar __P((void));
void scbr __P((void));
void sncbr __P((void));
void newgame __P((void));
void lprintf __P((const char *, ...));
void lprint __P((long));
void lwrite __P((char *, int));
long lgetc __P((void));
long lrint __P((void));
void lrfill __P((char *, int));
char *lgetw __P((void));
char *lgetl __P((void));
int lcreat __P((char *));
int lopen __P((char *));
int lappend __P((char *));
void lrclose __P((void));
void lwclose __P((void));
void lprcat __P((char *));
void cursor __P((int, int));
void cursors __P((void));
void init_term __P((void));
void cl_line __P((int, int));
void cl_up __P((int, int));
void cl_dn __P((int, int));
void lstandout __P((char *));
void set_score_output __P((void));
void lflush __P((void));
int xputchar __P((int));
void flush_buf __P((void));
char *tmcapcnv __P((char *, char *));
void lbeep __P((void));

/* main.c */
int main __P((int, char **));
void showstr __P((void));
void qshowstr __P((void));
void t_setup __P((int));
void t_endup __P((int));
void showwear __P((void));
void showwield __P((void));
void showread __P((void));
void showeat __P((void));
void showquaff __P((void));
void show1 __P((int, char *[]));
void show3 __P((int));
void randmonst __P((void));
void parse __P((void));
void parse2 __P((void));
void run __P((int));
void wield __P((void));
void ydhi __P((int));
void ycwi __P((int));
void wear __P((void));
void dropobj __P((void));
void readscr __P((void));
void eatcookie __P((void));
void quaff __P((void));
int whatitem __P((char *));
unsigned long readnum __P((long));
void szero __P((char *));

/* monster.c */
void createmonster __P((int));
int cgood __P((int, int, int, int));
void createitem __P((int, int));
void cast __P((void));
void speldamage __P((int));
void loseint __P((void));
int isconfuse __P((void));
int nospell __P((int, int));
int fullhit __P((int));
void direct __P((int, int, char *, int));
void godirect __P((int, int, char *, int, int));
void ifblind __P((int, int));
void tdirect __P((int));
void omnidirect __P((int, int, char *));
int vxy __P((int *, int *));
void dirpoly __P((int));
void hitmonster __P((int, int));
int hitm __P((int, int, int));
void hitplayer __P((int, int));
void dropsomething __P((int));
void dropgold __P((int));
void something __P((int));
int newobject __P((int, int *));
int spattack __P((int, int, int));
void checkloss __P((int));
int annihilate __P((void));
int newsphere __P((int, int, int, int));
int rmsphere __P((int, int));
void sphboom __P((int, int));
void genmonst __P((void));

/* moreobj.c */
void oaltar __P((void));
void othrone __P((int));
void odeadthrone __P((void));
void ochest __P((void));
void ofountain __P((void));
void fntchange __P((int));

/* movem.c */
void movemonst __P((void));
void movemt __P((int, int));
void mmove __P((int, int, int, int));
void movsphere __P((void));

/* nap.c */
void nap __P((int));

/* object.c */
void lookforobject __P((void));
void finditem __P((int));
void ostairs __P((int));
void oteleport __P((int));
void opotion __P((int));
void quaffpotion __P((int));
void oscroll __P((int));
void adjusttime __P((long));
void read_scroll __P((int));
void oorb __P((void));
void opit __P((void));
void obottomless __P((void));
void oelevator __P((int));
void ostatue __P((void));
void omirror __P((void));
void obook __P((void));
void readbook __P((int));
void ocookie __P((void));
void ogold __P((int));
void ohome __P((void));
void iopts __P((void));
void ignore __P((void));

/* regen.c */
void regen __P((void));

/* savelev.c */
void savelevel __P((void));
void getlevel __P((void));

/* scores.c */
int readboard __P((void));
int writeboard __P((void));
int makeboard __P((void));
int hashewon __P((void));
long paytaxes __P((long));
int winshou __P((void));
int shou __P((int));
void showscores __P((void));
void showallscores __P((void));
int sortboard __P((void));
void newscore __P((long, char *, int, int));
void new1sub __P((long, int, char *, long));
void new2sub __P((long, int, char *, int));
void died __P((int));
void diedsub __P((int));
void diedlog __P((void));
int getplid __P((char *));

/* signal.c */
void sigsetup __P((void));

/* store.c */
void dnd_2hed __P((void));
void dnd_hed __P((void));
void dndstore __P((void));
void sch_hed __P((void));
void oschool __P((void));
void obank __P((void));
void obank2 __P((void));
void ointerest __P((void));
void obanksub __P((void));
void appraise __P((int));
void otradepost __P((void));
void cnsitm __P((void));
void olrs __P((void));

/* tok.c */
int yylex __P((void));
void flushall __P((void));
void sethard __P((int));
void readopts __P((void));
