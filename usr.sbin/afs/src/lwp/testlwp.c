/*	$OpenBSD: testlwp.c,v 1.1.1.1 1998/09/14 21:53:13 art Exp $	*/
/*
 * Copyright (c) 1998 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      Högskolan and its contributors.
 * 
 * 4. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * testlwp
 *
 * Checks if lwp seams to work, and demostrates how to use lwp.
 * Give multiple commands om the argumentline to run several
 * tests on the same time.
 * 
 * $KTH: testlwp.c,v 1.2 1998/07/15 12:50:58 map Exp $
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <lwp.h>
#include <unistd.h>
#include <sys/time.h>

static void Producer(void *foo) ;
static void Consumer(void *foo) ;

#define LWPTEST_STACKSIZE (16*1024)

/*
 * Classic Producer Consumer thread testing
 *
 * Observe the use of SignalNoYield, you can't signal anything
 * that isn't sleeping.
 */

static char pcfoo = 'a' - 1;

static void 
Producer(void *foo)
{
    while (1) {
	LWP_WaitProcess((char *)Producer);
	pcfoo++;
	if (pcfoo > 'z')
	    pcfoo = 'a';
	printf("[producer] creating %c\n", pcfoo);
	LWP_NoYieldSignal ((char *)Consumer);
    }
}

static void 
Consumer(void *foo)
{
    LWP_NoYieldSignal ((char *)Producer);
    while (1) {
	LWP_WaitProcess((char *)Consumer);
	printf("[consumer] eating %c\n", pcfoo);
	LWP_NoYieldSignal ((char *)Producer);
    }
}

static int
startPCtest(void)
{
    PROCESS consumer, producer;

    if (LWP_CreateProcess (Producer, LWPTEST_STACKSIZE, 1,
			   NULL, "producer", &consumer))
	errx (1, "Cannot create producer process");
    
    if (LWP_CreateProcess (Consumer, LWPTEST_STACKSIZE, 1,
			   NULL, "consumer", &producer))
	errx (1, "Cannot create consumer process");
    return 0;
}

/*
 * Test the LWP_Sleep() function
 */

static void
MrSleepy(void *foo)
{
    printf("[mrsleepy] I'm going to bed\n");
    while (1) {
	IOMGR_Sleep(1);
	printf("[mrsleepy] yawn\n");
    }
}

static int
putMrSleeptoBed(void)
{
    PROCESS mrsleepy;

    if (LWP_CreateProcess (MrSleepy, LWPTEST_STACKSIZE, 1,
			   NULL, "mrsleepy", &mrsleepy))
	errx (1, "Cannot create consumer process");
    return 0;
}

/*
 * The Producer Consumer thingy done over a pipe
 */

int pipa[2] = { -1, -1 };

static void 
SelectProducer(void *foo)
{
    char *str = (char *) foo;
    int len ;
    
    if (str == NULL)
	str = "foo";
    len = strlen (str) ;

    while(1) {
	IOMGR_Sleep(1) ;
	printf("[selprodu] %s\n", str);
    }
}

static void 
SelectConsumer(void *foo)
{
    char str[200];
    int len ;
    fd_set readset;
    
    while(1) {
	FD_ZERO(&readset);
	FD_SET(pipa[0], &readset);
	IOMGR_Select(pipa[0] + 1, &readset, NULL, NULL, NULL); 
	len = read(pipa[0], str, 199);
	if (len < 0)
	    err(1, "read");
	str[len] = '\0';
	printf("[selcomsu] %s\n", str);
    }
}

void
startSelectPC (char *progname)
{
    int pid;
    PROCESS consumer;

    if (pipa[0] == -1) {
	if (pipe(pipa))
	    err(1, "pipe");
	
	if (LWP_CreateProcess (SelectConsumer, LWPTEST_STACKSIZE, 1,
			       NULL, "selconsu", &consumer))
	    errx (1, "Cannot create select consumer process");
    }

    pid = fork();
    switch (pid) {
    case -1: /* error */
	err(1, "fork");
    case 0: /* child */
	close(0);
	if (dup2(pipa[1], 0) == -1)
	    err(1, "dup2");
	close(pipa[1]);
	execl(progname, "testlwp", "selectproducer", NULL);
	err(1, "execl");
    default:
	break;
    }
}

/*
 * Test cancel
 */

static void
onStrike(void *foo)
{
    while (1) {
	printf("[onstrike] I'll never quit, block, block, block\n");
	IOMGR_Select(0, NULL, NULL, NULL, NULL);
	printf("[onstrike] Bah, you will never pay enough\n");
    }
}

static void
freeEnterprise(void *foo)
{
    PROCESS *pid = (PROCESS *) foo;

    while(1) {
	IOMGR_Sleep(1);
	printf("[enterpri] Raise salery\n");
	IOMGR_Cancel(*pid);
    }
}

static void
yaEndlessLoop(void) 
{
    static PROCESS worker, enterprise;

    if (LWP_CreateProcess (onStrike, LWPTEST_STACKSIZE, 1,
			   NULL, "worker", &worker))
	errx (1, "Cannot create worker process");
    
    if (LWP_CreateProcess (freeEnterprise, LWPTEST_STACKSIZE, 1,
			   (void *)&worker, "enterprise", &enterprise))
	errx (1, "Cannot create enterprise process");
}


/*
 * Usage
 */

static void
usage(char *progname)
{
    fprintf(stderr, "usage: %s cmd ...\nWhere cmd is one of:\n", progname);
    fprintf(stderr, 
	    "pc\t\tProducer Consumer test\n"
	    "sleep\t\tSleeptest\n"
	    "selectconsumer\tSelect consumer\n"
	    "selectproducer\t(special case, just print a string on stdout repeatally)\n"
	    "cancel\t\tTest iomgr cancel\n"
	    "version\t\tPrint version\n");

    printf("Use several of these tests together to test their interopability\n");
    exit(1);
}

int main(int argc, char **argv)
{
    PROCESS pid;
    char *progname = strdup(argv[0]);

    if (progname == NULL)
	progname = "foo";

    if (argc <= 1 ) 
	usage(progname);
    
    printf("starting LWP support\n");
    if (LWP_InitializeProcessSupport(LWP_NORMAL_PRIORITY, &pid))
	errx(1, "LWP_InitializeProcessSupport()\n");

    printf("starting IOMGR support\n");
    if (IOMGR_Initialize())
	errx(1, "IOMGR_Initialize()\n");

    while (argv[1]) {
	if (strcasecmp("pc", argv[1]) == 0) {
	    startPCtest();
	} else if (strcasecmp("sleep", argv[1]) == 0) {
	    putMrSleeptoBed();
	} else if (strcasecmp("selectproducer", argv[1]) == 0) {
	    SelectProducer(NULL); 
	    exit(1); /* Special case */
	} else if (strcasecmp("selectconsumer", argv[1]) == 0) {
	    startSelectPC (progname);
	} else if (strcasecmp("cancel", argv[1]) == 0) {
	    yaEndlessLoop();	 
	} else if (strcasecmp("version", argv[1]) == 0) {
	    printf("Version: $KTH: testlwp.c,v 1.2 1998/07/15 12:50:58 map Exp $\n");
	} else {
	    printf("unknown command %s\n", argv[1]);
	}

	argc--;
	argv++;
    }
    LWP_WaitProcess((char *) main);
    return 0;
}


