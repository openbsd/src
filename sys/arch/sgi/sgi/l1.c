/*	$OpenBSD: l1.c,v 1.7 2011/03/12 23:43:19 miod Exp $	*/

/*
 * Copyright (c) 2009 Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Communication with the L1 controller, on IP35 systems.
 * We use a direct 57600 bps serial link from each processor to the L1 chip.
 * Information is sent as ppp-encoded packets.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>

#include <machine/autoconf.h>
#include <machine/mnode.h>
#include <sgi/xbow/hub.h>

#include <dev/ic/ns16550reg.h>
#include <dev/ic/comreg.h>

#include <net/ppp_defs.h>

#include <sgi/sgi/ip27.h>
#include <sgi/sgi/l1.h>

/*
 * L1 communication defines
 */

/* packet types */
#define	L1PKT_REQUEST	0x00
#define	L1PKT_RESPONSE	0x20
#define	L1PKT_EVENT	0x40

/* packet subchannels */
#define	L1CH_CPU0	0x00	/* exclusive channel for cpu 0 */
#define	L1CH_CPU1	0x01	/* exclusive channel for cpu 1 */
#define	L1CH_CPU2	0x02	/* exclusive channel for cpu 2 */
#define	L1CH_CPU3	0x03	/* exclusive channel for cpu 3 */
#define	L1CH_CONSOLE	0x04	/* L1 console */
/* 05..0f reserved */
/* 10..1f available for operating system */
#define	L1CH_MISC	0x10

/* argument encoding */
#define	L1_ARG_INT		0x00	/* followed by 32 bit BE value */
#define	L1_ARG_ASCII		0x01	/* followed by NUL terminated string */
#define	L1_ARG_BINARY		0x80	/* length in low 7 bits */

int	l1_serial_getc(int16_t);
int	l1_serial_putc(int16_t, u_char);
int	l1_serial_ppp_write(int16_t, uint16_t *, u_char, int);
int	l1_serial_ppp_read(int16_t, int);
int	l1_packet_put(int16_t, u_char *, size_t);
int	l1_packet_get(int16_t, u_char *, size_t);
static inline
void	l1_packet_put_be32(u_char *, uint32_t);
static inline
void	l1_packet_put_be16(u_char *, uint16_t);
static inline
uint32_t l1_packet_get_be32(u_char *);
int	l1_packet_get_int(u_char **, size_t *, uint32_t *);
int	l1_packet_get_ascii(u_char **, size_t *, char **);
int	l1_packet_get_binary(u_char **, size_t *, u_char **, size_t *);
size_t	l1_command_build(u_char *, size_t, uint32_t, uint16_t, int, ...);
int	l1_receive_response(int16_t, u_char *, size_t *);
int	l1_response_to_errno(uint32_t);
int	l1_read_board_ia(int16_t, int, u_char **, size_t *);

static inline
size_t	ia_skip(u_char *, size_t, size_t);

/* l1_packet_get() return values */
#define	L1PG_TIMEOUT		-1
#define	L1PG_BADCRC		-2
#define	L1PG_SHORTPACKET	-3

/*
 * Basic serial routines (polled)
 */

#define	L1_UART_ADDRESS(nasid, r) \
	IP27_RHSPEC_ADDR(nasid, HSPEC_L1_UART(r))

int
l1_serial_getc(int16_t nasid)
{
	uint64_t lsr;
	int n;

	for (n = 1000000; n != 0; n--) {
		lsr = *(volatile uint64_t *)L1_UART_ADDRESS(nasid, com_lsr);
		if ((lsr & LSR_RXRDY) != 0)
			break;
	}

	if (n == 0) {
#ifdef L1_DEBUG
		printf("%s: RX timeout, lsr %02x\n", __func__, lsr);
#endif
		return -1;
	}

	return *(volatile uint64_t *)L1_UART_ADDRESS(nasid, com_data) & 0xff;

}

int
l1_serial_putc(int16_t nasid, u_char val)
{
	uint64_t lsr;
	int n;

	for (n = 1000000; n != 0; n--) {
		lsr = *(volatile uint64_t *)L1_UART_ADDRESS(nasid, com_lsr);
		if ((lsr & LSR_TXRDY) != 0)
			break;
	}

	if (n == 0) {
#ifdef L1_DEBUG
		printf("%s: TX timeout, lsr %02x\n", __func__, lsr);
#endif
		return EWOULDBLOCK;
	}

	*(volatile uint64_t *)L1_UART_ADDRESS(nasid, com_data) = (uint64_t)val;
	return 0;
}

/*
 * Single character routines, with optional ppp frame escaping, and optional
 * ppp crc update.
 */

/*
 * FCS lookup table as calculated by genfcstab.
 * Straight from <net/ppp_tty.c>, only 512 bytes long; probably not worth
 * trying to share with outsmart config(8) machinery...
 */
static const u_int16_t fcstab[256] = {
	0x0000,	0x1189,	0x2312,	0x329b,	0x4624,	0x57ad,	0x6536,	0x74bf,
	0x8c48,	0x9dc1,	0xaf5a,	0xbed3,	0xca6c,	0xdbe5,	0xe97e,	0xf8f7,
	0x1081,	0x0108,	0x3393,	0x221a,	0x56a5,	0x472c,	0x75b7,	0x643e,
	0x9cc9,	0x8d40,	0xbfdb,	0xae52,	0xdaed,	0xcb64,	0xf9ff,	0xe876,
	0x2102,	0x308b,	0x0210,	0x1399,	0x6726,	0x76af,	0x4434,	0x55bd,
	0xad4a,	0xbcc3,	0x8e58,	0x9fd1,	0xeb6e,	0xfae7,	0xc87c,	0xd9f5,
	0x3183,	0x200a,	0x1291,	0x0318,	0x77a7,	0x662e,	0x54b5,	0x453c,
	0xbdcb,	0xac42,	0x9ed9,	0x8f50,	0xfbef,	0xea66,	0xd8fd,	0xc974,
	0x4204,	0x538d,	0x6116,	0x709f,	0x0420,	0x15a9,	0x2732,	0x36bb,
	0xce4c,	0xdfc5,	0xed5e,	0xfcd7,	0x8868,	0x99e1,	0xab7a,	0xbaf3,
	0x5285,	0x430c,	0x7197,	0x601e,	0x14a1,	0x0528,	0x37b3,	0x263a,
	0xdecd,	0xcf44,	0xfddf,	0xec56,	0x98e9,	0x8960,	0xbbfb,	0xaa72,
	0x6306,	0x728f,	0x4014,	0x519d,	0x2522,	0x34ab,	0x0630,	0x17b9,
	0xef4e,	0xfec7,	0xcc5c,	0xddd5,	0xa96a,	0xb8e3,	0x8a78,	0x9bf1,
	0x7387,	0x620e,	0x5095,	0x411c,	0x35a3,	0x242a,	0x16b1,	0x0738,
	0xffcf,	0xee46,	0xdcdd,	0xcd54,	0xb9eb,	0xa862,	0x9af9,	0x8b70,
	0x8408,	0x9581,	0xa71a,	0xb693,	0xc22c,	0xd3a5,	0xe13e,	0xf0b7,
	0x0840,	0x19c9,	0x2b52,	0x3adb,	0x4e64,	0x5fed,	0x6d76,	0x7cff,
	0x9489,	0x8500,	0xb79b,	0xa612,	0xd2ad,	0xc324,	0xf1bf,	0xe036,
	0x18c1,	0x0948,	0x3bd3,	0x2a5a,	0x5ee5,	0x4f6c,	0x7df7,	0x6c7e,
	0xa50a,	0xb483,	0x8618,	0x9791,	0xe32e,	0xf2a7,	0xc03c,	0xd1b5,
	0x2942,	0x38cb,	0x0a50,	0x1bd9,	0x6f66,	0x7eef,	0x4c74,	0x5dfd,
	0xb58b,	0xa402,	0x9699,	0x8710,	0xf3af,	0xe226,	0xd0bd,	0xc134,
	0x39c3,	0x284a,	0x1ad1,	0x0b58,	0x7fe7,	0x6e6e,	0x5cf5,	0x4d7c,
	0xc60c,	0xd785,	0xe51e,	0xf497,	0x8028,	0x91a1,	0xa33a,	0xb2b3,
	0x4a44,	0x5bcd,	0x6956,	0x78df,	0x0c60,	0x1de9,	0x2f72,	0x3efb,
	0xd68d,	0xc704,	0xf59f,	0xe416,	0x90a9,	0x8120,	0xb3bb,	0xa232,
	0x5ac5,	0x4b4c,	0x79d7,	0x685e,	0x1ce1,	0x0d68,	0x3ff3,	0x2e7a,
	0xe70e,	0xf687,	0xc41c,	0xd595,	0xa12a,	0xb0a3,	0x8238,	0x93b1,
	0x6b46,	0x7acf,	0x4854,	0x59dd,	0x2d62,	0x3ceb,	0x0e70,	0x1ff9,
	0xf78f,	0xe606,	0xd49d,	0xc514,	0xb1ab,	0xa022,	0x92b9,	0x8330,
	0x7bc7,	0x6a4e,	0x58d5,	0x495c,	0x3de3,	0x2c6a,	0x1ef1,	0x0f78
};

