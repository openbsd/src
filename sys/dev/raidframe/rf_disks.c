/*	$OpenBSD: rf_disks.c,v 1.1 1999/01/11 14:29:17 niklas Exp $	*/
/*	$NetBSD: rf_disks.c,v 1.2 1998/12/03 15:06:25 oster Exp $	*/
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

/***************************************************************
 * rf_disks.c -- code to perform operations on the actual disks
 ***************************************************************/

/* :  
 * Log: rf_disks.c,v 
 * Revision 1.32  1996/07/27 18:40:24  jimz
 * cleanup sweep
 *
 * Revision 1.31  1996/07/22  19:52:16  jimz
 * switched node params to RF_DagParam_t, a union of
 * a 64-bit int and a void *, for better portability
 * attempted hpux port, but failed partway through for
 * lack of a single C compiler capable of compiling all
 * source files
 *
 * Revision 1.30  1996/07/19  16:11:21  jimz
 * pass devname to DoReadCapacity
 *
 * Revision 1.29  1996/07/18  22:57:14  jimz
 * port simulator to AIX
 *
 * Revision 1.28  1996/07/10  22:28:38  jimz
 * get rid of obsolete row statuses (dead,degraded2)
 *
 * Revision 1.27  1996/06/10  12:06:14  jimz
 * don't do any SCSI op stuff in simulator at all
 *
 * Revision 1.26  1996/06/10  11:55:47  jimz
 * Straightened out some per-array/not-per-array distinctions, fixed
 * a couple bugs related to confusion. Added shutdown lists. Removed
 * layout shutdown function (now subsumed by shutdown lists).
 *
 * Revision 1.25  1996/06/09  02:36:46  jimz
 * lots of little crufty cleanup- fixup whitespace
 * issues, comment #ifdefs, improve typing in some
 * places (esp size-related)
 *
 * Revision 1.24  1996/06/07  21:33:04  jimz
 * begin using consistent types for sector numbers,
 * stripe numbers, row+col numbers, recon unit numbers
 *
 * Revision 1.23  1996/06/03  23:28:26  jimz
 * more bugfixes
 * check in tree to sync for IPDS runs with current bugfixes
 * there still may be a problem with threads in the script test
 * getting I/Os stuck- not trivially reproducible (runs ~50 times
 * in a row without getting stuck)
 *
 * Revision 1.22  1996/06/02  17:31:48  jimz
 * Moved a lot of global stuff into array structure, where it belongs.
 * Fixed up paritylogging, pss modules in this manner. Some general
 * code cleanup. Removed lots of dead code, some dead files.
 *
 * Revision 1.21  1996/05/30  23:22:16  jimz
 * bugfixes of serialization, timing problems
 * more cleanup
 *
 * Revision 1.20  1996/05/30  11:29:41  jimz
 * Numerous bug fixes. Stripe lock release code disagreed with the taking code
 * about when stripes should be locked (I made it consistent: no parity, no lock)
 * There was a lot of extra serialization of I/Os which I've removed- a lot of
 * it was to calculate values for the cache code, which is no longer with us.
 * More types, function, macro cleanup. Added code to properly quiesce the array
 * on shutdown. Made a lot of stuff array-specific which was (bogusly) general
 * before. Fixed memory allocation, freeing bugs.
 *
 * Revision 1.19  1996/05/27  18:56:37  jimz
 * more code cleanup
 * better typing
 * compiles in all 3 environments
 *
 * Revision 1.18  1996/05/24  22:17:04  jimz
 * continue code + namespace cleanup
 * typed a bunch of flags
 *
 * Revision 1.17  1996/05/24  01:59:45  jimz
 * another checkpoint in code cleanup for release
 * time to sync kernel tree
 *
 * Revision 1.16  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.15  1996/05/23  00:33:23  jimz
 * code cleanup: move all debug decls to rf_options.c, all extern
 * debug decls to rf_options.h, all debug vars preceded by rf_
 *
 * Revision 1.14  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.13  1996/05/02  14:57:43  jimz
 * initialize sectorMask
 *
 * Revision 1.12  1995/12/01  15:57:04  root
 * added copyright info
 *
 */

