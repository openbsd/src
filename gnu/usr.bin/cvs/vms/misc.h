/*
 * Copyright © 1994 the Free Software Foundation, Inc.
 *
 * Author: Richard Levitte (levitte@e.kth.se)
 *
 * This file is a part of GNU VMSLIB, the GNU library for porting GNU
 * software to VMS.
 *
 * GNU VMSLIB is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * GNU VMSLIB is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

void x_free();
/* This is a trick, because the linker wants uppercase symbols, and in
   that case, xfree is confused with Xfree, which is bad.  */
#define xfree x_free

/*
 * Some string utilities.
 */
char *downcase ();
char *strndup ();

int fixpath ();
char *argvconcat ();
