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
 * serdrv.c - Synchronous Serial Driver for Angel.
 *            This is nice and simple just to get something going.
 */

#ifdef __hpux
#  define _POSIX_SOURCE 1
#endif

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

extern int baud_rate;   /* From gdb/top.c */

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
#  define UNUSED(x) (x = x)      /* Silence compiler warnings */
#endif
 
#define MAXREADSIZE 512
#define MAXWRITESIZE 512

#define SERIAL_FC_SET  ((1<<serial_XON)|(1<<serial_XOFF))
#define SERIAL_CTL_SET ((1<<serial_STX)|(1<<serial_ETX)|(1<<serial_ESC))
#define SERIAL_ESC_SET (SERIAL_FC_SET|SERIAL_CTL_SET)

static const struct re_config config = {
    serial_STX, serial_ETX, serial_ESC, /* self-explanatory?               */
    SERIAL_FC_SET,                      /* set of flow-control characters  */
    SERIAL_ESC_SET,                     /* set of characters to be escaped */
    NULL /* serial_flow_control */, NULL  ,    /* what to do with FC chars */
    angel_DD_RxEng_BufferAlloc, NULL                /* how to get a buffer */
};

static struct re_state rxstate;

typedef struct writestate {
  unsigned int wbindex;
  /*  static te_status testatus;*/
  unsigned char writebuf[MAXWRITESIZE];
  struct te_state txstate;
} writestate;

static struct writestate wstate;

/*
 * The set of parameter options supported by the device
 */
static unsigned int baud_options[] = {
#if defined(B115200) || defined(__hpux)
    115200,
#endif
#if defined(B57600) || defined(__hpux)
    57600, 
#endif
    38400, 19200, 9600
};

static ParameterList param_list[] = {
    { AP_BAUD_RATE,
      sizeof(baud_options)/sizeof(unsigned int),
      baud_options }
};

static const ParameterOptions serial_options = {
    sizeof(param_list)/sizeof(ParameterList), param_list };

/* 
 * The default parameter config for the device
 */
static Parameter param_default[] = {
    { AP_BAUD_RATE, 9600 }
};

static ParameterConfig serial_defaults = {
    sizeof(param_default)/sizeof(Parameter), param_default };

/*
 * The user-modified options for the device
 */
static unsigned int user_baud_options[sizeof(baud_options)/sizeof(unsigned)];

static ParameterList param_user_list[] = {
    { AP_BAUD_RATE,
      sizeof(user_baud_options)/sizeof(unsigned),
      user_baud_options }
};

static ParameterOptions user_options = {
    sizeof(param_user_list)/sizeof(ParameterList), param_user_list };

static bool user_options_set;

/* forward declarations */
static int serial_reset( void );
static int serial_set_params( const ParameterConfig *config );
static int SerialMatch(const char *name, const char *arg);

static void process_baud_rate( unsigned int target_baud_rate )
{
    const ParameterList *full_list;
    ParameterList       *user_list;

    /* create subset of full options */
    full_list = Angel_FindParamList( &serial_options, AP_BAUD_RATE );
    user_list = Angel_FindParamList( &user_options,   AP_BAUD_RATE );

    if ( full_list != NULL && user_list != NULL )
    {
        unsigned int i, j;
        unsigned int def_baud = 0;

        /* find lower or equal to */
        for ( i = 0; i < full_list->num_options; ++i )
           if ( target_baud_rate >= full_list->option[i] )
           {
               /* copy remaining */
               for ( j = 0; j < (full_list->num_options - i); ++j )
                  user_list->option[j] = full_list->option[i+j];
               user_list->num_options = j;

               /* check this is not the default */
               Angel_FindParam( AP_BAUD_RATE, &serial_defaults, &def_baud );
               if ( (j == 1) && (user_list->option[0] == def_baud) )
               {
#ifdef DEBUG
                   printf( "user selected default\n" );
#endif
               }
               else
               {
                   user_options_set = TRUE;
#ifdef DEBUG
                   printf( "user options are: " );
                   for ( j = 0; j < user_list->num_options; ++j )
                      printf( "%u ", user_list->option[j] );
                   printf( "\n" );
#endif
               }

               break;   /* out of i loop */
           }
                
#ifdef DEBUG
        if ( i >= full_list->num_options )
           printf( "couldn't match baud rate %u\n", target_baud_rate );
#endif
    }
#ifdef DEBUG
    else
       printf( "failed to find lists\n" );
#endif
}