int
l1_serial_ppp_write(int16_t nasid, uint16_t *crc, u_char data, int escape)
{
	/* update crc if necessary */
	if (crc != NULL)
		*crc = PPP_FCS(*crc, data);

	/* escape data if necessary */
	if (escape && (data == PPP_FLAG || data == PPP_ESCAPE)) {
		if (l1_serial_putc(nasid, PPP_ESCAPE) != 0)
			return EWOULDBLOCK;
		data ^= PPP_TRANS;
	}

	return l1_serial_putc(nasid, data);
}

int
l1_serial_ppp_read(int16_t nasid, int unescape)
{
	int data;

	if ((data = l1_serial_getc(nasid)) < 0)
		return data;

	/* unescape data if necessary */
	if (unescape && data == PPP_ESCAPE) {
		if ((data = l1_serial_getc(nasid)) < 0)
			return data;
		data ^= PPP_TRANS;
	}

	return data;
}

/*
 * Complete ppp packet emission and reception.
 */

int
l1_packet_put(int16_t nasid, u_char *packet, size_t len)
{
	uint16_t crc = PPP_INITFCS;

	/* send incoming packet flag */
	if (l1_serial_ppp_write(nasid, NULL, PPP_FLAG, 0) != 0)
		return EWOULDBLOCK;

	/* send packet data */
	while (len-- != 0)
		if (l1_serial_ppp_write(nasid, &crc, *packet++, 1) != 0)
			return EWOULDBLOCK;

	/* send crc */
	crc ^= PPP_INITFCS;
	if (l1_serial_ppp_write(nasid, NULL, crc & 0xff, 1) != 0)
		return EWOULDBLOCK;
	if (l1_serial_ppp_write(nasid, NULL, (crc >> 8) & 0xff, 1) != 0)
		return EWOULDBLOCK;

	/* send final packet byte flag */
	if (l1_serial_ppp_write(nasid, NULL, PPP_FLAG, 0) != 0)
		return EWOULDBLOCK;

	return 0;
}

int
l1_packet_get(int16_t nasid, u_char *buf, size_t buflen)
{
	uint16_t crc;
	size_t rcvlen;
	int data;

	/* wait for incoming packet flag */
	for (;;) {
		data = l1_serial_ppp_read(nasid, 0);
		if (data < 0)
			return L1PG_TIMEOUT;
		if (data == PPP_FLAG)
			break;
	}

	/* read packet */
	rcvlen = 0;
	crc = PPP_INITFCS;
	for (;;) {
		data = l1_serial_ppp_read(nasid, 1);
		if (data < 0)
			return L1PG_TIMEOUT;
		if (data == PPP_FLAG)	/* end of packet */
			break;
		if (rcvlen < buflen)
			buf[rcvlen] = data;
		rcvlen++;
		crc = PPP_FCS(crc, data);
	}

	if (rcvlen < 2) {
#ifdef L1_DEBUG
		printf("%s: short packet\n", __func__);
#endif
		return L1PG_SHORTPACKET;
	}

	/* check CRC */
	rcvlen -= 2;	/* crc bytes */
	if (crc != PPP_GOODFCS) {
#ifdef L1_DEBUG
		printf("%s: CRC error (%04x)\n", __func__, crc);
#endif
		return L1PG_BADCRC;
	}

	return rcvlen;
}

/*
 * L1 packet construction and deconstruction helpers
 */

static inline void
l1_packet_put_be32(u_char *buf, uint32_t data)
{
	*buf++ = data >> 24;
	*buf++ = data >> 16;
	*buf++ = data >> 8;
	*buf++ = data;
}

static inline void
l1_packet_put_be16(u_char *buf, uint16_t data)
{
	*buf++ = data >> 8;
	*buf++ = data;
}

static inline uint32_t
l1_packet_get_be32(u_char *buf)
{
	uint32_t data;

	data = *buf++;
	data <<= 8;
	data |= *buf++;
	data <<= 8;
	data |= *buf++;
	data <<= 8;
	data |= *buf++;

	return data;
}

int
l1_packet_get_int(u_char **buf, size_t *buflen, uint32_t *rval)
{
	u_char *b = *buf;

	if (*buflen < 5)
		return EIO;

	if (*b++ != L1_ARG_INT)
		return EINVAL;

	*rval = l1_packet_get_be32(b);

	b += 4;
	*buf = b;
	*buflen -= 5;

	return 0;
}

int
l1_packet_get_ascii(u_char **buf, size_t *buflen, char **rval)
{
	u_char *b = *buf;
	u_char *s, *e;

	if (*buflen < 2)
		return EIO;

	if (*b != L1_ARG_ASCII)
		return EINVAL;

	/* check for a terminating NUL within the given bounds */
	e = b + *buflen;
	for (s = b + 1; s != e; s++)
		if (*s == '\0')
			break;
	if (s == e)
		return ENOMEM;

	*rval = (char *)b + 1;

	s++;
	*buflen -= s - b;
	*buf = s;

	return 0;
}

