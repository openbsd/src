/*	$NetBSD: scsi_hi.c,v 1.1 1996/08/02 11:22:31 ragge Exp $	*/

/****************************************************************************
 * NS32K Monitor SCSI high-level driver
 * Bruce Culbertson
 * 8 March 1990
 * (This source is public domain source)
 *
 * There are three monitor SCSI commands.  "Read" and "write" I think are
 * fairly self explanatory once you read the help messages.  They, in fact,
 * execute the "extended read", "extended write", and "request sense"
 * commands from the SCSI standard.
 * 
 * "Raw" lets you execute any SCSI command but you need a SCSI reference to
 * know what the commands are and what their formats are.  The SCSI
 * standard specifies that there are six buffers which, for example, hold a
 * SCSI command or are the source or destination for data.  You provide
 * "raw" with an array of pointers to the six buffers.  Using "edit", you
 * can enter a SCSI command somewhere in memory and you can create the
 * array of pointers.  The array must actually be eight entries long; two
 * entries are not used.  By typing "raw <array address>", the SCSI command
 * is executed.
 * 
 * By the way, "read", "write", and "raw" talk only to the DP8490 SCSI
 * controller.  I have not had time to read the Adaptec data sheet and
 * write a driver for it.
 ****************************************************************************/
#include "so.h"

#define OK 			0
#define NOT_OK			OK+1
#define	PRIVATE
#define PUBLIC
#define U8			unsigned char

long	scsiAdr = DEFAULT_SCSI_ADR,	/* default SCSI address */
	scsiLun = DEFAULT_SCSI_LUN;

struct cmd_desc {			/* SCSI command description */
  const U8	*cmd;			/* command string */
  const U8	*odata;			/* data to output, if any */
  const struct cmd_desc *chain;		/* next command */
};

struct drive {				/* SCSI drive description */
  U8	adr, lun;			/* SCSI address and LUN */
  U8	flags;				/* drive characteristics */
  U8	stat;				/* drive state */
  const struct cmd_desc *init;		/* list of initialize commands */
};
/* for drive.flags */
#define EXTENDED_RDWR		1	/* device does extended read, write */
#define EXTENDED_SENSE		2	/* device does extended sense */
/* for drive.stat */
#define INITIALIZED		1	/* device is initialized */

PRIVATE struct drive drive_tbl[] = {
#if 1
  {0, 0, 0, 1, 0},
  {1, 0, 0, 1, 0},
  {2, 0, 0, 1, 0},
  {3, 0, 0, 1, 0},
  {4, 0, 0, 1, 0},
  {5, 0, 0, 1, 0},
  {6, 0, 0, 1, 0},
  {7, 0, 0, 1, 0},
#else
  {0, 0, EXTENDED_RDWR | EXTENDED_SENSE, 1, 0},
  {1, 0, EXTENDED_RDWR | EXTENDED_SENSE, 1, 0},
  {2, 0, EXTENDED_RDWR | EXTENDED_SENSE, 1, 0},
  {3, 0, EXTENDED_RDWR | EXTENDED_SENSE, 1, 0},
  {4, 0, EXTENDED_RDWR | EXTENDED_SENSE, 1, 0},
  {5, 0, EXTENDED_RDWR | EXTENDED_SENSE, 1, 0},
  {6, 0, EXTENDED_RDWR | EXTENDED_SENSE, 1, 0},
  {7, 0, EXTENDED_RDWR | EXTENDED_SENSE, 1, 0},
#endif
};
#define DRV_TBL_SZ (sizeof (drive_tbl) / sizeof (struct drive))

/* Round up to multiple of four since SCSI transfers are always multiples
 * of four bytes.
 */
#define CMD_LEN		12		/* longest SCSI command */
#define SENSE_LEN 	24		/* extended sense length */
#define MSG_LEN		4
#define STAT_LEN	4

#define MAX_SCSI_RETRIES	6
#define CMD_IX		2
#define CMD_SENSE	0x03
#define CMD_READ	0x08
#define CMD_WRITE	0x0a
#define CMD_XREAD	0x28
#define CMD_XWRITE	0x2a
PRIVATE U8		cmd_buf[CMD_LEN];

