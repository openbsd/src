/*	$OpenBSD: rf_geometry.c,v 1.1 1999/01/11 14:29:24 niklas Exp $	*/
/*	$NetBSD: rf_geometry.c,v 1.1 1998/11/13 04:20:30 oster Exp $	*/
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

/* 
 * Changes:
 *	10/24/91  Changes to support disk bus contention model
 *	(MCH)	  1.  Added media_done_time param to Access_time()
 *
 *	08/18/92  Geometry routines have been modified to support zone-bit 
 *		  recording.
 *	(AS)	  1.  Each routine which originally referenced the variable
 *		      'disk->geom->sectors_per_track' has been modified, 
 *		      since the number of sectors per track varies on disks
 *		      with zone-bit recording.
 */

/* :  
 * Log: rf_geometry.c,v 
 * Revision 1.18  1996/08/11 00:40:57  jimz
 * fix up broken comment
 *
 * Revision 1.17  1996/07/28  20:31:39  jimz
 * i386netbsd port
 * true/false fixup
 *
 * Revision 1.16  1996/07/18  22:57:14  jimz
 * port simulator to AIX
 *
 * Revision 1.15  1996/06/07  21:33:04  jimz
 * begin using consistent types for sector numbers,
 * stripe numbers, row+col numbers, recon unit numbers
 *
 * Revision 1.14  1996/06/05  18:06:02  jimz
 * Major code cleanup. The Great Renaming is now done.
 * Better modularity. Better typing. Fixed a bunch of
 * synchronization bugs. Made a lot of global stuff
 * per-desc or per-array. Removed dead code.
 *
 * Revision 1.13  1996/05/30  23:22:16  jimz
 * bugfixes of serialization, timing problems
 * more cleanup
 *
 * Revision 1.12  1996/05/30  11:29:41  jimz
 * Numerous bug fixes. Stripe lock release code disagreed with the taking code
 * about when stripes should be locked (I made it consistent: no parity, no lock)
 * There was a lot of extra serialization of I/Os which I've removed- a lot of
 * it was to calculate values for the cache code, which is no longer with us.
 * More types, function, macro cleanup. Added code to properly quiesce the array
 * on shutdown. Made a lot of stuff array-specific which was (bogusly) general
 * before. Fixed memory allocation, freeing bugs.
 *
 * Revision 1.11  1996/05/24  22:17:04  jimz
 * continue code + namespace cleanup
 * typed a bunch of flags
 *
 * Revision 1.10  1996/05/23  00:33:23  jimz
 * code cleanup: move all debug decls to rf_options.c, all extern
 * debug decls to rf_options.h, all debug vars preceded by rf_
 *
 * Revision 1.9  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.8  1995/12/12  18:10:06  jimz
 * MIN -> RF_MIN, MAX -> RF_MAX, ASSERT -> RF_ASSERT
 * fix 80-column brain damage in comments
 *
 * Revision 1.7  1995/12/01  18:29:34  root
 * added copyright info
 *
 */

#include "rf_types.h"
#include "rf_geometry.h"
#include "rf_raid.h"
#include "rf_general.h"
#include "rf_debugMem.h"

#define DISK_DB "disk_db"
#define DISK_NAME "HP2247"

#define ABS_DIFF(a,b) ( ((a)>(b)) ? ((a)-(b)) : ((b)-(a)) )

static RF_GeometryList_t *geom_list = (RF_GeometryList_t *) NULL;

RF_TICS_t rf_globalSpinup = 1.5;

#define NM_LGTH 80
#define NM_PATN " %80s"

static RF_GeometryList_t *Fetch_geometry_db(FILE *fd);
static void Format_disk(RF_DiskState_t *disk, long sectors_per_block);
static long Find_cyl(RF_SectorNum_t block, RF_DiskState_t *disk);
static long Find_track(RF_SectorNum_t block, RF_DiskState_t *disk);
static long Find_phys_sector(RF_SectorNum_t block, RF_DiskState_t *disk);
static RF_TICS_t Delay_to(RF_TICS_t cur_time, RF_SectorNum_t block,
	RF_DiskState_t *disk);
static RF_TICS_t Seek_time(long to_cyl, long to_track, long from_cyl,
	long from_track, RF_DiskState_t *disk);
static RF_TICS_t Seek(RF_TICS_t cur_time, RF_SectorNum_t block,
	RF_DiskState_t *disk, long update);
static RF_TICS_t Rotate(RF_TICS_t cur_time, RF_SectorNum_t block,
	RF_DiskState_t *disk, long update);