int
l1_packet_get_binary(u_char **buf, size_t *buflen, u_char **rval, size_t *rlen)
{
	u_char *b = *buf;
	size_t datalen;

	if (*buflen < 1)
		return EIO;

	if ((*b & L1_ARG_BINARY) == 0)
		return EINVAL;

	datalen = *b & ~L1_ARG_BINARY;
	if (*buflen < 1 + datalen)
		return ENOMEM;

	b++;
	*rval = b;
	*rlen = datalen;
	*buflen -= 1 + datalen;
	b += datalen;
	*buf = b;

	return 0;
}

/*
 * Build a whole request packet.
 */

size_t
l1_command_build(u_char *buf, size_t buflen, uint32_t address, uint16_t request,
    int nargs, ...)
{
	va_list ap;
	uint32_t data;
	size_t len = 0;
	int argtype;
	const char *str;

	/*
	 * Setup packet header (type, channel, address, request)
	 */

	if (buflen >= 1) {
		*buf++ = L1PKT_REQUEST | L1CH_MISC;
		buflen--;
	}
	len++;

	if (buflen >= 4) {
		l1_packet_put_be32(buf, address);
		buf += 4;
		buflen -= 4;
	}
	len += 4;

	if (buflen >= 2) {
		l1_packet_put_be16(buf, request);
		buf += 2;
		buflen -= 2;
	}
	len += 2;

	/*
	 * Setup command arguments
	 */

	if (buflen >= 1) {
		*buf++ = nargs;
		buflen--;
	}
	len++;

	va_start(ap, nargs);
	while (nargs-- != 0) {
		argtype = va_arg(ap, int);
		switch (argtype) {
		case L1_ARG_INT:
			data = va_arg(ap, uint32_t);
			if (buflen >= 5) {
				*buf++ = L1_ARG_INT;
				l1_packet_put_be32(buf, data);
				buf += 4;
				buflen -= 5;
			}
			len += 5;
			break;
		case L1_ARG_ASCII:
			str = va_arg(ap, const char *);
			data = strlen(str);
			if (buflen >= data + 2) {
				*buf++ = L1_ARG_ASCII;
				memcpy(buf, str, data + 1);
				buf += data + 1;
				buflen -= data + 2;
			}
			len += data + 2;
			break;
		case L1_ARG_BINARY:
			data = (uint32_t)va_arg(ap, size_t);	/* size */
			str = va_arg(ap, const char *);		/* data */
			if (buflen >= 1 + data) {
				*buf++ = L1_ARG_BINARY | data;
				memcpy(buf, str, data);
				buf += data;
				buflen -= data + 1;
			}
			len += data + 1;
			break;
		}
	}
	va_end(ap);

	return len;
}

/*
 * Get a packet response, ignoring any other packet seen in between.
 * Note that despite being the only user of L1 in the kernel, we may
 * receive event packets from the console.
 */
int
l1_receive_response(int16_t nasid, u_char *pkt, size_t *pktlen)
{
	int rc;

	for (;;) {
		rc = l1_packet_get(nasid, pkt, *pktlen);
		if (rc == L1PG_TIMEOUT)
			return EWOULDBLOCK;

		if (rc < 0)	/* bad packet */
			continue;

		if (pkt[0] != (L1PKT_RESPONSE | L1CH_MISC)) {
#ifdef L1_DEBUG
			printf("unexpected L1 packet: head %02x\n", pkt[0]);
#endif
			continue;	/* it's not our response */
		}

		*pktlen = (size_t)rc;
		return 0;
	}
}

/*
 * Process a response code.
 */
int
l1_response_to_errno(uint32_t response)
{
	int rc;

	switch (response) {
	case L1_RESP_OK:
		rc = 0;
		break;
	case L1_RESP_INVAL:
		rc =  EINVAL;
		break;
	case L1_RESP_NXDATA:
		rc = ENXIO;
		break;
	default:
#ifdef L1_DEBUG
		printf("unexpected L1 response code: %08x\n", response);
#endif
		rc = EIO;
		break;
	}

	return rc;
}

/*
 * Read a board IA information record from EEPROM
 */

#define	EEPROM_CHUNK	0x40

