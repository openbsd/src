/* 
 * Copyright (C) 1995 Advanced RISC Machines Limited. All rights reserved.
 * 
 * This software may be freely used, copied, modified, and distributed
 * provided that the above copyright notice is preserved in all copies of the
 * software.
 */

/* -*-C-*-
 *
 * $Revision: 1.3 $
 *     $Date: 2004/12/27 14:00:54 $
 *
 *
 * serpardv.c - Serial/Parallel Driver for Angel.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "crc.h"
#include "devices.h"
#include "buffers.h"
#include "rxtx.h"
#include "hostchan.h"
#include "params.h"
#include "logging.h"
#include "hsys.h"

#ifdef COMPILING_ON_WINDOWS
#  undef   ERROR
#  undef   IGNORE
#  include <windows.h>
#  include "angeldll.h"
#  include "comb_api.h"
#else
#  ifdef __hpux
#    define _TERMIOS_INCLUDED
#    include <sys/termio.h>
#    undef _TERMIOS_INCLUDED
#  else
#    include <termios.h>
#  endif
#  include "unixcomm.h"
#endif

#ifndef UNUSED
#  define UNUSED(x) (x = x)     /* Silence compiler warnings */
#endif

#define MAXREADSIZE     512
#define MAXWRITESIZE    512

#define SERPAR_FC_SET  ((1 << serial_XON) | (1 << serial_XOFF))
#define SERPAR_CTL_SET ((1 << serial_STX) | (1 << serial_ETX) | \
                        (1 << serial_ESC))
#define SERPAR_ESC_SET (SERPAR_FC_SET | SERPAR_CTL_SET)

static const struct re_config config = {
    serial_STX, serial_ETX, serial_ESC, /* self-explanatory?               */
    SERPAR_FC_SET,                      /* set of flow-control characters  */
    SERPAR_ESC_SET,                     /* set of characters to be escaped */
    NULL,                               /* serial_flow_control */
    NULL,                               /* what to do with FC chars */
    angel_DD_RxEng_BufferAlloc, NULL    /* how to get a buffer */
};

static struct re_state rxstate;

/*
 * structure used for manipulating transmit data
 */
typedef struct TxState
{
    struct te_state state;
    unsigned int    index;
    unsigned char   writebuf[MAXWRITESIZE];
} TxState;

/*
 * The set of parameter options supported by the device
 */
static unsigned int baud_options[] =
{
#ifdef __hpux
    115200, 57600,
#endif
    38400, 19200, 9600
};

static ParameterList param_list[] =
{
    {
        AP_BAUD_RATE,
        sizeof(baud_options) / sizeof(unsigned int),
        baud_options
    }
};

static const ParameterOptions serpar_options =
{
    sizeof(param_list) / sizeof(ParameterList),
    param_list
};

/*
 * The default parameter config for the device
 */
static Parameter param_default[] =
{
    { AP_BAUD_RATE, 9600 }
};

static const ParameterConfig serpar_defaults =
{
    sizeof(param_default)/sizeof(Parameter),
    param_default
};

/*
 * The user-modified options for the device
 */
static unsigned int user_baud_options[sizeof(baud_options) /
                                     sizeof(unsigned int)];

static ParameterList param_user_list[] =
{
    {
        AP_BAUD_RATE,
        sizeof(user_baud_options) / sizeof(unsigned),
        user_baud_options
    }
};

static ParameterOptions user_options =
{
    sizeof(param_user_list) / sizeof(ParameterList),
    param_user_list
};

static bool user_options_set;

/* forward declarations */
static int serpar_reset(void);
static int serpar_set_params(const ParameterConfig *config);
static int SerparMatch(const char *name, const char *arg);

static void process_baud_rate(unsigned int target_baud_rate)
{
    const ParameterList *full_list;
    ParameterList       *user_list;

    /* create subset of full options */
    full_list = Angel_FindParamList(&serpar_options, AP_BAUD_RATE);
    user_list = Angel_FindParamList(&user_options,   AP_BAUD_RATE);

    if (full_list != NULL && user_list != NULL)
    {
        unsigned int i, j;
        unsigned int def_baud = 0;

        /* find lower or equal to */
        for (i = 0; i < full_list->num_options; ++i)
           if (target_baud_rate >= full_list->option[i])
           {
               /* copy remaining */
               for (j = 0; j < (full_list->num_options - i); ++j)
                  user_list->option[j] = full_list->option[i+j];
               user_list->num_options = j;

               /* check this is not the default */
               Angel_FindParam(AP_BAUD_RATE, &serpar_defaults, &def_baud);
               if ((j == 1) && (user_list->option[0] == def_baud))
               {
#ifdef DEBUG
                   printf("user selected default\n");
#endif
               }
               else
               {
                   user_options_set = TRUE;
#ifdef DEBUG
                   printf("user options are: ");
                   for (j = 0; j < user_list->num_options; ++j)
                      printf("%u ", user_list->option[j]);
                   printf("\n");
#endif
               }

               break;   /* out of i loop */
           }

#ifdef DEBUG
        if (i >= full_list->num_options)
           printf("couldn't match baud rate %u\n", target_baud_rate);
#endif
    }
#ifdef DEBUG
    else
       printf("failed to find lists\n");
#endif
}

