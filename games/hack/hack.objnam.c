/*	$OpenBSD: hack.objnam.c,v 1.6 2003/04/06 18:50:37 deraadt Exp $	*/

/*
 * Copyright (c) 1985, Stichting Centrum voor Wiskunde en Informatica,
 * Amsterdam
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * - Neither the name of the Stichting Centrum voor Wiskunde en
 * Informatica, nor the names of its contributors may be used to endorse or
 * promote products derived from this software without specific prior
 * written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1982 Jay Fenlason <hack@gnu.org>
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef lint
static char rcsid[] = "$OpenBSD: hack.objnam.c,v 1.6 2003/04/06 18:50:37 deraadt Exp $";
#endif /* not lint */

#include	"hack.h"
#define	PREFIX	15
extern char *eos();
extern int bases[];

char *
strprepend(s,pref) register char *s, *pref; {
register int i = strlen(pref);
	if(i > PREFIX) {
		pline("WARNING: prefix too short.");
		return(s);
	}
	s -= i;
	(void) strncpy(s, pref, i);	/* do not copy trailing 0 */
	return(s);
}

char *
sitoa(a) int a; {
static char buf[13];
	snprintf(buf, sizeof buf, (a < 0) ? "%d" : "+%d", a);
	return(buf);
}

char *
typename(otyp)
register int otyp;
{
static char buf[BUFSZ];
register struct objclass *ocl = &objects[otyp];
register char *an = ocl->oc_name;
register char *dn = ocl->oc_descr;
register char *un = ocl->oc_uname;
register int nn = ocl->oc_name_known;
	switch(ocl->oc_olet) {
	case POTION_SYM:
		strlcpy(buf, "potion", sizeof buf);
		break;
	case SCROLL_SYM:
		strlcpy(buf, "scroll", sizeof buf);
		break;
	case WAND_SYM:
		strlcpy(buf, "wand", sizeof buf);
		break;
	case RING_SYM:
		strlcpy(buf, "ring", sizeof buf);
		break;
	default:
		if(nn) {
			strlcpy(buf, an, sizeof buf);
			if(otyp >= TURQUOISE && otyp <= JADE)
				strlcat(buf, " stone", sizeof buf);
			if(un)
				sprintf(eos(buf), " called %s", un);
			if(dn)
				sprintf(eos(buf), " (%s)", dn);
		} else {
			strlcpy(buf, dn ? dn : an, sizeof buf);
			if(ocl->oc_olet == GEM_SYM)
				strlcat(buf, " gem", sizeof buf);
			if(un)
				sprintf(eos(buf), " called %s", un);
		}
		return(buf);
	}
	/* here for ring/scroll/potion/wand */
	if(nn)
		sprintf(eos(buf), " of %s", an);
	if(un)
		sprintf(eos(buf), " called %s", un);
	if(dn)
		sprintf(eos(buf), " (%s)", dn);
	return(buf);
}

