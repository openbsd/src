/*	$NetBSD: version.c,v 1.28 1995/12/23 17:21:23 perry Exp $	*/

/*
 *	NOTE ANY CHANGES YOU MAKE TO THE BOOTBLOCKS HERE.
 *
 *	1.27 -> 1.28
 *		fix gets to use real timeout instead of loop and do
 *		a little cleanup, and add some prototypes. A lot more
 *		needs to be done here. (perry)
 *
 *	1.26 -> 1.27
 *		size reduction and code cleanup. (mycroft)
 *
 *	1.25 -> 1.26
 *		size reduction and code cleanup. (mycroft)
 *
 *	1.24 -> 1.25
 *		add support for serial consoles. (mycroft)
 *
 *	1.23 -> 1.24
 *		change RCS ID format.  NOW NEED TO UPDATE STRING BELOW
 *		BY HAND.
 *
 *	1.22 -> 1.23, 1.21.2.2
 *		fix problem with empty symbol tables. (mycroft)
 *
 *	1.21 -> 1.22, 1.21.2.1
 *		fix compatibility with pre-4.4 file systems. (mycroft)
 *
 *	1.20 -> 1.21
 *		update for 4.4-Lite file system includes and macros (cgd)
 *
 *	1.19 -> 1.20
 *		display options in std. format, more changes for size (cgd)
 *
 *	1.18 -> 1.19
 *		add a '-r' option, to specify RB_DFLTROOT (cgd)
 *
 *	1.17 -> 1.18
 *		removed some more code we don't need for BDB. (mycroft)
 *
 *	1.16 -> 1.17
 *		removed with prejudice the extra buffer for xread(), changes
 *		to make the code smaller, and general cleanup. (mycroft)
 *
 *	1.15 -> 1.16
 *		reduce BUFSIZE to 4k, because that's fixed the
 *		boot problems, for some. (cgd)
 *
 *	1.14 -> 1.15
 *		seperated 'version' out from boot.c (cgd)
 *
 *	1.1 -> 1.14
 *		look in boot.c revision logs
 */

char *version = "1.28";
