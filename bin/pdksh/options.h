/*	$OpenBSD: options.h,v 1.1.1.1 1996/08/14 06:19:11 downsj Exp $	*/

/*
 * Options configuration file for the PD ksh
 */

/* Define this to the path to use if the PATH environment variable is
 * not set (ie, either never set or explicitly unset with the unset
 * command).  A value without . in it is safest.
 * THIS DEFINE IS NOT USED if confstr() and _CS_PATH are available or
 * if <paths.h> defines _PATH_DEFPATH.
 */
#ifdef OS2
# define DEFAULT_PATH	"c:/usr/bin;c:/os2;/os2"	/* OS/2 only */
#else /* OS2 */ 
# define DEFAULT_PATH	"/bin:/usr/bin:/usr/ucb"	/* Unix */
#endif /* OS2 */


/* Define KSH to get KSH features; otherwise, you get a fairly basic
 * Bourne/POSIXish shell (undefining this results in EMACS, VI and
 * COMPLEX_HISTORY being undefined as well, regardless of their setting
 * here).
 */
#define KSH

/* Define EMACS if you want emacs command line editing compiled in (enabled
 * with "set -o emacs", or by setting the VISUAL or EDITOR variables to
 * something ending in emacs).
 */
#define	EMACS

/* Define VI if you want vi command line editing compiled in (enabled with
 * "set -o vi", or by setting the VISUAL or EDITOR variables to something
 * ending in vi).
 */
#define	VI

/* Define JOBS if you want job control compiled in.  This requires that your
 * system support process groups and reliable signal handling routines (it
 * will be automatically undefined if your system doesn't have them).
 */
#define	JOBS

/* Define BRACE_EXPAND if you want csh-like {} globbing compiled in and enabled
 * (can be disabled with "set +o braceexpand"; also disabled by "set -o posix",
 * but can be re-enabled with "set -o braceexpand").
 */
#define BRACE_EXPAND

/* Define COMPLEX_HISTORY if you want at&t ksh style history files (ie, file
 * is updated after each command is read; concurrent ksh's read each other's
 * commands, etc.). This option uses the mmap() and flock() functions - if
 * these aren't available, the option is automatically undefined.  If this
 * option is not defined, a simplier history mechanism which reads/saves the
 * history at startup/exit time, respectively, is used.  COMPLEX_HISTORY is
 * courtesy of Peter Collinson. 
 */
#undef COMPLEX_HISTORY

/* Define POSIXLY_CORRECT if you want POSIX behavior by default (otherwise,
 * posix behavior is only turned on if the environment variable POSIXLY_CORRECT
 * is present or by using "set -o posix"; it can be turned off with
 * "set +o posix").
 * See the POSIX Mode section in the man page for details on what this option
 * affects.
 * NOTE: posix mode is not compatable with some bourne sh/at&t ksh scripts.
 */
#undef POSIXLY_CORRECT

/* Define DEFAULT_ENV to be the name of the file (eg, "/etc/default.env") to
 * include if the ENV environment variable is not set when the shell starts
 * up.  This can be useful when used with rsh(1) which creates a non-login
 * shell (ie, profile not read) with an empty environment (ie, ENV not set).
 * Setting ENV to null disables the inclusion of DEFAULT_ENV.
 * NOTE: this is a non-standard feature (ie, at&t ksh has no default
 * environment) - undefining this disables the use of a default ENV file.
 */
#undef DEFAULT_ENV

/* Define SWTCH to handle SWITCH character, for use with shell layers (shl(1)).
 * This has not been tested for some time.
 */
#undef	SWTCH

/* SILLY: The name says it all - compile game of life code into the emacs
 * command line editing code.
 */
#undef	SILLY
