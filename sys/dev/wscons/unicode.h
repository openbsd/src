/* $OpenBSD: unicode.h,v 1.1 2000/05/16 23:49:10 mickey Exp $ */
/* $NetBSD: unicode.h,v 1.1 1999/02/20 18:20:02 drochner Exp $ */

/*
 * some private character definitions for stuff not found
 * in the Unicode database, for communication between
 * terminal emulation and graphics driver
 */

#define _e000U 0xe000 /* mirrored question mark? */
#define _e001U 0xe001 /* scan 1 */
#define _e002U 0xe002 /* scan 3 */
#define _e003U 0xe003 /* scan 5 */
#define _e004U 0xe004 /* scan 7 */
#define _e005U 0xe005 /* scan 9 */
#define _e006U 0xe006 /* N/L control */
#define _e007U 0xe007 /* bracelefttp */
#define _e008U 0xe008 /* braceleftbt */
#define _e009U 0xe009 /* bracerighttp */
#define _e00aU 0xe00a /* bracerighrbt */
#define _e00bU 0xe00b /* braceleftmid */
#define _e00cU 0xe00c /* bracerightmid */
#define _e00dU 0xe00d /* inverted angle? */
#define _e00eU 0xe00e /* angle? */
#define _e00fU 0xe00f /* mirrored not sign? */
