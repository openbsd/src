/* Help functions for MicroGnuEmacs 2 */

#include "def.h"

#ifndef NO_HELP
#include "kbd.h"
#include "key.h"
#ifndef NO_MACRO
#include "macro.h"
#endif
extern int rescan();

/*
 * Read a key from the keyboard, and look it
 * up in the keymap.  Display the name of the function
 * currently bound to the key.
 */
/*ARGSUSED*/
desckey(f, n)
{
    register KEYMAP *curmap;
    register PF funct;
    register char *pep;
    char prompt[80];
    int c;
    int m;
    int i;

#ifndef NO_MACRO
    if(inmacro) return TRUE;		/* ignore inside keyboard macro */
#endif
    (VOID) strcpy(prompt, "Describe key briefly: ");
    pep = prompt + strlen(prompt);
    key.k_count = 0;
    m = curbp->b_nmodes;
    curmap = curbp->b_modes[m]->p_map;
    for(;;) {
	for(;;) {
	    ewprintf("%s", prompt);
	    pep[-1] = ' ';
	    pep = keyname(pep, key.k_chars[key.k_count++] = c = getkey(FALSE));
	    if((funct = doscan(curmap, c)) != prefix) break;
	    *pep++ = '-';
	    *pep = '\0';
	    curmap = ele->k_prefmap;
	}
	if(funct != rescan) break;
	if(ISUPPER(key.k_chars[key.k_count-1])) {
	    funct = doscan(curmap, TOLOWER(key.k_chars[key.k_count-1]));
	    if(funct == prefix) {
		*pep++ = '-';
		*pep = '\0';
		curmap = ele->k_prefmap;
		continue;
	    }
	    if(funct != rescan) break;
	}
nextmode:
	if(--m < 0) break;
	curmap = curbp->b_modes[m]->p_map;
	for(i=0; i < key.k_count; i++) {
	    funct = doscan(curmap, key.k_chars[i]);
	    if(funct != prefix) {
		if(i == key.k_count - 1 && funct != rescan) goto found;
		funct = rescan;
		goto nextmode;
	    }
	    curmap = ele->k_prefmap;
	}
	*pep++ = '-';
	*pep = '\0';
    }
found:
    if(funct == rescan) ewprintf("%k is not bound to any function");
    else if((pep = function_name(funct)) != NULL)
	    ewprintf("%k runs the command %s", pep);
    else    ewprintf("%k is bound to an unnamed function");
    return TRUE;
}

/*
 * This function creates a table, listing all
 * of the command keys and their current bindings, and stores
 * the table in the *help* pop-up buffer.  This
 * lets MicroGnuEMACS produce it's own wall chart.
 */
static BUFFER	*bp;
static char buf[80];	/* used by showall and findbind */

/*ARGSUSED*/
wallchart(f, n)
{
	int m;
	static char locbind[80] = "Local keybindings for mode ";
	static int showall();

	bp = bfind("*help*", TRUE);
	if (bclear(bp) != TRUE) return FALSE;	/* Clear it out.	*/
	for(m=curbp->b_nmodes; m > 0; m--) {
	    (VOID) strcpy(&locbind[27], curbp->b_modes[m]->p_name);
	    (VOID) strcat(&locbind[27], ":");
	    if((addline(bp, locbind) == FALSE) ||
		(showall(buf, curbp->b_modes[m]->p_map) == FALSE) ||
		(addline(bp, "") == FALSE)) return FALSE;
	}
	if((addline(bp, "Global bindings:") == FALSE) ||
	    (showall(buf, map_table[0].p_map) == FALSE)) return FALSE;
	return popbuftop(bp);
}

