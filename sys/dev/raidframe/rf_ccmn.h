/*	$OpenBSD: rf_ccmn.h,v 1.1 1999/01/11 14:29:01 niklas Exp $	*/
/*	$NetBSD: rf_ccmn.h,v 1.1 1998/11/13 04:20:26 oster Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Mark Holland
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/* rf_ccmn.h
 * header file that declares the ccmn routines, and includes
 * the files needed to use them.
 */

/* :  
 * Log: rf_ccmn.h,v 
 * Revision 1.4  1996/07/18 22:57:14  jimz
 * port simulator to AIX
 *
 * Revision 1.3  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.2  1995/12/01  15:16:45  root
 * added copyright info
 *
 */

#ifndef _RF__RF_CCMN_H_
#define _RF__RF_CCMN_H_

#ifdef __osf__
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/param.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <io/common/iotypes.h>
#include <io/cam/cam_debug.h>
#include <io/cam/cam.h>
#include <io/cam/dec_cam.h>
#include <io/cam/uagt.h>
#include <io/cam/scsi_all.h>
#include <io/cam/scsi_direct.h>

#ifdef KERNEL
#include <sys/conf.h>
#include <sys/mtio.h>
#include <io/common/devio.h>
#include <io/common/devdriver.h>
#include <io/cam/scsi_status.h>
#include <io/cam/pdrv.h>
#include <io/common/pt.h>
#include <sys/disklabel.h>
#include <io/cam/cam_disk.h>
#include <io/cam/ccfg.h>

extern void ccmn_init();
extern long ccmn_open_unit();
extern void ccmn_close_unit();
extern u_long ccmn_send_ccb();
extern void ccmn_rem_ccb();
extern void ccmn_abort_que();
extern void ccmn_term_que();
extern CCB_HEADER *ccmn_get_ccb();
extern void ccmn_rel_ccb();
extern CCB_SCSIIO *ccmn_io_ccb_bld();
extern CCB_GETDEV *ccmn_gdev_ccb_bld();
extern CCB_SETDEV *ccmn_sdev_ccb_bld();
extern CCB_SETASYNC *ccmn_sasy_ccb_bld();
extern CCB_RELSIM *ccmn_rsq_ccb_bld();
extern CCB_PATHINQ *ccmn_pinq_ccb_bld();
extern CCB_ABORT *ccmn_abort_ccb_bld();
extern CCB_TERMIO *ccmn_term_ccb_bld();
extern CCB_RESETDEV *ccmn_bdr_ccb_bld();
extern CCB_RESETBUS *ccmn_br_ccb_bld();
extern CCB_SCSIIO *ccmn_tur();
extern CCB_SCSIIO *ccmn_mode_select();
extern u_long ccmn_ccb_status();
extern struct buf *ccmn_get_bp();
extern void ccmn_rel_bp();
extern u_char *ccmn_get_dbuf();
extern void ccmn_rel_dbuf();

extern struct device *camdinfo[];
extern struct controller *camminfo[];
extern PDRV_UNIT_ELEM pdrv_unit_table[];

#endif  /* KERNEL */
#endif /* __osf__ */

#endif /* !_RF__RF_CCMN_H_ */
