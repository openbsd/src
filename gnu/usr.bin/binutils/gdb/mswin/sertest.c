/*
 * ser.c:
 *    stand-alone program to test gdb's serial communication under win32s.
 *    
 *    mode com1:9600,n,7,1,-
 *    
 * SERIAL data and defines:
 *    SERIAL
 *    SERIAL_1_AND_A_HALF_STOPBITS 2 
 *    SERIAL_1_STOPBITS              
 *    SERIAL_2_STOPBITS 3
 *    SERIAL_BREAK, 0);
 *    SERIAL_COPY_TTY_STATE
 *    SERIAL_EOF
 *    SERIAL_ERROR -1         
 *    SERIAL_H
 *    SERIAL_T
 *    SERIAL_TIMEOUT
 * 
 * SERIAL functionss:
 *    SERIAL_CLOSE(st2000_desc);
 *    SERIAL_FDOPEN (0);
 *    SERIAL_FLUSH_INPUT (wiggler_desc);
 *    SERIAL_FLUSH_OUTPUT (mips_desc);
 *    SERIAL_GET_TTY_STATE(monitor_desc);
 *    SERIAL_NOFLUSH_SET_TTY_STATE (stdin_serial, our_ttystate,
 *    SERIAL_OPEN(dev_name);
 *    SERIAL_PRINT_TTY_STATE (stdin_serial, inferior_ttystate);
 *    SERIAL_RAW (desc);
 *    SERIAL_READCHAR
 *    SERIAL_RESTORE(0, &ttystate);
 *    SERIAL_SEND_BREAK (mips_desc);
 *    SERIAL_SETBAUDRATE (desc, baud_rate)) {
 *    SERIAL_SETSTOPBITS (monitor_desc, mon_ops->stopbits);
 *    SERIAL_SET_TTY_STATE (args->serial, args->state);
 *    SERIAL_UN_FDOPEN (gdb_stdout_serial);
 *    SERIAL_WAIT_2 (tty_desc, port_desc, -1);
 *    SERIAL_WRITE(tty_desc, &cx, 1);
*/


#include "defs.h"
#include "debugo.h"
#include "serial.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* 
 * Set these as needed to reflect effective mode command
 */
char dev_name[] = "com1";
  int baud_rate=9600;
  int timeout = 50;

/* 
 * Descriptor for I/O to remote machine.  Initialize it to NULL so that
 * array_open knows that we don't have a file open when the program starts.
 */
serial_t desc = NULL;
void _initialize_ser_win32s(void);

static int ser_readchar(int timeout)
{
  int c;
  c = SERIAL_READCHAR(desc, timeout);
  if (c >= 0)
    return c & 0x7f;
  if (c == SERIAL_TIMEOUT) {
    if (timeout <= 0)
      return c;		/* Polls shouldn't generate timeout errors */
    error("error:Timeout reading from remote system.\n");
  }
  error("error:ser_readchar\n");
}

static int ser_connect(char* dev_name, int baud_rate)
{
  if (desc == NULL)
    error("error:dev_name=%s\n",dev_name);
  if (baud_rate != -1) {
    if (SERIAL_SETBAUDRATE (desc, baud_rate)) {
      SERIAL_CLOSE (desc);
      error("error:dev_name=%s\n",dev_name);
    }
  }
  SERIAL_RAW(desc);
  printf("Remote target %s ser_connected \n", dev_name);
  return 1;
}

/*
 * ser_write -- send raw data to monitor.
 */
static void ser_write(data, len)
     char data[];
     int len;
{
  if (SERIAL_WRITE(desc, data, len))
    fprintf(stderr, "SERIAL_WRITE failed: %s\n", safe_strerror(errno));
  *(data + len+1) = '\0';
  printf("Sending: \"%s\".", data);

}
 

void main(void)
{
  int i;
  char buf[]="\n\rsomething \nto say\r\n";
  extern int write_dos_tick_delay;

  _initialize_ser_win32s();

  write_dos_tick_delay = 10;

  desc = SERIAL_OPEN(dev_name);
  ser_connect(dev_name, baud_rate);

  ser_write(buf, strlen(buf));

  for (i=0; i < 20; i++) 
  {
    unsigned int c = ser_readchar(timeout);
    if (c >= 'A' && c <= 'Z' || c >= 'a' && c <= 'z')
      printf("%d read char %c\n",i,c);
    else
      printf("%d read char 0x%x\n",i,c);
  }

  SERIAL_CLOSE(desc);
  desc=0;
}
