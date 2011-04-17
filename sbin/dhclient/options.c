/*	$OpenBSD: options.c,v 1.38 2011/04/17 19:57:23 phessler Exp $	*/

/* DHCP options parsing and reassembly. */

/*
 * Copyright (c) 1995, 1996, 1997, 1998 The Internet Software Consortium.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of The Internet Software Consortium nor the names
 *    of its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INTERNET SOFTWARE CONSORTIUM AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE INTERNET SOFTWARE CONSORTIUM OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This software has been written for the Internet Software Consortium
 * by Ted Lemon <mellon@fugue.com> in cooperation with Vixie
 * Enterprises.  To learn more about the Internet Software Consortium,
 * see ``http://www.vix.com/isc''.  To learn more about Vixie
 * Enterprises, see ``http://www.vix.com''.
 */

#include <ctype.h>

#include "dhcpd.h"

int parse_option_buffer(struct option_data *, unsigned char *, int);

/*
 * Parse options out of the specified buffer, storing addresses of
 * option values in options and setting client->options_valid if
 * no errors are encountered.
 */
int
parse_option_buffer(struct option_data *options, unsigned char *buffer,
    int length)
{
	unsigned char *s, *t, *end = buffer + length;
	int len, code;

	for (s = buffer; *s != DHO_END && s < end; ) {
		code = s[0];

		/* Pad options don't have a length - just skip them. */
		if (code == DHO_PAD) {
			s++;
			continue;
		}

		/*
		 * All options other than DHO_PAD and DHO_END have a
		 * one-byte length field.
		 */
		if (s + 2 > end)
			len = 0;
		else
			len = s[1];

		/*
		 * If the option claims to extend beyond the end of the buffer
		 * then mark the options buffer bad.
		 */
		if (s + len + 2 > end) {
			warning("option %s (%d) larger than buffer.",
			    dhcp_options[code].name, len);
			warning("rejecting bogus offer.");
			return (0);
		}
		/*
		 * If we haven't seen this option before, just make
		 * space for it and copy it there.
		 */
		if (!options[code].data) {
			if (!(t = calloc(1, len + 1)))
				error("Can't allocate storage for option %s.",
				    dhcp_options[code].name);
			/*
			 * Copy and NUL-terminate the option (in case
			 * it's an ASCII string).
			 */
			memcpy(t, &s[2], len);
			t[len] = 0;
			options[code].len = len;
			options[code].data = t;
		} else {
			/*
			 * If it's a repeat, concatenate it to whatever
			 * we last saw.   This is really only required
			 * for clients, but what the heck...
			 */
			t = calloc(1, len + options[code].len + 1);
			if (!t)
				error("Can't expand storage for option %s.",
				    dhcp_options[code].name);
			memcpy(t, options[code].data, options[code].len);
			memcpy(t + options[code].len, &s[2], len);
			options[code].len += len;
			t[options[code].len] = 0;
			free(options[code].data);
			options[code].data = t;
		}
		s += len + 2;
	}

	return (1);
}

/*
 * Copy as many options as fit in buflen bytes of buf. Return the
 * offset of the start of the last option copied. A caller can check
 * to see if it's DHO_END to decide if all the options were copied.
 */
int
cons_options(unsigned char *buf, const int buflen, struct option_data *options)
{
	int ix, incr, length, bufix, code, lastopt = -1;

	bzero(buf, buflen);

	if (buflen > 3)
		memcpy(buf, DHCP_OPTIONS_COOKIE, 4);
	bufix = 4;

	for (code = DHO_SUBNET_MASK; code < DHO_END; code++) {
		if (!options[code].data)
			continue;

		length = options[code].len;
		if (bufix + length + 2*((length+254)/255) >= buflen)
			return (lastopt);

		lastopt = bufix;
		ix = 0;

		while (length) {
			incr = length > 255 ? 255 : length;

			buf[bufix++] = code;
			buf[bufix++] = incr;
			memcpy(buf + bufix, options[code].data + ix, incr);

			length -= incr;
			ix += incr;
			bufix += incr;
		}
	}

	if (bufix < buflen) {
		buf[bufix] = DHO_END;
		lastopt = bufix;
	}

	return (lastopt);
}

/*
 * Format the specified option so that a human can easily read it.
 */
