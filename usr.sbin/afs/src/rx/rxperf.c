/*
 * Copyright (c) 2000 Kungliga Tekniska Högskolan
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

RCSID("$Id: rxperf.c,v 1.1 2000/09/11 14:41:23 art Exp $");

#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <strings.h>
#include <assert.h>
#include <unistd.h>
#include <cmd.h>

#include <roken.h>
#include <err.h>

#include "rx.h"
#include "rx_null.h"

#define DEFAULT_PORT 4711
#define DEFAULT_HOST "127.0.0.1"
#define DEFAULT_BYTES 1000000
#define RXPERF_BUFSIZE 1400

enum { RX_PERF_VERSION = 0 };
enum { RX_SERVER_ID = 147 };
enum { RX_PERF_SEND = 0, RX_PERF_RECV = 1 };


/*
 *
 */

static void
sigusr1 (int foo)
{
    exit (2); /* XXX profiler */
}

/*
 *
 */

static struct timeval timer_start;
static struct timeval timer_stop;
static int timer_check = 0;

static void
start_timer (void)
{
    timer_check++;
    gettimeofday (&timer_start, NULL);
}

/*
 *
 */

static void
end_and_print_timer (char *str)
{
    long long start_l, stop_l;

    timer_check--; 
    assert (timer_check == 0);
    gettimeofday(&timer_stop, NULL);
    start_l = timer_start.tv_sec * 1000000 + timer_start.tv_usec;
    stop_l = timer_stop.tv_sec * 1000000 + timer_stop.tv_usec;
    printf("%s:\t%8llu msec\n", str, (stop_l-start_l)/1000);
}

/*
 *
 */

static u_long
str2addr (const char *s)
{
    struct in_addr server;
    struct hostent *h;

    if (inet_aton (s, &server) == 1)
	return server.s_addr;
    h = gethostbyname (s);
    if (h != NULL) {
	memcpy (&server, h->h_addr_list[0], sizeof(server));
	return server.s_addr;
    }
    return 0;
}


/*
 *
 */

static void
get_sec(int serverp, struct rx_securityClass** sec, int *secureindex)
{
    if (serverp) {
	*sec = rxnull_NewServerSecurityObject();
	*secureindex = 1;
    } else {
	*sec = rxnull_NewClientSecurityObject();
	*secureindex = 0;
    }
}

/*
 * process the "RPC" and return the results
 */

static int32_t
rxperf_ExecuteRequest(struct rx_call *call)
{
    int32_t version;
    int32_t command;
    u_int32_t bytes;
    char buf[RXPERF_BUFSIZE];
    int size;

    if (rx_Read (call, &version, 4) != 4) {
	warn ("rx_Read failed to read version");
	return -1;
    }

    if (htonl(RX_PERF_VERSION) != version) {
	warnx ("client has wrong version");
	return -1;
    }
	
    if (rx_Read (call, &command, 4) != 4) {
	warnx ("rx_Read failed to read command");
	return -1;
    }
    command = ntohl(command);

    if (rx_Read (call, &bytes, 4) != 4) {
	warnx ("rx_Read failed to read bytes");
	return -1;
    }
    bytes = ntohl(bytes);

    memset (buf, 0, sizeof(buf));

    switch (command) {
    case RX_PERF_SEND:
	while (bytes > 0) {
	    size = sizeof(buf);
	    if (size > bytes)
		size = bytes;
	    if (rx_Read (call, buf, size) != size)
		errx (1, "rx_Read failed to read data");
	    bytes -= size;
	}
	{
	    int32_t data = htonl(4711); /* XXX */
	    if (rx_Write (call, &data, 4) != 4)
		errx (1, "rx_Write failed when sending back result");
	}
	break;
    case RX_PERF_RECV:
    default:
	warnx ("client sent a unsupported command: %d", command);
	return -1;
    }
    return 0;
}

/*
 *
 */

static void
do_server (int port)
{
    struct rx_service *service;
    struct rx_securityClass *secureobj;
    int secureindex;
    int ret;

    ret = rx_Init (port);
    if (ret)
	errx (1, "rx_Init failed");

    get_sec(1, &secureobj, &secureindex);
    
    service = rx_NewService (0,
			     RX_SERVER_ID,
			     "rxperf", 
			     &secureobj, 
			     secureindex, 
			     rxperf_ExecuteRequest);
    if (service == NULL) 
	errx(1, "Cant create server");

    rx_StartServer(1) ;
    abort();
}

