/*	$OpenBSD: rf_geometry.h,v 1.1 1999/01/11 14:29:24 niklas Exp $	*/
/*	$NetBSD: rf_geometry.h,v 1.1 1998/11/13 04:20:30 oster Exp $	*/
/* geometry.h
 * code from raidSim to model disk behavior
 */
/*
 * Changes:
 * 	8/18/92  Additional structures have been declared and existing 
 *		 structures have been modified in order to support zone-
 *		 bit recording.
 *	(AS)	 1. The types 'Zone_data' and 'Zone_list' have been defined.
 *	(AS)	 2. The type 'Geometry' has been modified.
 */

/* :  
 * Log: rf_geometry.h,v 
 * Revision 1.10  1996/08/06 22:25:08  jimz
 * include raidframe stuff before system stuff
 *
 * Revision 1.9  1996/06/07  21:33:04  jimz
 * begin using consistent types for sector numbers,
 * stripe numbers, row+col numbers, recon unit numbers
 *
 * Revision 1.8  1996/05/31  10:16:14  jimz
 * add raidsim note
 *
 * Revision 1.7  1996/05/30  23:22:16  jimz
 * bugfixes of serialization, timing problems
 * more cleanup
 *
 * Revision 1.6  1996/05/30  11:29:41  jimz
 * Numerous bug fixes. Stripe lock release code disagreed with the taking code
 * about when stripes should be locked (I made it consistent: no parity, no lock)
 * There was a lot of extra serialization of I/Os which I've removed- a lot of
 * it was to calculate values for the cache code, which is no longer with us.
 * More types, function, macro cleanup. Added code to properly quiesce the array
 * on shutdown. Made a lot of stuff array-specific which was (bogusly) general
 * before. Fixed memory allocation, freeing bugs.
 *
 * Revision 1.5  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.4  1995/12/01  18:29:45  root
 * added copyright info
 *
 */

#ifndef _RF__RF_GEOMETRY_H_
#define _RF__RF_GEOMETRY_H_

#include "rf_types.h"
#include "rf_sys.h"
#ifndef _KERNEL
#include <string.h>
#include <math.h>
#if defined(__NetBSD__) || defined(__OpenBSD__)
#include <stdio.h>
#endif /* __NetBSD__ || __OpenBSD__ */
#endif

#define RF_MAX_DISKNAME_LEN 80

typedef struct RF_ZoneData_s {
  long num_cylinders; /* Number of cylinders in zone */
  long sec_per_track; /* Sectors per track in zone */
  long track_skew;    /* Skew of each track in zone */
  long num_sectors;   /* Number of sectors in zone */
} RF_ZoneData_t;

/*
 * Linked list containing zone data
 */
typedef struct RF_ZoneList_s RF_ZoneList_t;
struct RF_ZoneList_s {
  RF_ZoneData_t   zone; /* for each disk */
  RF_ZoneList_t  *next;	
};

typedef struct RF_Geometry_s {
  char disk_name[RF_MAX_DISKNAME_LEN]; /* name for a type of disk */
  long       tracks_per_cyl;  /* tracks in a cylinder */
  /* assume 1 head per track, 1 set of read/write electronics */
  long       num_zones;       /* number of ZBR zones on disk */
  RF_TICS_t  revolution_time; /* milliseconds per revolution */
  RF_TICS_t  seek_one_cyl;    /* adjacent cylinder seek time */
  RF_TICS_t  seek_max_stroke; /* end to end seek time */
  RF_TICS_t  seek_avg;        /* random from/to average time */
  /*
   * seek time = a * (x-1)^0.5 + b * (x-1) + c
   * x >= 1 is the seek distance in cylinders
   */
  RF_TICS_t       seek_sqrt_coeff;     /* a */
  RF_TICS_t       seek_linear_coeff;   /* b */
  RF_TICS_t       seek_constant_coeff; /* c */
  RF_ZoneList_t  *zbr_data;            /* linked list with ZBR data */
  RF_TICS_t       time_to_sleep;       /* seconds of idle time before disks goes to sleep */
  RF_TICS_t       time_to_spinup;      /* seconds spin up takes */
} RF_Geometry_t;

typedef struct RF_GeometryList_s RF_GeometryList_t;
struct RF_GeometryList_s {
  RF_Geometry_t      *disk;
  RF_GeometryList_t  *next;
};

typedef struct RF_DiskStats_s {
  long       num_events;
  RF_TICS_t  seek_sum;
  RF_TICS_t  seekSq_sum;
  RF_TICS_t  rotate_sum;
  RF_TICS_t  rotateSq_sum;
  RF_TICS_t  transfer_sum;
  RF_TICS_t  transferSq_sum;
  RF_TICS_t  access_sum;
  RF_TICS_t  accessSq_sum;
  RF_TICS_t  sleep_sum;
  RF_TICS_t  idle_sum;
  RF_TICS_t  rw_sum;
  RF_TICS_t  spinup_sum;
  RF_TICS_t  last_acc;       /* time the last acces was finished */
} RF_DiskStats_t;

struct RF_DiskState_s {
  int              row;
  int              col;
  RF_Geometry_t   *geom;
  long             sectors_per_block;  /* formatted per disk */
  long             last_block_index;   /* format result for convenience */
  RF_TICS_t        index_offset;       /* powerup head offset to index mark */
  long             cur_track;          /* current track */
  long             cur_cyl;            /* current cylinder */
  RF_DiskStats_t   stats;              /* disk statistics */

  RF_TICS_t        queueFinishTime;    /* used by shortest-seek code */
  long             lastBlock;
  FILE            *traceFile;
};
typedef struct RF_DiskState_s RF_DiskState_t;

extern RF_TICS_t rf_globalSpinup;

void rf_InitDisk(RF_DiskState_t *disk, char *disk_name, char *disk_db, long init_cyl,
	long init_track, RF_TICS_t init_offset, int row, int col);
void rf_StopStats(RF_DiskState_t *disk, RF_TICS_t cur_time);
void rf_Report_stats(RF_DiskState_t *disk, long *numEventsPtr, RF_TICS_t *avgSeekPtr,
	RF_TICS_t *avgRotatePtr, RF_TICS_t *avgTransferPtr, RF_TICS_t *avgAccessPtr,
	RF_TICS_t *SleepPtr, RF_TICS_t *IdlePtr, RF_TICS_t *RwPtr, RF_TICS_t *SpinupPtr);
int rf_Access_time(RF_TICS_t *access_time, RF_TICS_t cur_time,
	RF_SectorNum_t block, RF_SectorCount_t numblocks, RF_DiskState_t *disk,
	RF_TICS_t *media_done_time, long update);
void rf_GeometryDoReadCapacity(RF_DiskState_t *disk, RF_SectorCount_t *numBlocks,
	int *blockSize);

#endif /* !_RF__RF_GEOMETRY_H_ */
