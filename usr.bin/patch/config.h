/*	$OpenBSD: config.h,v 1.6 2003/07/21 14:32:21 deraadt Exp $	*/

/*
 * config.h This file was produced by running the config.h.SH script, which
 * gets its values from config.sh, which is generally produced by running
 * Configure.
 * 
 * Feel free to modify any of this as the need arises.  Note, however, that
 * running config.h.SH again will wipe out any changes you've made. For a
 * more permanent change edit config.sh and rerun config.h.SH.
 * 
 */


/*
 * CPPSTDIN: This symbol contains the first part of the string which will
 * invoke the C preprocessor on the standard input and produce to standard
 * output.	 Typical value of "cc -E" or "/lib/cpp".
 */
/*
 * CPPMINUS: This symbol contains the second part of the string which will
 * invoke the C preprocessor on the standard input and produce to standard
 * output.  This symbol will have the value "-" if CPPSTDIN needs a minus to
 * specify standard input, otherwise the value is "".
 */
#define CPPSTDIN "/usr/bin/cpp"
#define CPPMINUS ""

/*
 * DIRHEADER: This definition indicates which directory library header to
 * use.
 */
#define DIRENT