static RF_TICS_t Seek_Rotate(RF_TICS_t cur_time, RF_SectorNum_t block,
	RF_DiskState_t *disk, long update);
static RF_TICS_t GAP(long sec_per_track, RF_DiskState_t *disk);
static RF_TICS_t Block_access_time(RF_TICS_t cur_time, RF_SectorNum_t block,
	RF_SectorCount_t numblocks, RF_DiskState_t *disk, long update);
static void Zero_stats(RF_DiskState_t *disk);
static RF_TICS_t Update_stats(RF_TICS_t cur_time, RF_TICS_t seek, RF_TICS_t rotate,
	RF_TICS_t transfer, RF_DiskState_t *disk);
static void rf_DiskParam(long numCyls, RF_TICS_t minSeek, RF_TICS_t avgSeek, RF_TICS_t maxSeek,
	RF_TICS_t *a, RF_TICS_t *b, RF_TICS_t *c);

static RF_GeometryList_t *Fetch_geometry_db(fd)
  FILE  *fd;
{
  long ret, lineno;
  char name[NM_LGTH], title[20];
  RF_GeometryList_t * list = (RF_GeometryList_t *) NULL,
  ** next_ptr = & list;
  
  if( RF_MAX_DISKNAME_LEN<NM_LGTH ) RF_PANIC();
  lineno = 0;
  while( (ret = fscanf( fd, " %20s", title )) != EOF ) {
    float tmp_f1, tmp_f2, tmp_f3, tmp_f4;
    float tmp_f5=0.0;
    float tmp_f6=0.0; 
    RF_Geometry_t *g;
    long i, x, y, z, num_cylinders;
    RF_ZoneList_t ** znext_ptr;

    if( ret == 1 && strncmp( "enddisk", title, 8 ) == 0 ) break;

    RF_Calloc(*next_ptr, 1, sizeof(RF_GeometryList_t), (RF_GeometryList_t *));
    (*next_ptr)->next = (RF_GeometryList_t *) NULL;
    RF_Calloc(g, 1, sizeof(RF_Geometry_t), (RF_Geometry_t *));
    (*next_ptr)->disk = g;
    next_ptr = &( (*next_ptr)->next ); /*prep for next iteration */
    lineno++;
    if (fscanf( fd, NM_PATN, name ) != 1) {
      fprintf(stderr,"Disk DB Error: Can't get disk name from disk db\n");
      fprintf(stderr,"lineno=%d\n", lineno);
      fprintf(stderr,"name=\"%s\"\n", name);
      exit(1);
    }
    lineno++;
    if ( (fscanf(fd, " tracks per cylinder %ld", &(g->tracks_per_cyl)) != 1) || g->tracks_per_cyl <= 0) {
		    fprintf(stderr,"Disk DB Error: Missing or invalid tracks/cyl for disk %s\n", name); exit(1);
		  }
    lineno++;
    if ( (fscanf(fd, " number of disk zones %ld", &(g->num_zones)) != 1) || g->num_zones <= 0) {
      fprintf(stderr,"Disk DB Error: Missing or invalid number of zones for disk %s\n", name); exit(1);
    }
    
    
    
    /* This section of code creates the linked list which
       contains the disk's zone information. */
    g->zbr_data = (RF_ZoneList_t *) NULL;
    znext_ptr = &(g->zbr_data);
    num_cylinders = 0;
    
    /* This for-loop reads in the cylinder count, the sectors
       per track, and track skew for each zone on the disk. */
    for (i=1; i <= g->num_zones; i++) {
      lineno++;
      if ( (fscanf(fd, " number of cylinders in zone %ld", &x) != 1) || x < 1) {
	fprintf(stderr,"Disk DB Error: Zone %ld: Missing or invalid cyls/zone for disk %s\n", i, name); exit(1);
      }
      lineno++;
      if ( (fscanf(fd, " sectors per track in zone %ld", &y) != 1) || y < 1 ) {
	fprintf(stderr,"Disk DB Error: Zone %ld: Missing or invalid sectors/track for disk %s\n", i, name); exit(1);
      }
      lineno++;
      if ( (fscanf(fd, " track skew in zone %ld", &z) != 1) || z < 0 ) {
	fprintf(stderr,"Disk DB Error: Zone %ld: Missing or invalid track skew for disk %s\n",i, name); exit(1);
      }

      RF_Calloc(*znext_ptr, 1, sizeof(RF_ZoneList_t), (RF_ZoneList_t *));
      (*znext_ptr)->next = (RF_ZoneList_t *) NULL;
      (*znext_ptr)->zone.num_cylinders = x;
      (*znext_ptr)->zone.sec_per_track = y;
      (*znext_ptr)->zone.track_skew = z;
      (*znext_ptr)->zone.num_sectors = 
	(*znext_ptr)->zone.num_cylinders *
	  g->tracks_per_cyl *
	    (*znext_ptr)->zone.sec_per_track;
      znext_ptr = &((*znext_ptr)->next);
      num_cylinders = num_cylinders + x;
    } /* End of for-loop */
    
    lineno++;
    if ( (fscanf(fd, " revolution time %f", &tmp_f1) != 1) || tmp_f1 <= 0) {
      fprintf(stderr,"Disk DB Error: Missing or invalid revolution time for disk %s\n",name); exit(1);
    }
    lineno++;
    if ( (fscanf(fd, " 1 cylinder seek time %f", &tmp_f2 ) != 1) || tmp_f2 <= 0) {
      fprintf(stderr,"Disk DB Error: Missing or invalid 1-cyl seek time for disk %s\n",name); exit(1);
    }
    lineno++;
    if ( (fscanf(fd, " max stroke seek time %f", &tmp_f3) != 1) || tmp_f3 <= 0) {
      fprintf(stderr,"Disk DB Error: Missing or invalid max seek time for disk %s\n",name); exit(1);
    }
    lineno++;
    if ( (fscanf(fd, " average seek time %f", &tmp_f4) != 1) || tmp_f4 <= 0) {
      fprintf(stderr,"Disk DB Error: Missing or invalid avg seek time for disk %s\n",name); exit(1);
    }
    lineno++;
    if ( (fscanf(fd, " time to sleep %f", &tmp_f5) != 1) || tmp_f4 <= 0) {
      fprintf(stderr,"Disk DB Error: Missing or invalid time to sleep for disk %s\n",name); exit(1);
		}
    lineno++;
    if ( (fscanf(fd, " time to spinup %f", &tmp_f6) != 1) || tmp_f4 <= 0) {
      fprintf(stderr,"Disk DB Error: Missing or invalid time to sleep for disk %s\n",name); exit(1);
    }   
    strcpy( g->disk_name, name );
    g->revolution_time = tmp_f1;
    g->seek_one_cyl = tmp_f2;
    g->seek_max_stroke = tmp_f3;
    g->seek_avg = tmp_f4;
    g->time_to_sleep = tmp_f5;
    g->time_to_spinup = tmp_f6;
    /* convert disk specs to seek equation coeff */
    rf_DiskParam( num_cylinders, g->seek_one_cyl,
	      g->seek_avg, g->seek_max_stroke,
	      &g->seek_sqrt_coeff, &g->seek_linear_coeff,
	      &g->seek_constant_coeff );
  }
  return( list );
}

