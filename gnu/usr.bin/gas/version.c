#if (__STDC__ == 1) || defined(const)
const
#endif

/* DO NOT PUT COMMENTS ABOUT CHANGES IN THIS FILE.
   
   This file exists only to define `version_string'.
   
   Log changes in ChangeLog.  The easiest way to do this is with
   the Emacs command `add-change-log-entry'.  If you don't use Emacs,
   add entries of the form:
   
   Thu Jan  1 00:00:00 1970  Dennis Ritchie  (dmr at alice)
   
   universe.c (temporal_reality): Began Time.
   */

#ifndef lint
static char rcsid[] = "$Id: version.c,v 1.1.1.1 1995/10/18 08:39:00 deraadt Exp $";
#endif

char version_string[] = "GNU assembler version 1.92.3, NetBSD $Revision: 1.1.1.1 $\n";

#ifdef HO_VMS
dummy3()
{
}
#endif

/* end of version.c */
