/*	$OpenBSD: rf_demo.c,v 1.1 1999/01/11 14:29:15 niklas Exp $	*/
/*	$NetBSD: rf_demo.c,v 1.1 1998/11/13 04:20:28 oster Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Mark Holland, Khalil Amiri
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

/**********************************************************************************
 *
 * rf_demo.c -- code for supporting demos.  this is not actually part of the driver.
 *
 **********************************************************************************/

/* :  
 * Log: rf_demo.c,v 
 * Revision 1.24  1996/06/17 14:38:33  jimz
 * properly #if out RF_DEMO code
 * fix bug in MakeConfig that was causing weird behavior
 * in configuration routines (config was not zeroed at start)
 * clean up genplot handling of stacks
 *
 * Revision 1.23  1996/06/17  03:23:09  jimz
 * explicitly do pthread stuff (for join)
 * NOTE: this should be changed!
 *
 * Revision 1.22  1996/06/14  23:15:38  jimz
 * attempt to deal with thread GC problem
 *
 * Revision 1.21  1996/06/09  02:36:46  jimz
 * lots of little crufty cleanup- fixup whitespace
 * issues, comment #ifdefs, improve typing in some
 * places (esp size-related)
 *
 * Revision 1.20  1996/06/05  18:06:02  jimz
 * Major code cleanup. The Great Renaming is now done.
 * Better modularity. Better typing. Fixed a bunch of
 * synchronization bugs. Made a lot of global stuff
 * per-desc or per-array. Removed dead code.
 *
 * Revision 1.19  1996/05/30  23:22:16  jimz
 * bugfixes of serialization, timing problems
 * more cleanup
 *
 * Revision 1.18  1996/05/30  11:29:41  jimz
 * Numerous bug fixes. Stripe lock release code disagreed with the taking code
 * about when stripes should be locked (I made it consistent: no parity, no lock)
 * There was a lot of extra serialization of I/Os which I've removed- a lot of
 * it was to calculate values for the cache code, which is no longer with us.
 * More types, function, macro cleanup. Added code to properly quiesce the array
 * on shutdown. Made a lot of stuff array-specific which was (bogusly) general
 * before. Fixed memory allocation, freeing bugs.
 *
 * Revision 1.17  1996/05/27  18:56:37  jimz
 * more code cleanup
 * better typing
 * compiles in all 3 environments
 *
 * Revision 1.16  1996/05/23  00:33:23  jimz
 * code cleanup: move all debug decls to rf_options.c, all extern
 * debug decls to rf_options.h, all debug vars preceded by rf_
 *
 * Revision 1.15  1996/05/20  16:14:08  jimz
 * switch to rf_{mutex,cond}_{init,destroy}
 *
 * Revision 1.14  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.13  1995/12/01  15:56:07  root
 * added copyright info
 *
 */

#include "rf_archs.h"

#if RF_DEMO > 0

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <strings.h>
#include <unistd.h>
#include <sys/time.h>
#include <signal.h>

#include "rf_threadstuff.h"
#include "rf_demo.h"
#include "rf_utils.h"
#include "rf_general.h"
#include "rf_options.h"

#ifdef SIMULATE
#include "rf_diskevent.h"
#endif /* SIMULATE */

static int doMax = 0;                   /* currently no way to set this */

/****************************************************************************************
 * fault-free demo code
 ***************************************************************************************/

static int user_iops_meter = -1;
static int disk_iops_meter = -1;
static int max_user_meter = -1;
static int max_disk_meter = -1;
static int recon_pctg_meter = -1;
static int avg_resp_time_meter = -1;
static int recon_time_meter = -1;
static int ff_avg_resp_time_meter = -1;
static int deg_avg_resp_time_meter = -1;
static int recon_avg_resp_time_meter = -1;
static int user_ios_ff=0;
static int user_ios_deg=0;
static int user_ios_recon=0;
static long user_resp_time_sum_ff = 0;
static long user_resp_time_sum_deg = 0;
static long user_resp_time_sum_recon = 0;

int rf_demo_op_mode = 0;

RF_DECLARE_STATIC_MUTEX(iops_mutex)
static int user_ios_so_far, disk_ios_so_far, max_user, max_disk;
static long user_resp_time_sum_ms;
static int recon_pctg;
static struct timeval iops_starttime;
#ifndef SIMULATE
static RF_Thread_t update_thread_desc;
#endif /* !SIMULATE */
static int meter_update_terminate;