static void Format_disk(disk, sectors_per_block)
  RF_DiskState_t  *disk;
  long             sectors_per_block;
{
  long sector_count = 0;
  RF_ZoneList_t *z;

  if( disk == (RF_DiskState_t *) NULL ) RF_PANIC();
  if( disk->geom == (RF_Geometry_t *) NULL ) RF_PANIC();
  if( sectors_per_block <=0 ) RF_PANIC();
  
  disk->sectors_per_block = sectors_per_block;
  z = disk->geom->zbr_data;
  /* This while-loop visits each disk zone and computes the total
     number of sectors on the disk. */
  while (z != (RF_ZoneList_t *) NULL) {
    sector_count = sector_count + (z->zone.num_cylinders * 
				   disk->geom->tracks_per_cyl * 
				   z->zone.sec_per_track);
    z = z->next;
  }
  
  disk->last_block_index = (sector_count / sectors_per_block) - 1;
}

void rf_InitDisk( disk, disk_db, disk_name, init_cyl, init_track, init_offset, row, col)
  RF_DiskState_t  *disk;
  char            *disk_db;
  char            *disk_name;
  long            init_cyl;
  long            init_track;
  RF_TICS_t       init_offset;
  int             row;
  int             col;
{
  RF_GeometryList_t *gp;
  FILE *f;
  
  RF_ASSERT( disk != (RF_DiskState_t *) NULL );
  
  disk->cur_cyl = init_cyl;
  disk->cur_track = init_track;
  disk->index_offset = init_offset;
  disk->geom = (RF_Geometry_t *) NULL;
  disk->queueFinishTime = 0.0;
  disk->lastBlock = 0;
  disk->row=row;
  disk->col=col;
  Zero_stats(disk);

  if (strncmp(disk_name,"/dev",4 )==0) strcpy(disk_name,"HP2247");
  
  if( geom_list == (RF_GeometryList_t *) NULL ) {
    f = fopen(disk_db,"r");
    if (f == NULL) {
      fprintf(stderr, "ERROR: RAIDframe could not open disk db %s\n", disk_db);
      exit(1);
    }
    geom_list = Fetch_geometry_db( f );
    fclose( f );
  }
  for( gp = geom_list; gp != (RF_GeometryList_t *) NULL; gp = gp->next ) {
    RF_ASSERT( gp->disk != (RF_Geometry_t *) NULL
	   &&  gp->disk->disk_name != (char *) NULL );
    if( strncmp( disk_name, gp->disk->disk_name, RF_MAX_DISKNAME_LEN )
       == 0 ) {
      disk->geom = gp->disk;
      break;
    }
  }
  if( disk->geom == (RF_Geometry_t *) NULL ) {
    fprintf( stderr, "Disk %s not found in database %s\n",
	    disk_name, disk_db );
    exit(1);
  }
  
  Format_disk( disk, 1 );
}