int
l1_read_board_ia(int16_t nasid, int type, u_char **ria, size_t *rialen)
{
	u_char pkt[64 + EEPROM_CHUNK];	/* command and response packet buffer */
	u_char *pktbuf, *chunk, *ia = NULL;
	size_t pktlen, chunklen, ialen, iapos;
	uint32_t data;
	int rc;

	/*
	 * Build a first packet, asking for 0 bytes to be read.
	 */
	pktlen = l1_command_build(pkt, sizeof pkt,
	    L1_ADDRESS(type, L1_ADDRESS_LOCAL | L1_TASK_GENERAL),
	    L1_REQ_EEPROM, 4,
	    L1_ARG_INT, (uint32_t)L1_EEP_LOGIC,
	    L1_ARG_INT, (uint32_t)L1_EEP_BOARD,
	    L1_ARG_INT, (uint32_t)0,	/* offset */
	    L1_ARG_INT, (uint32_t)0);	/* size */
	if (pktlen > sizeof pkt) {
#ifdef DIAGNOSTIC
		panic("%s: L1 command packet too large (%zu) for buffer",
		    __func__, pktlen);
#endif
		return ENOMEM;
	}

	if (l1_packet_put(nasid, pkt, pktlen) != 0)
		return EWOULDBLOCK;

	pktlen = sizeof pkt;
	if (l1_receive_response(nasid, pkt, &pktlen) != 0)
		return EWOULDBLOCK;

	if (pktlen < 6) {
#ifdef L1_DEBUG
		printf("truncated response (length %d)\n", pktlen);
#endif
		return EIO;
	}

	/*
	 * Check the response code.
	 */

	data = l1_packet_get_be32(&pkt[1]);
	rc = l1_response_to_errno(data);
	if (rc != 0)
		return rc;

	/*
	 * EEPROM read commands should return either one or two values:
	 * the first value is the size of the remaining EEPROM data, and
	 * the second value is the data read itself, if we asked for a
	 * nonzero size in the command (that size might be shorter than
	 * the data we asked for).
	 */

	if (pkt[5] != 1) {
#ifdef L1_DEBUG
		printf("unexpected L1 response: %d values\n", pkt[5]);
#endif
		return EIO;
	}

	pktbuf = pkt + 6;
	pktlen -= 6;

	if (l1_packet_get_int(&pktbuf, &pktlen, &data) != 0) {
#ifdef L1_DEBUG
		printf("unable to parse response as integer\n");
#endif
		return EIO;
	}

	/*
	 * Now that we know the size of the IA entry, allocate memory for it.
	 */

	ialen = (size_t)data;
	ia = (u_char *)malloc(ialen, M_DEVBUF, M_NOWAIT);
	if (ia == NULL)
		return ENOMEM;

	/*
	 * Read the EEPROM contents in small chunks, so as not to keep L1
	 * busy for too long.
	 */

	iapos = 0;
	while (iapos < ialen) {
		/*
		 * Build a command packet, this time actually reading data.
		 */
		pktlen = l1_command_build(pkt, sizeof pkt,
		    L1_ADDRESS(type, L1_ADDRESS_LOCAL | L1_TASK_GENERAL),
		    L1_REQ_EEPROM, 4,
		    L1_ARG_INT, (uint32_t)L1_EEP_LOGIC,
		    L1_ARG_INT, (uint32_t)L1_EEP_BOARD,
		    L1_ARG_INT, (uint32_t)iapos,
		    L1_ARG_INT, (uint32_t)EEPROM_CHUNK);
		/* no need to check size again, it's the same size as earlier */

		if (l1_packet_put(nasid, pkt, pktlen) != 0) {
			rc = EWOULDBLOCK;
			goto fail;
		}

		pktlen = sizeof pkt;
		if (l1_receive_response(nasid, pkt, &pktlen) != 0) {
			rc = EWOULDBLOCK;
			goto fail;
		}

		if (pktlen < 6) {
#ifdef L1_DEBUG
			printf("truncated response (length %d)\n", pktlen);
#endif
			rc = EIO;
			goto fail;
		}

		/*
		 * Check the response code.
		 */

		data = l1_packet_get_be32(&pkt[1]);
		rc = l1_response_to_errno(data);
		if (rc != 0)
			goto fail;

		if (pkt[5] != 2) {
#ifdef L1_DEBUG
			printf("unexpected L1 response: %d values\n", pkt[5]);
#endif
			rc = EIO;
			goto fail;
		}

		pktbuf = pkt + 6;
		pktlen -= 6;

		if (l1_packet_get_int(&pktbuf, &pktlen, &data) != 0) {
#ifdef L1_DEBUG
			printf("unable to parse first response as integer\n");
#endif
			rc = EIO;
			goto fail;
		}

		if (l1_packet_get_binary(&pktbuf, &pktlen,
		    &chunk, &chunklen) != 0) {
#ifdef L1_DEBUG
			printf("unable to parse second response as binary\n");
#endif
			rc = EIO;
			goto fail;
		}

		/* should not happen, but we don't like infinite loops */
		if (chunklen == 0) {
#ifdef L1_DEBUG
			printf("read command returned 0 bytes\n");
#endif
			rc = EIO;
			goto fail;
		}

		memcpy(ia + iapos, chunk, chunklen);
		iapos += chunklen;
#ifdef L1_DEBUG
		printf("got %02x bytes of eeprom, %x/%x\n",
		    chunklen, iapos, ialen);
#endif
	}

	/*
	 * Verify the checksum
	 */

	chunk = ia;
	iapos = ialen;
	data = 0;
	while (iapos-- != 0)
		data += *chunk++;
	if ((data & 0xff) != 0) {
#ifdef L1_DEBUG
		printf("wrong IA checksum\n");
#endif
		rc = EINVAL;
		goto fail;
	}

	*ria = ia;
	*rialen = ialen;
	return 0;

fail:
	if (ia != NULL)
		free(ia, M_DEVBUF);
	return rc;
}

