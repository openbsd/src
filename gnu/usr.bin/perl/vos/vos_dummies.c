/*  +++begin copyright+++ *******************************************  */
/*                                                                     */
/*  COPYRIGHT (c) 1997, 1998, 1999, 2000 Stratus Computer (DE), Inc.   */
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

#include <stdio.h>
#include <string.h>
#include <sys/types.h>

extern void s$stop_program (char_varying (256) *command_line, 
                            short int          *error_code);
extern void s$write_code (char_varying     *record_buffer, 
                          short int        *error_code);
extern int vos_call_debug ();

#pragma page
static void bomb (char *p_name)
{
char_varying(256)   msgvs;

     strcpy_vstr_nstr (&msgvs, "FATAL ERROR: Call to unimplemented function '");
     strcat_vstr_nstr (&msgvs, p_name);
     strcat_vstr_nstr (&msgvs, "'. Entering debugger.");
     s$write_code (&msgvs, &0);

     strcpy_vstr_nstr (&msgvs, "Please capture the output of the 'trace' request and mail it to Paul_Green@stratus.com.");
     s$write_code (&msgvs, &0);

     vos_call_debug ();

     strcpy_vstr_nstr (&msgvs, "Return from debugger. Stopping program. Sorry but this error is unrecoverable.");
     s$write_code (&msgvs, &0);
     s$stop_program (&"", &1);
}

extern int dup (int _fildes)
{
     bomb ("dup");
}

extern int do_aspawn ()
{
     bomb ("do_aspawn");
}

extern int do_spawn ()
{
     bomb ("do_spawn");
}

extern pid_t fork (void)
{
     bomb ("fork");
}

extern void Perl_dump_mstats (char *s)
{
     bomb ("Perl_dump_mstats");
}

extern int Perl_get_mstats (struct perl_mstats *buf, int buflen, int level)
{
     bomb ("Perl_get_mstats");
}

extern pid_t waitpid (pid_t pid, int *stat_loc, int options)
{

     bomb ("waitpid");
}