#include "rf_types.h"
#include "rf_raid.h"
#include "rf_alloclist.h"
#include "rf_utils.h"
#include "rf_configure.h"
#include "rf_general.h"
#if !defined(__NetBSD__) && !defined(__OpenBSD__)
#include "rf_camlayer.h"
#endif
#include "rf_options.h"
#include "rf_sys.h"

#if (defined(__NetBSD__) || defined(__OpenBSD__)) && defined(_KERNEL)
#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#ifdef __NETBSD__
#include <sys/vnode.h>
#endif

int raidlookup __P((char *, struct proc *p, struct vnode **));
#endif

#ifdef SIMULATE
static char disk_db_file_name[120], disk_type_name[120];
static double init_offset;
#endif /* SIMULATE  */

#define DPRINTF6(a,b,c,d,e,f) if (rf_diskDebug) printf(a,b,c,d,e,f)
#define DPRINTF7(a,b,c,d,e,f,g) if (rf_diskDebug) printf(a,b,c,d,e,f,g)

#include "rf_ccmn.h"

/****************************************************************************************
 *
 * initialize the disks comprising the array
 *
 * We want the spare disks to have regular row,col numbers so that we can easily
 * substitue a spare for a failed disk.  But, the driver code assumes throughout
 * that the array contains numRow by numCol _non-spare_ disks, so it's not clear
 * how to fit in the spares.  This is an unfortunate holdover from raidSim.  The
 * quick and dirty fix is to make row zero bigger than the rest, and put all the
 * spares in it.  This probably needs to get changed eventually.
 *
 ***************************************************************************************/
