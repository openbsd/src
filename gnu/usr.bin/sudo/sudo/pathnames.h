/*	$OpenBSD: pathnames.h,v 1.7 1998/11/21 01:34:53 millert Exp $	*/

/* pathnames.h.  Generated automatically by configure.  */
/*
 *  CU sudo version 1.5.7
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 1, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Please send bugs, changes, problems to sudo-bugs@courtesan.com
 *
 *  $From: pathnames.h.in,v 1.34 1998/09/20 23:10:04 millert Exp $
 */

/*
 *  Pathnames to programs and files used by sudo.
 */

#ifdef HAVE_PATHS_H
#include <paths.h>
#endif /* HAVE_PATHS_H */

#ifndef _PATH_DEV
#define _PATH_DEV		"/dev/"
#endif /* _PATH_DEV */

#ifndef _PATH_TTY
#define _PATH_TTY		"/dev/tty"
#endif /* _PATH_TTY */

/*
 * NOTE: _PATH_SUDO_SUDOERS is usually overriden by the Makefile.
 */
#ifndef _PATH_SUDO_SUDOERS
#define _PATH_SUDO_SUDOERS	"/etc/sudoers"
#endif /* _PATH_SUDO_SUDOERS */

/*
 * NOTE:  _PATH_SUDO_STMP is usually overriden by the Makefile.
 *        _PATH_SUDO_STMP *MUST* be on the same partition
 *        as _PATH_SUDO_SUDOERS!
 */
#ifndef _PATH_SUDO_STMP
#define _PATH_SUDO_STMP		"/etc/stmp"
#endif /* _PATH_SUDO_STMP */

/*
 * The following paths are controlled via the configure script.
 */

/*
 * Where to put the timestamp files.  Defaults to /var/run/sudo if
 * /var/run exists, else /tmp/.odus.
 */
#ifndef _PATH_SUDO_TIMEDIR
#define _PATH_SUDO_TIMEDIR "/var/run/sudo"
#endif /* _PATH_SUDO_TIMEDIR */

/*
 * Where to put the sudo log file when logging to a file.  Defaults to
 * /var/log/sudo.log if /var/log exists, else /var/adm/sudo.log.
 */
#ifndef _PATH_SUDO_LOGFILE
#define _PATH_SUDO_LOGFILE "/var/log/sudo.log"
#endif /* _PATH_SUDO_LOGFILE */

#ifndef _PATH_SENDMAIL
#define _PATH_SENDMAIL "/usr/sbin/sendmail"
#endif /* _PATH_SENDMAIL */

#ifndef _PATH_VI
#define _PATH_VI "/usr/bin/vi"
#endif /* _PATH_VI */

#ifndef _PATH_MV
#define _PATH_MV "/bin/mv"
#endif /* _PATH_MV */

#ifndef _PATH_BSHELL
#define _PATH_BSHELL "/bin/sh"
#endif /* _PATH_BSHELL */
