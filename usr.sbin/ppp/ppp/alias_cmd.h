/*-
 * The code in this file was written by Eivind Eklund <perhaps@yes.no>,
 * who places it in the public domain without restriction.
 *
 *	$Id: alias_cmd.h,v 1.2 1999/02/06 03:22:30 brian Exp $
 */

struct cmdargs;

extern int alias_RedirectPort(struct cmdargs const *);
extern int alias_RedirectAddr(struct cmdargs const *);