static int SerialOpen(const char *name, const char *arg)
{
    const char *port_name = name;

#ifdef DEBUG
    printf("SerialOpen: name %s arg %s\n", name, arg ? arg : "<NULL>");
#endif

#ifdef COMPILING_ON_WINDOWS
    if (IsOpenSerial()) return -1;
#else
    if (Unix_IsSerialInUse()) return -1;
#endif

#ifdef COMPILING_ON_WINDOWS
    if (SerialMatch(name, arg) != adp_ok)
        return adp_failed;
#else
    port_name = Unix_MatchValidSerialDevice(port_name);
# ifdef DEBUG
    printf("translated port to %s\n", port_name == 0 ? "NULL" : port_name);
# endif
    if (port_name == 0) return adp_failed;
#endif

    user_options_set = FALSE;

    /* interpret and store the arguments */
    if ( arg != NULL )
    {
        unsigned int target_baud_rate;
        target_baud_rate = (unsigned int)strtoul(arg, NULL, 10);
        if (target_baud_rate > 0)
        {
#ifdef DEBUG
            printf( "user selected baud rate %u\n", target_baud_rate );
#endif
            process_baud_rate( target_baud_rate );
        }
#ifdef DEBUG
        else
           printf( "could not understand baud rate %s\n", arg );
#endif
    }
    else if (baud_rate > 0)
    {
      /* If the user specified a baud rate on the command line "-b" or via
         the "set remotebaud" command then try to use that one */
      process_baud_rate( baud_rate );
    }

#ifdef COMPILING_ON_WINDOWS
    {
        int port = IsValidDevice(name);
        if (OpenSerial(port, FALSE) != COM_OK)
            return -1;
    }
#else
    if (Unix_OpenSerial(port_name) < 0)
      return -1;
#endif

    serial_reset();

#if defined(__unix) || defined(__CYGWIN__)
    Unix_ioctlNonBlocking();
#endif

    Angel_RxEngineInit(&config, &rxstate);
    /*
     * DANGER!: passing in NULL as the packet is ok for now as it is just
     * IGNOREd but this may well change
     */
    Angel_TxEngineInit(&config, NULL, &wstate.txstate); 
    return 0;
}

static int SerialMatch(const char *name, const char *arg)
{
    UNUSED(arg);
#ifdef COMPILING_ON_WINDOWS
    if (IsValidDevice(name) == COM_DEVICENOTVALID)
        return -1;
    else
        return 0;
#else
    return Unix_MatchValidSerialDevice(name) == 0 ? -1 : 0;
#endif
}

static void SerialClose(void)
{
#ifdef DO_TRACE
    printf("SerialClose()\n");
#endif

#ifdef COMPILING_ON_WINDOWS
    CloseSerial();
#else
    Unix_CloseSerial();
#endif
}

