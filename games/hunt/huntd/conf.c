/*	$OpenBSD: conf.c,v 1.1 1999/01/29 07:30:34 d Exp $	*/
/* David Leonard <d@openbsd.org>, 1999. Public domain. */

#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>
#include <ctype.h>

#include "conf.h"

/* Configuration option variables for the server: */

int conf_random =	1;	/* enable dispersion doors */
int conf_reflect =	1;	/* enable generation of reflection walls */
int conf_monitor =	1;	/* enable monitors */
int conf_ooze =		1;	/* enable slime shots */
int conf_fly =		1;	/* enable flight */
int conf_volcano =	1;	/* enable volcanoes */
int conf_drone =	1;	/* enable drone */
int conf_boots =	1;	/* enable boots */
int conf_scan =		1;	/* enable scanning */
int conf_cloak =	1;	/* enable cloaking */
int conf_logerr =	1;	/* errors to stderr as well as syslog(8) */

int conf_scoredecay =	15;	/* nr deaths before nr kills begins to decay */
int conf_maxremove =	40;	/* Maximum number of holes in the maze wall */
int conf_linger =	90;	/* Seconds to keep game open with no players */

int conf_flytime =	20;	/* max time flying */
int conf_flystep =	5;	/* max displacement each flying time unit */
int conf_volcano_max =	50;	/* max size of volcano */
int conf_ptrip_face =	2;	/* chace of tripping a grenade on pickup,  */
int conf_ptrip_back =	95;	/*   - when backing onto it */
int conf_ptrip_side =	50;	/*   - when walking sideways into it */
int conf_prandom =	1;	/* percentage of time dispersion doors appear */
int conf_preflect =	1;	/* percentage of time reflection walls appear */
int conf_pshot_coll =	5;	/* percent chance of shots colliding */
int conf_pgren_coll =	10;	/* percent chance of grenades colliding */
int conf_pgren_catch =	10;	/* facing player chance of catching grenade */
int conf_pmiss =	5;	/* chance of bullet missing player */
int conf_pdroneabsorb =	1;	/* chance of absorbing a drone */
int conf_fall_frac =	5;	/* divisor of damage used for fall damage */

int conf_bulspd =	5;	/* speed of bullets */
int conf_ishots =	15;	/* initial ammo for player */
int conf_nshots =	5;	/* ammo boost for all when new player joins */
int conf_maxncshot =	2;	/* max number of simultaneous shots per player*/
int conf_maxdam =	10;	/* the initial shield for each player */
int conf_mindam =	5;	/* minimum damage from one unit of ammo */
int conf_stabdam =	2;	/* damage from stabbing */
int conf_killgain =	2;	/* shield gained from killing someone */
int conf_slimefactor =	3;	/* charge multiplier for slime */
int conf_slimespeed =	5;	/* speed of slime */
int conf_lavaspeed =	1;	/* speed of volcano lava */
int conf_cloaklen =	20;	/* duration of a cloak */
int conf_scanlen =	20;	/* duration of a scan */
int conf_mindshot =	2;	/* minium shot class needed to make a drone */

struct kwvar {
	char *	kw;
	int *	var;
};

static struct kwvar keywords[] = {
	{ "random",		&conf_random },
	{ "reflect",		&conf_reflect },
	{ "monitor",		&conf_monitor },
	{ "ooze",		&conf_ooze },
	{ "fly",		&conf_fly },
	{ "volcano",		&conf_volcano },
	{ "drone",		&conf_drone },
	{ "boots",		&conf_boots },
	{ "scan",		&conf_scan },
	{ "cloak",		&conf_cloak },
	{ "logerr",		&conf_logerr },
	{ "scoredecay",		&conf_scoredecay },
	{ "maxremove",		&conf_maxremove },
	{ "linger",		&conf_linger },