/*
 * Information Area (IA) decoding helpers
 *
 * The format of an Information Area is as follows:
 *   B format byte (00)
 *   B length in 8 byte units
 *   B language (00 = english)
 *  3B manufacturing date, minutes since 1/1/1996
 *   B type/length of manufacturer name string (up to 20 chars)
 *  #B manufacturer name
 *   B type/length of product name string (up to 14 chars)
 *  #B product name
 *   B type/length of serial number (up to 6 chars)
 *  #B serial number
 *   B type/length of part number (up to 10 chars)
 *  #B part number
 *   B FRU file id
 *   B type/length of board rev (always 0xC2)
 *  2B board revision
 *   B type/length of eeprom size field (0x01)
 *  1B size code for eeprom (02)
 *   B type/length of temp waiver field (0xC2)
 *  2B temp waiver
 *
 * and then in main boards only:
 *  G, P and Y encryption keys, each being
 *   B type/length (0x04)
 *  4B key
 *
 * and then on main boards being I-Bricks only:
 *   B type/length of mac address (as ascii string)
 * 12B mac address
 * followed by IEEE1394 configuration info, as type/length followed
 * by data again.
 *
 * A 0xC1 byte is the EOF record, and the last byte is a checksum.
 *
 * Type/length encoding is done as follows:
 * bits 7-6 are the type:
 *	00 binary data
 *	01 BCD
 *	02 packed 6 bit ascii
 *	03 regular 8 bit ascii
 * bits 5-0 are the length.
 */

#define	IA_TYPE_SHIFT	6
#define	IA_TYPE_BINARY	0
#define	IA_TYPE_BCD	1
#define	IA_TYPE_PACKED	2
#define	IA_TYPE_ASCII	3
#define	IA_LENGTH_MASK	0x3f

#define	IA_TL(t,l)	(((t) << IA_TYPE_SHIFT) | (l))

#define	IA_EOF		IA_TL(IA_TYPE_ASCII, 1)

static inline size_t
ia_skip(u_char *ia, size_t iapos, size_t ialen)
{
	size_t npos;

	ia += iapos;

	/* don't go past EOF marker */
	if (*ia == IA_EOF)
		return iapos;

	npos = iapos + 1 + (*ia & IA_LENGTH_MASK);
	return npos >= ialen ? iapos : npos;
}

int
l1_get_brick_ethernet_address(int16_t nasid, uint8_t *enaddr)
{
	u_char *ia;
	size_t iapos, ialen;
	char hexaddr[18], *d, *s;
	int type;
	int rc;

	/*
	 * If we are running on a C-Brick, the Ethernet address is stored
	 * in the matching I-Brick.
	 */
	if (sys_config.system_subtype == IP35_CBRICK)
		type = L1_TYPE_IOBRICK;
	else
		type = L1_TYPE_L1;

	/* read the Board IA of this node */
	rc = l1_read_board_ia(nasid, type, &ia, &ialen);
	if (rc != 0)
		return rc;

	/* simple sanity checks */
	if (ia[0] != 0 || ia[1] < 2 || ia[1] * 8UL > ialen) {
		rc = EINVAL;
		goto out;
	}

	/* skip fixed part */
	iapos = 6;
	/* skip 4 records */
	iapos = ia_skip(ia, iapos, ialen);
	iapos = ia_skip(ia, iapos, ialen);
	iapos = ia_skip(ia, iapos, ialen);
	iapos = ia_skip(ia, iapos, ialen);
	/* skip FRU */
	if (iapos < ialen - 1)
		iapos++;
	/* skip 3 records */
	iapos = ia_skip(ia, iapos, ialen);
	iapos = ia_skip(ia, iapos, ialen);
	iapos = ia_skip(ia, iapos, ialen);
	/* skip encryption key records if applicable */
	if (iapos < ialen && ia[iapos] == IA_TL(IA_TYPE_BINARY, 4)) {
		iapos = ia_skip(ia, iapos, ialen);
		iapos = ia_skip(ia, iapos, ialen);
		iapos = ia_skip(ia, iapos, ialen);
	}
	/* check the next record looks like an Ethernet address */
	if (iapos >= ialen - 1 - 12 || ia[iapos] != IA_TL(IA_TYPE_ASCII, 12)) {
		rc = EINVAL;
		goto out;
	}

	iapos++;
	s = (char *)ia + iapos;
	d = hexaddr;
	*d++ = *s++; *d++ = *s++; *d++ = ':';
	*d++ = *s++; *d++ = *s++; *d++ = ':';
	*d++ = *s++; *d++ = *s++; *d++ = ':';
	*d++ = *s++; *d++ = *s++; *d++ = ':';
	*d++ = *s++; *d++ = *s++; *d++ = ':';
	*d++ = *s++; *d++ = *s++; *d++ = '\0';
	enaddr_aton(hexaddr, enaddr);

out:
	free(ia, M_DEVBUF);
	return rc;
}