static int SerialRead(DriverCall *dc, bool block) {
  static unsigned char readbuf[MAXREADSIZE];
  static int rbindex=0;

  int nread;
  int read_errno;
  int c=0;
  re_status restatus;
  int ret_code = -1;            /* assume bad packet or error */

  /* must not overflow buffer and must start after the existing data */
#ifdef COMPILING_ON_WINDOWS
  {
    BOOL dummy = FALSE;
    nread = BytesInRXBufferSerial();

    if (nread > MAXREADSIZE - rbindex)
      nread = MAXREADSIZE - rbindex;

    if ((read_errno = ReadSerial(readbuf+rbindex, nread, &dummy)) == COM_READFAIL)
    {
        MessageBox(GetFocus(), "Read error\n", "Angel", MB_OK | MB_ICONSTOP);
        return -1;   /* SJ - This really needs to return a value, which is picked up in */
                     /*      DevSW_Read as meaning stop debugger but don't kill. */
    }
    else if (pfnProgressCallback != NULL && read_errno == COM_OK)
    {
      progressInfo.nRead += nread;
      (*pfnProgressCallback)(&progressInfo);
    }
  }
#else
  nread = Unix_ReadSerial(readbuf+rbindex, MAXREADSIZE-rbindex, block);
  read_errno = errno;
#endif

  if ((nread > 0) || (rbindex > 0)) {

#ifdef DO_TRACE
    printf("[%d@%d] ", nread, rbindex);
#endif

    if (nread>0)
       rbindex = rbindex+nread;

    do {
      restatus = Angel_RxEngine(readbuf[c], &(dc->dc_packet), &rxstate);
#ifdef DO_TRACE
      printf("<%02X ",readbuf[c]);
      if (!(++c % 16))
          printf("\n");
#else
      c++;
#endif
    } while (c<rbindex &&
             ((restatus == RS_IN_PKT) || (restatus == RS_WAIT_PKT)));

#ifdef DO_TRACE
   if (c % 16)
        printf("\n");
#endif

    switch(restatus) {
      
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
        printf("SerialRead() processed %d, moving down %d\n", c, rbindex-c);
#endif
        if (c != rbindex) memmove((char *) readbuf, (char *) (readbuf+c),
                                  rbindex-c);
        rbindex -= c;
        break;

      case RS_IN_PKT:
      case RS_WAIT_PKT:
        rbindex = 0;            /* will have processed all we had */
        ret_code = 0;
        break;

      default:
#ifdef DEBUG
        printf("Bad re_status in serialRead()\n");
#endif
        break;
    }
  } else if (nread == 0)
    ret_code = 0;               /* nothing to read */
  else if (read_errno == ERRNO_FOR_BLOCKED_IO) /* nread < 0 */
    ret_code = 0;

#ifdef DEBUG
  if ((nread<0) && (read_errno!=ERRNO_FOR_BLOCKED_IO))
    perror("read() error in serialRead()");
#endif

  return ret_code;
}


