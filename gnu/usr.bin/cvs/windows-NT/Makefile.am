## Process this file with automake to produce Makefile.in

# *** Under Windows NT and Microsoft Visual C++, we use cvsnt.mak, not
# *** this makefile.  However, we need this file in order for 'make
# *** dist' to work properly on Unix machines.

# Makefile for GNU CVS windows-NT dist.
# Copyright (C) 2000 Free Software Foundation, Inc.

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

EXTRA_DIST = \
	config.h \
	ndir.h \
	options.h \
	pwd.h \
	rcmd.h \
	filesubr.c \
	mkdir.c \
	ndir.c \
	pwd.c \
	rcmd.c \
	run.c \
	sockerror.c \
	startserver.c \
	stripslash.c \
	waitpid.c \
	win32.c \
	.cvsignore

SUBDIRS = SCC

# for backwards compatibility with the old makefiles
realclean: maintainer-clean
.PHONY: realclean
