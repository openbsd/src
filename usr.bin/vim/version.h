/*	$OpenBSD: version.h,v 1.3 1996/10/14 03:55:34 downsj Exp $	*/
/* vi:set ts=4 sw=4:
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 */

/*
 * This file is currently only used for the Win32 version.
 * Should probably generate all of this from the Makefile or in a separate
 * little C program that reads a small file; e.g., version.dat: 
 *    major=3
 *    minor=29
 *    build=101
 *    patchlevel=0
 *    date=1996 May 23
 */


#define VIM_VERSION_MAJOR			   4
#define VIM_VERSION_MAJOR_STR		  "4"

#define VIM_VERSION_MINOR			   5
#define VIM_VERSION_MINOR_STR		  "5"

#define VIM_VERSION_BUILD			   1
#define VIM_VERSION_BUILD_STR		  "1"

#define VIM_VERSION_PATCHLEVEL		   0
#define VIM_VERSION_PATCHLEVEL_STR	  "0"