static int meter_update_interval = 2; /* seconds between meter updates */
static int iops_initialized = 0, recon_initialized = 0;

static char *demoMeterTags[] = {"FF", "Degr", "Recon"};

static int vpos=0;

static int rf_CreateMeter(char *title, char *geom, char *color);
static void rf_UpdateMeter(int meterid, int value);
static void rf_DestroyMeter(int meterid, int killproc);

void rf_startup_iops_demo(meter_vpos, C, G)
  int  meter_vpos;
  int  C;
  int  G;
{
  char buf[100], title[100];
  int rc;

  vpos = meter_vpos;
  sprintf(buf, "%dx%d-0+%d",RF_DEMO_METER_WIDTH, RF_DEMO_METER_HEIGHT, vpos * (RF_DEMO_METER_HEIGHT+RF_DEMO_METER_VSPACE));
  sprintf(title,"%s %d/%d User IOs/sec",demoMeterTags[rf_demoMeterTag],C,G);
  user_iops_meter = rf_CreateMeter(title, buf, "black");
  sprintf(buf, "%dx%d-%d+%d",RF_DEMO_METER_WIDTH, RF_DEMO_METER_HEIGHT, RF_DEMO_METER_WIDTH+RF_DEMO_METER_SPACING,vpos * (RF_DEMO_METER_HEIGHT+RF_DEMO_METER_VSPACE));
  sprintf(title,"%s %d/%d Disk IOs/sec",demoMeterTags[rf_demoMeterTag],C,G);
  disk_iops_meter = rf_CreateMeter(title, buf, "red");
  if (doMax) {
    sprintf(buf, "%dx%d-%d+%d",RF_DEMO_METER_WIDTH, RF_DEMO_METER_HEIGHT, 2*(RF_DEMO_METER_WIDTH+RF_DEMO_METER_SPACING),vpos * (RF_DEMO_METER_HEIGHT+RF_DEMO_METER_VSPACE));
    sprintf(title,"%s %d/%d Avg User IOs/s",demoMeterTags[rf_demoMeterTag],C,G);
    max_user_meter  = rf_CreateMeter(title, buf, "black");
    sprintf(buf, "%dx%d-%d+%d",RF_DEMO_METER_WIDTH, RF_DEMO_METER_HEIGHT, 3*(RF_DEMO_METER_WIDTH+RF_DEMO_METER_SPACING), vpos * (RF_DEMO_METER_HEIGHT+RF_DEMO_METER_VSPACE));
    sprintf(title,"%s %d/%d Avg Disk IOs/s",demoMeterTags[rf_demoMeterTag],C,G);
    max_disk_meter  = rf_CreateMeter(title, buf, "red");
    sprintf(buf, "%dx%d-%d+%d",RF_DEMO_METER_WIDTH, RF_DEMO_METER_HEIGHT, 4*(RF_DEMO_METER_WIDTH+RF_DEMO_METER_SPACING), vpos * (RF_DEMO_METER_HEIGHT+RF_DEMO_METER_VSPACE));
  } else {
    sprintf(buf, "%dx%d-%d+%d",RF_DEMO_METER_WIDTH, RF_DEMO_METER_HEIGHT, 2*(RF_DEMO_METER_WIDTH+RF_DEMO_METER_SPACING), vpos * (RF_DEMO_METER_HEIGHT+RF_DEMO_METER_VSPACE));
  }
  sprintf(title,"%s %d/%d Avg User Resp Time (ms)",demoMeterTags[rf_demoMeterTag],C,G);
  avg_resp_time_meter  = rf_CreateMeter(title, buf, "blue");
  rc = rf_mutex_init(&iops_mutex);
  if (rc) {
    RF_ERRORMSG3("Unable to init mutex file %s line %d rc=%d\n", __FILE__,
      __LINE__, rc);
    return;
  }
  user_ios_so_far = disk_ios_so_far = max_user = max_disk = 0;
  user_resp_time_sum_ms = 0;

  meter_update_terminate = 0;
#ifndef SIMULATE
  pthread_create(&update_thread_desc, raidframe_attr_default, (pthread_startroutine_t)rf_meter_update_thread, NULL);
#endif /* !SIMULATE */
  gettimeofday(&iops_starttime, NULL);
  iops_initialized = 1;
}