char *
xname(obj)
register struct obj *obj;
{
static char bufr[BUFSZ];
register char *buf = &(bufr[PREFIX]);	/* leave room for "17 -3 " */
register int nn = objects[obj->otyp].oc_name_known;
register char *an = objects[obj->otyp].oc_name;
register char *dn = objects[obj->otyp].oc_descr;
register char *un = objects[obj->otyp].oc_uname;
register int pl = (obj->quan != 1);
	if(!obj->dknown && !Blind) obj->dknown = 1; /* %% doesn't belong here */
	switch(obj->olet) {
	case AMULET_SYM:
		strcpy(buf, (obj->spe < 0 && obj->known)
			? "cheap plastic imitation of the " : "");
		strcat(buf,"Amulet of Yendor");
		break;
	case TOOL_SYM:
		if(!nn) {
			strcpy(buf, dn);
			break;
		}
		strcpy(buf,an);
		break;
	case FOOD_SYM:
		if(obj->otyp == DEAD_HOMUNCULUS && pl) {
			pl = 0;
			strcpy(buf, "dead homunculi");
			break;
		}
		/* fungis ? */
		/* fall into next case */
	case WEAPON_SYM:
		if(obj->otyp == WORM_TOOTH && pl) {
			pl = 0;
			strcpy(buf, "worm teeth");
			break;
		}
		if(obj->otyp == CRYSKNIFE && pl) {
			pl = 0;
			strcpy(buf, "crysknives");
			break;
		}
		/* fall into next case */
	case ARMOR_SYM:
	case CHAIN_SYM:
	case ROCK_SYM:
		strcpy(buf,an);
		break;
	case BALL_SYM:
		sprintf(buf, "%sheavy iron ball",
		  (obj->owt > objects[obj->otyp].oc_weight) ? "very " : "");
		break;
	case POTION_SYM:
		if(nn || un || !obj->dknown) {
			strcpy(buf, "potion");
			if(pl) {
				pl = 0;
				strcat(buf, "s");
			}
			if(!obj->dknown) break;
			if(un) {
				strcat(buf, " called ");
				strcat(buf, un);
			} else {
				strcat(buf, " of ");
				strcat(buf, an);
			}
		} else {
			strcpy(buf, dn);
			strcat(buf, " potion");
		}
		break;
	case SCROLL_SYM:
		strcpy(buf, "scroll");
		if(pl) {
			pl = 0;
			strcat(buf, "s");
		}
		if(!obj->dknown) break;
		if(nn) {
			strcat(buf, " of ");
			strcat(buf, an);
		} else if(un) {
			strcat(buf, " called ");
			strcat(buf, un);
		} else {
			strcat(buf, " labeled ");
			strcat(buf, dn);
		}
		break;
	case WAND_SYM:
		if(!obj->dknown)
			sprintf(buf, "wand");
		else if(nn)
			sprintf(buf, "wand of %s", an);
		else if(un)
			sprintf(buf, "wand called %s", un);
		else
			sprintf(buf, "%s wand", dn);
		break;
	case RING_SYM:
		if(!obj->dknown)
			sprintf(buf, "ring");
		else if(nn)
			sprintf(buf, "ring of %s", an);
		else if(un)
			sprintf(buf, "ring called %s", un);
		else
			sprintf(buf, "%s ring", dn);
		break;
	case GEM_SYM:
		if(!obj->dknown) {
			strcpy(buf, "gem");
			break;
		}
		if(!nn) {
			sprintf(buf, "%s gem", dn);
			break;
		}
		strcpy(buf, an);
		if(obj->otyp >= TURQUOISE && obj->otyp <= JADE)
			strcat(buf, " stone");
		break;
	default:
		sprintf(buf,"glorkum %c (0%o) %u %d",
			obj->olet,obj->olet,obj->otyp,obj->spe);
	}
	if(pl) {
		register char *p;

		for(p = buf; *p; p++) {
			if(!strncmp(" of ", p, 4)) {
				/* pieces of, cloves of, lumps of */
				register int c1, c2 = 's';

				do {
					c1 = c2; c2 = *p; *p++ = c1;
				} while(c1);
				goto nopl;
			}
		}
		p = eos(buf)-1;
		if(*p == 's' || *p == 'z' || *p == 'x' ||
		    (*p == 'h' && p[-1] == 's'))
			strcat(buf, "es");	/* boxes */
		else if(*p == 'y' && !strchr(vowels, p[-1]))
			strcpy(p, "ies");	/* rubies, zruties */
		else
			strcat(buf, "s");
	}
nopl:
	if(obj->onamelth) {
		strcat(buf, " named ");
		strcat(buf, ONAME(obj));
	}
	return(buf);
}

char *
doname(obj)
register struct obj *obj;
{
char prefix[PREFIX];
register char *bp = xname(obj);
	if(obj->quan != 1)
		snprintf(prefix, sizeof prefix, "%u ", obj->quan);
	else
		strlcpy(prefix, "a ", sizeof prefix);
	switch(obj->olet) {
	case AMULET_SYM:
		if(strncmp(bp, "cheap ", 6))
			strlcpy(prefix, "the ", sizeof prefix);
		break;
	case ARMOR_SYM:
		if(obj->owornmask & W_ARMOR)
			strcat(bp, " (being worn)");
		/* fall into next case */
	case WEAPON_SYM:
		if(obj->known) {
			strlcat(prefix, sitoa(obj->spe), sizeof prefix);
			strlcat(prefix, " ", sizeof prefix);
		}
		break;
	case WAND_SYM:
		if(obj->known)
			sprintf(eos(bp), " (%d)", obj->spe);
		break;
	case RING_SYM:
		if(obj->owornmask & W_RINGR) strcat(bp, " (on right hand)");
		if(obj->owornmask & W_RINGL) strcat(bp, " (on left hand)");
		if(obj->known && (objects[obj->otyp].bits & SPEC)) {
			strlcat(prefix, sitoa(obj->spe), sizeof prefix);
			strlcat(prefix, " ", sizeof prefix);
		}
		break;
	}
	if(obj->owornmask & W_WEP)
		strcat(bp, " (weapon in hand)");
	if(obj->unpaid)
		strcat(bp, " (unpaid)");
	if(!strcmp(prefix, "a ") && strchr(vowels, *bp))
		strlcpy(prefix, "an ", sizeof prefix);
	bp = strprepend(bp, prefix);
	return(bp);
}