	{ "flytime",		&conf_flytime },
	{ "flystep",		&conf_flystep },
	{ "volcano_max",	&conf_volcano_max },
	{ "ptrip_face",		&conf_ptrip_face },
	{ "ptrip_back",		&conf_ptrip_back },
	{ "ptrip_side",		&conf_ptrip_side },
	{ "prandom",		&conf_prandom },
	{ "preflect",		&conf_preflect },
	{ "pshot_coll",		&conf_pshot_coll },
	{ "pgren_coll",		&conf_pgren_coll },
	{ "pgren_catch",	&conf_pgren_catch },
	{ "pmiss",		&conf_pmiss },
	{ "pdroneabsorb",	&conf_pdroneabsorb },
	{ "fall_frac",		&conf_fall_frac },

	{ "bulspd",		&conf_bulspd },
	{ "ishots",		&conf_ishots },
	{ "nshots",		&conf_nshots },
	{ "maxncshot",		&conf_maxncshot },
	{ "maxdam",		&conf_maxdam },
	{ "mindam",		&conf_mindam },
	{ "stabdam",		&conf_stabdam },
	{ "killgain",		&conf_killgain },
	{ "slimefactor",	&conf_slimefactor },
	{ "slimespeed",		&conf_slimespeed },
	{ "lavaspeed",		&conf_lavaspeed },
	{ "cloaklen",		&conf_cloaklen },
	{ "scanlen",		&conf_scanlen },
	{ "mindshot",		&conf_mindshot },

	{ NULL, NULL}
};

static void
load_config(f, fnm)
	FILE *	f;
	char *	fnm;
{
	char buf[BUFSIZ];
	char *p;
	char *word, *value;
	struct kwvar *kvp;
	int *varp;
	int *nextp;
	int line = 0;
	int len;
	int newval;

	static const char *delim = " \t\n\r\f";

	while ((p = fgetln(f, &len)) != NULL) {
		line++;
		if (p[len-1] == '\n')
			len--;
		if (len >= sizeof(buf))
			continue;
		(void)memcpy(buf, p, len);
		buf[len] = '\0';	/* code assumes newlines later on */
		p = buf;

		/* skip leading white */
		while (*p && isspace(*p))
			p++;
		/* allow blank lines and comment lines */
		if (*p == '\0' || *p == '#')
			continue;


		/* first word must match a keyword */
		varp = NULL;
		for (kvp = keywords; kvp->kw; kvp++) {
			int len;
			len = strlen(kvp->kw);

			if (strncmp(kvp->kw, p, len) != 0)
				continue;
			if (isspace(p[len]) || p[len] == '=')
				break;
		}

		if (kvp->kw == NULL) {
			fprintf(stderr, "%s:%d: unrecognised keyword\n", 
			    fnm, line);
			continue;
		}

		p += strlen(kvp->kw);

		/* skip whitespace */
		while (*p && isspace(*p))
			p++;

		if (*p++ != '=') {
			fprintf(stderr, "%s:%d: expected `='\n", fnm, line);
			continue;
		}

		/* skip whitespace */
		while (*p && isspace(*p))
			p++;

		/* expect a number */
		value = p;
		while (*p && isdigit(*p))
			p++;
		if (!(*p == '\0' || isspace(*p) || *p == '#') || value == p) {
			fprintf(stderr, "%s:%d: invalid value\n", 
			    fnm, line);
			continue;
		}
		*p = '\0';
		newval = atoi(value);

#ifdef DIAGNOSTIC
		if (newval != *kvp->var)
			printf("%s:%d: %s: %d -> %d\n", fnm, line, 
			    kvp->kw, *kvp->var, newval);
#endif

		*kvp->var = newval;
	}
}

/*
 * load various config file, allowing later ones to 
 * overwrite earlier values
 */
void
config()
{
	char *home;
	char nm[MAXNAMLEN + 1];
	static char *fnms[] = { 
		"/etc/hunt.conf"
		"%s/.hunt.conf", 
		".hunt.conf", 
		NULL
	};
	int fn;
	FILE *f;

	if ((home = getenv("HOME")) == NULL)
		home = "";

	for (fn = 0; fnms[fn]; fn++) {
		snprintf(nm, sizeof nm, fnms[fn], home);
		if (f = fopen(nm, "r")) {
			load_config(f, nm);
			fclose(f);
		}
	}
}