void rf_update_user_stats(resptime)
  int  resptime;
{
  if (!iops_initialized && !recon_initialized) return;
  RF_LOCK_MUTEX(iops_mutex);
  user_ios_so_far++;
  user_resp_time_sum_ms += resptime;
  RF_UNLOCK_MUTEX(iops_mutex);
}

void rf_update_disk_iops(val)
  int  val;
{
  if (!iops_initialized) return;
  RF_LOCK_MUTEX(iops_mutex);
  disk_ios_so_far += val;
  RF_UNLOCK_MUTEX(iops_mutex);
}

void rf_meter_update_thread()
{
  struct timeval now, diff;
  int iops, resptime;
  float secs;
  
#ifndef SIMULATE
  while (!meter_update_terminate) {   
    gettimeofday(&now, NULL);
    RF_TIMEVAL_DIFF(&iops_starttime, &now, &diff);
    secs = ((float) diff.tv_sec) + ((float) diff.tv_usec)/1000000.0;
#else /* !SIMULATE */
    secs = rf_cur_time;
#endif /* !SIMULATE */
    if (user_iops_meter >= 0) {
      iops = (secs!=0.0) ? (int) (((float) user_ios_so_far) / secs) : 0;
      rf_UpdateMeter(user_iops_meter, iops);
      if (max_user_meter && iops > max_user) {max_user = iops; rf_UpdateMeter(max_user_meter, iops);}
    }

    if (disk_iops_meter >= 0) {
      iops = (secs!=0.0) ? (int) (((float) disk_ios_so_far) / secs) : 0;
      rf_UpdateMeter(disk_iops_meter, iops);
      if (max_disk_meter && iops > max_disk) {max_disk = iops; rf_UpdateMeter(max_disk_meter, iops);}
    }
    
    if (recon_pctg_meter >= 0) {
      rf_UpdateMeter(recon_pctg_meter, recon_pctg);
    }
   
    switch (rf_demo_op_mode){
       case RF_DEMO_FAULT_FREE:  
         resptime = (user_ios_so_far != 0) ? user_resp_time_sum_ms / user_ios_so_far : 0;
         if (resptime && (ff_avg_resp_time_meter >=0)) 
		rf_UpdateMeter(ff_avg_resp_time_meter, resptime);
         user_ios_ff += user_ios_so_far;
	 user_resp_time_sum_ff += user_resp_time_sum_ms;
	 break;
       case RF_DEMO_DEGRADED:
         resptime = (user_ios_so_far != 0) ? user_resp_time_sum_ms / user_ios_so_far : 0;
         if (resptime &&(deg_avg_resp_time_meter >=0)) 
		rf_UpdateMeter(deg_avg_resp_time_meter, resptime);
	 user_ios_deg += user_ios_so_far;
	 user_resp_time_sum_deg += user_resp_time_sum_ms;
       case RF_DEMO_RECON:
         resptime = (user_ios_so_far != 0) ? user_resp_time_sum_ms / user_ios_so_far : 0;
         if (resptime && (recon_avg_resp_time_meter >= 0)) 
		rf_UpdateMeter(recon_avg_resp_time_meter, resptime);
	 user_ios_recon += user_ios_so_far;
	 user_resp_time_sum_recon += user_resp_time_sum_ms;
	 break;
       default: printf("WARNING: demo meter update thread: Invalid op mode! \n");
    }
    user_ios_so_far = 0;
    user_resp_time_sum_ms = 0;
#ifndef SIMULATE
    RF_DELAY_THREAD(1,0);
  }
#endif /* !SIMULATE */
}

void rf_finish_iops_demo()
{
  long status;

  if (!iops_initialized) return;
  iops_initialized = 0;              /* make sure any subsequent update calls don't do anything */
  meter_update_terminate = 1;
#ifndef SIMULATE
  pthread_join(update_thread_desc, (pthread_addr_t)&status);
#endif /* !SIMULATE */

  rf_DestroyMeter(user_iops_meter, (doMax) ? 1 : 0);
  rf_DestroyMeter(disk_iops_meter, (doMax) ? 1 : 0);
  rf_DestroyMeter(max_user_meter, 0);
  rf_DestroyMeter(max_disk_meter, 0);
  rf_DestroyMeter(avg_resp_time_meter, 0);
  rf_mutex_destroy(&iops_mutex);
}

