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
 * etherdrv.c - Ethernet Driver for Angel.
 */

#ifdef __hpux
# define _POSIX_SOURCE 1
# define _HPUX_SOURCE 1
# define _XOPEN_SOURCE 1
#endif

#include <stdio.h>
#ifdef __hpux
# define uint hide_HPs_uint
#endif
#ifdef STDC_HEADERS
# include <unistd.h>
# ifdef __hpux
#   undef uint
# endif
#endif
#include <stdlib.h>
#include <string.h>
#ifdef __hpux
# define uint hide_HPs_uint
#endif
#include <fcntl.h>
#ifdef __hpux
# undef uint
#endif
#include <errno.h>
#include <stdarg.h>
#include <ctype.h>
#include "host.h"

#ifdef COMPILING_ON_WINDOWS
  typedef char * caddr_t;
# undef IGNORE
# include <winsock.h>
# include "angeldll.h"
#else
# ifdef __hpux
#   define uint hide_HPs_uint
# endif
# include <sys/types.h>
# include <sys/socket.h>
# ifdef __hpux
#   undef uint
# endif
# include <netdb.h>
# include <sys/time.h>
# include <sys/ioctl.h>
# ifdef HAVE_SYS_FILIO_H
#   include <sys/filio.h>
# endif
# include <netinet/in.h>
# include <arpa/inet.h>
#endif

#include "hsys.h"
#include "devices.h"
#include "angel_endian.h"
#include "buffers.h"
#include "hostchan.h"
#include "params.h"
#include "logging.h"
#include "ethernet.h"


#if !defined(COMPILING_ON_WINDOWS) && !defined(STDC_HEADERS)
/* These two might not work for windows.  */
extern int sys_nerr;
extern char * sys_errlist[];
#endif

#ifndef UNUSED
# define UNUSED(x) (x = x)      /* Silence compiler warnings */
#endif

/*
 * forward declarations of static functions
 */
static int EthernetOpen(const char *name, const char *arg);
static int EthernetMatch(const char *name, const char *arg);
static void EthernetClose(void);
static int EthernetRead(DriverCall *dc, bool block);
static int EthernetWrite(DriverCall *dc);
static int EthernetIoctl(const int opcode, void *args);

/*
 * the device descriptor for Ethernet
 */
DeviceDescr angel_EthernetDevice =
{
    "Ethernet",
    EthernetOpen,
    EthernetMatch,
    EthernetClose,
    EthernetRead,
    EthernetWrite,
    EthernetIoctl
};

/*
 * descriptor for the socket that we talk down
 */
static int sock = -1;

/*
 * address of the remote target
 */
static struct sockaddr_in remote, *ia = &remote;

/*
 * array of dynamic port numbers on target
 */
static unsigned short int ports[2];

/*
 *  Function: set_address
 *   Purpose: Try to get an address into an understandable form
 *
 *    Params:
 *       Input: addr    The address to parse
 *
 *      Output: ia      Structure to hold the parsed address
 *
 *   Returns:
 *          OK:  0
 *       Error: -1
 */
static int set_address(const char *const addr, struct sockaddr_in *const ia)
{
    ia->sin_family = AF_INET;

    /*
     * Try address as a dotted decimal
     */
    ia->sin_addr.s_addr = inet_addr(addr);

    /*
     * If that failed, try it as a hostname
     */
    if (ia->sin_addr.s_addr == (u_int)-1)
    {
        struct hostent *hp = gethostbyname(addr);

        if (hp == NULL)
            return -1;

        (void)memcpy((caddr_t)&ia->sin_addr, hp->h_addr, hp->h_length);
    }

    return 0;
}

/*
 *  Function: open_socket
 *   Purpose: Open a non-blocking UDP socket, and bind it to a port
 *              assigned by the system.
 *
 *    Params: None
 *
 *   Returns:
 *          OK: socket descriptor
 *       Error: -1
 */