static long Find_cyl( block, disk )
  RF_SectorNum_t   block;
  RF_DiskState_t  *disk;
{
  RF_ZoneList_t * z;
  long tmp;

  long log_sector = block * disk->sectors_per_block;
  long cylinder = 0;
  z = disk->geom->zbr_data;
  /* This while-loop finds the zone to which log_sector belongs, 
     computes the starting cylinder number of this zone, and 
     computes the sector offset into this zone. */
  while (log_sector >= z->zone.num_sectors) {
    log_sector = log_sector - z->zone.num_sectors;
    cylinder = cylinder + z->zone.num_cylinders;
    z = z->next;
  }
  
  /* The cylinder to which log_sector belongs equals the starting
     cylinder number of its zone plus the cylinder offset into
     the zone. */
  tmp = cylinder + (log_sector / (z->zone.sec_per_track * 
				  disk->geom->tracks_per_cyl));
  
  return( tmp );
}

static long Find_track( block, disk )
  RF_SectorNum_t   block;
  RF_DiskState_t  *disk;
{
  RF_ZoneList_t * z;
  long tmp;
  
  long log_sector = block * disk->sectors_per_block;
  long track = 0;
  z = disk->geom->zbr_data;
  /* This while-loop finds the zone to which log_sector belongs,
     computes the starting track number of this zone, and computes
     the sector offset into this zone. */
  while (log_sector >= z->zone.num_sectors) {
    log_sector = log_sector - z->zone.num_sectors;
    track = track + (z->zone.num_cylinders * 
		     disk->geom->tracks_per_cyl);
    z = z->next;
  }
  
  /* The track to which log_sector belongs equals the starting
     track number of its zone plus the track offset into the zone,
     modulo the number of tracks per cylinder on the disk. */
  tmp = (track + (log_sector / z->zone.sec_per_track)) % 
    disk->geom->tracks_per_cyl;
  
  return( tmp );
}

/*
 ** The position of a logical sector relative to the index mark on any track
 ** is not simple.  A simple organization would be:
**
** 	track 0	:	0,  1,  2,  3,  ...  N-1
**	track 1 :	N,N+1,N+2,N+3,  ... 2N-1
**			^
**			Index mark just before this point
**
** This is not good because sequential access of sectors N-1 then N
** will require a full revolution in between (because track switch requires
** a couple of sectors to recalibrate from embedded servo).  So frequently
** sequentially numbered sectors are physically skewed so that the next
** accessible sector after N-1 will be N (with a skew of 2)
**
** 	track 0	:	   0,   1,   2,   3,  ...  N-1
**	track 1 :	2N-2,2N-1,   N, N+1,  ... 2N-3
**			^
**			Index mark just before this point
**
** Layout gets even more complex with cylinder boundaries.  Seek time
** is A + B*M where M is the number of cylinders to seek over.  On a sequential
** access that crosses a cylinder boundary, the disk will rotate for
** A+B seconds, then "track skew" sectors (inter-sector gaps actually)
** before it can access another sector, so the cylinder to cylinder skew
** is "track skew" + CEIL( sectors_per_track*(A+B)/revolution_time ).
**
** So if sector 0 is 0 sectors from the index mark on the first track,
** where is sector X relative to the index mark on its track?
**
**	( ( X % sectors_per_track ) 	 basic relative position **
**	+ track_skew * ( X / sectors_per_track )  skewed for each track **   
**	+ CEIL( sectors_per_track*(A+B)/revolution_time )
**		* ( X / sectors_per_cylinder )	 skewed more for each cyl **
**	) % sectors_per_track		 wrapped around in the track **
**
**
*/