void rf_demo_update_mode(arg_mode)
  int  arg_mode;
{
  int hpos;
  char buf[100], title[100];

  switch (rf_demo_op_mode = arg_mode) {
  case RF_DEMO_DEGRADED: 

    /* freeze fault-free response time meter; create degraded mode meter */
    hpos=rf_demoMeterHpos+2; 
    sprintf(buf, "%dx%d-%d+%d",RF_DEMO_METER_WIDTH, RF_DEMO_METER_HEIGHT, hpos * (RF_DEMO_METER_WIDTH+RF_DEMO_METER_SPACING), vpos * (RF_DEMO_METER_HEIGHT+RF_DEMO_METER_VSPACE));
    sprintf(title,"Degraded Mode Average Response Time (ms)",demoMeterTags[rf_demoMeterTag]);
    deg_avg_resp_time_meter = rf_CreateMeter(title, buf, "purple");
    rf_UpdateMeter(ff_avg_resp_time_meter, (user_ios_ff == 0)? 0: user_resp_time_sum_ff/user_ios_ff);
    break;

  case RF_DEMO_RECON:

    /* freeze degraded mode response time meter; create recon meters */
    hpos = rf_demoMeterHpos+1;
    sprintf(buf, "%dx%d-%d+%d",RF_DEMO_METER_WIDTH, RF_DEMO_METER_HEIGHT, hpos * (RF_DEMO_METER_WIDTH+RF_DEMO_METER_SPACING), vpos * (RF_DEMO_METER_HEIGHT+RF_DEMO_METER_VSPACE));
    sprintf(title,"Reconstruction Average Response Time (ms)",demoMeterTags[rf_demoMeterTag]);
    recon_avg_resp_time_meter  = rf_CreateMeter(title, buf, "darkgreen");
    sprintf(buf, "%dx%d-%d+%d",RF_DEMO_METER_WIDTH, RF_DEMO_METER_HEIGHT, (rf_demoMeterHpos) * (RF_DEMO_METER_WIDTH + RF_DEMO_METER_SPACING), vpos * (RF_DEMO_METER_HEIGHT+RF_DEMO_METER_VSPACE));
     sprintf(title,"Percent Complete / Recon Time");     
     recon_pctg_meter = rf_CreateMeter(title,buf,"red");
    rf_UpdateMeter(deg_avg_resp_time_meter, (user_ios_deg == 0)? 0: user_resp_time_sum_deg/user_ios_deg);
   break;

  default: /*do nothing -- finish_recon_demo will update rest of meters */;
  }
   
}


/****************************************************************************************
 * reconstruction demo code
 ***************************************************************************************/


void rf_startup_recon_demo(meter_vpos, C, G, init)
  int  meter_vpos;
  int  C;
  int  G;
  int  init;
{
  char buf[100], title[100];
  int rc;

  vpos = meter_vpos;
  if (init) {
      /* init demo -- display ff resp time meter */
     sprintf(buf, "%dx%d-%d+%d",RF_DEMO_METER_WIDTH, RF_DEMO_METER_HEIGHT, (rf_demoMeterHpos+3) * (RF_DEMO_METER_WIDTH+RF_DEMO_METER_SPACING), vpos * (RF_DEMO_METER_HEIGHT+RF_DEMO_METER_VSPACE));
     sprintf(title,"%s %d/%d Fault-Free Avg User Resp Time (ms)",demoMeterTags[rf_demoMeterTag],C,G);
     ff_avg_resp_time_meter  = rf_CreateMeter(title, buf, "blue");
  }  
  rc = rf_mutex_init(&iops_mutex);
  if (rc) {
    RF_ERRORMSG3("Unable to init mutex file %s line %d rc=%d\n", __FILE__,
      __LINE__, rc);
  }

  meter_update_terminate = 0;
#ifndef SIMULATE
  pthread_create(&update_thread_desc, raidframe_attr_default, (pthread_startroutine_t)rf_meter_update_thread, NULL);
#endif /* !SIMULATE */
  gettimeofday(&iops_starttime, NULL);
  recon_initialized = 1;
}