#define SENSE_KEY	2
#define NO_SENSE	0
#define RECOVERY_ERR	1
#define UNIT_ATTN	6
#define ADD_SENSE_CODE	12
#define SENSE_RST	0x29
PRIVATE	U8		sense_buf[SENSE_LEN];

#define CHECK_CONDITION	2
#define STAT_IX		3
#define STAT_MASK	0x1f
PRIVATE U8		stat_buf[STAT_LEN];
#define IMSG_IX		7
PRIVATE U8		msg_buf[MSG_LEN];

#define ODATA_IX	0
#define IDATA_IX	1
PRIVATE struct scsi_args scsi_args;

/*===========================================================================*
 *				sc_rdwt					     * 
 *===========================================================================*/
/* Carry out a read or write request for the SCSI disk. */
PRIVATE int
sc_rdwt(op, block, ram_adr, len, sc_adr, lun)
long block, ram_adr, len, sc_adr, lun;
{
  int retries, ret;
  U8 *p;
  struct drive *dp;

  printf ("sc_rdwt: op %x, block %d, ram %x, len %d, sc_adr %d, lun %d\n",
	  op, block, ram_adr, len, sc_adr, lun);

  /* get drive characteristics */
  for (dp = drive_tbl; dp < drive_tbl + DRV_TBL_SZ - 1; ++dp)
    if (dp->adr == sc_adr && dp->lun == lun) break;
  if (dp == drive_tbl + DRV_TBL_SZ - 1) {
    dp->adr = sc_adr;			/* have default, set adr, lun */
    dp->lun = lun;
  }
  for (retries = 0; retries < MAX_SCSI_RETRIES; ++retries) {
    if (dp->init && !(dp->stat & INITIALIZED))
      if (OK != sc_initialize (dp)) {
	printf("SCSI cannot initialize device\n");
	return NOT_OK;
      }
    p = cmd_buf;			/* build SCSI command */
    if (dp->flags & EXTENDED_RDWR) {	/* use extended commands */
      *p++ = (op == DISK_READ)? CMD_XREAD: CMD_XWRITE;
      *p++ = lun << 5;
      *p++ = (block >> 24) & 0xff;
      *p++ = (block >> 16) & 0xff;
      *p++ = (block >> 8) & 0xff;
      *p++ = (block >> 0) & 0xff;
      *p++ = 0;
      *p++ = (len >> 8) & 0xff;
      *p++ = (len >> 0) & 0xff;
      *p = 0;
    } else {				/* use short (SASI) commands */
      *p++ = (op == DISK_READ)? CMD_READ: CMD_WRITE;
      *p++ = (lun << 5) | ((block >> 16) & 0x1f);
      *p++ = (block >> 8) & 0xff;
      *p++ = (block >> 0) & 0xff;
      *p++ = len;
      *p = 0;
    }
    if (op == DISK_READ)
      ret = exec_scsi_hi (cmd_buf, (U8 *)ram_adr, (U8 *)0, dp);
    else
      ret = exec_scsi_hi (cmd_buf, (U8 *)0, (U8 *)ram_adr, dp);
    if (OK == ret) return OK;
    dp->stat &= ~INITIALIZED;
  }
  printf("SCSI %s, block %d failed even after retries\n",
    op == DISK_READ? "READ": "WRITE", block);
  return NOT_OK;
}

/*===========================================================================*
 *				sc_initialize				     *
 *===========================================================================*/
/* Execute the list of initialization commands for the given drive.
 */
int
sc_initialize (dp)
struct drive *dp;
{
  const struct cmd_desc *cp;

  for (cp = dp->init; cp != 0; cp = cp->chain)
    if (OK != exec_scsi_hi (cp->cmd, 0, cp->odata, dp)) {
      dp->stat &= ~INITIALIZED;
      return NOT_OK;
    }
  dp->stat |= INITIALIZED;
  return OK;
}

/*===========================================================================*
 *				exec_scsi_hi				     * 
 *===========================================================================*/
/* Execute a "high-level" SCSI command.  This means execute a low level
 * command and, if it fails, execute a request sense to find out why.
 */