/* used only in hack.fight.c (thitu) */
setan(str,buf)
register char *str,*buf;
{
	if(strchr(vowels,*str))
		sprintf(buf, "an %s", str);
	else
		sprintf(buf, "a %s", str);
}

char *
aobjnam(otmp,verb) register struct obj *otmp; register char *verb; {
register char *bp = xname(otmp);
char prefix[PREFIX];
	if(otmp->quan != 1) {
		snprintf(prefix, sizeof prefix, "%u ", otmp->quan);
		bp = strprepend(bp, prefix);
	}

	if(verb) {
		/* verb is given in plural (i.e., without trailing s) */
		strcat(bp, " ");
		if(otmp->quan != 1)
			strcat(bp, verb);
		else if(!strcmp(verb, "are"))
			strcat(bp, "is");
		else {
			strcat(bp, verb);
			strcat(bp, "s");
		}
	}
	return(bp);
}

char *
Doname(obj)
register struct obj *obj;
{
	register char *s = doname(obj);

	if('a' <= *s && *s <= 'z') *s -= ('a' - 'A');
	return(s);
}

char *wrp[] = { "wand", "ring", "potion", "scroll", "gem" };
char wrpsym[] = { WAND_SYM, RING_SYM, POTION_SYM, SCROLL_SYM, GEM_SYM };

