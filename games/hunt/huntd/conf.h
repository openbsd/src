/*	$OpenBSD: conf.h,v 1.1 1999/01/29 07:30:34 d Exp $	*/

/* Configuration option variables for the server: */

extern int conf_random;		/* enable dispersion doors */
extern int conf_reflect;	/* enable generation of reflection walls */
extern int conf_monitor;	/* enable monitors */
extern int conf_ooze;		/* enable slime shots */
extern int conf_fly;		/* enable flight */
extern int conf_volcano;	/* enable volcanoes */
extern int conf_drone;		/* enable drone */
extern int conf_boots;		/* enable boots */
extern int conf_scan;		/* enable scanning */
extern int conf_cloak;		/* enable cloaking */
extern int conf_logerr;		/* errors to stderr as well as syslog(8) */

extern int conf_scoredecay;
extern int conf_maxremove;	/* Maximum number of holes in the maze wall */
extern int conf_linger;		/* Seconds to keep game open with no players */

extern int conf_flytime;	/* max time flying */
extern int conf_flystep;	/* max displacement each flying time unit */
extern int conf_volcano_max;	/* max size of volcano */
extern int conf_ptrip_face;	/* chace of tripping a grenade on pickup,  */
extern int conf_ptrip_back;	/*   - when backing onto it */
extern int conf_ptrip_side;	/*   - when walking sideways into it */
extern int conf_prandom;	/* percentage of time dispersion doors appear */
extern int conf_preflect;	/* percentage of time reflection walls appear */
extern int conf_pshot_coll;	/* percent chance of shots colliding */
extern int conf_pgren_coll;	/* percent chance of grenades colliding */
extern int conf_pgren_catch;	/* facing player chance of catching grenade */
extern int conf_pmiss;		/* chance of bullet missing player */
extern int conf_pdroneabsorb;	/* chance of absorbing a drone */
extern int conf_fall_frac;	/* divisor of damage used for fall damage */

extern int conf_bulspd;		/* speed of bullets */
extern int conf_ishots;		/* initial ammo for player */
extern int conf_nshots;		/* ammo boost for all when new player joins */
extern int conf_maxncshot;	/* max number of simultaneous shots per player*/
extern int conf_maxdam;		/* the initial shield for each player */
extern int conf_mindam;		/* minimum damage from one unit of ammo */
extern int conf_stabdam;	/* damage from stabbing */
extern int conf_killgain;	/* shield gained from killing someone */
extern int conf_slimefactor;	/* charge multiplier for slime */
extern int conf_slimespeed;	/* speed of slime */ 
extern int conf_lavaspeed;	/* speed of volcano lava */
extern int conf_cloaklen;	/* duration of a cloak */
extern int conf_scanlen;	/* duration of a scan */
extern int conf_mindshot;	/* minium shot class needed to make a drone */

void config __P((void));