/*
 * Issue an arbitrary L1 command.
 */
int
l1_exec_command(int16_t nasid, const char *cmd)
{
	u_char pkt[64 + 64];	/* command and response packet buffer */
	size_t pktlen;
	uint32_t data;
	int rc;

	/*
	 * Build the command packet.
	 */
	pktlen = l1_command_build(pkt, sizeof pkt,
	    L1_ADDRESS(L1_TYPE_L1, L1_ADDRESS_LOCAL | L1_TASK_COMMAND),
	    L1_REQ_EXEC_CMD, 1,
	    L1_ARG_ASCII, cmd);
	if (pktlen > sizeof pkt) {
#ifdef DIAGNOSTIC
		panic("%s: L1 command packet too large (%zu) for buffer",
		    __func__, pktlen);
#endif
		return ENOMEM;
	}

	if (l1_packet_put(nasid, pkt, pktlen) != 0)
		return EWOULDBLOCK;

	pktlen = sizeof pkt;
	if (l1_receive_response(nasid, pkt, &pktlen) != 0)
		return EWOULDBLOCK;

	if (pktlen < 6) {
#ifdef L1_DEBUG
		printf("truncated response (length %d)\n", pktlen);
#endif
		return EIO;
	}

	/*
	 * Check the response code.
	 */

	data = l1_packet_get_be32(&pkt[1]);
	rc = l1_response_to_errno(data);
	if (rc != 0)
		return rc;

	/*
	 * We do not expect anything in return.
	 */

	if (pkt[5] != 0) {
#ifdef L1_DEBUG
		printf("unexpected L1 response: %d values\n", pkt[5]);
#endif
		return EIO;
	}

	return 0;
}

/*
 * Get a DIMM SPD record.
 */

