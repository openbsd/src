/*	$OpenBSD: kprop.h,v 1.4 1998/02/25 15:50:37 art Exp $	*/
/* $KTH: kprop.h,v 1.5 1997/02/07 21:39:52 assar Exp $ */

/*
 * This source code is no longer held under any constraint of USA
 * `cryptographic laws' since it was exported legally.  The cryptographic
 * functions were removed from the code and a "Bones" distribution was
 * made.  A Commodity Jurisdiction Request #012-94 was filed with the
 * USA State Department, who handed it to the Commerce department.  The
 * code was determined to fall under General License GTDA under ECCN 5D96G,
 * and hence exportable.  The cryptographic interfaces were re-added by Eric
 * Young, and then KTH proceeded to maintain the code in the free world.
 */

/*-
 * Copyright (C) 1987 by the Massachusetts Institute of Technology
 *
 * Export of this software from the United States of America is assumed
 * to require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 *
 */

#define KPROP_SERVICE_NAME "rcmd"
#define KPROP_SRVTAB "/etc/kerberosIV/srvtab"
#define TGT_SERVICE_NAME "krbtgt"
#define KPROP_PROT_VERSION_LEN 8
#define KPROP_PROT_VERSION "kprop01"
#define KPROP_TRANSFER_PRIVATE 1
#define KPROP_TRANSFER_SAFE 2
#define KPROP_TRANSFER_CLEAR 3
#define KPROP_BUFSIZ 32768
#define KPROP_PORT 754