static int open_socket(void)
{
    int sfd;
#if 0                           /* see #if 0 just below -VVV- */
    int yesplease = 1;
#endif
    struct sockaddr_in local;

    /*
     * open the socket
     */
#ifdef COMPILING_ON_WINDOWS
    if ((sfd = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET)
        return -1;
#else
    if ((sfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
# ifdef DEBUG
        perror("socket");
# endif
        return -1;
    }
#endif

    /*
     * 960731 KWelton
     *
     * I don't believe that this should be necessary - if we
     * use select(), then non-blocking I/O is redundant.
     * Unfortunately, select() appears to be broken (under
     * Solaris, with a limited amount of time available for
     * debug), so this code stays in for the time being
     */
#if 0
    /*
     * enable non-blocking I/O
     */
    if (ioctlsocket(sfd, FIONBIO, &yesplease) < 0)
    {
# ifdef DEBUG
        perror("ioctl(FIONBIO)");
# endif
        closesocket(sfd);

        return -1;
    }
#endif /* 0/1 */

    /*
     * bind local address to a system-assigned port
     */
    memset((char *)&local, 0, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_port = htons(0);
    local.sin_addr.s_addr = INADDR_ANY;
    if (bind(sfd, (struct sockaddr *)&local, sizeof(local)) < 0)
    {
#ifdef DEBUG
        perror("bind");
#endif
        closesocket(sfd);

        return -1;
    }

    /*
     * all done
     */
    return sfd;
}

/*
 *  Function: fetch_ports
 *   Purpose: Request assigned port numbers from remote target
 *
 *    Params: None
 *
 *   Returns: Nothing
 *
 * Post-conditions: This routine will *always* return something for the
 *                      port numbers.  If the remote target does not
 *                      respond, then it makes something up - this allows
 *                      the standard error message (from ardi.c) to be
 *                      generated when the target is dead for whatever
 *                      reason.
 */
static void fetch_ports(void)
{
    int i;
    char ctrlpacket[10];
    CtrlResponse response;

    memset (ctrlpacket, 0, 10);
    strcpy (ctrlpacket, CTRL_MAGIC);
    memset (response, 0, sizeof(CtrlResponse));
    /*
     * we will try 3 times to elicit a response from the target
     */
    for (i = 0; i < 3; ++i)
    {
        struct timeval tv;
        fd_set fdset;

        /*
         * send the magic string to the control
         * port on the remote target
         */
        ia->sin_port = htons(CTRL_PORT);
#ifdef DEBUG
	printf("CTLR_PORT=0x%04x  sin_port=0x%04x\n");
#endif

        if (sendto(sock, ctrlpacket, sizeof(ctrlpacket), 0,
                       (struct sockaddr *)ia, sizeof(*ia)) < 0)
        {
#ifdef DEBUG
            perror("fetch_ports: sendto");
#endif
            return;
        }

        FD_ZERO(&fdset);
        FD_SET(sock, &fdset);
        tv.tv_sec = 0;
        tv.tv_usec = 250000;

        if (select(sock + 1, &fdset, NULL, NULL, &tv) < 0)
        {
#ifdef DEBUG
            perror("fetch_ports: select");
#endif
            return;
        }

        if (FD_ISSET(sock, &fdset))
        {
            /*
             * there is something there - read it
             */
            if (recv(sock, (char *)&response, sizeof(response), 0) < 0)
            {
#ifdef COMPILING_ON_WINDOWS
                unsigned int werrno = WSAGetLastError();

                if (werrno == WSAEWOULDBLOCK || werrno == 0)
#else
                if (errno == EWOULDBLOCK)
#endif
                {
                    --i;
                    continue;
                }
                else
                {
#ifdef DEBUG
                    perror("fetch_ports: recv");
#endif
                    return;
                }
            }
            {
                /*
                 * XXX
                 *
                 * this is *very* unpleasant - try to match the structure
                 * layout
                 */
                unsigned short *sptr = (unsigned short *)(response + RESP_DBUG);

                if (strcmp(response, ctrlpacket) == 0)
                {
                    ports[DBUG_INDEX] = htons(*sptr);
                    sptr++;
                    ports[APPL_INDEX] = htons(*sptr);
                }

#ifdef DEBUG
                printf("fetch_ports: got response, DBUG=%d, APPL=%d\n",
                       ports[DBUG_INDEX], ports[APPL_INDEX]);
#endif
                return;
            }
        }
    }

    /*
     * we failed to get a response
     */
#ifdef DEBUG
    printf("fetch_ports: failed to get a real answer\n");
#endif
}

/*
 *  Function: read_packet
 *   Purpose: read a packet, and pass it back to higher levels
 *
 *    Params:
 *      In/Out: packet  Holder for the read packet
 *
 *   Returns:  1 - Packet is complete
 *             0 - No complete packet read
 *
 * Post-conditions: Will call panic() if something goes wrong with the OS
 */
static int read_packet(struct data_packet *const packet)
{
    struct sockaddr_in from;
    int nbytes, fromlen = sizeof(from);
    DevChanID devchan;

    /*
     * try to get the packet
     */
    if ((nbytes = recvfrom(sock, (char *)(packet->data), packet->buf_len, 0,
                           (struct sockaddr *)&from, &fromlen)) < 0)
    {
#ifdef COMPILING_ON_WINDOWS
        if (nbytes == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)
            MessageBox(GetFocus(), "Error receiving packet\n", "Angel", MB_OK | MB_ICONSTOP);
#else
        if (errno != EWOULDBLOCK)
        {
# ifdef DEBUG
            perror("recv");
# endif
            panic("ethernet recv failure");
        }
#endif
        return 0;
    }

#ifdef COMPILING_ON_WINDOWS
    if (pfnProgressCallback != NULL && nbytes != SOCKET_ERROR)
    {
        progressInfo.nRead += nbytes;
        (*pfnProgressCallback)(&progressInfo);
    }
#endif

    /*
     * work out where the packet was from
     */
    if (from.sin_addr.s_addr != remote.sin_addr.s_addr)
    {
        /*
         * not from our target - ignore it
         */
#ifdef DEBUG
        printf("read_packet: ignoring packet from %s\n",
               inet_ntoa(from.sin_addr));
#endif

        return 0;
    }
    else if (ntohs(from.sin_port) == ports[DBUG_INDEX])
        devchan = DC_DBUG;
    else if (ntohs(from.sin_port) == ports[APPL_INDEX])
        devchan = DC_APPL;
    else
    {
        /*
         * unknown port number - ignore it
         */
#ifdef DEBUG
        printf("read_packet: ignore packet from port %hd\n",
               htons(from.sin_port));
#endif

        return 0;
    }

#if defined(DEBUG) && !defined(DO_TRACE)
    printf("EthernetRead: %d bytes from %s channel\n",
           nbytes, (devchan == DC_DBUG) ? "DBUG" : "APPL");
#endif

#ifdef DO_TRACE
    printf("[%d on %d]\n", nbytes, devchan);
    {
        int i = 0;
        unsigned char *cptr = packet->data;

        while (i < nbytes)
        {
            printf("<%02X ", *(cptr++));

            if (!(++i % 16))
                printf("\n");
        }

        if (i % 16)
            printf("\n");
    }
#endif

    /*
     * OK - fill in the details
     */
    packet->type = devchan;
    packet->len = nbytes;
    return 1;
}

/**********************************************************************/

/*
 *  Function: Ethernet_Open
 *   Purpose: Open the Ethernet device.  See the documentation for
 *              DeviceOpen in drivers.h
 *
 * Post-conditions: Will have updated struct sockaddr_in remote (*ia)
 *                      with the address of the remote target.
 */
static int EthernetOpen(const char *name, const char *arg)
{
#ifdef COMPILING_ON_WINDOWS
    WORD wVersionRequested;
    WSADATA wsaData;
#endif
    /*
     * name is passed as e=<blah>, so skip 1st two characters
     */
    const char *etheraddr = name + 2;

#ifdef DEBUG
    printf("EthernetOpen: name `%s'\n", name);
#endif

    /* Check that the name is a valid one */
    if (EthernetMatch(name, arg) != 0)
        return -1;

#ifdef COMPILING_ON_WINDOWS
    wVersionRequested = MAKEWORD(1, 1);
    if (WSAStartup(wVersionRequested, &wsaData) != 0)
        /*
         * Couldn't find a useable winsock.dll.
         */
        return -1;

    if ( LOBYTE( wsaData.wVersion ) != 1 || HIBYTE( wsaData.wVersion ) != 1 )
    {
        WSACleanup();

        /*
         * Couldn't find a winsock.dll with supported version.
         */
        return -1;
    }
#endif

    memset((char *)ia, 0, sizeof(*ia));
    if (set_address(etheraddr, ia) < 0)
    {
#ifdef COMPILING_ON_WINDOWS
        /*
         * SJ - I'm not sure that this is the correct way to handle this
         * as Fail calls remote_disable and exits, while panic just exits.
         * However at the time of writing remote_disable does nothing!
         */
 /*     Panic("EthernetOpen: bad name `%s'\n", etheraddr); */
#else
        Fail("EthernetOpen: bad name `%s'\n", etheraddr);
#endif
        return -1;
    }

    if ((sock = open_socket()) < 0)
        return -1;

    /*
     * fetch the port numbers assigned by the remote target
     * to its Debug and Application sockets
     */
    fetch_ports();

    return 0;
}

static int EthernetMatch(const char *name, const char *arg)
{
    /* IGNORE arg */
    if (0)
        arg = arg;

    if (name == NULL)
        return -1;

    if (tolower(name[0]) != 'e' || name[1] != '=')
        return -1;

    return 0;
}

static void EthernetClose(void)
{
    if (sock >= 0)
    {
        closesocket(sock);
        sock = -1;
    }

#ifdef COMPILING_ON_WINDOWS
    WSACleanup();
#endif
}

static int EthernetRead(DriverCall *dc, bool block)
{
    fd_set fdset;
    struct timeval tv;
    int err;

    FD_ZERO(&fdset);
    FD_SET(sock, &fdset);

#ifdef COMPILING_ON_WINDOWS
    UNUSED(block);
    tv.tv_sec = tv.tv_usec = 0;
#else
    tv.tv_sec = 0;
    tv.tv_usec = (block ? 10000 : 0);
#endif

    err = select(sock + 1, &fdset, NULL, NULL, &tv);

    if (err < 0) {
      if (errno == EINTR) {
        return 0;
      }
      panic("ethernet select failure (errno=%i)",errno);
      return 0;
    }

    if (FD_ISSET(sock, &fdset))
      return read_packet(&dc->dc_packet);
    else
      return 0;
}

static int EthernetWrite(DriverCall *dc)
{
    int nbytes;
    struct data_packet *packet = &dc->dc_packet;

    if (packet->type == DC_DBUG)
        ia->sin_port = htons(ports[DBUG_INDEX]);
    else if (packet->type == DC_APPL)
        ia->sin_port = htons(ports[APPL_INDEX]);
    else
    {
        panic("EthernetWrite: unknown devchan");
        return 0;
    }

#if defined(DEBUG) && !defined(DO_TRACE)
    printf("EthernetWrite: %d bytes to %s channel\n",
           packet->len, (packet->type == DC_DBUG) ? "DBUG" : "APPL");
#endif

#ifdef DO_TRACE
    printf("[%d on %d]\n", packet->len, packet->type);
    {
        int i = 0;
        unsigned char *cptr = packet->data;

        while (i < packet->len)
        {
            printf(">%02X ", *(cptr++));

            if (!(++i % 16))
                printf("\n");
        }

        if (i % 16)
            printf("\n");
    }
#endif

    if ((nbytes = sendto(sock, (char *)(packet->data), packet->len, 0,
                         (struct sockaddr *)ia, sizeof(*ia))) != packet->len)
    {
#ifdef COMPILING_ON_WINDOWS
        if (nbytes == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)
#else
        if (nbytes < 0 && errno != EWOULDBLOCK)
#endif
        {
#ifdef DEBUG
            perror("sendto");
#endif

#ifdef COMPILING_ON_WINDOWS
            panic("ethernet send failure\n");
#else
            /* might not work for Windows */
            panic("ethernet send failure [%s]\n",
#ifdef STDC_HEADERS
		  strerror(errno));
#else
                  errno < sys_nerr ? sys_errlist[errno] : "unknown errno");
#endif /* STDC_HEADERS */
#endif
        }
#ifdef DEBUG
        else if (nbytes >= 0)
            fprintf(stderr, "ethernet send: asked for %d, sent %d\n", packet->len, nbytes);
#endif
        return 0;
    }

#ifdef COMPILING_ON_WINDOWS
    if (pfnProgressCallback != NULL && nbytes != SOCKET_ERROR)
    {
        progressInfo.nWritten += nbytes;
        (*pfnProgressCallback)(&progressInfo);
    }
#endif

    return 1;
}

static int EthernetIoctl(const int opcode, void *args)
{
#ifdef DEBUG
    printf( "EthernetIoctl: op %d arg %x\n", opcode, args );
#endif

    /*
     * IGNORE(opcode)
     */
    if (0)
    {
        int dummy = opcode;
        UNUSED(dummy);
    }
    UNUSED(args);

    switch ( opcode )
    {
        case DC_RESYNC:
        {
#ifdef DEBUG
            printf( "EthernetIoctl: resync\n" );
#endif
            fetch_ports();
            return 0;
        }

        default:
        {
            return -1;
        }
    }
}

/* EOF etherdrv.c */
