#	$OpenBSD: Makefile,v 1.3 1997/09/21 11:44:27 deraadt Exp $

#  PINT Makefile for utilities
#  $Id: Makefile,v 1.3 1997/09/21 11:44:27 deraadt Exp $
#
# Copyright (c) 1996 Kenneth Stailey
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. All advertising materials mentioning features or use of this software
#    must display the following acknowledgement:
#	This product includes software developed for the NetBSD Project
#	by Kenneth Stailey
# 4. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
# AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
#

CC = gcc
CFLAGS = -O -g -DTRUE=1 -DFALSE=0

PROGS = get_scanner set_scanner use_adf reset_scanner fs1toppm
SCRIPTS = fujitsu_m3096g_grabscan.sh grabscan.sh hp_scanjet_IIc_grabscan.sh \
	ricoh_fs1_grabscan.sh ricoh_is410_grabscan.sh ricoh_is50_grabscan.sh \
	sharp_jx600_grabscan.sh umax_uc630_grabscan.sh

BIN_DIR = /usr/local/bin
MAN_DIR = /usr/local/man/man1

all: $(PROGS)

install: all
	cp $(PROGS) $(BIN_DIR)
	cp *.1 $(MAN_DIR)

clean:
	rm -f $(PROGS) *.o core a.out
