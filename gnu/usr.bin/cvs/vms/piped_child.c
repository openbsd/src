/*
 * piped_child.c
 *
 * An experimental VMS implementation of the same routine in [-.src]run.c
 * <benjamin@cyclic.com>
 *
 * Derived in part from pipe.c, in this directory.
 */

#include "vms.h"
#include "vms-types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iodef.h>
#include <ssdef.h>
#include <syidef.h>
#include <clidef.h>
#include <stsdef.h>
#include <dvidef.h>
#include <nam.h>
#include <descrip.h>
#include <errno.h>
#include <file.h>
#include <lib$routines.h>
#include <starlet.h>

extern int trace;

/* Subprocess IO structure */
typedef struct _SUBIO {
  struct _SUBIO *self;
  struct _SUBIO *prev;
  struct _SUBIO *next;
  short read_chan;
  short write_chan;
  FILE *read_fp;
  FILE *write_fp;
  struct IOSB read_iosb;
  struct IOSB write_iosb;
  int pid;
  int return_status;
  unsigned long event_flag;
  unsigned char event_flag_byte;
} SUBIO;

static SUBIO *siop_head = NULL, *siop_tail = NULL;

static int piped_child_exith(int);

static struct EXHCB piped_child_exit_handler_block = 
  {0, piped_child_exith, 1, &piped_child_exit_handler_block.exh$l_status, 0};

typedef struct
{
  short length;
  char body[NAM$C_MAXRSS+1];
} Vstring;

/* Subprocess Completion AST */
void piped_child_done(siop)
  SUBIO *siop;
{
  struct IOSB iosb;
  int status;

  if (siop->self != siop)
     return;
  siop->read_iosb.status = SS$_NORMAL;
  siop->write_iosb.status = SS$_NORMAL;

}

/* Exit handler, established by piped_child() */
static int
piped_child_exith(status)
  int status;
{
  struct IOSB iosb;
  SUBIO *siop;
  int return_value = 0;
  
  siop = siop_head;
  while (siop)
    {
    if (siop->self != siop)
      {
        return_value = -1;
	continue;
      }

    /* Finish pending reads and shutdown */      
    if(!siop->read_iosb.status)
      {
        fflush (siop->read_fp);
	fclose (siop->read_fp);
      }
    else
      fclose (siop->read_fp);
    sys$dassgn (siop->read_chan);

    /* Finish pending writes and shutdown */
    if(!siop->write_iosb.status)
      {
        fflush (siop->write_fp);
	sys$qio (0, siop->write_chan, IO$_WRITEOF, &iosb,
	         0, 0, 0, 0, 0, 0, 0, 0);
	fclose (siop->write_fp);
      }
    else
      fclose (siop->write_fp);
    sys$dassgn (siop->write_chan);    

    sys$synch (siop->event_flag, &siop->write_iosb);

    siop = siop->next;
    }
  return return_value;
}