static int SerparOpen(const char *name, const char *arg)
{
    char *sername = NULL;
    char *parname = NULL;

#ifdef DEBUG
    printf("SerparOpen: name %s arg %s\n", name, arg ? arg : "<NULL>");
#endif

#ifdef COMPILING_ON_WINDOWS
    if (IsOpenSerial() || IsOpenParallel()) return -1;
#else
    if (Unix_IsSerialInUse() || Unix_IsParallelInUse()) return -1;
#endif

#ifdef COMPILING_ON_WINDOWS
    if (SerparMatch(name, arg) == -1)
        return -1;
#else
    Unix_IsValidParallelDevice(name,&sername,&parname);
# ifdef DEBUG
    printf("translated %s to serial %s and parallel %s\n",
           name==0 ? "NULL" : name,
           sername==0 ? "NULL" : sername,
           parname==0 ? "NULL" : parname);
# endif
    if (sername==NULL || parname==NULL) return -1;
#endif

    user_options_set = FALSE;

    /* interpret and store the arguments */
    if (arg != NULL)
    {
        unsigned int target_baud_rate;

        target_baud_rate = (unsigned int)strtoul(arg, NULL, 10);

        if (target_baud_rate > 0)
        {
#ifdef DEBUG
            printf("user selected baud rate %u\n", target_baud_rate);
#endif
            process_baud_rate(target_baud_rate);
        }
#ifdef DEBUG
        else
            printf("could not understand baud rate %s\n", arg);
#endif
    }

#ifdef COMPILING_ON_WINDOWS
    {
        /*
         * The serial port number is in name[0] followed by
         * the parallel port number in name[1]
         */

        int sport = name[0] - '0';
        int pport = name[1] - '0';

        if (OpenParallel(pport) != COM_OK)
            return -1;

        if (OpenSerial(sport, FALSE) != COM_OK)
        {
            CloseParallel();
            return -1;
        }
    }
#else
    Unix_OpenParallel(parname);
    Unix_OpenSerial(sername);
#endif

    serpar_reset();

#if defined(__unix) || defined(__CYGWIN__)
    Unix_ioctlNonBlocking();
#endif

    Angel_RxEngineInit(&config, &rxstate);

    return 0;
}

#ifdef COMPILING_ON_WINDOWS
static int SerparMatch(const char *name, const char *arg)
{
    char sername[2];
    char parname[2];

    UNUSED(arg);

    sername[0] = name[0];
    parname[0] = name[1];
    sername[1] = parname[1] = 0;

    if (IsValidDevice(sername) == COM_DEVICENOTVALID ||
        IsValidDevice(parname) == COM_DEVICENOTVALID)
        return -1;
    else
        return 0;
}
#else
static int SerparMatch(const char *portstring, const char *arg)
{
    char *sername=NULL, *parname=NULL;
    UNUSED(arg);

    Unix_IsValidParallelDevice(portstring,&sername,&parname);

      /* Match failed if either sername or parname are still NULL */
    if (sername==NULL || parname==NULL) return -1;
    return 0;
}
#endif

static void SerparClose(void)
{
#ifdef COMPILING_ON_WINDOWS
    CloseParallel();
    CloseSerial();
#else
    Unix_CloseParallel();
    Unix_CloseSerial();
#endif
}

