/*	$OpenBSD: top.local.h,v 1.3 2002/07/15 17:20:36 deraadt Exp $	*/

/*
 *  Top - a top users display for Berkeley Unix
 *
 *  Definitions for things that might vary between installations.
 */

/*
 *  The space command forces an immediate update.  Sometimes, on loaded
 *  systems, this update will take a significant period of time (because all
 *  the output is buffered).  So, if the short-term load average is above
 *  "LoadMax", then top will put the cursor home immediately after the space
 *  is pressed before the next update is attempted.  This serves as a visual
 *  acknowledgement of the command.  On Suns, "LoadMax" will get multiplied by
 *  "FSCALE" before being compared to avenrun[0].  Therefore, "LoadMax"
 *  should always be specified as a floating point number.
 */
#ifndef LoadMax
#define LoadMax  5.0
#endif

/*
 *  "Table_size" defines the size of the hash tables used to map uid to
 *  username.  The number of users in /etc/passwd CANNOT be greater than
 *  this number.  If the error message "table overflow: too many users"
 *  is printed by top, then "Table_size" needs to be increased.  Things will
 *  work best if the number is a prime number that is about twice the number
 *  of lines in /etc/passwd.
 */
#ifndef Table_size
#define Table_size	503
#endif

/*
 *  "Nominal_TOPN" is used as the default TOPN when Default_TOPN is Infinity
 *  and the output is a dumb terminal.  If we didn't do this, then
 *  installations who use a default TOPN of Infinity will get every
 *  process in the system when running top on a dumb terminal (or redirected
 *  to a file).  Note that Nominal_TOPN is a default:  it can still be
 *  overridden on the command line, even with the value "infinity".
 */
#ifndef Nominal_TOPN
#define Nominal_TOPN	18
#endif

#ifndef Default_TOPN
#define Default_TOPN	-1
#endif

#ifndef Default_DELAY
#define Default_DELAY	5
#endif

/*
 *  Top users/processes display for Unix
 *  Version 3
 *
 * Copyright (c) 1984, 1989, William LeFebvre, Rice University
 * Copyright (c) 1989, 1990, 1992, William LeFebvre, Northwestern University
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS EMPLOYER BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  If the local system's getpwnam interface uses random access to retrieve
 *  a record (i.e.: 4.3 systems, Sun "yellow pages"), then defining
 *  RANDOM_PW will take advantage of that fact.  If RANDOM_PW is defined,
 *  then getpwnam is used and the result is cached.  If not, then getpwent
 *  is used to read and cache the password entries sequentially until the
 *  desired one is found.
 *
 *  We initially set RANDOM_PW to something which is controllable by the
 *  Configure script.  Then if its value is 0, we undef it.
 */

#define RANDOM_PW	1
#if RANDOM_PW == 0
#undef RANDOM_PW
#endif
