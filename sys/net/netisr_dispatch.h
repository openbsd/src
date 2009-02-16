/*	$OpenBSD: netisr_dispatch.h,v 1.17 2009/02/16 00:31:25 dlg Exp $	*/
/* $NetBSD: netisr_dispatch.h,v 1.2 2000/07/02 04:40:47 cgd Exp $ */

/*
 * netisr_dispatch: This file is included by the 
 *	machine dependant softnet function.  The
 *	DONETISR macro should be set before including
 *	this file.  i.e.:
 *
 * softintr() {
 *	...do setup stuff...
 *	#define DONETISR(bit, fn) do { ... } while (0)
 *	#include <net/netisr_dispatch.h>
 *	#undef DONETISR
 *	...do cleanup stuff.
 * }
 */

#ifndef _NET_NETISR_H_
#error <net/netisr.h> must be included before <net/netisr_dispatch.h>
#endif

#ifndef _NET_NETISR_DISPATCH_H_
#define _NET_NETISR_DISPATCH_H_
#include "bluetooth.h"
#include "ether.h"
#include "ppp.h"
#include "bridge.h"
#include "pppoe.h"
#include "pfsync.h"
#endif

/*
 * When adding functions to this list, be sure to add headers to provide
 * their prototypes in <net/netisr.h> (if necessary).
 */

#ifdef INET
#if NETHER > 0
	DONETISR(NETISR_ARP,arpintr);
#endif
	DONETISR(NETISR_IP,ipintr);
#endif
#ifdef INET6
	DONETISR(NETISR_IPV6,ip6intr);
#endif
#ifdef MPLS
	DONETISR(NETISR_MPLS,mplsintr);
#endif
#ifdef NETATALK
	DONETISR(NETISR_ATALK,atintr);
#endif
#if NATM > 0
	DONETISR(NETISR_NATM,natmintr);
#endif
#if NPPP > 0
	DONETISR(NETISR_PPP,pppintr);
#endif
#if NBRIDGE > 0
        DONETISR(NETISR_BRIDGE,bridgeintr);
#endif
#if NPPPOE > 0
	DONETISR(NETISR_PPPOE,pppoeintr);
#endif
#if NBLUETOOTH > 0
	DONETISR(NETISR_BT,btintr);
#endif
#if NPFSYNC > 0
	DONETISR(NETISR_PFSYNC,pfsyncintr);
#endif
	DONETISR(NETISR_TX,nettxintr);
