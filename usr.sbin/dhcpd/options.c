/*	$OpenBSD: options.c,v 1.16 2007/10/21 01:08:17 krw Exp $	*/

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
int	store_options(unsigned char *, int, struct tree_cache **,
	    unsigned char *, int, int, int, int);


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
	unsigned char *s, *t;
	unsigned char *end = buffer + length;
	int len;
	int code;

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
				warning("Many bogus options seen in offers.");
				warning("Taking this offer in spite of bogus");
				warning("options - hope for the best!");
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
			if (!(t = dmalloc(len + 1, "parse_option_buffer")))
				error("Can't allocate storage for option %s.",
				    dhcp_options[code].name);
			/*
			 * Copy and NUL-terminate the option (in case
			 * it's an ASCII string).
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
			t = dmalloc(len + packet->options[code].len + 1,
			    "parse_option_buffer");
			if (!t)
				error("Can't expand storage for option %s.",
				    dhcp_options[code].name);
			memcpy(t, packet->options[code].data,
				packet->options[code].len);
			memcpy(t + packet->options[code].len,
				&s[2], len);
			packet->options[code].len += len;
			t[packet->options[code].len] = 0;
			dfree(packet->options[code].data,
			    "parse_option_buffer");
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
cons_options(struct packet *inpacket, struct dhcp_packet *outpacket,
    int mms, struct tree_cache **options,
    int overload, /* Overload flags that may be set. */
    int terminate, int bootpp, u_int8_t *prl, int prl_len)
{
	unsigned char priority_list[300];
	int priority_len;
	unsigned char buffer[4096];	/* Really big buffer... */
	int main_buffer_size;
	int mainbufix, bufix;
	int option_size;

	/*
	 * If the client has provided a maximum DHCP message size, use
	 * that; otherwise, if it's BOOTP, only 64 bytes; otherwise use
	 * up to the minimum IP MTU size (576 bytes).
	 *
	 * XXX if a BOOTP client specifies a max message size, we will
	 * honor it.
	 */
	if (!mms &&
	    inpacket &&
	    inpacket->options[DHO_DHCP_MAX_MESSAGE_SIZE].data &&
	    (inpacket->options[DHO_DHCP_MAX_MESSAGE_SIZE].len >=
	    sizeof(u_int16_t))) {
		mms = getUShort(
		    inpacket->options[DHO_DHCP_MAX_MESSAGE_SIZE].data);
		if (mms < 576)
			mms = 576;	/* mms must be >= minimum IP MTU */
	}

	if (mms)
		main_buffer_size = mms - DHCP_FIXED_LEN;
	else if (bootpp)
		main_buffer_size = 64;
	else
		main_buffer_size = 576 - DHCP_FIXED_LEN;

	if (main_buffer_size > sizeof(outpacket->options))
		main_buffer_size = sizeof(outpacket->options);

	/* Preload the option priority list with mandatory options. */
	priority_len = 0;
	priority_list[priority_len++] = DHO_DHCP_MESSAGE_TYPE;
	priority_list[priority_len++] = DHO_DHCP_SERVER_IDENTIFIER;
	priority_list[priority_len++] = DHO_DHCP_LEASE_TIME;
	priority_list[priority_len++] = DHO_DHCP_MESSAGE;

	/*
	 * If the client has provided a list of options that it wishes
	 * returned, use it to prioritize.  Otherwise, prioritize based
	 * on the default priority list.
	 */
	if (inpacket &&
	    inpacket->options[DHO_DHCP_PARAMETER_REQUEST_LIST].data) {
		int prlen =
		    inpacket->options[DHO_DHCP_PARAMETER_REQUEST_LIST].len;
		if (prlen + priority_len > sizeof(priority_list))
			prlen = sizeof(priority_list) - priority_len;

		memcpy(&priority_list[priority_len],
		    inpacket->options[DHO_DHCP_PARAMETER_REQUEST_LIST].data,
		    prlen);
		priority_len += prlen;
		prl = priority_list;
	} else if (prl) {
		if (prl_len + priority_len > sizeof(priority_list))
			prl_len = sizeof(priority_list) - priority_len;

		memcpy(&priority_list[priority_len], prl, prl_len);
		priority_len += prl_len;
		prl = priority_list;
	} else {
		memcpy(&priority_list[priority_len],
		    dhcp_option_default_priority_list,
		    sizeof_dhcp_option_default_priority_list);
		priority_len += sizeof_dhcp_option_default_priority_list;
	}

	/* Copy the options into the big buffer... */
	option_size = store_options(
	    buffer,
	    (main_buffer_size - 7 + ((overload & 1) ? DHCP_FILE_LEN : 0) +
		((overload & 2) ? DHCP_SNAME_LEN : 0)),
	    options, priority_list, priority_len, main_buffer_size,
	    (main_buffer_size + ((overload & 1) ? DHCP_FILE_LEN : 0)),
	    terminate);

	/* Initialize the buffers to be used and put the cookie up front. */
	memset(outpacket->options, DHO_PAD, sizeof(outpacket->options));
	if (overload & 1)
		memset(outpacket->file, DHO_PAD, DHCP_FILE_LEN);
	if (overload & 2)
		memset(outpacket->sname, DHO_PAD, DHCP_SNAME_LEN);

	memcpy(outpacket->options, DHCP_OPTIONS_COOKIE, 4);
	mainbufix = 4;

	/*
	 * If we can, just store the whole thing in the packet's option buffer
	 * and leave it at that.
	 */
	if (option_size <= main_buffer_size - mainbufix) {
		memcpy(&outpacket->options[mainbufix], buffer, option_size);
		mainbufix += option_size;
		if (mainbufix < main_buffer_size)
			outpacket->options[mainbufix++] = DHO_END;
		return (DHCP_FIXED_NON_UDP + mainbufix);
	}

	/*
	 * We're going to have to overload. Store the overload option
	 * at the beginning.
	 */
	outpacket->options[mainbufix++] = DHO_DHCP_OPTION_OVERLOAD;
	outpacket->options[mainbufix++] = 1;
	if (option_size > main_buffer_size - mainbufix + DHCP_FILE_LEN)
		outpacket->options[mainbufix++] = 3;
	else
		outpacket->options[mainbufix++] = 1;

	bufix = main_buffer_size - mainbufix;
	memcpy(&outpacket->options[mainbufix], buffer, bufix);

	if (overload & 1) {
		mainbufix = option_size - bufix;
		if (mainbufix <= DHCP_FILE_LEN) {
			memcpy(outpacket->file, &buffer[bufix], mainbufix);
			if (mainbufix < DHCP_FILE_LEN)
				outpacket->file[mainbufix] = (char)DHO_END;
			bufix = option_size;
		} else {
			memcpy(outpacket->file, &buffer[bufix], DHCP_FILE_LEN);
			bufix += DHCP_FILE_LEN;
		}
	}

	if ((overload & 2) && option_size > bufix) {
		mainbufix = option_size - bufix;
		memcpy(outpacket->sname, &buffer[bufix], mainbufix);
		if (mainbufix < DHCP_SNAME_LEN)
			outpacket->sname[mainbufix] = (char)DHO_END;
	}

	return (DHCP_FIXED_NON_UDP + main_buffer_size);
}