void rf_update_recon_meter(val)
  int  val;
{
  recon_pctg = val;
}


void rf_finish_recon_demo(etime)
  struct timeval  *etime;
{
  long status;
  int hpos;

  hpos = rf_demoMeterHpos;

  recon_initialized = 0;         /* make sure any subsequent 
				update calls don't do anything */
  recon_pctg = etime->tv_sec;      /* display recon time on meter */

  rf_UpdateMeter(recon_avg_resp_time_meter, (user_ios_recon == 0)? 0: user_resp_time_sum_recon/user_ios_recon);

  rf_UpdateMeter(recon_pctg_meter, etime->tv_sec);

  meter_update_terminate = 1;

#ifndef SIMULATE
  pthread_join(update_thread_desc, (pthread_addr_t)&status);   /* join the meter update thread */
#endif /* !SIMULATE */
  rf_DestroyMeter(recon_pctg_meter, 0);
  rf_DestroyMeter(ff_avg_resp_time_meter, 0);
  rf_DestroyMeter(deg_avg_resp_time_meter, 0);
  rf_DestroyMeter(recon_avg_resp_time_meter, 0);
  rf_mutex_destroy(&iops_mutex);
}


/****************************************************************************************
 * meter manipulation code
 ***************************************************************************************/

#define MAXMETERS 50
static struct meter_info { int sd; int pid; char name[100]; } minfo[MAXMETERS];
static int meter_num = 0;

int rf_ConfigureMeters()
{
  int i;
  for (i=0; i<MAXMETERS; i++)
    minfo[i].sd = -1;
  return(0);
}

/* forks a dmeter process to create a 4-digit meter window
 * "title" appears in the title bar of the meter window
 * returns an integer handle (really a socket descriptor) by which
 * the new meter can be accessed.
 */
static int rf_CreateMeter(title, geom, color)
  char  *title;
  char  *geom;
  char  *color;
{
  char geombuf[100], *clr;
  int sd, pid, i, status;
  struct sockaddr sa;

  if (!geom) sprintf(geombuf,"120x40-0+%d", 50*meter_num); else sprintf(geombuf, "%s", geom);
  clr = (color) ? color : "black";
  sprintf(minfo[meter_num].name,"/tmp/xm_%d",meter_num);
  unlink(minfo[meter_num].name);

  if ( !(pid = fork()) ) {
    execlp("dmeter","dmeter","-noscroll","-t",title,"-geometry",geombuf,"-sa",minfo[meter_num].name,"-fg",clr,NULL);
    perror("rf_CreateMeter: exec failed");
    return(-1);
  }

  sd = socket(AF_UNIX,SOCK_STREAM,0);
  sa.sa_family = AF_UNIX;
  strcpy(sa.sa_data, minfo[meter_num].name);
  for (i=0; i<50; i++) {        /* this give us 25 seconds to get the meter running */
    if ( (status = connect(sd,&sa,sizeof(sa))) != -1) break;
#ifdef SIMULATE
    sleep (1);
#else /* SIMULATE */
   RF_DELAY_THREAD(0, 500);
#endif /* SIMULATE */
  }
  if (status == -1) {
    perror("Unable to connect to meter");
    exit(1);
  }
  minfo[meter_num].sd = sd;
  minfo[meter_num].pid = pid;
  return(meter_num++);
}

/* causes the meter to display the given value */
void rf_UpdateMeter(meterid, value)
  int  meterid;
  int  value;
{
  if (write(minfo[meterid].sd, &value, sizeof(int)) < sizeof(int)) {
    fprintf(stderr,"Unable to write to meter %d\n",meterid);
  }
}

void rf_DestroyMeter(meterid, killproc)
  int  meterid;
  int  killproc;
{
  close(minfo[meterid].sd);
  if (killproc) kill(minfo[meterid].pid, SIGTERM);
  minfo[meterid].sd = -1;
}

int rf_ShutdownAllMeters()
{
  int i;

  for (i=0; i<MAXMETERS; i++)
    if (minfo[i].sd >= 0)
      rf_DestroyMeter(i, 0);
  return(0);
}

#endif /* RF_DEMO > 0 */