char *
pretty_print_option(unsigned int code, unsigned char *data, int len,
    int emit_commas, int emit_quotes)
{
	static char optbuf[32768]; /* XXX */
	int hunksize = 0, numhunk = -1, numelem = 0;
	char fmtbuf[32], *op = optbuf;
	int i, j, k, opleft = sizeof(optbuf);
	unsigned char *dp = data;
	struct in_addr foo;
	char comma;

	/* Code should be between 0 and 255. */
	if (code > 255)
		error("pretty_print_option: bad code %d", code);

	if (emit_commas)
		comma = ',';
	else
		comma = ' ';

	/* Figure out the size of the data. */
	for (i = 0; dhcp_options[code].format[i]; i++) {
		if (!numhunk) {
			warning("%s: Excess information in format string: %s",
			    dhcp_options[code].name,
			    &(dhcp_options[code].format[i]));
			break;
		}
		numelem++;
		fmtbuf[i] = dhcp_options[code].format[i];
		switch (dhcp_options[code].format[i]) {
		case 'A':
			--numelem;
			fmtbuf[i] = 0;
			numhunk = 0;
			if (hunksize == 0) {
				warning("%s: no size indicator before A"
				    " in format string: %s",
				    dhcp_options[code].name,
				    dhcp_options[code].format);
				return ("<fmt error>");
			}
			break;
		case 'X':
			for (k = 0; k < len; k++)
				if (!isascii(data[k]) ||
				    !isprint(data[k]))
					break;
			if (k == len) {
				fmtbuf[i] = 't';
				numhunk = -2;
			} else {
				fmtbuf[i] = 'x';
				hunksize++;
				comma = ':';
				numhunk = 0;
			}
			fmtbuf[i + 1] = 0;
			break;
		case 't':
			fmtbuf[i] = 't';
			fmtbuf[i + 1] = 0;
			numhunk = -2;
			break;
		case 'I':
		case 'l':
		case 'L':
			hunksize += 4;
			break;
		case 's':
		case 'S':
			hunksize += 2;
			break;
		case 'b':
		case 'B':
		case 'f':
			hunksize++;
			break;
		case 'e':
			break;
		default:
			warning("%s: garbage in format string: %s",
			    dhcp_options[code].name,
			    &(dhcp_options[code].format[i]));
			break;
		}
	}

	/* Check for too few bytes... */
	if (hunksize > len) {
		warning("%s: expecting at least %d bytes; got %d",
		    dhcp_options[code].name, hunksize, len);
		return ("<error>");
	}
	/* Check for too many bytes... */
	if (numhunk == -1 && hunksize < len)
		warning("%s: %d extra bytes",
		    dhcp_options[code].name, len - hunksize);

	/* If this is an array, compute its size. */
	if (!numhunk)
		numhunk = len / hunksize;
	/* See if we got an exact number of hunks. */
	if (numhunk > 0 && numhunk * hunksize < len)
		warning("%s: %d extra bytes at end of array",
		    dhcp_options[code].name, len - numhunk * hunksize);

	/* A one-hunk array prints the same as a single hunk. */
	if (numhunk < 0)
		numhunk = 1;

	/* Cycle through the array (or hunk) printing the data. */
	for (i = 0; i < numhunk; i++) {
		for (j = 0; j < numelem; j++) {
			int opcount;
			size_t oplen;
			switch (fmtbuf[j]) {
			case 't':
				if (emit_quotes) {
					*op++ = '"';
					opleft--;
				}
				for (; dp < data + len; dp++) {
					if (!isascii(*dp) ||
					    !isprint(*dp)) {
						if (dp + 1 != data + len ||
						    *dp != 0) {
							size_t oplen;
							snprintf(op, opleft,
							    "\\%03o", *dp);
							oplen = strlen(op);
							op += oplen;
							opleft -= oplen;
						}
					} else if (*dp == '"' ||
					    *dp == '\'' ||
					    *dp == '$' ||
					    *dp == '`' ||
					    *dp == '\\') {
						*op++ = '\\';
						*op++ = *dp;
						opleft -= 2;
					} else {
						*op++ = *dp;
						opleft--;
					}
				}
				if (emit_quotes) {
					*op++ = '"';
					opleft--;
				}

				*op = 0;
				break;
			case 'I':
				foo.s_addr = htonl(getULong(dp));
				opcount = strlcpy(op, inet_ntoa(foo), opleft);
				if (opcount >= opleft)
					goto toobig;
				opleft -= opcount;
				dp += 4;
				break;
			case 'l':
				opcount = snprintf(op, opleft, "%ld",
				    (long)getLong(dp));
				if (opcount >= opleft || opcount == -1)
					goto toobig;
				opleft -= opcount;
				dp += 4;
				break;
			case 'L':
				opcount = snprintf(op, opleft, "%ld",
				    (unsigned long)getULong(dp));
				if (opcount >= opleft || opcount == -1)
					goto toobig;
				opleft -= opcount;
				dp += 4;
				break;
			case 's':
				opcount = snprintf(op, opleft, "%d",
				    getShort(dp));
				if (opcount >= opleft || opcount == -1)
					goto toobig;
				opleft -= opcount;
				dp += 2;
				break;
			case 'S':
				opcount = snprintf(op, opleft, "%d",
				    getUShort(dp));
				if (opcount >= opleft || opcount == -1)
					goto toobig;
				opleft -= opcount;
				dp += 2;
				break;
			case 'b':
				opcount = snprintf(op, opleft, "%d",
				    *(char *)dp++);
				if (opcount >= opleft || opcount == -1)
					goto toobig;
				opleft -= opcount;
				break;
			case 'B':
				opcount = snprintf(op, opleft, "%d", *dp++);
				if (opcount >= opleft || opcount == -1)
					goto toobig;
				opleft -= opcount;
				break;
			case 'x':
				opcount = snprintf(op, opleft, "%x", *dp++);
				if (opcount >= opleft || opcount == -1)
					goto toobig;
				opleft -= opcount;
				break;
			case 'f':
				opcount = strlcpy(op,
				    *dp++ ? "true" : "false", opleft);
				if (opcount >= opleft)
					goto toobig;
				opleft -= opcount;
				break;
			default:
				warning("Unexpected format code %c", fmtbuf[j]);
			}
			oplen = strlen(op);
			op += oplen;
			opleft -= oplen;
			if (opleft < 1)
				goto toobig;
			if (j + 1 < numelem && comma != ':') {
				*op++ = ' ';
				opleft--;
			}
		}
		if (i + 1 < numhunk) {
			*op++ = comma;
			opleft--;
		}
		if (opleft < 1)
			goto toobig;

	}
	return (optbuf);
 toobig:
	warning("dhcp option too large");
	return ("<error>");
}

