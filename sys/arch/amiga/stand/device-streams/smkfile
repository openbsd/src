#    Makefile for stream-device utilities.
#
#    Copyright (C) 1993  Christian E. Hopps
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program; if not, write to the Free Software
#    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

protos=protos.h
stdsrcs= getdevices.c devices.c util.c
stdobjs= getdevices.o devices.o util.o
srcs=streamtodev.c rdbinfo.c devtostream.c $(stdsrcs)
objs=streamtodev.o rdbinfo.o devtostream.o $(stdobjs)

copts= nostkchk # DEFINE DEBUG_ENABLED_VERSION=1 debug=s

all: $(protos) devtostream rdbinfo streamtodev xdevtostream xstreamtodev

streamtodev: streamtodev.o $(stdobjs)
	slink SC SD ND to $@ from lib:c.o streamtodev.o $(stdobjs) lib getopt.lib unixemul.lib lib:sc.lib lib:amiga.lib

devtostream: devtostream.o $(stdobjs)
	slink SC SD ND to $@ from lib:c.o devtostream.o $(stdobjs) lib getopt.lib unixemul.lib lib:sc.lib lib:amiga.lib 

rdbinfo: rdbinfo.o $(stdobjs)
	slink SC SD ND to $@ from lib:c.o rdbinfo.o $(stdobjs) lib getopt.lib unixemul.lib lib:sc.lib lib:amiga.lib 

xstreamtodev: xstreamtodev.o $(stdobjs)
	slink SC SD ND to $@ from lib:c.o xstreamtodev.o $(stdobjs) lib getopt.lib unixemul.lib lib:sc.lib lib:amiga.lib

xdevtostream: xdevtostream.o $(stdobjs)
	slink SC SD ND to $@ from lib:c.o xdevtostream.o $(stdobjs) lib getopt.lib unixemul.lib lib:sc.lib lib:amiga.lib 


.c.o:
	sc $(copts) gst=custom:system.gst $<

$(protos): $(stdsrcs)
	protoman db=$(protos) $?

clean:
	-delete \#?.o
	-delete devtostream rdbinfo streamtodev
	copy clone protos_template.h protos.h

xstreamtodev.o: streamtodev.c
	sc $(copts) DEFINE EXPERT_VERSION=1 gst=custom:system.gst objname=$@ $<

xdevtostream.o: devtostream.c
	sc $(copts) DEFINE EXPERT_VERSION=1 gst=custom:system.gst objname=$@ $<