static int SerparRead(DriverCall *dc, bool block)
{
    static unsigned char readbuf[MAXREADSIZE];
    static int rbindex = 0;

    int nread;
    int read_errno;
    int c = 0;
    re_status restatus;
    int ret_code = -1;            /* assume bad packet or error */

    /*
     * we must not overflow buffer, and must start after
     * the existing data
     */
#ifdef COMPILING_ON_WINDOWS
    {
        BOOL dummy = FALSE;
        nread = BytesInRXBufferSerial();

        if (nread > MAXREADSIZE - rbindex)
            nread = MAXREADSIZE - rbindex;
        read_errno = ReadSerial(readbuf+rbindex, nread, &dummy);
        if (pfnProgressCallback != NULL && read_errno == COM_OK)
        {
            progressInfo.nRead += nread;
            (*pfnProgressCallback)(&progressInfo);
        }
    }
#else
    nread = Unix_ReadSerial(readbuf+rbindex, MAXREADSIZE-rbindex, block);
    read_errno = errno;
#endif

    if ((nread > 0) || (rbindex > 0))
    {
#ifdef DO_TRACE
        printf("[%d@%d] ", nread, rbindex);
#endif

        if (nread > 0)
            rbindex = rbindex + nread;

        do
        {
            restatus = Angel_RxEngine(readbuf[c], &(dc->dc_packet), &rxstate);

#ifdef DO_TRACE
            printf("<%02X ",readbuf[c]);
#endif
            c++;
        } while (c < rbindex &&
                 ((restatus == RS_IN_PKT) || (restatus == RS_WAIT_PKT)));

#ifdef DO_TRACE
        printf("\n");
#endif

        switch(restatus)
        {
          case RS_GOOD_PKT:
              ret_code = 1;
              /* fall through to: */

          case RS_BAD_PKT:
              /*
               * We now need to shuffle any left over data down to the
               * beginning of our private buffer ready to be used
               *for the next packet
               */
#ifdef DO_TRACE
              printf("SerparRead() processed %d, moving down %d\n",
                     c, rbindex - c);
#endif

              if (c != rbindex)
                  memmove((char *) readbuf, (char *) (readbuf + c), rbindex - c);

              rbindex -= c;

              break;

          case RS_IN_PKT:
          case RS_WAIT_PKT:
              rbindex = 0;            /* will have processed all we had */
              ret_code = 0;
              break;

          default:
#ifdef DEBUG
              printf("Bad re_status in SerparRead()\n");
#endif
              break;
        }
    }
    else if (nread == 0)
        /* nothing to read */
        ret_code = 0;
    else if (read_errno == ERRNO_FOR_BLOCKED_IO) /* nread < 0 */
        ret_code = 0;

#ifdef DEBUG
    if ((nread < 0) && (read_errno != ERRNO_FOR_BLOCKED_IO))
        perror("read() error in SerparRead()");
#endif

    return ret_code;
}

/*
 *  Function: send_packet
 *   Purpose: Send a stream of bytes to Angel through the parallel port
 *
 * Algorithm: We need to present the data in a form that all boards can
 *            swallow.  With the PID board, this is a problem: for reasons
 *            described in the driver (angel/pid/st16c552.c), data are
 *            sent a nybble at a time on D0-D2 and D4; D3 is wired to ACK,
 *            which generates an interrupt when it goes low.  This routine
 *            fills in an array of nybbles, with ACK clear in all but the
 *            last one.  If, for whatever reason, the write fails, then
 *            ACK is forced high (thereby enabling the next write a chance
 *            to be noticed when the falling edge of ACK generates an
 *            interrupt (hopefully).
 *
 *    Params:
 *       Input: txstate Contains the packet to be sent
 *
 *   Returns: Number of *complete* bytes written
 */

static int SerparWrite(DriverCall *dc)
{
    te_status status;
    int nwritten = 0;
    static TxState txstate;

    /*
     * is this a new packet?
     */
    if (dc->dc_context == NULL)
    {
        /*
         * yes - initialise TxEngine
         */
        Angel_TxEngineInit(&config, &dc->dc_packet, &txstate.state);

        txstate.index = 0;
        dc->dc_context = &txstate;
    }

    /*
     * fill the buffer using the Tx Engine
     */
    do
    {
        status = Angel_TxEngine(&dc->dc_packet, &txstate.state,
                                &txstate.writebuf[txstate.index]);
        if (status != TS_IDLE) txstate.index++;

    } while (status == TS_IN_PKT && txstate.index < MAXWRITESIZE);

#ifdef DO_TRACE
    {
        unsigned int i = 0;

        while (i < txstate.index)
        {
            printf(">%02X ", txstate.writebuf[i]);

            if (!(++i % 16))
                putc('\n', stdout);
        }

        if (i % 16)
            putc('\n', stdout);
    }
#endif

    /*
     * the data are ready, all we need now is to send them out
     * in a form that Angel can swallow.
     */
#ifdef COMPILING_ON_WINDOWS
  if (WriteParallel(txstate.writebuf, txstate.index) == COM_OK)
  {
    nwritten = txstate.index;
    if (pfnProgressCallback != NULL)
    {
      progressInfo.nWritten += nwritten;
      (*pfnProgressCallback)(&progressInfo);
    }
  }
  else
  {
      MessageBox(GetFocus(), "Write error\n", "Angel", MB_OK | MB_ICONSTOP);
      return -1;   /* SJ - This really needs to return a value, which is picked up in */
                   /*      DevSW_Read as meaning stop debugger but don't kill. */
  }
#else
    nwritten = Unix_WriteParallel(txstate.writebuf, txstate.index);
#endif

    if (nwritten < 0) nwritten = 0;

#ifdef DO_TRACE
    printf("SerparWrite: wrote %d out of %d bytes\n",
           nwritten, txstate.index);
#endif

    /*
     * has the whole packet gone?
     */
    if (nwritten == (int)txstate.index &&
        (status == TS_DONE_PKT || status == TS_IDLE))
        /*
         * yes it has
         */
        return 1;
    else
    {
        /*
         * if some data are left, shuffle them
         * to the start of the buffer
         */
        if (nwritten != (int)txstate.index && nwritten != 0)
        {
            txstate.index -= nwritten;
            (void)memmove((char *) txstate.writebuf,
                          (char *) (txstate.writebuf + nwritten),
                          txstate.index);
        }
        else if (nwritten == (int)txstate.index)
            txstate.index = 0;

        return 0;
    }
}