static	int showall(ind, map)
char	*ind;
KEYMAP	*map;
{
	register MAP_ELEMENT *ele;
	register int i;
	PF	functp;
	char	*cp;
	char	*cp2;
	int	last;

	if(addline(bp, "") == FALSE) return FALSE;
	last = -1;
	for(ele = &map->map_element[0]; ele < &map->map_element[map->map_num] ; ele++) {
	    if(map->map_default != rescan && ++last < ele->k_base) {
		cp = keyname(ind, last);
		if(last < ele->k_base - 1) {
		    (VOID) strcpy(cp, " .. ");
		    cp = keyname(cp + 4, ele->k_base - 1);
		}
		do { *cp++ = ' '; } while(cp < &buf[16]);
		(VOID) strcpy(cp, function_name(map->map_default));
		if(addline(bp, buf) == FALSE) return FALSE;
	    }
	    last = ele->k_num;
	    for(i=ele->k_base; i <= last; i++) {
		functp = ele->k_funcp[i - ele->k_base];
		if(functp != rescan) {
		    if(functp != prefix) cp2 = function_name(functp);
		    else cp2 = map_name(ele->k_prefmap);
		    if(cp2 != NULL) {
			cp = keyname(ind, i);
			do { *cp++ = ' '; } while(cp < &buf[16]);
			(VOID) strcpy(cp, cp2);
			if (addline(bp, buf) == FALSE) return FALSE;
		    }
		}
	    }
	}
	for(ele = &map->map_element[0]; ele < &map->map_element[map->map_num]; ele++) {
	    if(ele->k_prefmap != NULL) {
		for(i = ele->k_base; ele->k_funcp[i - ele->k_base] != prefix; i++) {
		    if(i >= ele->k_num)  /* damaged map */
			return FALSE;
		}
		cp = keyname(ind, i);
		*cp++ = ' ';
		if(showall(cp, ele->k_prefmap) == FALSE) return FALSE;
	    }
	}
	return TRUE;
}

help_help(f, n)
int f, n;
{
    KEYMAP *kp;
    PF	funct;

    if((kp = name_map("help")) == NULL) return FALSE;
    ewprintf("a b c: ");
    do {
	funct = doscan(kp, getkey(FALSE));
    } while(funct==NULL || funct==help_help);
#ifndef NO_MACRO
    if(macrodef && macrocount < MAXMACRO) macro[macrocount-1].m_funct = funct;
#endif
    return (*funct)(f, n);
}

static char buf2[128];
static char *buf2p;

/*ARGSUSED*/
apropos_command(f, n)
int f, n;
{
    register char *cp1, *cp2;
    char string[32];
    FUNCTNAMES *fnp;
    BUFFER *bp;
    static VOID findbind();

    if(eread("apropos: ", string, sizeof(string), EFNEW) == ABORT) return ABORT;
	/* FALSE means we got a 0 character string, which is fine */
    bp = bfind("*help*", TRUE);
    if(bclear(bp) == FALSE) return FALSE;
    for(fnp = &functnames[0]; fnp < &functnames[nfunct]; fnp++) {
	for(cp1 = fnp->n_name; *cp1; cp1++) {
	    cp2 = string;
	    while(*cp2 && *cp1 == *cp2)
		cp1++, cp2++;
	    if(!*cp2) {
		(VOID) strcpy(buf2, fnp->n_name);
		buf2p = &buf2[strlen(buf2)];
		findbind(fnp->n_funct, buf, map_table[0].p_map);
		if(addline(bp, buf2) == FALSE) return FALSE;
		break;
	    } else cp1 -= cp2 - string;
	}
    }
    return popbuftop(bp);
}

static VOID findbind(funct, ind, map)
PF funct;
char *ind;
KEYMAP	*map;
{
    register MAP_ELEMENT *ele;
    register int i;
    char	*cp;
    int		last;
    static VOID	bindfound();

    last = -1;
    for(ele = &map->map_element[0]; ele < &map->map_element[map->map_num]; ele++) {
	if(map->map_default == funct && ++last < ele->k_base) {
	    cp = keyname(ind, last);
	    if(last < ele->k_base - 1) {
		(VOID) strcpy(cp, " .. ");
		(VOID) keyname(cp + 4, ele->k_base - 1);
	    }
	    bindfound();
	}
	last = ele->k_num;
	for(i=ele->k_base; i <= last; i++) {
	    if(funct == ele->k_funcp[i - ele->k_base]) {
		if(funct == prefix) {
		    cp = map_name(ele->k_prefmap);
		    if(!cp || strncmp(cp, buf2, strlen(cp)) != 0) continue;
		}
		(VOID) keyname(ind, i);
		bindfound();
	    }
	}
    }
    for(ele = &map->map_element[0]; ele < &map->map_element[map->map_num]; ele++) {
	if(ele->k_prefmap != NULL) {
	    for(i = ele->k_base; ele->k_funcp[i - ele->k_base] != prefix; i++) {
		if(i >= ele->k_num) return; /* damaged */
	    }
	    cp = keyname(ind, i);
	    *cp++ = ' ';
	    findbind(funct, cp, ele->k_prefmap);
	}
    }
}

static VOID bindfound() {
    if(buf2p < &buf2[32]) {
	do { *buf2p++ = ' '; } while(buf2p < &buf2[32]);
    } else {
	*buf2p++ = ',';
	*buf2p++ = ' ';
    }
    (VOID) strcpy(buf2p, buf);
    buf2p += strlen(buf);
}
#endif