static long Find_phys_sector(block, disk)
  RF_SectorNum_t   block;
  RF_DiskState_t  *disk;
{
  long phys = 0;
  RF_ZoneList_t * z;
  long previous_spt = 1;
  long sector = block * disk->sectors_per_block;
  
  z = disk->geom->zbr_data;
  /* This while-loop finds the zone to which sector belongs,
     and computes the physical sector up to that zone. */
  while (sector >= z->zone.num_sectors) {
    sector = sector - z->zone.num_sectors;
    /* By first multiplying 'phys' by the sectors per track in
       the current zone divided by the sectors per track in the
       previous zone, we convert a given physical sector in one
       zone to an equivalent physical sector in another zone. */
	  phys = ((phys * z->zone.sec_per_track / previous_spt) +
		  (((z->zone.num_sectors - 1) % z->zone.sec_per_track) + 
		   (z->zone.track_skew * z->zone.num_cylinders *
		    disk->geom->tracks_per_cyl) +
		   (long) ceil( (double) z->zone.sec_per_track *
			       (disk->geom->seek_constant_coeff) /
			       disk->geom->revolution_time) *
		   z->zone.num_cylinders)) %
		     z->zone.sec_per_track;
    previous_spt = z->zone.sec_per_track;
    z = z->next;
  }
  
  /* The final physical sector equals the physical sector up to 
     the particular zone, plus the physical sector caused by the
     sector offset into this zone. */
  phys = ((phys * z->zone.sec_per_track / previous_spt) +
	  ((sector % z->zone.sec_per_track) +
	   (z->zone.track_skew * (sector / z->zone.sec_per_track)) +
	   (long) ceil( (RF_TICS_t) z->zone.sec_per_track * 
		       (disk->geom->seek_constant_coeff) /
		       disk->geom->revolution_time) *
	   (sector / (z->zone.sec_per_track * 
		      disk->geom->tracks_per_cyl)))) %
			z->zone.sec_per_track;
  
  
  return( phys );
}

/*
 ** When each disk starts up, its index mark is a fraction (f) of a rotation
 ** ahead from its heads (in the direction of rotation).  The sector 
 ** under its heads is at a fraction f of a rotation from the index
 ** mark.  After T time has past, T/rotation_time revolutions have occured, so
 ** the sector under the heads is at a fraction FRAC(f+T/rotation_time) of a
 ** rotation from the index mark.  If the target block is at physical sector
 ** X relative to its index mark, then it is at fraction (X/sectors_per_track),
 ** so the rotational delay is 
 ** ((X/sectors_per_track)-FRAC(f+T/rotation_time)) * revolution_time
 ** if this is positive, otherwise it is
 ** (1+(X/sectors_per_track)-FRAC(f+T/rotation_time)) * revolution_time
 */

#define FRAC(a) ( (a) - (long) floor(a) )

static RF_TICS_t Delay_to(cur_time, block, disk)
  RF_TICS_t        cur_time;
  RF_SectorNum_t   block;
  RF_DiskState_t  *disk;
{
  RF_TICS_t tmp;
  RF_ZoneList_t *z;
  
  long sector = block * disk->sectors_per_block;
  z = disk->geom->zbr_data;
  /* This while-loop finds the zone to which sector belongs. */
  while (sector >= z->zone.num_sectors) {
    sector = sector - z->zone.num_sectors;
    z = z->next;
  }
  
  tmp = (
	 (RF_TICS_t) Find_phys_sector(block,disk)/z->zone.sec_per_track
	 - FRAC(disk->index_offset+cur_time/disk->geom->revolution_time)
	 ) * disk->geom->revolution_time;
  if( tmp < 0 ) tmp += disk->geom->revolution_time;
  if( tmp < 0 ) RF_PANIC();
  return( tmp );
}

/* Hmmm...they seem to be computing the head switch time as
 * equal to the track skew penalty.  Is this an approximation?
 * (MCH)
 */
