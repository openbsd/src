/*	$OpenBSD: options.c,v 1.20 2005/07/09 14:36:16 krw Exp $	*/

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

int bad_options = 0;
int bad_options_max = 5;

void	parse_options(struct packet *);
void	parse_option_buffer(struct packet *, unsigned char *, int);
int	store_options(unsigned char *, int, struct option_data *, int);

/*
 * Parse all available options out of the specified packet.
 */
void
parse_options(struct packet *packet)
{
	/* Initially, zero all option pointers. */
	memset(packet->options, 0, sizeof(packet->options));

	/* If we don't see the magic cookie, there's nothing to parse. */
	if (memcmp(packet->raw->options, DHCP_OPTIONS_COOKIE, 4)) {
		packet->options_valid = 0;
		return;
	}

	/*
	 * Go through the options field, up to the end of the packet or
	 * the End field.
	 */
	parse_option_buffer(packet, &packet->raw->options[4],
	    packet->packet_length - DHCP_FIXED_NON_UDP - 4);

	/*
	 * If we parsed a DHCP Option Overload option, parse more
	 * options out of the buffer(s) containing them.
	 */
	if (packet->options_valid &&
	    packet->options[DHO_DHCP_OPTION_OVERLOAD].data) {
		if (packet->options[DHO_DHCP_OPTION_OVERLOAD].data[0] & 1)
			parse_option_buffer(packet,
			    (unsigned char *)packet->raw->file,
			    sizeof(packet->raw->file));
		if (packet->options[DHO_DHCP_OPTION_OVERLOAD].data[0] & 2)
			parse_option_buffer(packet,
			    (unsigned char *)packet->raw->sname,
			    sizeof(packet->raw->sname));
	}
}

/*
 * Parse options out of the specified buffer, storing addresses of
 * option values in packet->options and setting packet->options_valid if
 * no errors are encountered.
 */
void
parse_option_buffer(struct packet *packet,
    unsigned char *buffer, int length)
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
		if (s + 2 > end) {
			len = 65536;
			goto bogus;
		}

		/*
		 * All other fields (except end, see above) have a
		 * one-byte length.
		 */
		len = s[1];

		/*
		 * If the length is outrageous, silently skip the rest,
		 * and mark the packet bad. Unfortunately some crappy
		 * dhcp servers always seem to give us garbage on the
		 * end of a packet. so rather than keep refusing, give
		 * up and try to take one after seeing a few without
		 * anything good.
		 */
		if (s + len + 2 > end) {
		    bogus:
			bad_options++;
			warning("option %s (%d) %s.",
			    dhcp_options[code].name, len,
			    "larger than buffer");
			if (bad_options == bad_options_max) {
				packet->options_valid = 1;
				bad_options = 0;
				warning("Many bogus options seen in offers. "
				    "Taking this offer in spite of bogus "
				    "options - hope for the best!");
			} else {
				warning("rejecting bogus offer.");
				packet->options_valid = 0;
			}
			return;
		}
		/*
		 * If we haven't seen this option before, just make
		 * space for it and copy it there.
		 */
		if (!packet->options[code].data) {
			if (!(t = calloc(1, len + 1)))
				error("Can't allocate storage for option %s.",
				    dhcp_options[code].name);
			/*
			 * Copy and NUL-terminate the option (in case
			 * it's an ASCII string.
			 */
			memcpy(t, &s[2], len);
			t[len] = 0;
			packet->options[code].len = len;
			packet->options[code].data = t;
		} else {
			/*
			 * If it's a repeat, concatenate it to whatever
			 * we last saw.   This is really only required
			 * for clients, but what the heck...
			 */
			t = calloc(1, len + packet->options[code].len + 1);
			if (!t)
				error("Can't expand storage for option %s.",
				    dhcp_options[code].name);
			memcpy(t, packet->options[code].data,
				packet->options[code].len);
			memcpy(t + packet->options[code].len,
				&s[2], len);
			packet->options[code].len += len;
			t[packet->options[code].len] = 0;
			free(packet->options[code].data);
			packet->options[code].data = t;
		}
		s += len + 2;
	}
	packet->options_valid = 1;
}

/*
 * cons options into a big buffer, and then split them out into the
 * three separate buffers if needed.  This allows us to cons up a set of
 * vendor options using the same routine.
 */
