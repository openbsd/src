#!/bin/csh -f
#
#	$OpenBSD: vgrind.sh,v 1.5 2008/04/11 14:24:29 millert Exp $
#	$NetBSD: vgrind.sh,v 1.3 1994/11/17 08:28:06 jtc Exp $
#
# Copyright (c) 1980, 1993
#	The Regents of the University of California.  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. Neither the name of the University nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
#       @(#)vgrind.sh	8.1 (Berkeley) 6/6/93
#

# Allow troff to be overridden
if ( $?TROFF ) then
	set troff = "$TROFF"
else
	set troff = "groff"
endif

set vf=/usr/libexec/vfontedpr
set tm=/usr/share/tmac

set voptions=
set options=
set files=
set f=''
set head=""

top:
if ($#argv > 0) then
    switch ($1:q)

    case -f:
	set f='filter'
	set options = "$options $1:q"
	shift
	goto top

    case -T:
	if ($#argv < 2) then
	    echo "vgrind: $1:q option must have argument"
	    goto done
	else
	    set voptions = ($voptions $1:q $2)
	    shift
	    shift
	    goto top
	endif

    case -t:
	# ignore for backwards compatibility
	shift
	goto top

    case -o*:
	set voptions="$voptions $1:q"
	shift
	goto top

    case -W:
	set voptions = "$voptions -W"
	shift
	goto top

    case -d:
	if ($#argv < 2) then
	    echo "vgrind: $1:q option must have argument"
	    goto done
	else
	    set options = ($options $1:q $2)
	    shift
	    shift
	    goto top
	endif
			
    case -h:
	if ($#argv < 2) then
	    echo "vgrind: $1:q option must have argument"
	    goto done
	else
	    set head="$2"
	    shift
	    shift
	    goto top
	endif
			
    case -*:
	set options = "$options $1:q"
	shift
	goto top

    default:
	set files = "$files $1:q"
	shift
	goto top
    endsw
endif
if (-r index) then
    echo > nindex
    foreach i ($files)
	#	make up a sed delete command for filenames
	#	being careful about slashes.
	echo "? $i ?d" | sed -e "s:/:\\/:g" -e "s:?:/:g" >> nindex
    end
    sed -f nindex index >xindex
    if ($f == 'filter') then
	if ("$head" != "") then
	    $vf $options -h "$head" $files | cat $tm/tmac.vgrind -
	else
	    $vf $options $files | cat $tm/tmac.vgrind -
	endif
    else
	if ("$head" != "") then
	    $vf $options -h "$head" $files | \
		sh -c "$troff -rx1 $voptions -i -mvgrind 2>> xindex"
	else
	    $vf $options $files | \
		sh -c "$troff -rx1 $voptions -i -mvgrind 2>> xindex"
	endif
    endif
    sort -df +0 -2 xindex >index
    rm nindex xindex
else
    if ($f == 'filter') then
	if ("$head" != "") then
	    $vf $options -h "$head" $files | cat $tm/tmac.vgrind -
	else
	    $vf $options $files | cat $tm/tmac.vgrind -
	endif
    else
	if ("$head" != "") then
	    $vf $options -h "$head" $files | $troff -i $voptions -mvgrind
	else
	    $vf $options $files | $troff -i $voptions -mvgrind
	endif
    endif
endif

done:
