/*	$Id: ntpleaps.h,v 1.1 2002/07/27 08:46:51 jakob Exp $	*/

/*
 * Copyright (c) 2002 by Thorsten "mirabile" Glaser <x86@ePOST.de>
 *
 * Permission is hereby granted to any person obtaining a copy of this work
 * to deal in the work, without restrictions, including unlimited rights to
 * use, copy, modify, merge, publish, distribute, sublicense or sell copies
 * of the work, and to permit persons to whom the work is furnished to also
 * do so, as long as due credit is given to the original author and contri-
 * butors, and the following disclaimer is kept in all substantial portions
 * of the work or accompanying documentation:
 *
 * This work is provided "AS IS", without warranty of any kind, neither ex-
 * press nor implied, including, but not limited to, the warranties of mer-
 * chantability, fitness for particular purposes and noninfringement. In NO
 * event shall the author and contributors be liable for any claim, damages
 * and such, whether in contract, strict liability or otherwise, arising in
 * any way out of this work, even if advised of the possibility of such.
 */

/* Leap second support for SNTP clients
 * This header file and its corresponding C file provide generic
 * ability for NTP or SNTP clients to correctly handle leap seconds
 * by reading them from an always existing file and subtracting the
 * leap seconds from the NTP return value before setting the posix
 * clock. This is fairly portable between operating systems and may
 * be used for patching other ntp clients, too. The tzfile used is:
 * /usr/share/zoneinfo/right/UTC which is available on any unix-like
 * platform with the Olson tz library, which is necessary to get real
 * leap second zoneinfo files and userland support anyways.
 */

#ifndef _NTPLEAPS_H
#define _NTPLEAPS_H

/* Offset between struct timeval.tv_sec and a tai64_t */
#define	NTPLEAPS_OFFSET	(4611686018427387914ULL)

/* Initializes the leap second table. Does not need to be called
 * before usage of the subtract funtion, but calls ntpleaps_read.
 * returns 0 on success, -1 on error (displays a warning on stderr)
 */
int ntpleaps_init(void);

/* Re-reads the leap second table, thus consuming quite much time.
 * Ought to be called from within daemons at least once a month to
 * ensure the in-memory table is always up-to-date.
 * returns 0 on success, -1 on error (leap seconds will not be available)
 */
int ntpleaps_read(void);

/* Subtracts leap seconds from the given value (converts NTP time
 * to posix clock tick time.
 * returns 0 on success, -1 on error (time is unchanged), 1 on leap second
 */
int ntpleaps_sub(u_int64_t *);

#endif