void
do_packet(int len, unsigned int from_port, struct iaddr from,
    struct hardware *hfrom)
{
	struct dhcp_packet *packet = &client->packet;
	struct option_data options[256];
	struct iaddrlist *ap;
	void (*handler)(struct iaddr, struct option_data *);
	char *type;
	int i, options_valid = 1;

	if (packet->hlen > sizeof(packet->chaddr)) {
		note("Discarding packet with invalid hlen.");
		return;
	}

	/*
	 * Silently drop the packet if the client hardware address in the
	 * packet is not the hardware address of the interface being managed.
	 */
	if ((ifi->hw_address.hlen != packet->hlen) ||
	    (memcmp(ifi->hw_address.haddr, packet->chaddr, packet->hlen)))
		return;

	memset(options, 0, sizeof(options));

	if (memcmp(&packet->options, DHCP_OPTIONS_COOKIE, 4) == 0) {
		/* Parse the BOOTP/DHCP options field. */
		options_valid = parse_option_buffer(options,
		    &packet->options[4], sizeof(packet->options) - 4);

		/* Only DHCP packets have overload areas for options. */
		if (options_valid &&
		    options[DHO_DHCP_MESSAGE_TYPE].data &&
		    options[DHO_DHCP_OPTION_OVERLOAD].data) {
			if (options[DHO_DHCP_OPTION_OVERLOAD].data[0] & 1)
				options_valid = parse_option_buffer(options,
				    (unsigned char *)packet->file,
				    sizeof(packet->file));
			if (options_valid &&
			    options[DHO_DHCP_OPTION_OVERLOAD].data[0] & 2)
				options_valid = parse_option_buffer(options,
				    (unsigned char *)packet->sname,
				    sizeof(packet->sname));
		}
	}

	type = "";
	handler = NULL;

	if (options[DHO_DHCP_MESSAGE_TYPE].data) {
		/* Always try a DHCP packet, even if a bad option was seen. */
		switch (options[DHO_DHCP_MESSAGE_TYPE].data[0]) {
		case DHCPOFFER:
			handler = dhcpoffer;
			type = "DHCPOFFER";
			break;
		case DHCPNAK:
			handler = dhcpnak;
			type = "DHCPNACK";
			break;
		case DHCPACK:
			handler = dhcpack;
			type = "DHCPACK";
			break;
		default:
			break;
		}
	} else if (options_valid && packet->op == BOOTREPLY) {
		handler = dhcpoffer;
		type = "BOOTREPLY";
	}

	if (handler && client->xid == client->packet.xid) {
		if (hfrom->hlen == 6)
			note("%s from %s (%s)", type, piaddr(from),
			    ether_ntoa((struct ether_addr *)hfrom->haddr));
		else
			note("%s from %s", type, piaddr(from));
	} else
		handler = NULL;

	for (ap = config->reject_list; ap && handler; ap = ap->next)
		if (addr_eq(from, ap->addr)) {
			note("%s from %s rejected.", type, piaddr(from));
			handler = NULL;
		}

	if (handler)
		(*handler)(from, options);

	for (i = 0; i < 256; i++)
		if (options[i].len && options[i].data)
			free(options[i].data);
}