PRIVATE int
exec_scsi_hi(cmd, data_in, data_out, dp)
U8 *cmd, *data_out, *data_in;
struct drive *dp;
{
  scsi_args.ptr[CMD_IX] = (long)cmd;
  scsi_args.ptr[STAT_IX] = (long)stat_buf;
  scsi_args.ptr[IMSG_IX] = (long)msg_buf;
  scsi_args.ptr[IDATA_IX] = (long)data_in;
  scsi_args.ptr[ODATA_IX] = (long)data_out;
  if (OK != exec_scsi_low (&scsi_args, dp->adr))
    return NOT_OK;
  *stat_buf &= STAT_MASK;		/* strip off lun */
  if (*stat_buf == 0)
    /* Success -- this should be the usual case */
    return OK;
  if (*stat_buf != CHECK_CONDITION) {
    /* do not know how to handle this so return error */
    printf("SCSI device returned unknown status: %d\n", *stat_buf);
    return NOT_OK;
  }
  /* Something funny happened, need to execute request-sense command
   * to learn more.
   */
  if (OK == get_sense(dp))
    /* Something funny happened, but the device recovered from it and
     * the command succeeded.
     */
    return OK;
  return NOT_OK;
}

/*===========================================================================*
 *				get_sense				     * 
 *===========================================================================*/
/* Execute a "request sense" SCSI command and check results.  When a SCSI
 * command returns CHECK_CONDITION, a request-sense command must be executed.
 * A request-sense command provides information about the original command.
 * The original command might have succeeded, in which case it does not
 * need to be retried and OK is returned.  Examples: read error corrected
 * with error correction code, or error corrected by retries performed by
 * the SCSI device.  The original command also could have failed, in
 * which case NOT_OK is returned.
 */
#define XLOGICAL_ADR	\
  (sense_buf[3]<<24 | sense_buf[4]<<16 | sense_buf[5]<<8 | sense_buf[6])
#define LOGICAL_ADR	\
  (sense_buf[1]<<16 | sense_buf[2]<<8 | sense_buf[3])

PRIVATE int
get_sense (dp)
struct drive *dp;
{
  U8 *p;

  p = cmd_buf;				/* build SCSI command */
  *p++ = CMD_SENSE;
  *p++ = dp->lun << 5;
  *p++ = 0;
  *p++ = 0;
  *p++ = (dp->flags & EXTENDED_SENSE)? SENSE_LEN: 0;
  *p = 0;
  scsi_args.ptr[IDATA_IX] = (long)sense_buf;
  scsi_args.ptr[ODATA_IX] = 0;
  scsi_args.ptr[CMD_IX] = (long)cmd_buf;
  scsi_args.ptr[STAT_IX] = (long)stat_buf;
  scsi_args.ptr[IMSG_IX] = (long)msg_buf;
  if (OK != exec_scsi_low (&scsi_args, dp->adr)) {
    printf("SCSI SENSE command failed\n");
    return NOT_OK;
  }
  if ((*stat_buf & STAT_MASK) != 0) {
    printf("SCSI SENSE returned wrong status %d\n", *stat_buf);
    return NOT_OK;
  }
  if (0 == (dp->flags & EXTENDED_SENSE)) {
    printf("SCSI request sense, code 0x%x, log_adr 0x%x\n",
      sense_buf[0], LOGICAL_ADR);
    return NOT_OK;
  }
  switch (sense_buf[SENSE_KEY] & 0xf) {
    case NO_SENSE:
    case UNIT_ATTN:			/* reset */
      return NOT_OK;			/* must retry command */
    case RECOVERY_ERR:
      /* eventually, we probably do not want to hear about these. */
      printf("SCSI ok with recovery, code 0x%x, logical address 0x%x\n",
	     sense_buf[ADD_SENSE_CODE], XLOGICAL_ADR);
      return OK;			/* orig command was ok with recovery */
    default:
      printf("SCSI failure: key 0x%x code 0x%x log adr 0x%x sense buf 0x%x\n",
	     sense_buf[SENSE_KEY], sense_buf[ADD_SENSE_CODE],
	XLOGICAL_ADR, sense_buf);
      return NOT_OK;			/* orig command failed */
  }
}
