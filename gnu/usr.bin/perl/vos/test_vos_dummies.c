/*  +++begin copyright+++ *******************************************  */
/*                                                                     */
/*  COPYRIGHT (c) 1997, 1998, 2000 Stratus Computer (DE), Inc.         */
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

/* This program tests the code in vos_dummies.c to make sure it
   works as expected.  */

extern int dup (int _fildes);

int t_dummies ()
{
int  fildes;

     fildes=3;
     dup (fildes);
}