static RF_TICS_t Seek_time( to_cyl, to_track, from_cyl, from_track, disk )
  long             to_cyl;
  long             to_track;
  long             from_cyl;
  long             from_track;
  RF_DiskState_t  *disk;
{
  long cyls = ABS_DIFF( from_cyl, to_cyl ) - 1;
  RF_TICS_t seek = 0.0;
  RF_ZoneList_t * z;
  
  /* printf("Seek_time:  from_cyl %ld, to_cyl %ld, from_trk %ld, to_trk %ld\n",from_cyl, to_cyl, from_track, to_track); */
  if( from_cyl != to_cyl ) {
    z = disk->geom->zbr_data;
    /* This while-loop finds the zone to which to_cyl belongs. */
    while (to_cyl >= z->zone.num_cylinders) {
      to_cyl = to_cyl - z->zone.num_cylinders;
      z = z->next;
    }
    
    seek = disk->geom->seek_constant_coeff 
      + disk->geom->seek_linear_coeff * cyls
	+ disk->geom->seek_sqrt_coeff * sqrt( (double) cyls ) 
	  + z->zone.track_skew * disk->geom->revolution_time /
	    z->zone.sec_per_track;
    
  } else if( from_track != to_track ) {
    /* from_track and to_track must lie in the same zone. */
    z = disk->geom->zbr_data;
    /* This while-loop finds the zone to which from_cyl belongs. */
    while (from_cyl >= z->zone.num_cylinders) {
      from_cyl = from_cyl - z->zone.num_cylinders;
      z = z->next;
    } 
    
    seek = z->zone.track_skew
      * disk->geom->revolution_time
	/ z->zone.sec_per_track;
  }
  return( seek );
}

static RF_TICS_t Seek(cur_time, block, disk, update)
  RF_TICS_t        cur_time;
  RF_SectorNum_t   block;
  RF_DiskState_t  *disk;
  long             update;
{
  long cur_cyl, cur_track;
  /* 
   ** current location is derived from the time,
   ** current track and current cylinder
   ** 
   ** update current location as you go
   */
  
  RF_ASSERT( block <= disk->last_block_index );
  cur_cyl = disk->cur_cyl;
  cur_track = disk->cur_track;
  if (update) {
    disk->cur_cyl = Find_cyl( block, disk );
    disk->cur_track = Find_track( block, disk );
  }
  return( Seek_time( disk->cur_cyl, disk->cur_track,
		    cur_cyl, cur_track, disk ) );
}

static RF_TICS_t Rotate(cur_time, block, disk, update)
  RF_TICS_t        cur_time;
  RF_SectorNum_t   block;
  RF_DiskState_t  *disk;
  long             update;
{
  /* 
   ** current location is derived from the time,
   ** current track and current cylinder
   ** 
   ** block the process until at the appropriate block
   ** updating current location as you go
   */
  
  RF_ASSERT( block <= disk->last_block_index );
  return( Delay_to( cur_time, block, disk ) );
}

static RF_TICS_t Seek_Rotate(cur_time, block, disk, update)
  RF_TICS_t        cur_time;
  RF_SectorNum_t   block;
  RF_DiskState_t  *disk;
  long             update;
{
  RF_TICS_t seek, delay;

  RF_ASSERT( block <= disk->last_block_index );
  seek = Seek( cur_time, block, disk, update );
  delay =  seek + Rotate( cur_time+seek, block, disk, update );
  return( delay );
}

static RF_TICS_t GAP(sec_per_track, disk)
  long             sec_per_track;
  RF_DiskState_t  *disk;
{
  RF_TICS_t tmp = (disk->geom->revolution_time/(100*sec_per_track));
  return (tmp);
}

RF_TICS_t Block_access_time(cur_time, block, numblocks, disk, update)
  RF_TICS_t          cur_time;
  RF_SectorNum_t     block;
  RF_SectorCount_t   numblocks;
  RF_DiskState_t    *disk;
  long               update;
{
  RF_TICS_t delay = 0;
  long cur = block, end = block + numblocks;
  long sector, tmp;
  RF_ZoneList_t * z;
  /*
   ** this is the same as Seek_Rotate by merit of the mapping
   ** except that the access ends before the gap to the next block
   */
  RF_ASSERT( numblocks > 0 && end-1 <= disk->last_block_index );
  
  while( cur < end ) {
    sector = cur * disk->sectors_per_block;
    z = disk->geom->zbr_data;
    /* This while-loop finds the zone to which sector belongs. */
    while (sector >= z->zone.num_sectors) {
      sector = sector - z->zone.num_sectors;
      z = z->next;
    }
    
    tmp = RF_MIN( end - cur, z->zone.sec_per_track
	      - cur % z->zone.sec_per_track );
    delay += tmp * disk->geom->revolution_time /
      z->zone.sec_per_track - 
	GAP(z->zone.sec_per_track, disk);
    cur += tmp;
    if( cur != end )
      delay += Seek_Rotate( cur_time+delay, cur, disk, update );
  }
  return( delay );
}