/*
 * Store all the requested options into the requested buffer.
 */
int
store_options(unsigned char *buffer, int buflen, struct tree_cache **options,
    unsigned char *priority_list, int priority_len, int first_cutoff,
    int second_cutoff, int terminate)
{
	int bufix = 0;
	int option_stored[256];
	int i;
	int ix;
	int tto;

	/* Zero out the stored-lengths array. */
	memset(option_stored, 0, sizeof(option_stored));

	/*
	 * Copy out the options in the order that they appear in the
	 * priority list...
	 */
	for (i = 0; i < priority_len; i++) {
		/* Code for next option to try to store. */
		int code = priority_list[i];
		int optstart;

		/*
		 * Number of bytes left to store (some may already have
		 * been stored by a previous pass).
		 */
		int length;

		/* If no data is available for this option, skip it. */
		if (!options[code]) {
			continue;
		}

		/*
		 * The client could ask for things that are mandatory,
		 * in which case we should avoid storing them twice...
		 */
		if (option_stored[code])
			continue;
		option_stored[code] = 1;

		/* Find the value of the option... */
		if (!tree_evaluate(options[code]))
			continue;

		/* We should now have a constant length for the option. */
		length = options[code]->len;

		/* Do we add a NUL? */
		if (terminate && dhcp_options[code].format[0] == 't') {
			length++;
			tto = 1;
		} else
			tto = 0;

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
			else if (bufix < second_cutoff &&
			    bufix + incr > second_cutoff)
				incr = second_cutoff - bufix;

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
			if (tto && incr == length) {
				memcpy(buffer + bufix + 2,
				    options[code]->value + ix, incr - 1);
				buffer[bufix + 2 + incr - 1] = 0;
			} else
				memcpy(buffer + bufix + 2,
				    options[code]->value + ix, incr);
			length -= incr;
			ix += incr;
			bufix += 2 + incr;
		}
	}
	return (bufix);
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
			dfree(tp.options[i].data, "do_packet");
}
