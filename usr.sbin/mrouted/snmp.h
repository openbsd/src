/*	$NetBSD: snmp.h,v 1.3 1995/12/10 10:07:18 mycroft Exp $	*/

extern int portlist[32], sdlen;
extern in_port_t dest_port;
extern int quantum;

extern int snmp_read_packet();

#define DEFAULT_PORT 161