static int SerialWrite(DriverCall *dc) {
  int nwritten = 0;
  te_status testatus = TS_IN_PKT;

  if (dc->dc_context == NULL) {
    Angel_TxEngineInit(&config, &(dc->dc_packet), &(wstate.txstate));
    wstate.wbindex = 0;
    dc->dc_context = &wstate;
  }

  while ((testatus == TS_IN_PKT) && (wstate.wbindex < MAXWRITESIZE))
  {
    /* send the raw data through the tx engine to escape and encapsulate */
    testatus = Angel_TxEngine(&(dc->dc_packet), &(wstate.txstate),
                              &(wstate.writebuf)[wstate.wbindex]);
    if (testatus != TS_IDLE) wstate.wbindex++;
  }

  if (testatus == TS_IDLE) {
#ifdef DEBUG
    printf("SerialWrite: testatus is TS_IDLE during preprocessing\n");
#endif
  }

#ifdef DO_TRACE
  { 
    int i = 0;

    while (i<wstate.wbindex)
    {
        printf(">%02X ",wstate.writebuf[i]);

        if (!(++i % 16))
            printf("\n");
    }
    if (i % 16)
        printf("\n");
  }
#endif

#ifdef COMPILING_ON_WINDOWS
  if (WriteSerial(wstate.writebuf, wstate.wbindex) == COM_OK)
  {
    nwritten = wstate.wbindex;
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
  nwritten = Unix_WriteSerial(wstate.writebuf, wstate.wbindex);

  if (nwritten < 0) {
    nwritten=0;
  }
#endif

#ifdef DEBUG
  if (nwritten > 0)
    printf("Wrote %#04x bytes\n", nwritten);
#endif

  if ((unsigned) nwritten == wstate.wbindex && 
      (testatus == TS_DONE_PKT || testatus == TS_IDLE)) {

    /* finished sending the packet */

#ifdef DEBUG
    printf("SerialWrite: calling Angel_TxEngineInit after sending packet (len=%i)\n",wstate.wbindex);
#endif
    testatus = TS_IN_PKT;
    wstate.wbindex = 0;
    return 1;
  }
  else {
#ifdef DEBUG
    printf("SerialWrite: Wrote part of packet wbindex=%i, nwritten=%i\n",
           wstate.wbindex, nwritten);
#endif
   
    /*
     *  still some data left to send shuffle whats left down and reset
     * the ptr
     */
    memmove((char *) wstate.writebuf, (char *) (wstate.writebuf+nwritten),
            wstate.wbindex-nwritten);
    wstate.wbindex -= nwritten;
    return 0;
  }
  return -1;
}


static int serial_reset( void )
{
#ifdef DEBUG
    printf( "serial_reset\n" );
#endif

#ifdef COMPILING_ON_WINDOWS
    FlushSerial();
#else
    Unix_ResetSerial();
#endif

    return serial_set_params( &serial_defaults );
}


static int find_baud_rate( unsigned int *speed )
{
    static struct {
          unsigned int baud;
          int termiosValue;
    } possibleBaudRates[] = {
#if defined(__hpux)
        {115200,_B115200}, {57600,_B57600},
#else
#ifdef B115200
        {115200,B115200},
#endif
#ifdef B57600
	{57600,B57600},
#endif
#endif
#ifdef COMPILING_ON_WINDOWS
        {38400,CBR_38400}, {19200,CBR_19200}, {9600, CBR_9600}, {0,0}
#else
        {38400,B38400}, {19200,B19200}, {9600, B9600}, {0,0}
#endif
    };
    unsigned int i;

    /* look for lower or matching -- will always terminate at 0 end marker */
    for ( i = 0; possibleBaudRates[i].baud > *speed; ++i )
       /* do nothing */ ;

    if ( possibleBaudRates[i].baud > 0 )
       *speed = possibleBaudRates[i].baud;

    return possibleBaudRates[i].termiosValue;
}


static int serial_set_params( const ParameterConfig *config )
{
    unsigned int speed;
    int termios_value;

#ifdef DEBUG
    printf( "serial_set_params\n" );
#endif

    if ( ! Angel_FindParam( AP_BAUD_RATE, config, &speed ) )
    {
#ifdef DEBUG
        printf( "speed not found in config\n" );
#endif
        return DE_OKAY;
    }

    termios_value = find_baud_rate( &speed );
    if ( termios_value == 0 )
    {
#ifdef DEBUG
        printf( "speed not valid: %u\n", speed );
#endif
        return DE_OKAY;
    }

#ifdef DEBUG
    printf( "setting speed to %u\n", speed );
#endif

#ifdef COMPILING_ON_WINDOWS
    SetBaudRate((WORD)termios_value);
#else
    Unix_SetSerialBaudRate(termios_value);
#endif

    return DE_OKAY;
}


static int serial_get_user_params( ParameterOptions **p_options )
{
#ifdef DEBUG
    printf( "serial_get_user_params\n" );
#endif

    if ( user_options_set )
    {
        *p_options = &user_options;
    }
    else
    {
        *p_options = NULL;
    }

    return DE_OKAY;
}


static int serial_get_default_params( ParameterConfig **p_config )
{
#ifdef DEBUG
    printf( "serial_get_default_params\n" );
#endif

    *p_config = (ParameterConfig *) &serial_defaults;
    return DE_OKAY;
}


static int SerialIoctl(const int opcode, void *args) {

    int ret_code;

#ifdef DEBUG
    printf( "SerialIoctl: op %d arg %p\n", opcode, args ? args : "<NULL>");
#endif

    switch (opcode)
    {
       case DC_RESET:         
           ret_code = serial_reset();
           break;

       case DC_SET_PARAMS:     
           ret_code = serial_set_params((const ParameterConfig *)args);
           break;

       case DC_GET_USER_PARAMS:     
           ret_code = serial_get_user_params((ParameterOptions **)args);
           break;

       case DC_GET_DEFAULT_PARAMS:
           ret_code = serial_get_default_params((ParameterConfig **)args);
           break;

       default:               
           ret_code = DE_BAD_OP;
           break;
    }

  return ret_code;
}

DeviceDescr angel_SerialDevice = {
    "SERIAL",
    SerialOpen,
    SerialMatch,
    SerialClose,
    SerialRead,
    SerialWrite,
    SerialIoctl
};
