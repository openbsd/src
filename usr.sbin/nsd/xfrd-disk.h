/*
 * xfrd-disk.h - XFR (transfer) Daemon TCP system header file. Save/Load state to disk.
 *
 * Copyright (c) 2001-2011, NLnet Labs. All rights reserved.
 *
 * See LICENSE for the license.
 *
 */

#ifndef XFRD_DISK_H
#define XFRD_DISK_H

#include <config.h>
struct xfrd_state;

/* magic string to identify xfrd state file */
#define XFRD_FILE_MAGIC "NSDXFRD1"

/* read from state file as many zones as possible (until error/eof).*/
void xfrd_read_state(struct xfrd_state* xfrd);
/* write xfrd zone state if possible */
void xfrd_write_state(struct xfrd_state* xfrd);

#endif /* XFRD_DISK_H */