static int serpar_reset(void)
{
#ifdef COMPILING_ON_WINDOWS
    FlushParallel();
    FlushSerial();
#else
    Unix_ResetParallel();
    Unix_ResetSerial();
#endif

    return serpar_set_params(&serpar_defaults);
}

static int find_baud_rate(unsigned int *speed)
{
    static struct
    {
        unsigned int baud;
        int termiosValue;
    } possibleBaudRates[] =
      {
#if defined(__hpux)
          {115200, _B115200}, {57600, _B57600},
#endif
#ifdef COMPILING_ON_WINDOWS
        {38400, CBR_38400}, {19200, CBR_19200}, {9600, CBR_9600}, {0, 0}
#else
        {38400, B38400}, {19200, B19200}, {9600, B9600}, {0, 0}
#endif
    };
    unsigned int i;

    /* look for lower or matching -- will always terminate at 0 end marker */
    for (i = 0; possibleBaudRates[i].baud > *speed; ++i)
        /* do nothing */
        ;

    if (possibleBaudRates[i].baud > 0)
       *speed = possibleBaudRates[i].baud;

    return possibleBaudRates[i].termiosValue;
}

static int serpar_set_params(const ParameterConfig *config)
{
    unsigned int speed;
    int termios_value;

#ifdef DEBUG
    printf("serpar_set_params\n");
#endif

    if (!Angel_FindParam(AP_BAUD_RATE, config, &speed))
    {
#ifdef DEBUG
        printf("speed not found in config\n");
#endif
        return DE_OKAY;
    }

    termios_value = find_baud_rate(&speed);
    if (termios_value == 0)
    {
#ifdef DEBUG
        printf("speed not valid: %u\n", speed);
#endif
        return DE_OKAY;
    }

#ifdef DEBUG
    printf("setting speed to %u\n", speed);
#endif

#ifdef COMPILING_ON_WINDOWS
    SetBaudRate((WORD)termios_value);
#else
    Unix_SetSerialBaudRate(termios_value);
#endif

    return DE_OKAY;
}


static int serpar_get_user_params(ParameterOptions **p_options)
{
#ifdef DEBUG
    printf("serpar_get_user_params\n");
#endif

    if (user_options_set)
    {
        *p_options = &user_options;
    }
    else
    {
        *p_options = NULL;
    }

    return DE_OKAY;
}


static int serial_get_default_params( const ParameterConfig **p_config )
{
#ifdef DEBUG
    printf( "serial_get_default_params\n" );
#endif

    *p_config = &serpar_defaults;
    return DE_OKAY;
}


static int SerparIoctl(const int opcode, void *args)
{
    int ret_code;

#ifdef DEBUG
    printf("SerparIoctl: op %d arg %p\n", opcode, args ? args : "<NULL>");
#endif

    switch (opcode)
    {
       case DC_RESET:
           ret_code = serpar_reset();
           break;

       case DC_SET_PARAMS:
           ret_code = serpar_set_params((const ParameterConfig *)args);
           break;

       case DC_GET_USER_PARAMS:
           ret_code = serpar_get_user_params((ParameterOptions **)args);
           break;

       case DC_GET_DEFAULT_PARAMS:
           ret_code =
               serial_get_default_params((const ParameterConfig **)args);
           break;

       default:
           ret_code = DE_BAD_OP;
           break;
    }

  return ret_code;
}

DeviceDescr angel_SerparDevice =
{
    "SERPAR",
    SerparOpen,
    SerparMatch,
    SerparClose,
    SerparRead,
    SerparWrite,
    SerparIoctl
};

/* EOF serpardr.c */
