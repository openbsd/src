/*
 * Copyright (c) 2000 - 2002 Kungliga Tekniska Högskolan
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
 * 3. Neither the name of the Institute nor the names of its contributors
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

/* 
 * We are using getopt since we want it to be possible to link to
 * transarc libs.
 */

#define HAVE_GETRUSAGE 1

#ifdef RCSID
RCSID("$arla: rxperf.c,v 1.23 2003/04/08 00:19:47 lha Exp $");
#endif

#include <stdarg.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/stat.h>
#if HAVE_GETRUSAGE
#include <sys/resource.h>
#endif
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <signal.h>
#ifdef HAVE_ERRX
#include <err.h> /* not stricly right, but if we have a errx() there
		  * is hopefully a err.h */
#endif
#include "rx.h"
#include "rx_globs.h"
#include "rx_null.h"

#if defined(u_int32)
#define uint32_t u_int32
#elif defined(hget32)
#define uint32_t afs_uint32
#endif

static const char *__progname;

#ifndef HAVE_WARNX
static void
warnx(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    fprintf(stderr, "%s: ", __progname);
    vfprintf (stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}
#endif /* !HAVE_WARNX */
     
#ifndef HAVE_ERRX
static void
errx(int eval, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    fprintf(stderr, "%s: ", __progname);
    vfprintf (stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);

    exit(eval);
}
#endif /* !HAVE_ERRX */

#ifndef HAVE_WARN
static void
warn(const char *fmt, ...)
{
    va_list args;
    char *errstr;

    va_start(args, fmt);
    fprintf(stderr, "%s: ", __progname);
    vfprintf (stderr, fmt, args);

    errstr = strerror(errno);
    
    fprintf(stderr, ": %s\n", errstr ? errstr : "unknown error");
    va_end(args);
}
#endif /* !HAVE_WARN */
     
#ifndef HAVE_ERR
static void
err(int eval, const char *fmt, ...)
{
    va_list args;
    char *errstr;

    va_start(args, fmt);
    fprintf(stderr, "%s: ", __progname);
    vfprintf (stderr, fmt, args);

    errstr = strerror(errno);
    
    fprintf(stderr, ": %s\n", errstr ? errstr : "unknown error");
    va_end(args);

    exit(eval);
}
#endif /* !HAVE_ERR */

#define DEFAULT_PORT 7009	/* To match tcpdump */
#define DEFAULT_HOST "127.0.0.1"
#define DEFAULT_BYTES 1000000
#define RXPERF_BUFSIZE 10000

enum { RX_PERF_VERSION = 3 };
enum { RX_SERVER_ID = 147 };
enum { RX_PERF_UNKNOWN = -1, RX_PERF_SEND = 0, RX_PERF_RECV = 1, 
       RX_PERF_RPC=3, RX_PERF_FILE=4 };

enum { RXPERF_MAGIC_COOKIE = 0x4711 };

/*
 *
 */

#if DEBUG
#define DBFPRINT(x) do { printf x ; } while(0)
#else
#define DBFPRINT(x)
#endif

static void
sigusr1 (int foo)
{
    exit (2); /* XXX profiler */
}

static void
sigint (int foo)
{
    rx_Finalize();
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

#ifndef INADDR_NONE
#define INADDR_NONE 0xffffffff
#endif
    if (inet_addr(s) != INADDR_NONE)
        return inet_addr(s);
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

char somebuf[RXPERF_BUFSIZE];

int32_t rxwrite_size = sizeof(somebuf);
int32_t rxread_size = sizeof(somebuf);

static int
readbytes(struct rx_call *call, int32_t bytes)
{
    int32_t size;

    while (bytes > 0) {
	size = rxread_size;
	if (size > bytes)
	    size = bytes;
	if (rx_Read (call, somebuf, size) != size)
	    return 1;
	bytes -= size;
    }
    return 0;
}

static int
sendbytes(struct rx_call *call, int32_t bytes)
{
    int32_t size;

    while (bytes > 0) {
	size = rxwrite_size;
	if (size > bytes)
	    size = bytes;
	if (rx_Write (call, somebuf, size) != size)
	    return 1;
	bytes -= size;
    }
    return 0;
}


static int32_t
rxperf_ExecuteRequest(struct rx_call *call)
{
    int32_t version;
    int32_t command;
    uint32_t bytes;
    uint32_t recvb;
    uint32_t sendb;
    uint32_t data;
    uint32_t num;
    uint32_t *readwrite;
    int i;
    int readp=TRUE;

    DBFPRINT(("got a request\n"));

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

    if (rx_Read (call, &data, 4) != 4) {
	warnx ("rx_Read failed to read size");
	return -1;
    }
    rxread_size = ntohl(data);
    if (rxread_size > sizeof(somebuf)) {
	warnx("rxread_size too large %d", rxread_size);
	return -1;
    }

    if (rx_Read (call, &data, 4) != 4) {
	warnx ("rx_Read failed to write size");
	return -1;
    }
    rxwrite_size = ntohl(data);
    if (rxwrite_size > sizeof(somebuf)) {
	warnx("rxwrite_size too large %d", rxwrite_size);
	return -1;
    }

    switch (command) {
    case RX_PERF_SEND:
	DBFPRINT(("got a send request\n"));

	if (rx_Read (call, &bytes, 4) != 4) {
	    warnx ("rx_Read failed to read bytes");
	    return -1;
	}
	bytes = ntohl(bytes);

	DBFPRINT(("reading(%d) ", bytes));
	readbytes(call, bytes);

	data = htonl(RXPERF_MAGIC_COOKIE);
	if (rx_Write (call, &data, 4) != 4) {
	    warnx ("rx_Write failed when sending back result");
	    return -1;
	}
	DBFPRINT(("done\n"));

	break;
    case RX_PERF_RPC:
	DBFPRINT(("got a rpc request, reading commands\n"));
	
	if (rx_Read (call, &recvb, 4) != 4) {
	    warnx ("rx_Read failed to read recvbytes");
	    return -1;
	}
	recvb = ntohl(recvb);
	if (rx_Read (call, &sendb, 4) != 4) {
	    warnx ("rx_Read failed to read recvbytes");
	    return -1;
	}
	sendb = ntohl(sendb);

	DBFPRINT(("read(%d) ", recvb));
	if (readbytes(call, recvb)) {
	    warnx("readbytes failed");
	    return -1;
	}
	DBFPRINT(("send(%d) ", sendb));
	if (sendbytes(call, sendb)) {
	    warnx("sendbytes failed");
	    return -1;
	}
	
	DBFPRINT(("done\n"));

	data = htonl(RXPERF_MAGIC_COOKIE);
	if (rx_Write (call, &data, 4) != 4) {
	    warnx ( "rx_Write failed when sending back magic cookie");
	    return -1;
	}

	break;
    case RX_PERF_FILE:
	if (rx_Read (call, &data, 4) != 4)
	    errx (1, "failed to read num from client");
	num = ntohl(data);

	readwrite = malloc(num*sizeof(uint32_t));
	if(readwrite == NULL)
	    err(1, "malloc");

	if (rx_Read (call, readwrite, num*sizeof(uint32_t)) !=
	    num*sizeof(uint32_t))
	    errx (1, "failed to read recvlist from client");

	    for(i=0; i < num; i++) {
		if(readwrite[i] == 0) {
		    DBFPRINT(("readp %d", readwrite[i] ));
		    readp = !readp;
		}

		bytes = ntohl(readwrite[i])*sizeof(uint32_t);

		if(readp) {
		    DBFPRINT(("read\n"));
		    readbytes(call, bytes);
		} else {
		    sendbytes(call, bytes);
		    DBFPRINT(("send\n"));
		}
	    }

	break;
    case RX_PERF_RECV:
	DBFPRINT(("got a recv request\n"));

	if (rx_Read (call, &bytes, 4) != 4) {
	    warnx ("rx_Read failed to read bytes");
	    return -1;
	}
	bytes = ntohl(bytes);

	DBFPRINT(("sending(%d) ", bytes));
	sendbytes(call, bytes);

	data = htonl(RXPERF_MAGIC_COOKIE);
	if (rx_Write (call, &data, 4) != 4) {
	    warnx ("rx_Write failed when sending back result");
	    return -1;
	}
	DBFPRINT(("done\n"));

	break;
    default:
	warnx ("client sent a unsupported command");
	return -1;
    }
    DBFPRINT(("done with command\n"));

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
readfile(const char *filename, uint32_t **readwrite, uint32_t *size)
{
    FILE *f;
    uint32_t len=16;
    uint32_t num=0;
    uint32_t data;
    char *ptr;
    char buf[RXPERF_BUFSIZE];

    *readwrite = malloc(sizeof(uint32_t)*len);

    if(*readwrite == NULL)
	err(1, "malloc");

    f=fopen(filename, "r");
    if(f==NULL)
	err(1, "fopen");

    while(fgets(buf, sizeof(buf), f) != NULL) {

	buf[strcspn(buf, "\n")] = '\0';

	if(num >= len) {
	    len=len*2;
	    *readwrite = realloc(*readwrite, len*sizeof(uint32_t));
	    if(*readwrite == NULL)
		err(1, "realloc");
	}

	if(*buf != '\0') {
	    data = htonl(strtol (buf, &ptr, 0));
	    if (ptr && ptr == buf)
		errx (1, "can't resolve number of bytes to transfer");
	} else {
	    data = 0;
	}
	
	(*readwrite)[num] =data;
	num++;
    }

    *size = num;

    
    if(fclose(f) == -1)
	err(1, "fclose");
}


/*
 *
 */

static void
do_client (const char *server, int port, char *filename,
	   int32_t command, int32_t times, 
	   int32_t bytes, int32_t rpc_sendbytes, int32_t rpc_recvbytes)
{
    struct rx_connection *conn;
    struct rx_call *call;
    uint32_t addr = str2addr(server);
    struct rx_securityClass *secureobj;
    int secureindex;
    int32_t data;
    int32_t num;
    int ret;
    int i;
    int readp = FALSE;
    char stamp[1024];
    uint32_t size;

    uint32_t *readwrite;

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
	errx (1, "failed to contact server");

    if (command == RX_PERF_RPC)
	    snprintf (stamp, sizeof(stamp), 
		      "send\t%d times\t%d writes\t%d reads", 
		      times, rpc_sendbytes, rpc_recvbytes);
    else
	    snprintf (stamp, sizeof(stamp),
		      "send\t%d times", times);

    start_timer();

    for(i=0; i < times ; i++) {

	DBFPRINT(("starting command "));

	call = rx_NewCall (conn);
	if (call == NULL)
	    errx (1, "rx_NewCall failed");
	
	data = htonl(RX_PERF_VERSION);
	if (rx_Write (call, &data, 4) != 4)
	    errx (1, "rx_Write failed to send version");
	
	data = htonl(command);
	if (rx_Write (call, &data, 4) != 4)
	    errx (1, "rx_Write failed to send command");

	data = htonl(rxread_size);
	if (rx_Write (call, &data, 4) != 4)
	    errx (1, "rx_Write failed to send read size");
	data = htonl(rxwrite_size);
	if (rx_Write (call, &data, 4) != 4)
	    errx (1, "rx_Write failed to send write read");


	switch (command) {
	case RX_PERF_RECV:
	    DBFPRINT(("command "));

	    data = htonl (bytes);
	    if (rx_Write (call, &data, 4) != 4)
		errx (1, "rx_Write failed to send size");	    
	    
	    DBFPRINT(("sending(%d) ", bytes));
	    if (readbytes(call, bytes))
		errx(1, "sendbytes");

	    if (rx_Read (call, &data, 4) != 4)
		errx (1, "failed to read result from server");
	    
	    if (data != htonl(RXPERF_MAGIC_COOKIE))
		warn("server send wrong magic cookie in responce");

	    DBFPRINT(("done\n"));

	    break;
	case RX_PERF_SEND:
	    DBFPRINT(("command "));

	    data = htonl (bytes);
	    if (rx_Write (call, &data, 4) != 4)
		errx (1, "rx_Write failed to send size");	    
	    
	    DBFPRINT(("sending(%d) ", bytes));
	    if (sendbytes(call, bytes))
		errx(1, "sendbytes");

	    if (rx_Read (call, &data, 4) != 4)
		errx (1, "failed to read result from server");
	    
	    if (data != htonl(RXPERF_MAGIC_COOKIE))
		warn("server send wrong magic cookie in responce");

	    DBFPRINT(("done\n"));

	    break;
	case RX_PERF_RPC:
	    DBFPRINT(("commands "));

	    data = htonl(rpc_sendbytes);
	    if (rx_Write(call, &data, 4) != 4)
		errx (1, "rx_Write failed to send command");
	    
	    data = htonl(rpc_recvbytes);
	    if (rx_Write (call, &data, 4) != 4)
		errx (1, "rx_Write failed to send command");
	    
	    DBFPRINT(("send(%d) ", rpc_sendbytes));
	    sendbytes(call, rpc_sendbytes);
	    
	    DBFPRINT(("recv(%d) ", rpc_recvbytes));
	    readbytes(call, rpc_recvbytes);
	    
	    if (rx_Read (call, &bytes, 4) != 4)
		errx (1, "failed to read result from server");
	    
	    if (bytes != htonl(RXPERF_MAGIC_COOKIE))
		warn("server send wrong magic cookie in responce");

	    DBFPRINT(("done\n"));

	    break;
	case RX_PERF_FILE:
	    readfile(filename, &readwrite, &num);

	    data = htonl(num);
	    if (rx_Write(call, &data, sizeof(data)) != 4)
		errx (1, "rx_Write failed to send size");

	    if (rx_Write(call, readwrite, num*sizeof(uint32_t)) 
		!= num*sizeof(uint32_t))
		errx (1, "rx_Write failed to send list");

	    for(i=0; i < num; i++) {
		if(readwrite[i] == 0)
		    readp = !readp;

		size = ntohl(readwrite[i])*sizeof(uint32_t);

		if(readp) {
		    readbytes(call, size);
		    DBFPRINT(("read\n"));
		} else {
		    sendbytes(call, size);
		    DBFPRINT(("send\n"));
		}
	    }
	    break;
	default:
	    abort();
	}

	rx_EndCall (call, 0);
    }

    end_and_print_timer (stamp);
    DBFPRINT(("done for good\n"));

    rx_Finalize();
}

static void
usage()
{
#define COMMON ""

    fprintf(stderr, "usage: %s client -c send -b <bytes>\n",
	    __progname);
    fprintf(stderr, "usage: %s client -c recv -b <bytes>\n",
	    __progname);
    fprintf(stderr, "usage: %s client -c rpc  -S <rpc_sendbytes> -R <rpc_recvbytes>\n",
	    __progname);
    fprintf(stderr, "usage: %s client -c file -f filename\n", 
	    __progname);
    fprintf (stderr, "%s: usage:	common option to the client "
	     "-w <rx_write size> -r <rx_read size> -T times -p port -s server\n",
	     __progname);
    fprintf(stderr, "usage: %s server -p port\n", __progname);
#undef COMMMON
    exit(1);
}

/*
 * do argument processing and call networking functions
 */

static int
rxperf_server (int argc, char **argv)
{
    int port	   = DEFAULT_PORT;
    char *ptr;
    int ch;

    while ((ch = getopt(argc, argv, "r:d:p:w:")) != -1) {
	switch (ch) {
	case 'd':
#ifdef RXDEBUG
	    rx_debugFile = fopen(optarg, "w");
	    if (rx_debugFile == NULL)
		err(1, "fopen %s", optarg);
#else
	    errx(1, "compiled without RXDEBUG");
#endif
	    break;
	case 'r':
	    rxread_size = strtol(optarg, &ptr, 0);
	    if (ptr != 0 && ptr[0] != '\0')
		errx (1, "can't resolve readsize");
	    if (rxread_size > sizeof(somebuf))
	      errx(1, "%d > sizeof(somebuf) (%d)",
		   rxread_size, sizeof(somebuf));
	    break;
	case 'p':
	    port = strtol(optarg, &ptr, 0);
	    if (ptr != 0 && ptr[0] != '\0')
		errx (1, "can't resolve portname");
	    break;
	case 'w':
	    rxwrite_size = strtol(optarg, &ptr, 0);
	    if (ptr != 0 && ptr[0] != '\0')
		errx (1, "can't resolve writesize");
	    if (rxwrite_size > sizeof(somebuf))
		errx(1, "%d > sizeof(somebuf) (%d)",
		     rxwrite_size, sizeof(somebuf));
	    break;
	default:
	    usage();
	}
    }

    if (optind != argc)
	usage();
    
    do_server (htons(port));

    return 0;
}

/*
 * do argument processing and call networking functions
 */

static int
rxperf_client (int argc, char **argv)
{
    char *host	   = DEFAULT_HOST;
    int bytes	   = DEFAULT_BYTES;
    int port	   = DEFAULT_PORT;
    char *filename = NULL;
    int32_t cmd;
    int rpc_sendbytes = 3;
    int rpc_recvbytes = 30;
    int print_stats = 0;
    int times = 100;
    char *ptr;
    int ch;

    cmd = RX_PERF_UNKNOWN;
    
    while ((ch  = getopt(argc, argv, "GT:S:R:b:c:d:p:r:s:w:f:")) != -1) {
	switch (ch) {
	case 'G':
	    print_stats = 1;
#ifndef HAVE_GETRUSAGE
	    printf("could not find getrusage, can't print stats\n");
#endif
	    break;
	case 'b':
	    bytes = strtol (optarg, &ptr, 0);
	    if (ptr && *ptr != '\0')
		errx (1, "can't resolve number of bytes to transfer");
	    break;
	case 'c':
	    if (strcasecmp(optarg, "send") == 0)
	      cmd = RX_PERF_SEND;
	    else if (strcasecmp(optarg, "recv") == 0)
		cmd = RX_PERF_RECV;
	    else if (strcasecmp(optarg, "rpc") == 0)
		cmd = RX_PERF_RPC;
	    else if (strcasecmp(optarg, "file") == 0)
		cmd = RX_PERF_FILE;
	    else
		errx(1, "unknown command %s", optarg);
	    break;
	case 'd':
#ifdef RXDEBUG
	    rx_debugFile = fopen(optarg, "w");
	    if (rx_debugFile == NULL)
		err(1, "fopen %s", optarg);
#else
	    errx(1, "compiled without RXDEBUG");
#endif
	    break;
	case 'p':
	    port = strtol(optarg, &ptr, 0);
	    if (ptr != 0 && ptr[0] != '\0')
		errx (1, "can't resolve portname");
	    break;
	case 'r':
	    rxread_size = strtol(optarg, &ptr, 0);
	    if (ptr != 0 && ptr[0] != '\0')
		errx (1, "can't resolve readsize");
	    if (rxread_size > sizeof(somebuf))
		errx(1, "%d > sizeof(somebuf) (%d)",
		     rxread_size, sizeof(somebuf));
	    break;
	case 's':
	    host = strdup(optarg);
	    if (host == NULL)
		err(1, "strdup");
	    break;
	case 'w':
	    rxwrite_size = strtol(optarg, &ptr, 0);
	    if (ptr != 0 && ptr[0] != '\0')
		errx (1, "can't resolve writesize");
	    if (rxwrite_size > sizeof(somebuf))
		errx(1, "%d > sizeof(somebuf) (%d)",
		     rxwrite_size, sizeof(somebuf));
	    break;
	case 'T':
	    times = strtol (optarg, &ptr, 0);
	    if (ptr && *ptr != '\0')
		errx (1, "can't resolve number times to run the test");
	    break;
	case 'S':
	    rpc_sendbytes = strtol (optarg, &ptr, 0);
	    if (ptr && *ptr != '\0')
		errx (1, "can't resolve number of bytes to transfer");
	    break;
	case 'R':
	    rpc_recvbytes = strtol (optarg, &ptr, 0);
	    if (ptr && *ptr != '\0')
		errx (1, "can't resolve number of bytes to transfer");
	    break;
	case 'f':
	    filename = optarg;
	    break;
	default:
	    usage();
	}
    }

    if (optind != argc)
	usage();
    
    if (cmd == RX_PERF_UNKNOWN)
	errx(1, "no command given to the client");
    
    do_client(host, htons(port), filename, cmd, times, bytes, 
	      rpc_sendbytes, rpc_recvbytes);
    
#if HAVE_GETRUSAGE
    if (print_stats) {
	struct rusage rusage;
	if(getrusage (RUSAGE_SELF, &rusage) < 0)
	    printf("no stats\n");
	else
	    printf ("Status:\n"
		    "- utime = (%ld, %ld)\n"
		    "- stime = (%ld, %ld)\n"
		    "- maxrss = %ld\n"
		    "- ixrss = %ld\n"
		    "- idrss = %ld\n"
		    "- isrss = %ld\n"
		    "- minflt = %ld\n"
		    "- majflt = %ld\n"
		    "- nswap = %ld\n"
		    "- inblock = %ld\n"
		    "- oublock = %ld\n"
		    "- msgsnd = %ld\n"
		    "- msgrcv = %ld\n"
		    "- nsignals = %ld\n"
		    "- nvcsw = %ld\n"
		    "- nivcws = %ld\n",
		    rusage.ru_utime.tv_sec,
		    rusage.ru_utime.tv_usec,
		    rusage.ru_stime.tv_sec,
		    rusage.ru_stime.tv_usec,
		    rusage.ru_maxrss,
		    rusage.ru_ixrss,
		    rusage.ru_idrss,
		    rusage.ru_isrss,
		    rusage.ru_minflt,
		    rusage.ru_majflt,
		    rusage.ru_nswap,
		    rusage.ru_inblock,
		    rusage.ru_oublock,
		    rusage.ru_msgsnd,
		    rusage.ru_msgrcv,
		    rusage.ru_nsignals,
		    rusage.ru_nvcsw,
		    rusage.ru_nivcsw);
    }
#endif /* HAVE_GETRUSAGE */
    return 0;
}
    
/*
 * setup world and call cmd
 */

int
main(int argc, char **argv)
{
    PROCESS pid;

    __progname = strrchr(argv[0], '/');
    if (__progname)
	__progname++;
    else
	__progname = argv[0];

    signal (SIGUSR1, sigusr1);
    signal (SIGINT, sigint);

    LWP_InitializeProcessSupport (LWP_NORMAL_PRIORITY, &pid);
    
    memset (somebuf, 0, sizeof(somebuf));

    if (argc >= 2 && strcmp(argv[1], "server") == 0)
	rxperf_server(argc - 1, argv + 1);
    else if (argc >= 2 && strcmp(argv[1], "client") == 0)
	rxperf_client(argc - 1, argv + 1);
    else
	usage();
    return 0;
}