int rf_ConfigureDisks(
  RF_ShutdownList_t  **listp,
  RF_Raid_t           *raidPtr,
  RF_Config_t         *cfgPtr)
{
  RF_RaidDisk_t **disks;
  RF_SectorCount_t min_numblks = (RF_SectorCount_t)0x7FFFFFFFFFFFLL;
  RF_RowCol_t r, c;
  int bs, ret;
  unsigned i, count, foundone=0, numFailuresThisRow;
  RF_DiskOp_t *rdcap_op = NULL, *tur_op = NULL;
  int num_rows_done,num_cols_done;

#if (defined(__NetBSD__) || defined(__OpenBSD__)) && defined(_KERNEL)
	struct proc *proc = 0;  
#endif
#ifndef SIMULATE
#if !defined(__NetBSD__) && !defined(__OpenBSD__)
  ret = rf_SCSI_AllocReadCapacity(&rdcap_op);
  if (ret)
    goto fail;
  ret = rf_SCSI_AllocTUR(&tur_op);
  if (ret)
    goto fail;
#endif /* !__NetBSD__ && !__OpenBSD__ */
#endif /* !SIMULATE */

  num_rows_done = 0;
  num_cols_done = 0;


  RF_CallocAndAdd(disks, raidPtr->numRow, sizeof(RF_RaidDisk_t *), (RF_RaidDisk_t **), raidPtr->cleanupList);
  if (disks == NULL) {
    ret = ENOMEM;
    goto fail;
  }
  raidPtr->Disks = disks;

#if (defined(__NetBSD__) || defined(__OpenBSD__)) && defined(_KERNEL)

  proc = raidPtr->proc; /* Blah XXX */

  /* get space for the device-specific stuff... */
  RF_CallocAndAdd(raidPtr->raid_cinfo, raidPtr->numRow, 
		  sizeof(struct raidcinfo *), (struct raidcinfo **),
		  raidPtr->cleanupList);
  if (raidPtr->raid_cinfo == NULL) {
	  ret = ENOMEM;
	  goto fail;
  }
#endif

  for (r=0; r<raidPtr->numRow; r++) {
    numFailuresThisRow = 0;
    RF_CallocAndAdd(disks[r], raidPtr->numCol + ((r==0) ? raidPtr->numSpare : 0), sizeof(RF_RaidDisk_t), (RF_RaidDisk_t *), raidPtr->cleanupList);
    if (disks[r] == NULL) {
      ret = ENOMEM;
      goto fail;
    }

    /* get more space for device specific stuff.. */
    RF_CallocAndAdd(raidPtr->raid_cinfo[r], 
		    raidPtr->numCol + ((r==0) ? raidPtr->numSpare : 0), 
		    sizeof(struct raidcinfo), (struct raidcinfo *), 
		    raidPtr->cleanupList);
    if (raidPtr->raid_cinfo[r] == NULL) {
      ret = ENOMEM;
      goto fail;
    }


    for (c=0; c<raidPtr->numCol; c++) {
      ret = rf_ConfigureDisk(raidPtr,&cfgPtr->devnames[r][c][0], 
			     &disks[r][c], rdcap_op, tur_op, 
			     cfgPtr->devs[r][c],r,c);
      if (ret)
        goto fail;
      if (disks[r][c].status != rf_ds_optimal) {
        numFailuresThisRow++;
      }
      else {
        if (disks[r][c].numBlocks < min_numblks)
          min_numblks = disks[r][c].numBlocks;
        DPRINTF7("Disk at row %d col %d: dev %s numBlocks %ld blockSize %d (%ld MB)\n",
          r,c,disks[r][c].devname,
		 (long int) disks[r][c].numBlocks,
		 disks[r][c].blockSize,
		 (long int) disks[r][c].numBlocks * disks[r][c].blockSize / 1024 / 1024);
      }
      num_cols_done++;
    }
    /* XXX fix for n-fault tolerant */
    if (numFailuresThisRow > 0)
      raidPtr->status[r] = rf_rs_degraded;
    num_rows_done++;
  }
#ifndef SIMULATE
#if (defined(__NetBSD__) || defined(__OpenBSD__)) && defined(_KERNEL)
  /* we do nothing */
#else
  rf_SCSI_FreeDiskOp(rdcap_op, 1); rdcap_op = NULL;
  rf_SCSI_FreeDiskOp(tur_op, 0);   tur_op   = NULL;
#endif
#endif /* !SIMULATE */
  /* all disks must be the same size & have the same block size, bs must be a power of 2 */
  bs = 0;
  for (foundone=r=0; !foundone && r<raidPtr->numRow; r++) {
    for (c=0; !foundone && c<raidPtr->numCol; c++) {
      if (disks[r][c].status == rf_ds_optimal) {
        bs = disks[r][c].blockSize;
        foundone = 1;
      }
    }
  }
  if (!foundone) {
    RF_ERRORMSG("RAIDFRAME: Did not find any live disks in the array.\n");
    ret = EINVAL;
    goto fail;
  }
  for (count=0,i=1; i; i<<=1) if (bs & i)
    count++;
  if (count != 1) {
    RF_ERRORMSG1("Error: block size on disks (%d) must be a power of 2\n",bs);
    ret = EINVAL;
    goto fail;
  }
  for (r=0; r<raidPtr->numRow; r++) {
    for (c=0; c<raidPtr->numCol; c++) {
      if (disks[r][c].status == rf_ds_optimal) {
	if (disks[r][c].blockSize != bs) {
	  RF_ERRORMSG2("Error: block size of disk at r %d c %d different from disk at r 0 c 0\n",r,c);
	  ret = EINVAL;
	  goto fail;
	}
	if (disks[r][c].numBlocks != min_numblks) {
	  RF_ERRORMSG3("WARNING: truncating disk at r %d c %d to %d blocks\n",
		       r,c,(int) min_numblks);
	  disks[r][c].numBlocks = min_numblks;
	}
      }
    }
  }

  raidPtr->sectorsPerDisk = min_numblks;
  raidPtr->logBytesPerSector = ffs(bs) - 1;
  raidPtr->bytesPerSector = bs;
  raidPtr->sectorMask = bs-1;
  return(0);

fail:

#ifndef SIMULATE
#if (defined(__NetBSD__) || defined(__OpenBSD__)) && defined(_KERNEL)

  for(r=0;r<raidPtr->numRow;r++) {
	  for(c=0;c<raidPtr->numCol;c++) {
		  /* Cleanup.. */
#ifdef DEBUG
		  printf("Cleaning up row: %d col: %d\n",r,c);
#endif
		  if (raidPtr->raid_cinfo[r][c].ci_vp) {
			  (void)vn_close(raidPtr->raid_cinfo[r][c].ci_vp, 
					 FREAD|FWRITE, proc->p_ucred, proc);
		  }
	  }
  }
  /* Space allocated for raid_vpp will get cleaned up at some other point */
  /* XXX Need more #ifdefs in the above... */

#else 

  if (rdcap_op) rf_SCSI_FreeDiskOp(rdcap_op, 1);
  if (tur_op)   rf_SCSI_FreeDiskOp(tur_op, 0);

#endif
#endif /* !SIMULATE */
  return(ret);
}