static void Zero_stats(disk)
  RF_DiskState_t  *disk;
{
  char traceFileName[64];
  disk->stats.num_events = 0;
  disk->stats.seek_sum = 0;
  disk->stats.seekSq_sum = 0;
  disk->stats.rotate_sum = 0;
  disk->stats.rotateSq_sum = 0;
  disk->stats.transfer_sum = 0;
  disk->stats.transferSq_sum = 0;
  disk->stats.access_sum = 0;
  disk->stats.accessSq_sum = 0;
  disk->stats.sleep_sum=0;
  disk->stats.idle_sum=0;
  disk->stats.rw_sum=0;
  disk->stats.spinup_sum=0;
  disk->stats.last_acc=0;
  if (rf_diskTrace){
    sprintf (traceFileName,"rf_diskTracer%dc%d\0",disk->row,disk->col);
    if ( (disk->traceFile= fopen(traceFileName, "w")) == NULL) {
      perror(traceFileName); RF_PANIC();}
  }
}

static RF_TICS_t Update_stats(cur_time, seek, rotate, transfer, disk)
  RF_TICS_t        cur_time;
  RF_TICS_t        seek;
  RF_TICS_t        rotate;
  RF_TICS_t        transfer;
  RF_DiskState_t  *disk;
{
  RF_TICS_t spinup=0;
  RF_TICS_t sleep=0;
  RF_TICS_t idle=0;
  
  disk->stats.num_events++;
  disk->stats.seek_sum += seek;
  disk->stats.seekSq_sum += seek*seek;
  disk->stats.rotate_sum += rotate;
  disk->stats.rotateSq_sum += rotate*rotate;
  disk->stats.transfer_sum += transfer;
  disk->stats.transferSq_sum += transfer*transfer;
  disk->stats.access_sum += seek+rotate+transfer;
  disk->stats.accessSq_sum +=
    (seek+rotate+transfer)*(seek+rotate+transfer);

/*   ASSERT (cur_time - disk->stats.last_acc >= 0);  */

  if (cur_time-disk->stats.last_acc>disk->geom->time_to_sleep){
    idle=disk->geom->time_to_sleep;  
    
    sleep = cur_time - disk->stats.last_acc - idle;
    spinup=disk->geom->time_to_spinup;
    rf_globalSpinup = spinup;
  }

  else{
    idle=cur_time - disk->stats.last_acc;
  }
  
  
  disk->stats.sleep_sum+=sleep;
  disk->stats.idle_sum+=idle;
  disk->stats.rw_sum+=seek+rotate+transfer;
  disk->stats.spinup_sum+=spinup;

  if (rf_diskTrace){
    fprintf(disk->traceFile,"%g %g\n",disk->stats.last_acc,2.0);
    fprintf(disk->traceFile,"%g %g\n",(disk->stats.last_acc+idle),2.0);
    if (sleep){
      fprintf(disk->traceFile,"%g %g\n",(disk->stats.last_acc+idle),1.0);
      fprintf(disk->traceFile,"%g %g\n",(disk->stats.last_acc+idle+sleep),1.0);
    }
     
    if (spinup){
      fprintf(disk->traceFile,"%g %g\n",(cur_time),4.0);
      fprintf(disk->traceFile,"%g %g\n",(cur_time+spinup),4.0);
    }
 
    fprintf(disk->traceFile,"%g %g\n",(cur_time+spinup),3.0);
    fprintf(disk->traceFile,"%g %g\n",(cur_time+spinup+seek+rotate+transfer),3.0);
    
      
  }

  disk->stats.last_acc=cur_time+spinup+seek+rotate+transfer; 
  
  return(spinup);
}


