/******************************************************************************
* Copyright 1991 Advanced Micro Devices, Inc.
* 
* This software is the property of Advanced Micro Devices, Inc  (AMD)  which
* specifically  grants the user the right to modify, use and distribute this
* software provided this notice is not removed or altered.  All other rights
* are reserved by AMD.
*
* AMD MAKES NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, WITH REGARD TO THIS
* SOFTWARE.  IN NO EVENT SHALL AMD BE LIABLE FOR INCIDENTAL OR CONSEQUENTIAL
* DAMAGES IN CONNECTION WITH OR ARISING FROM THE FURNISHING, PERFORMANCE, OR
* USE OF THIS SOFTWARE.
*
* So that all may benefit from your experience, please report  any  problems
* or  suggestions about this software to the 29K Technical Support Center at
* 800-29-29-AMD (800-292-9263) in the USA, or 0800-89-1131  in  the  UK,  or
* 0031-11-1129 in Japan, toll free.  The direct dial number is 512-462-4118.
*
* Advanced Micro Devices, Inc.
* 29K Support Products
* Mail Stop 573
* 5900 E. Ben White Blvd.
* Austin, TX 78741
* 800-292-9263
*****************************************************************************
*       NAME	@(#)dfe_test.c	1.4 91/08/06  Daniel Mann
* 
*	This module is used for testing of DFE services.
********************************************************************** HISTORY
*/
#include <stdio.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>
#include "udiproc.h"

extern char	dfe_errmsg[];
UDISessionId	SessionID;
int		test_errno;

sig_handler()
{
    printf("DFE socket shutdown\n");
    test_errno = UDIDisconnect(SessionID);
    if(test_errno)printf("DFE Error: UDIDisconnect failed\n");
    if(test_errno)printf("DFE errno= %d  errmsg = %s\n",
	test_errno, dfe_errmsg);
    exit();
}

/***************************************************************** MAIN
*/
main(argc, argv)
int 	argc;
char*	argv[];
{
    char	*session = argv[1];
    char	buf[256];
    int		iarray[4];
    int		cnt;

    if(argc < 2)
    {	fprintf(stderr, "ERROR, format:\n");
    	fprintf(stderr, "%s session_id \n", argv[0]);
	exit();
    }
    signal(SIGINT, sig_handler);
    signal(SIGQUIT, sig_handler);
    signal(SIGTERM, sig_handler);

    test_errno = UDIConnect(argv[1], &SessionID);
    if(test_errno)printf("Error: UDIConnect failed errno=%d\n", test_errno);
    if(test_errno)
    {	printf("%s\n", dfe_errmsg);
	exit(1);
    }

    for(;;)
    {
	printf("\ninput an INT ....");
	scanf("%d", &cnt);
	printf("input a word ....");
	scanf("%s", buf);
	printf("input 4 INTs (for an array)....");
	scanf("%d%d%d%d", &iarray[0],&iarray[1],&iarray[2], &iarray[3]);
	UDITest(cnt, buf, iarray);
    }
}