/****************************************************************************************
 * set up the data structures describing the spare disks in the array
 * recall from the above comment that the spare disk descriptors are stored
 * in row zero, which is specially expanded to hold them.
 ***************************************************************************************/
int rf_ConfigureSpareDisks(
  RF_ShutdownList_t  **listp,
  RF_Raid_t           *raidPtr,
  RF_Config_t         *cfgPtr)
{
  char buf[256];
  int r,c,i, ret;
  RF_DiskOp_t *rdcap_op = NULL, *tur_op = NULL;
  unsigned bs;
  RF_RaidDisk_t *disks;
  int num_spares_done;

#if (defined(__NetBSD__) || defined(__OpenBSD__)) && defined(_KERNEL)
	struct proc *proc;  
#endif

#ifndef SIMULATE
#if !defined(__NetBSD__) && !defined(__OpenBSD__)
  ret = rf_SCSI_AllocReadCapacity(&rdcap_op);
  if (ret)
    goto fail;
  ret = rf_SCSI_AllocTUR(&tur_op);
  if (ret)
    goto fail;
#endif /* !__NetBSD__ && !__OpenBSD__ */
#endif /* !SIMULATE */

  num_spares_done = 0;

#if (defined(__NetBSD__) || defined(__OpenBSD__)) && defined(_KERNEL)
  proc = raidPtr->proc;
  /* The space for the spares should have already been 
     allocated by ConfigureDisks() */
#endif

  disks = &raidPtr->Disks[0][raidPtr->numCol];
  for (i=0; i<raidPtr->numSpare; i++) {
    ret = rf_ConfigureDisk(raidPtr,&cfgPtr->spare_names[i][0], 
			   &disks[i], rdcap_op, tur_op, 
			   cfgPtr->spare_devs[i],0,raidPtr->numCol+i);
    if (ret)
      goto fail;
    if (disks[i].status != rf_ds_optimal) {
      RF_ERRORMSG1("Warning: spare disk %s failed TUR\n",buf);
    } else {
      disks[i].status = rf_ds_spare;      /* change status to spare */
      DPRINTF6("Spare Disk %d: dev %s numBlocks %ld blockSize %d (%ld MB)\n",i,
	       disks[i].devname,
	       (long int) disks[i].numBlocks,disks[i].blockSize,
	       (long int) disks[i].numBlocks * disks[i].blockSize / 1024 / 1024);
    }
    num_spares_done++;
  }
#ifndef SIMULATE
#if (defined(__NetBSD__) || defined(__OpenBSD__)) && (_KERNEL)

#else
  rf_SCSI_FreeDiskOp(rdcap_op, 1); rdcap_op = NULL;
  rf_SCSI_FreeDiskOp(tur_op, 0);   tur_op   = NULL;
#endif
#endif /* !SIMULATE */

  /* check sizes and block sizes on spare disks */
  bs = 1 << raidPtr->logBytesPerSector;
  for (i=0; i<raidPtr->numSpare; i++) {
    if (disks[i].blockSize != bs) {
      RF_ERRORMSG3("Block size of %d on spare disk %s is not the same as on other disks (%d)\n",disks[i].blockSize, disks[i].devname, bs);
      ret = EINVAL;
      goto fail;
    }
    if (disks[i].numBlocks < raidPtr->sectorsPerDisk) {
	    RF_ERRORMSG3("Spare disk %s (%d blocks) is too small to serve as a spare (need %ld blocks)\n", 
		disks[i].devname, disks[i].blockSize, (long int)raidPtr->sectorsPerDisk);
      ret = EINVAL;
      goto fail;
    } else if (disks[i].numBlocks > raidPtr->sectorsPerDisk) {
	    RF_ERRORMSG2("Warning: truncating spare disk %s to %ld blocks\n",disks[i].devname, (long int) raidPtr->sectorsPerDisk);

      disks[i].numBlocks = raidPtr->sectorsPerDisk;
    }
  }
  
  return(0);

fail:
#ifndef SIMULATE
#if (defined(__NetBSD__) || defined(__OpenBSD__)) && defined(_KERNEL)

  /* Release the hold on the main components.  We've failed to allocate a 
     spare, and since we're failing, we need to free things.. */

  for(r=0;r<raidPtr->numRow;r++) {
	  for(c=0;c<raidPtr->numCol;c++) {
		  /* Cleanup.. */
#ifdef DEBUG
		  printf("Cleaning up row: %d col: %d\n",r,c);
#endif
		  if (raidPtr->raid_cinfo[r][c].ci_vp) {
			  (void)vn_close(raidPtr->raid_cinfo[r][c].ci_vp, 
					 FREAD|FWRITE, proc->p_ucred, proc);
		  }
	  }
  }

  for(i=0;i<raidPtr->numSpare;i++) {
	  /* Cleanup.. */
#ifdef DEBUG
	  printf("Cleaning up spare: %d\n",i);
#endif
	  if (raidPtr->raid_cinfo[0][raidPtr->numCol+i].ci_vp) {
		  (void)vn_close(raidPtr->raid_cinfo[0][raidPtr->numCol+i].ci_vp, 
				 FREAD|FWRITE, proc->p_ucred, proc);
	  }
  }

#else 

  if (rdcap_op) rf_SCSI_FreeDiskOp(rdcap_op, 1);
  if (tur_op)   rf_SCSI_FreeDiskOp(tur_op, 0);

#endif

#endif /* !SIMULATE */
  return(ret);
}