void rf_StopStats(disk, cur_time)
  RF_DiskState_t  *disk;
  RF_TICS_t        cur_time;
{

	RF_TICS_t sleep=0;
	RF_TICS_t idle=0;

	if (cur_time - disk->stats.last_acc > disk->geom->time_to_sleep){
		
		sleep = cur_time - disk->stats.last_acc-disk->geom->time_to_sleep;
		idle = disk->geom->time_to_sleep;
			
	}

	  

	else{
		idle=cur_time - disk->stats.last_acc;
	}

	disk->stats.sleep_sum+=sleep;
	disk->stats.idle_sum+=idle;

	if (rf_diskTrace){
	  fprintf(disk->traceFile,"%g %g\n",disk->stats.last_acc,2.0);
	  fprintf(disk->traceFile,"%g %g\n",(disk->stats.last_acc+idle),2.0);
	  if (sleep){
	      fprintf(disk->traceFile,"%g %g\n",(disk->stats.last_acc+idle),1.0);
	      fprintf(disk->traceFile,"%g %g\n",(disk->stats.last_acc+idle+sleep),1.0);
	    }
	  fclose(disk->traceFile);
	  }
}

/* Sometimes num_events is zero because the disk was failed at the start
 * of the simulation and never replaced.  This causes a crash on some
 * architectures, which is why we have the conditional.
 */
void rf_Report_stats(
  RF_DiskState_t  *disk,
  long            *numEventsPtr,
  RF_TICS_t       *avgSeekPtr,
  RF_TICS_t       *avgRotatePtr,
  RF_TICS_t       *avgTransferPtr,
  RF_TICS_t       *avgAccessPtr,
  RF_TICS_t       *SleepPtr,
  RF_TICS_t       *IdlePtr,
  RF_TICS_t       *RwPtr,
  RF_TICS_t       *SpinupPtr)
{
    *numEventsPtr = disk->stats.num_events;
    if (disk->stats.num_events) {
	*avgSeekPtr = disk->stats.seek_sum / disk->stats.num_events;
	*avgRotatePtr = disk->stats.rotate_sum / disk->stats.num_events;
	*avgTransferPtr = disk->stats.transfer_sum / disk->stats.num_events;
	*avgAccessPtr = disk->stats.access_sum / disk->stats.num_events;
    } else {
	*avgSeekPtr = 0;
	*avgRotatePtr = 0;
	*avgTransferPtr = 0;
	*avgAccessPtr = 0;
    }
    *SleepPtr = disk->stats.sleep_sum;
    *IdlePtr = disk->stats.idle_sum;
    *RwPtr = disk->stats.rw_sum ;
    *SpinupPtr = disk->stats.spinup_sum ;
}

int rf_Access_time( access_time, cur_time, block, numblocks, disk, media_done_time, update )
  RF_TICS_t         *access_time;
  RF_TICS_t          cur_time;
  RF_SectorNum_t     block;
  RF_SectorCount_t   numblocks;
  RF_DiskState_t    *disk;
  RF_TICS_t         *media_done_time;
  long               update; /* 1 => update disk state, 0 => don't */
{
	/*
	 * first move to the start of the data, then sweep to the end
	 */
	RF_TICS_t spinup=0;
	RF_TICS_t seek = Seek( cur_time, block, disk, update );
	RF_TICS_t rotate =  Rotate( cur_time+seek, block, disk, update );
	RF_TICS_t transfer = Block_access_time( cur_time+seek+rotate, block,
				numblocks, disk, update );

	if (update) spinup=Update_stats(cur_time, seek, rotate, transfer, disk );
	*media_done_time = seek+rotate+transfer;
	*access_time =( seek+rotate+transfer+spinup);
	return(0);
}

/* added to take into account the fact that maping code acounts for the disk label */

void rf_GeometryDoReadCapacity(disk, numBlocks, blockSize)
  RF_DiskState_t    *disk;
  RF_SectorCount_t  *numBlocks;
  int               *blockSize;
{
	*numBlocks= (disk->last_block_index + 1 )-rf_protectedSectors;	
	
	*blockSize= (disk->sectors_per_block*512 );
	
	/* in bytes */
}


/* END GEOMETRY ROUTINES **********************************************/


static void rf_DiskParam(numCyls, minSeek, avgSeek, maxSeek, a, b, c)
  long        numCyls;
  RF_TICS_t   minSeek;
  RF_TICS_t   avgSeek;
  RF_TICS_t   maxSeek;
  RF_TICS_t  *a;
  RF_TICS_t  *b;
  RF_TICS_t  *c;
{
    if (minSeek == avgSeek && minSeek == maxSeek) {
	*a = 0.0; *b = 0.0; *c = minSeek;
    } else {
	*a = ( 15 * avgSeek - 10 * minSeek - 5 * maxSeek ) / ( 3 * sqrt( (double) numCyls ));
	*b = ( 7 * minSeek + 8 * maxSeek - 15 * avgSeek ) / ( 3 * numCyls );
	*c = minSeek;
    }
}
