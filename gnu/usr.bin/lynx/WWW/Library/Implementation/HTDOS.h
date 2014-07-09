/*
 * $LynxId: HTDOS.h,v 1.14 2009/09/09 00:16:06 tom Exp $
 *
 * DOS specific routines
 */

#ifndef HTDOS_H
#define HTDOS_H

#ifndef HTUTILS_H
#include <HTUtils.h>
#endif /* HTUTILS_H */

/* PUBLIC                                                       HTDOS_wwwName()
 *              CONVERTS DOS Name into WWW Name
 * ON ENTRY:
 *	dosname		DOS file specification (NO NODE)
 *
 * ON EXIT:
 *	returns		WWW file specification
 *
 */
const char *HTDOS_wwwName(const char *dosname);

/*
 * Converts Unix slashes to DOS
 */
char *HTDOS_slashes(char *path);

/* PUBLIC                                                       HTDOS_name()
 *              CONVERTS WWW name into a DOS name
 * ON ENTRY:
 *	wwwname		WWW file name
 *
 * ON EXIT:
 *	returns		DOS file specification
 *
 * Bug:	Returns pointer to static -- non-reentrant
 */
char *HTDOS_name(const char *wwwname);

#ifdef WIN_EX
char *HTDOS_short_name(const char *fn);

#else
#define HTDOS_short_name(fn)  fn
#endif

#ifdef DJGPP
/*
 * Poll tcp/ip lib and yield to DPMI-host while nothing in
 * keyboard buffer (head = tail) (simpler than kbhit).
 */
void djgpp_idle_loop(void);
#endif
#endif /*  HTDOS_H */