/* configure a single disk in the array */
int rf_ConfigureDisk(raidPtr, buf, diskPtr, rdcap_op, tur_op, dev, row, col)
  RF_Raid_t      *raidPtr;  /* We need this down here too!! GO */
  char           *buf;
  RF_RaidDisk_t  *diskPtr;
  RF_DiskOp_t    *rdcap_op;
  RF_DiskOp_t    *tur_op;
  dev_t           dev;      /* device number used only in kernel */
  RF_RowCol_t     row;
  RF_RowCol_t     col;
{
  char *p;
#ifdef SIMULATE
  double	init_offset;
#else /* SIMULATE  */
#if (defined(__NetBSD__) || defined(__OpenBSD__)) && defined(_KERNEL)
  int retcode;
#else
  int busid, targid, lun, retcode;
#endif
#endif /* SIMULATE  */

#if (defined(__NetBSD__) || defined(__OpenBSD__)) && defined(_KERNEL)
	struct partinfo dpart;
	struct vnode *vp;
	struct vattr va;
	struct proc *proc;
	int error;
#endif

retcode = 0;
  p = rf_find_non_white(buf);
  if (p[strlen(p)-1] == '\n') {
    /* strip off the newline */
    p[strlen(p)-1] = '\0';
  }
  (void) strcpy(diskPtr->devname, p);

#ifdef SIMULATE

  init_offset = 0.0;
  rf_InitDisk(&diskPtr->diskState, disk_db_file_name,diskPtr->devname,0,0,init_offset,row,col);
  rf_GeometryDoReadCapacity(&diskPtr->diskState, &diskPtr->numBlocks, &diskPtr->blockSize);
  diskPtr->numBlocks = diskPtr->numBlocks * rf_sizePercentage / 100;
  
  /* we allow the user to specify that only a fraction of the disks should be used
   * this is just for debug:  it speeds up the parity scan
   */

#else /* SIMULATE */
#if !defined(__NetBSD__) && !defined(__OpenBSD__)
  /* get bus, target, lun */
  retcode = rf_extract_ids(p, &busid, &targid, &lun);
  if (retcode)
    return(retcode);

  /* required in kernel, nop at user level */
  retcode = rf_SCSI_OpenUnit(dev);
  if (retcode)
    return(retcode);

  diskPtr->dev = dev;
  if (rf_SCSI_DoTUR(tur_op, (u_char)busid, (u_char)targid, (u_char)lun, dev)) {
    RF_ERRORMSG1("Disk %s failed TUR.  Marked as dead.\n",diskPtr->devname);
    diskPtr->status = rf_ds_failed;
  } else {
    diskPtr->status = rf_ds_optimal;
    retcode = rf_SCSI_DoReadCapacity(raidPtr,rdcap_op, busid, targid, lun, dev,
      &diskPtr->numBlocks, &diskPtr->blockSize, diskPtr->devname);
    if (retcode)
      return(retcode);
    
    /* we allow the user to specify that only a fraction of the disks should be used
     * this is just for debug:  it speeds up the parity scan
     */
    diskPtr->numBlocks = diskPtr->numBlocks * rf_sizePercentage / 100;
  }
#endif
#if (defined(__NetBSD__) || defined(__OpenBSD__)) && defined(_KERNEL)
  
  proc = raidPtr->proc;  /* XXX Yes, this is not nice.. */
  
  /* Let's start by claiming the component is fine and well... */
  /* XXX not the case if the disk is toast.. */
  diskPtr->status = rf_ds_optimal; 


  raidPtr->raid_cinfo[row][col].ci_vp = NULL;
  raidPtr->raid_cinfo[row][col].ci_dev = NULL;

  error = raidlookup(diskPtr->devname, proc, &vp);
  if (error) {
	  printf("raidlookup on device: %s failed!\n",diskPtr->devname);
	  if (error == ENXIO) {
		  /* XXX the component isn't there... must be dead :-( */
		  diskPtr->status = rf_ds_failed;
	  } else {
		  return(error);
	  }
  }

  if (diskPtr->status == rf_ds_optimal) {

	  if ((error = VOP_GETATTR(vp, &va, proc->p_ucred, proc)) != 0) {
		  return(error);
	  } 

	  error = VOP_IOCTL(vp, DIOCGPART, (caddr_t)&dpart, 
			    FREAD, proc->p_ucred, proc);
	  if (error) {
		  return(error);
	  } 


	  diskPtr->blockSize = dpart.disklab->d_secsize;
	  
	  diskPtr->numBlocks = dpart.part->p_size - rf_protectedSectors;
	  
	  raidPtr->raid_cinfo[row][col].ci_vp = vp;
	  raidPtr->raid_cinfo[row][col].ci_dev = va.va_rdev;

#if 0
	  diskPtr->dev = dev;
#endif

	  diskPtr->dev = va.va_rdev; /* XXX or the above? */

	  /* we allow the user to specify that only a fraction of the disks should be used
	   * this is just for debug:  it speeds up the parity scan
	   */
	  diskPtr->numBlocks = diskPtr->numBlocks * rf_sizePercentage / 100;
    
  }

#endif /* !__NetBSD__ && !__OpenBSD__ */
#endif /* SIMULATE */

  return(0);
}

#ifdef SIMULATE

void rf_default_disk_names()
{
    sprintf(disk_db_file_name,"disk.db");
    sprintf(disk_type_name,"HP2247");
}

void rf_set_disk_db_name(s)
 char  *s;
{
    strcpy(disk_db_file_name,s);
}

void rf_set_disk_type_name(s)
 char  *s;
{
    strcpy(disk_type_name,s);
}

#endif /* SIMULATE */
