/*
 * tclWinInt.h --
 *
 *	This header file handles thunking issues to produce a synchronous
 *      spawn operation under Win32s.
 *
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 */

/*
 * Constants for Universal Thunking dispatcher *
 */

#define TCLSYNCHSPAWN     1

/*
 * The following function is a universal thunk wrapper used to
 * invoke 16-bit code.
 */

DWORD APIENTRY TclSynchSpawn( LPCSTR CmdLine, UINT CmdShow );


