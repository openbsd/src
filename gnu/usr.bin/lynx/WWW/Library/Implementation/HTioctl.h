/*
 *  A routine to mimic the ioctl function for UCX.
 *  Bjorn S. Nilsson, 25-Nov-1993. Based on an example in the UCX manual.
 */
#include <iodef.h>
#define IOC_OUT (int)0x40000000
extern int vaxc$get_sdc(), sys$qiow();

#ifndef UCX$C_IOCTL
#define UCX$C_IOCTL TCPIP$C_IOCTL
#endif