int
l1_get_brick_spd_record(int16_t nasid, int dimm, u_char **rspd, size_t *rspdlen)
{
	u_char pkt[64 + EEPROM_CHUNK];	/* command and response packet buffer */
	u_char *pktbuf, *chunk, *spd = NULL;
	size_t pktlen, chunklen, spdlen, spdpos;
	uint32_t address, data;
	int rc;

	/*
	 * The L1 address of SPD records differs between Fuel and Origin 350
	 * systems. This is likely because the Fuel is a single-PIMM system,
	 * while all other IP35 are dual-PIMM, and thus carry one more PIMM
	 * record at a lower address (and are interleaving DIMM accesses).
	 * Since Fuel is also a single-node system, we can safely check for
	 * the system subtype to decide which address to use.
	 */
	switch (sys_config.system_subtype) {
	case IP35_FUEL:
		address = L1_EEP_DIMM_NOINTERLEAVE(dimm);
		break;
	case IP35_CBRICK:
		address = L1_EEP_DIMM_INTERLEAVE(L1_EEP_DIMM_BASE_CBRICK, dimm);
		break;
	default:
		address =
		    L1_EEP_DIMM_INTERLEAVE(L1_EEP_DIMM_BASE_CHIMERA, dimm);
		break;
	}

	/*
	 * Build a first packet, asking for 0 bytes to be read.
	 */
	pktlen = l1_command_build(pkt, sizeof pkt,
	    L1_ADDRESS(L1_TYPE_L1, L1_ADDRESS_LOCAL | L1_TASK_GENERAL),
	    L1_REQ_EEPROM, 4,
	    L1_ARG_INT, address,
	    L1_ARG_INT, (uint32_t)L1_EEP_SPD,
	    L1_ARG_INT, (uint32_t)0,	/* offset */
	    L1_ARG_INT, (uint32_t)0);	/* size */
	if (pktlen > sizeof pkt) {
#ifdef DIAGNOSTIC
		panic("%s: L1 command packet too large (%zu) for buffer",
		    __func__, pktlen);
#endif
		return ENOMEM;
	}

	if (l1_packet_put(nasid, pkt, pktlen) != 0)
		return EWOULDBLOCK;

	pktlen = sizeof pkt;
	if (l1_receive_response(nasid, pkt, &pktlen) != 0)
		return EWOULDBLOCK;

	if (pktlen < 6) {
#ifdef L1_DEBUG
		printf("truncated response (length %d)\n", pktlen);
#endif
		return EIO;
	}

	/*
	 * Check the response code.
	 */

	data = l1_packet_get_be32(&pkt[1]);
	rc = l1_response_to_errno(data);
	if (rc != 0)
		return rc;

	/*
	 * EEPROM read commands should return either one or two values:
	 * the first value is the size of the remaining EEPROM data, and
	 * the second value is the data read itself, if we asked for a
	 * nonzero size in the command (that size might be shorter than
	 * the data we asked for).
	 */

	if (pkt[5] != 1) {
#ifdef L1_DEBUG
		printf("unexpected L1 response: %d values\n", pkt[5]);
#endif
		return EIO;
	}

	pktbuf = pkt + 6;
	pktlen -= 6;

	if (l1_packet_get_int(&pktbuf, &pktlen, &data) != 0) {
#ifdef L1_DEBUG
		printf("unable to parse response as integer\n");
#endif
		return EIO;
	}

	/*
	 * Now that we know the size of the spd record, allocate memory for it.
	 */

	spdlen = (size_t)data;
	spd = (u_char *)malloc(spdlen, M_DEVBUF, M_NOWAIT);
	if (spd == NULL)
		return ENOMEM;

	/*
	 * Read the EEPROM contents in small chunks, so as not to keep L1
	 * busy for too long.
	 */

	spdpos = 0;
	while (spdpos < spdlen) {
		/*
		 * Build a command packet, this time actually reading data.
		 */
		pktlen = l1_command_build(pkt, sizeof pkt,
		    L1_ADDRESS(L1_TYPE_L1, L1_ADDRESS_LOCAL | L1_TASK_GENERAL),
		    L1_REQ_EEPROM, 4,
		    L1_ARG_INT, address,
		    L1_ARG_INT, (uint32_t)L1_EEP_SPD,
		    L1_ARG_INT, (uint32_t)spdpos,
		    L1_ARG_INT, (uint32_t)EEPROM_CHUNK);
		/* no need to check size again, it's the same size as earlier */

		if (l1_packet_put(nasid, pkt, pktlen) != 0) {
			rc = EWOULDBLOCK;
			goto fail;
		}

		pktlen = sizeof pkt;
		if (l1_receive_response(nasid, pkt, &pktlen) != 0) {
			rc = EWOULDBLOCK;
			goto fail;
		}

		if (pktlen < 6) {
#ifdef L1_DEBUG
			printf("truncated response (length %d)\n", pktlen);
#endif
			rc = EIO;
			goto fail;
		}

		/*
		 * Check the response code.
		 */

		data = l1_packet_get_be32(&pkt[1]);
		rc = l1_response_to_errno(data);
		if (rc != 0)
			goto fail;

		if (pkt[5] != 2) {
#ifdef L1_DEBUG
			printf("unexpected L1 response: %d values\n", pkt[5]);
#endif
			rc = EIO;
			goto fail;
		}

		pktbuf = pkt + 6;
		pktlen -= 6;

		if (l1_packet_get_int(&pktbuf, &pktlen, &data) != 0) {
#ifdef L1_DEBUG
			printf("unable to parse first response as integer\n");
#endif
			rc = EIO;
			goto fail;
		}

		if (l1_packet_get_binary(&pktbuf, &pktlen,
		    &chunk, &chunklen) != 0) {
#ifdef L1_DEBUG
			printf("unable to parse second response as binary\n");
#endif
			rc = EIO;
			goto fail;
		}

		/* should not happen, but we don't like infinite loops */
		if (chunklen == 0) {
#ifdef L1_DEBUG
			printf("read command returned 0 bytes\n");
#endif
			rc = EIO;
			goto fail;
		}

		memcpy(spd + spdpos, chunk, chunklen);
		spdpos += chunklen;
#ifdef L1_DEBUG
		printf("got %02x bytes of eeprom, %x/%x\n",
		    chunklen, spdpos, spdlen);
#endif
	}

	*rspd = spd;
	*rspdlen = spdlen;
	return 0;

fail:
	if (spd != NULL)
		free(spd, M_DEVBUF);
	return rc;
}
