/*  +++begin copyright+++ *******************************************  */
/*                                                                     */
/*  COPYRIGHT (c) 1999 Stratus Computer, Inc.                          */
/*                                                                     */
/*  This program is free software; you can redistribute it and/or      */
/*  modify it under the terms of either:                               */
/*                                                                     */
/*  a) the GNU General Public License as published by the Free         */
/*  Software Foundation; either version 1, or (at your option) any     */
/*  later version, or                                                  */
/*                                                                     */
/*  b) the "Artistic License" which comes with this Kit.               */
/*                                                                     */
/*  This program is distributed in the hope that it will be useful,    */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of     */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See either   */
/*  the GNU General Public License or the Artistic License for more    */
/*  details.                                                           */
/*                                                                     */
/*  You should have received a copy of the Artistic License with this  */
/*  Kit, in the file named "Artistic".  If not, you can get one from   */
/*  the Perl distribution.                                             */
/*                                                                     */
/*  You should also have received a copy of the GNU General Public     */
/*  License along with this program; if not, you can get one from      */
/*  the Perl distribution or else write to the Free Software           */
/*  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA          */
/*  02111-1307, USA.                                                   */
/*                                                                     */
/*  +++end copyright+++ *********************************************  */

#define _POSIX_C_SOURCE 199309L

/* Beginning of modification history */
/* Written 99-02-03 by Paul Green. */
/* End of modification history */

/* This short program soaks up the call to "accept" and
   transfers it to "_accept".  This is necessary because the VOS
   C compilers treat "accept" as a keyword unless the -Xc
   (strict ANSI option) has been specified.  This program must
   be compiled with -Xc.  Because "accept" is a keyword, the VOS
   OS TCP/IP product has renamed the usual TCP/IP "accept"
   function to "_accept".  */

extern int _accept (int a, struct sockaddr *b, int *c);

extern int accept (int a, struct sockaddr *b, int *c)
{
     return _accept (a, b, c);
}