struct obj *
readobjnam(bp) register char *bp; {
register char *p;
register int i;
int cnt, spe, spesgn, typ, heavy;
char let;
char *un, *dn, *an;
/* int the = 0; char *oname = 0; */
	cnt = spe = spesgn = typ = heavy = 0;
	let = 0;
	an = dn = un = 0;
	for(p = bp; *p; p++)
		if('A' <= *p && *p <= 'Z') *p += 'a'-'A';
	if(!strncmp(bp, "the ", 4)){
/*		the = 1; */
		bp += 4;
	} else if(!strncmp(bp, "an ", 3)){
		cnt = 1;
		bp += 3;
	} else if(!strncmp(bp, "a ", 2)){
		cnt = 1;
		bp += 2;
	}
	if(!cnt && digit(*bp)){
		cnt = atoi(bp);
		while(digit(*bp)) bp++;
		while(*bp == ' ') bp++;
	}
	if(!cnt) cnt = 1;		/* %% what with "gems" etc. ? */

	if(*bp == '+' || *bp == '-'){
		spesgn = (*bp++ == '+') ? 1 : -1;
		spe = atoi(bp);
		while(digit(*bp)) bp++;
		while(*bp == ' ') bp++;
	} else {
		p = strrchr(bp, '(');
		if(p) {
			if(p > bp && p[-1] == ' ') p[-1] = 0;
			else *p = 0;
			p++;
			spe = atoi(p);
			while(digit(*p)) p++;
			if(strcmp(p, ")")) spe = 0;
			else spesgn = 1;
		}
	}
	/* now we have the actual name, as delivered by xname, say
		green potions called whisky
		scrolls labeled "QWERTY"
		egg
		dead zruties
		fortune cookies
		very heavy iron ball named hoei
		wand of wishing
		elven cloak
	*/
	for(p = bp; *p; p++) if(!strncmp(p, " named ", 7)) {
		*p = 0;
/*		oname = p+7; */
	}
	for(p = bp; *p; p++) if(!strncmp(p, " called ", 8)) {
		*p = 0;
		un = p+8;
	}
	for(p = bp; *p; p++) if(!strncmp(p, " labeled ", 9)) {
		*p = 0;
		dn = p+9;
	}

	/* first change to singular if necessary */
	if(cnt != 1) {
		/* find "cloves of garlic", "worthless pieces of blue glass" */
		for(p = bp; *p; p++) if(!strncmp(p, "s of ", 5)){
			while(*p = p[1]) p++;
			goto sing;
		}
		/* remove -s or -es (boxes) or -ies (rubies, zruties) */
		p = eos(bp);
		if(p[-1] == 's') {
			if(p[-2] == 'e') {
				if(p[-3] == 'i') {
					if(!strcmp(p-7, "cookies"))
						goto mins;
					strcpy(p-3, "y");
					goto sing;
				}

				/* note: cloves / knives from clove / knife */
				if(!strcmp(p-6, "knives")) {
					strcpy(p-3, "fe");
					goto sing;
				}

				/* note: nurses, axes but boxes */
				if(!strcmp(p-5, "boxes")) {
					p[-2] = 0;
					goto sing;
				}
			}
		mins:
			p[-1] = 0;
		} else {
			if(!strcmp(p-9, "homunculi")) {
				strcpy(p-1, "us"); /* !! makes string longer */
				goto sing;
			}
			if(!strcmp(p-5, "teeth")) {
				strcpy(p-5, "tooth");
				goto sing;
			}
			/* here we cannot find the plural suffix */
		}
	}
sing:
	if(!strcmp(bp, "amulet of yendor")) {
		typ = AMULET_OF_YENDOR;
		goto typfnd;
	}
	p = eos(bp);
	if(!strcmp(p-5, " mail")){	/* Note: ring mail is not a ring ! */
		let = ARMOR_SYM;
		an = bp;
		goto srch;
	}
	for(i = 0; i < sizeof(wrpsym); i++) {
		register int j = strlen(wrp[i]);
		if(!strncmp(bp, wrp[i], j)){
			let = wrpsym[i];
			bp += j;
			if(!strncmp(bp, " of ", 4)) an = bp+4;
			/* else if(*bp) ?? */
			goto srch;
		}
		if(!strcmp(p-j, wrp[i])){
			let = wrpsym[i];
			p -= j;
			*p = 0;
			if(p[-1] == ' ') p[-1] = 0;
			dn = bp;
			goto srch;
		}
	}
	if(!strcmp(p-6, " stone")){
		p[-6] = 0;
		let = GEM_SYM;
		an = bp;
		goto srch;
	}
	if(!strcmp(bp, "very heavy iron ball")){
		heavy = 1;
		typ = HEAVY_IRON_BALL;
		goto typfnd;
	}
	an = bp;
srch:
	if(!an && !dn && !un)
		goto any;
	i = 1;
	if(let) i = bases[letindex(let)];
	while(i <= NROFOBJECTS && (!let || objects[i].oc_olet == let)){
		register char *zn = objects[i].oc_name;

		if(!zn) goto nxti;
		if(an && strcmp(an, zn))
			goto nxti;
		if(dn && (!(zn = objects[i].oc_descr) || strcmp(dn, zn)))
			goto nxti;
		if(un && (!(zn = objects[i].oc_uname) || strcmp(un, zn)))
			goto nxti;
		typ = i;
		goto typfnd;
	nxti:
		i++;
	}
any:
	if(!let) let = wrpsym[rn2(sizeof(wrpsym))];
	typ = probtype(let);
typfnd:
	{ register struct obj *otmp;
	  extern struct obj *mksobj();
	let = objects[typ].oc_olet;
	otmp = mksobj(typ);
	if(heavy)
		otmp->owt += 15;
	if(cnt > 0 && strchr("%?!*)", let) &&
		(cnt < 4 || (let == WEAPON_SYM && typ <= ROCK && cnt < 20)))
		otmp->quan = cnt;

	if(spe > 3 && spe > otmp->spe)
		spe = 0;
	else if(let == WAND_SYM)
		spe = otmp->spe;
	if(spe == 3 && u.uluck < 0)
		spesgn = -1;
	if(let != WAND_SYM && spesgn == -1)
		spe = -spe;
	if(let == BALL_SYM)
		spe = 0;
	else if(let == AMULET_SYM)
		spe = -1;
	else if(typ == WAN_WISHING && rn2(10))
		spe = (rn2(10) ? -1 : 0);
	otmp->spe = spe;

	if(spesgn == -1)
		otmp->cursed = 1;

	return(otmp);
    }
}