int piped_child(command, tofdp, fromfdp)
char **command;
int *tofdp, *fromfdp;
{
  static int exit_handler = 0;
  struct IOSB iosb1, iosb2;
  int rs1, rs2, i;
  unsigned long flags, vmspid, return_status;
  char cmd[1024];
  struct dsc$descriptor_s cmddsc;
  struct dsc$descriptor_s read_mbxdsc, write_mbxdsc;
  SUBIO *siop;
  static Vstring read_mbxname, write_mbxname;
  static struct itm$list3 write_mbxlist[2] = {
      {sizeof(write_mbxname.body)-1, DVI$_DEVNAM,
      &write_mbxname.body, (size_t *) &write_mbxname.length},
      {0, 0, 0, 0} };
  static struct itm$list3 read_mbxlist[2] = {
      {sizeof(read_mbxname.body)-1, DVI$_DEVNAM,
      &read_mbxname.body, (size_t *) &read_mbxname.length},
      {0, 0, 0, 0} };
  
  read_mbxname.length = sizeof(read_mbxname.body);
  write_mbxname.length = sizeof(write_mbxname.body);

  siop = (SUBIO *) calloc(1, sizeof(SUBIO));
  if (!siop)
    {
    perror("piped_child: malloc failed\n");
    return -1;
    }

  siop->self = siop;

  /* Construct command line by concatenating argument list */
  strcpy(cmd, command[0]);
  for(i=1; command[i] != NULL; i++)
     {
     strcat(cmd, " ");
     strcat(cmd, command[i]);
     }

  if(trace)
    fprintf(stderr, "piped_child: running '%s'\n", cmd);

  /* Allocate a pair of temporary mailboxes (2kB each) */
  rs1 = sys$crembx (0, &siop->read_chan, 2048, 2048, 0, 0, 0, 0);
  rs2 = sys$crembx (0, &siop->write_chan, 2048, 2048, 0, 0, 0, 0);

  if (rs1 != SS$_NORMAL || rs2 != SS$_NORMAL)
    {
      vaxc$errno = rs1 | rs2;
      errno = EVMSERR;
      free (siop);
      perror ("piped_child: $CREMBX failure");
      return -1;
    }

  /* Get mailbox names, so we can fopen() them */
  rs1 = sys$getdviw (0, siop->read_chan, 0, &read_mbxlist,
                    &iosb1, 0, 0, 0);

  rs2 = sys$getdviw (0, siop->write_chan, 0, &write_mbxlist,
                    &iosb2, 0, 0, 0);

  if ((rs1 != SS$_NORMAL && !(iosb1.status & STS$M_SUCCESS)) ||
      (rs2 != SS$_NORMAL && !(iosb2.status & STS$M_SUCCESS)))
    {
      vaxc$errno = iosb1.status | iosb2.status;
      errno = EVMSERR;
      sys$dassgn (siop->read_chan);
      sys$dassgn (siop->write_chan);
      free (siop);
      perror ("piped_child: $GETDVIW failure, could not get mailbox names");
      return -1;
    }

  if (trace)
    {
    fprintf(stderr, "piped_child: $GETDVIW succeeded, got mailbox names\n");
    fprintf(stderr, "piped_child: ReadMBX: %s, WriteMBX: %s\n",
            read_mbxname.body, write_mbxname.body);
    }

  /* Make C happy */
  write_mbxname.body[write_mbxname.length] = '\0';
  read_mbxname.body[read_mbxname.length] = '\0';

  /* Make VMS happy */
  write_mbxdsc.dsc$w_length  = write_mbxname.length;
  write_mbxdsc.dsc$b_dtype   = DSC$K_DTYPE_T;
  write_mbxdsc.dsc$b_class   = DSC$K_CLASS_S;
  write_mbxdsc.dsc$a_pointer = write_mbxname.body;

  read_mbxdsc.dsc$w_length  = read_mbxname.length;
  read_mbxdsc.dsc$b_dtype   = DSC$K_DTYPE_T;
  read_mbxdsc.dsc$b_class   = DSC$K_CLASS_S;
  read_mbxdsc.dsc$a_pointer = read_mbxname.body;

  /* Build descriptor for command line */
  cmddsc.dsc$w_length  = strlen(cmd);
  cmddsc.dsc$b_dtype   = DSC$K_DTYPE_T;
  cmddsc.dsc$b_class   = DSC$K_CLASS_S;
  cmddsc.dsc$a_pointer = (char *) cmd;

  flags = CLI$M_NOWAIT;

  /* Allocate an event flag to signal process termination */
  rs1 = lib$get_ef(&siop->event_flag);
  if (rs1 != SS$_NORMAL)
     {
     vaxc$errno = rs1;
     errno = EVMSERR;
     sys$dassgn(siop->read_chan);
     sys$dassgn(siop->write_chan);
     perror("piped_child: LIB$GET_EF failed");
     return -1;
     }

  /* Save the EFN as a byte for later calls to other routines */
  siop->event_flag_byte = 0xff & siop->event_flag;

  if (trace)
     fprintf(stderr, "piped_child: Got an EFN: %d\n", siop->event_flag_byte);

  rs1 = lib$spawn(&cmddsc, &write_mbxdsc, &read_mbxdsc, &flags, 0,
                  &siop->pid, &siop->return_status, &siop->event_flag_byte,
                  &piped_child_done, siop->self);

  if (rs1 != SS$_NORMAL)
     {
       vaxc$errno = rs1;
       errno = EVMSERR;
       sys$dassgn(siop->read_chan);
       sys$dassgn(siop->write_chan);
       perror("piped_child: LIB$SPAWN failure");
       return -1;
     }

  if (trace)
    fprintf(stderr, "piped_child: LIB$SPAWN succeeded, pid is %08x.\n",
            siop->pid);

  /* Establish an exit handler so the process isn't prematurely terminated */
  if (!exit_handler)
    {
      rs1 = sys$dclexh (&piped_child_exit_handler_block);
      if (rs1 != SS$_NORMAL)
        {
	  vaxc$errno = rs1;
	  errno = EVMSERR;
	  sys$dassgn (siop->read_chan);
	  sys$dassgn (siop->write_chan);
	  sys$delprc (siop->pid, 0);
	  free (siop);
	  perror("piped_child: $DCLEXH failure");
	  return -1;
	}
      exit_handler = 1;
    }

  /* Let's open some files */
  siop->read_fp = fopen (read_mbxname.body, "r");
  siop->write_fp = fopen (write_mbxname.body, "w");
  
  if (!siop->read_fp || !siop->write_fp)
    {
      sys$dassgn (siop->read_chan);
      sys$dassgn (siop->write_chan);
      sys$delprc (siop->pid);
      free (siop);
      perror("piped_child: fopen() failed");
      return -1;
    } 

  *fromfdp = fileno(siop->read_fp);
  *tofdp = fileno(siop->write_fp);

  if (trace)
     fprintf(stderr, "piped_child: file open successful: tofd=%d fromfd=%d\n",
             *tofdp, *fromfdp);

  /* Keep track of active subprocess I/O (SUBIO) structures */
  if (siop_head)
    {
      siop_tail->next = siop;
      siop->prev = siop_tail;
      siop_tail = siop;
    }
  else
    siop_head = siop_tail = siop;

  return siop->pid;
}

