/*
 * ipc.h - Interprocess communication routines. Handlers read and write.
 *
 * Copyright (c) 2001-2006, NLnet Labs. All rights reserved.
 *
 * See LICENSE for the license.
 *
 */

#ifndef NSD_IPC_H
#define NSD_IPC_H

#include <config.h>
#include "netio.h"
struct buffer;
struct nsd;
struct nsd_child;
struct xfrd_tcp;

/*
 * Data for the server_main IPC handler 
 * Used by parent side to listen to children, and write to children.
 */
struct main_ipc_handler_data
{
	struct nsd	*nsd;
	struct nsd_child *child;
	int		child_num;

	/* pointer to the socket, as it may change if it is restarted */
	int		*xfrd_sock;
	struct buffer	*packet;
	int		forward_mode;
	size_t		got_bytes;
	uint16_t	total_bytes;
	uint32_t	acl_num;
	
	/* writing data, connection and state */
	uint8_t		busy_writing_zone_state;
	struct xfrd_tcp	*write_conn;
};

/*
 * Data for ipc handler, nsd and a conn for reading ipc msgs.
 * Used by children to listen to parent. 
 * Used by parent to listen to xfrd.
 */
struct ipc_handler_conn_data
{
	struct nsd	*nsd;
	struct xfrd_tcp	*conn;
};

/*
 * Routine used by server_main.
 * Handle a command received from the xfrdaemon processes.
 */
void parent_handle_xfrd_command(netio_type *netio,
	netio_handler_type *handler, netio_event_types_type event_types);

/*
 * Routine used by server_main.
 * Handle a command received from the reload process.
 */
void parent_handle_reload_command(netio_type *netio,
	netio_handler_type *handler, netio_event_types_type event_types);

/*
 * Routine used by server_main.
 * Handle a command received from the children processes.
 * Send commands and forwarded xfrd packets when writable.
 */
void parent_handle_child_command(netio_type *netio,
	netio_handler_type *handler, netio_event_types_type event_types);

/*
 * Routine used by server_child.
 * Handle a command received from the parent process.
 */
void child_handle_parent_command(netio_type *netio,
	netio_handler_type *handler, netio_event_types_type event_types);

/*
 * Routine used by xfrd
 * Handle interprocess communication with parent process, read and write.
 */
void xfrd_handle_ipc(netio_type *netio,
	netio_handler_type *handler, netio_event_types_type event_types);

/* check if all children have exited in an orderly fashion and set mode */
void parent_check_all_children_exited(struct nsd* nsd);

#endif /* NSD_IPC_H */