/*
 *
 */

static void
do_client (const char *server, int port, int32_t bytes)
{
    struct rx_connection *conn;
    struct rx_call *call;
    u_int32_t addr = str2addr(server);
    struct rx_securityClass *secureobj;
    int secureindex;
    char buf[RXPERF_BUFSIZE];
    int32_t data;
    int size;
    int ret;
    char *stamp;

    memset (buf, 0, sizeof(buf));

    ret = rx_Init (0);
    if (ret)
	errx (1, "rx_Init failed");

    get_sec(0, &secureobj, &secureindex);

    conn = rx_NewConnection(addr, 
			    port, 
			    RX_SERVER_ID,
			    secureobj,
			    secureindex);
    if (conn == NULL)
	errx (1, "failed to contact %s", server);

    call = rx_NewCall (conn);
    if (call == NULL)
	errx (1, "rx_NewCall failed");

    data = htonl(RX_PERF_VERSION);
    if (rx_Write (call, &data, 4) != 4)
	errx (1, "rx_Write failed to send version");

    data = htonl(RX_PERF_SEND);
    if (rx_Write (call, &data, 4) != 4)
	errx (1, "rx_Write failed to send command");

    asprintf (&stamp, "send       %d bytes", bytes);
    start_timer();
    
    data = htonl (bytes);
    if (rx_Write (call, &data, 4) != 4)
	errx (1, "rx_Write failed to send size");

    while (bytes > 0) {
	size = sizeof (buf);
	if (size > bytes)
	    size = bytes;
	if (rx_Write (call, buf, size) != size)
	    errx (1, "failed when %d bytes was left to send", bytes);
	bytes -= size;
    }
    if (rx_Read (call, &bytes, 4) != 4)
	errx (1, "failed to read result from server");
    end_and_print_timer (stamp);
    rx_EndCall (call, 0);
}

/*
 * do argument processing and call networking functions
 */

static int
rxperf_server (struct cmd_syndesc *as, void *arock)
{
    int port	   = DEFAULT_PORT;
    char *portname;
    char *ptr;

    if (as->parms[0].items) {
	portname = as->parms[1].items->data;
	port = strtol (portname, &ptr, 0);
	if (ptr && ptr != '\0')
	    errx (1, "can't resolve portname");
    }
    
    do_server (htons(port));

    return 0;
}

/*
 * do argument processing and call networking functions
 */

static int
rxperf_client (struct cmd_syndesc *as, void *arock)
{
    char *host	   = DEFAULT_HOST;
    int bytes	   = DEFAULT_BYTES;
    int port	   = DEFAULT_PORT;
    char *numbytes = NULL;
    char *portname;
    char *ptr;

    if (as->parms[0].items)
	host = as->parms[0].items->data;

    if (as->parms[1].items) {
	portname = as->parms[1].items->data;
	port = strtol (portname, &ptr, 0);
	if (ptr && ptr != '\0')
	    errx (1, "can't resolve portname");
    }
    
    if (as->parms[2].items) {
	numbytes = as->parms[2].items->data;
	bytes = strtol (numbytes, &ptr, 0);
	if (ptr && *ptr != '\0')
	    errx (1, "can't resolve number of bytes to transfer");
    }

    do_client (host, htons(port), bytes);

    return 0;
}

/*
 * setup world and call cmd
 */

int
main(int argc, char **argv)
{
    struct cmd_syndesc *ts;
    PROCESS pid;

    set_progname (argv[0]);

    signal (SIGUSR1, sigusr1);

    ts = cmd_CreateSyntax("server", rxperf_server, NULL, "server");
    cmd_AddParm(ts, "-port", CMD_SINGLE, CMD_OPTIONAL, "udp port");

    ts = cmd_CreateSyntax("client", rxperf_client, NULL, "client");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_OPTIONAL, "server machine");
    cmd_AddParm(ts, "-port", CMD_SINGLE, CMD_OPTIONAL, "udp port");
    cmd_AddParm(ts, "-bytes", CMD_SINGLE, CMD_OPTIONAL, "number of bytes to transfer");

    LWP_InitializeProcessSupport (LWP_NORMAL_PRIORITY, &pid);

    cmd_Dispatch(argc, argv);
    return 0;
}