/*
 * Return codes
 * >0  VMS exit status of subprocess
 *  0  success, subprocess was shutdown
 * -1  pid not found in list of subprocesses
 * -2  memory corruption detected
 */
int
piped_child_shutdown(pid)
  pid_t pid;
{
  int return_status;
  struct IOSB iosb;
  SUBIO *siop = siop_head;
  
  while (siop && siop->self == siop && siop->pid != pid)
    siop = siop->next;
    
  if (!siop)
    return -1;
  else if (siop->self != siop)
    return -2;

  /* Finish reading and writing and shutdown */  
  if (siop->read_iosb.status)
    {
      fflush (siop->read_fp);
      fclose (siop->read_fp);
    }
  else
    fclose(siop->read_fp);
  sys$dassgn (siop->read_chan);
  
   if (siop->write_iosb.status)
    {
      fflush (siop->write_fp);
      sys$qio (0, siop->write_chan, IO$_WRITEOF, &iosb,
               0, 0, 0, 0, 0, 0, 0, 0);
      fclose (siop->write_fp);
    }
  else
    fclose(siop->write_fp);
  sys$dassgn (siop->write_chan);

  sys$synch (siop->event_flag, &siop->write_iosb);
  lib$free_ef(&siop->event_flag);

  /* Ditch SUBIO structure */
  if (siop == siop_tail)
    siop_tail = siop->prev;
  if (siop == siop_head)
    siop_head = siop->next;
  if (siop->prev)
    siop->prev->next = siop->next;
  if (siop->next)
    siop->next->prev = siop->prev;
  
  if (siop->return_status)
    return_status = siop->return_status;
  else
    return_status = 0;

  free (siop);

  return return_status;
}
