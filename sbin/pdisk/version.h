/*
 * version.h - version number for pdisk program
 *
 * Written by Eryk Vershen (eryk@apple.com)
 */

/*
 * Copyright 1997 by Apple Computer, Inc.
 *              All Rights Reserved
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies and
 * that both the copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * APPLE COMPUTER DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE.
 *
 * IN NO EVENT SHALL APPLE COMPUTER BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifndef __version__
#define __version__


/*
 * Defines
 */
/*
 *	TO ADJUST THE VERSION - change the following six macros.
 *
 * A version is of the form:	N.M{.X}{yZ}
 *
 * 	N is two digits indicating the major version
 *	M is a single digit indicating relative revision
 *	X is a single digit indicating a bug fix revision
 *	y is a character from the set [dab] indicating stage (dev,alpha,beta)
 *	Z is two digits indicating the delta within the stage
 *
 * Note that within the 'vers' resource all these fields end up
 * comprising a four byte unsigned integer with the property that any later
 * version will be represented by a larger number.
 */

#define	VERSION	"0.7a3"
#define RELEASE_DATE "18 February 1998"

#define	kVersionMajor	0x00		/* ie. N has two BCD digits */
#define	kVersionMinor	0x7		/* ie. M has a single BCD digit */
#define kVersionBugFix	0x0		/* ie. X has a single BCD digit */
#define	kVersionStage	alpha		/* ie. y is one of the set - */
					/*    {development,alpha,beta,final}
					 * also, release is a synonym for final
					 */
#define	kVersionDelta	0x03		/* ie. Z has two BCD digits */


/*
 * Types
 */


/*
 * Global Constants
 */


/*
 * Global Variables
 */


/*
 * Forward declarations
 */

#endif /* __version__ */