int
cons_options(struct dhcp_packet *outpacket, struct option_data *options)
{
	unsigned char buffer[4096];
	int main_buffer_size, mainbufix, bufix;
	int option_size, length;

	main_buffer_size = 576 - DHCP_FIXED_LEN;

	/* Copy the options into the big buffer... */
	option_size = store_options( buffer, (main_buffer_size - 7), options,
	    main_buffer_size);

	/* Put the cookie up front... */
	memcpy(outpacket->options, DHCP_OPTIONS_COOKIE, 4);
	mainbufix = 4;

	/*
	 * If we're going to have to overload, store the overload option
	 * at the beginning.  If we can, though, just store the whole
	 * thing in the packet's option buffer and leave it at that.
	 */
	if (option_size <= main_buffer_size - mainbufix) {
		memcpy(&outpacket->options[mainbufix],
		    buffer, option_size);
		mainbufix += option_size;
		if (mainbufix < main_buffer_size)
			outpacket->options[mainbufix++] = DHO_END;
		length = DHCP_FIXED_NON_UDP + mainbufix;
	} else {
		outpacket->options[mainbufix++] = DHO_DHCP_OPTION_OVERLOAD;
		outpacket->options[mainbufix++] = 1;
		if (option_size >
		    main_buffer_size - mainbufix + DHCP_FILE_LEN)
			outpacket->options[mainbufix++] = 3;
		else
			outpacket->options[mainbufix++] = 1;

		memcpy(&outpacket->options[mainbufix],
		    buffer, main_buffer_size - mainbufix);
		bufix = main_buffer_size - mainbufix;
		length = DHCP_FIXED_NON_UDP + mainbufix;
	}
	return (length);
}

/*
 * Store all the requested options into the requested buffer.
 */
int
store_options(unsigned char *buffer, int buflen, struct option_data *options,
    int first_cutoff)
{
	int bufix = 0, i, ix;

	/*
	 * Copy out the options in the order that they appear in the
	 * priority list.
	 */
	for (i = 0; i < sizeof dhcp_option_default_priority_list; i++) {
		/* Code for next option to try to store. */
		int code = dhcp_option_default_priority_list[i];
		int optstart;

		/*
		 * Number of bytes left to store (some may already have
		 * been stored by a previous pass).
		 */
		int length;

		/* If no data is available for this option, skip it. */
		if (!options[code].data)
			continue;

		/* We should now have a constant length for the option. */
		length = options[code].len;

		/* Try to store the option. */

		/*
		 * If the option's length is more than 255, we must
		 * store it in multiple hunks.   Store 255-byte hunks
		 * first.  However, in any case, if the option data will
		 * cross a buffer boundary, split it across that
		 * boundary.
		 */
		ix = 0;

		optstart = bufix;
		while (length) {
			unsigned char incr = length > 255 ? 255 : length;

			/*
			 * If this hunk of the buffer will cross a
			 * boundary, only go up to the boundary in this
			 * pass.
			 */
			if (bufix < first_cutoff &&
			    bufix + incr > first_cutoff)
				incr = first_cutoff - bufix;

			/*
			 * If this option is going to overflow the
			 * buffer, skip it.
			 */
			if (bufix + 2 + incr > buflen) {
				bufix = optstart;
				break;
			}

			/* Everything looks good - copy it in! */
			buffer[bufix] = code;
			buffer[bufix + 1] = incr;
			memcpy(buffer + bufix + 2, options[code].data + ix,
			    incr);
			length -= incr;
			ix += incr;
			bufix += 2 + incr;
		}
	}
	return (bufix);
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
							snprintf(op, opleft,
							    "\\%03o", *dp);
							op += 4;
							opleft -= 4;
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
			op += strlen(op);
			opleft -= strlen(op);
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
do_packet(struct interface_info *interface, struct dhcp_packet *packet,
    int len, unsigned int from_port, struct iaddr from, struct hardware *hfrom)
{
	struct packet tp;
	int i;

	if (packet->hlen > sizeof(packet->chaddr)) {
		note("Discarding packet with invalid hlen.");
		return;
	}

	memset(&tp, 0, sizeof(tp));
	tp.raw = packet;
	tp.packet_length = len;
	tp.client_port = from_port;
	tp.client_addr = from;
	tp.interface = interface;
	tp.haddr = hfrom;

	parse_options(&tp);
	if (tp.options_valid &&
	    tp.options[DHO_DHCP_MESSAGE_TYPE].data)
		tp.packet_type = tp.options[DHO_DHCP_MESSAGE_TYPE].data[0];
	if (tp.packet_type)
		dhcp(&tp);
	else
		bootp(&tp);

	/* Free the data associated with the options. */
	for (i = 0; i < 256; i++)
		if (tp.options[i].len && tp.options[i].data)
			free(tp.options[i].data);
}
