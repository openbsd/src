#!/bin/bash
################################################################################
#
#  mkapidoc.sh -- generate apidoc.fnc from scanning the Perl source
#
################################################################################
#
#  $Revision: 1.2 $
#  $Author: millert $
#  $Date: 2006/03/28 19:23:02 $
#
################################################################################
#
#  Version 3.x, Copyright (C) 2004-2005, Marcus Holland-Moritz.
#  Version 2.x, Copyright (C) 2001, Paul Marquess.
#  Version 1.x, Copyright (C) 1999, Kenneth Albanowski.
#
#  This program is free software; you can redistribute it and/or
#  modify it under the same terms as Perl itself.
#
################################################################################

function isperlroot
{
  [ -f "$1/embed.fnc" ] && [ -f "$1/perl.h" ]
}

function usage
{
  echo "USAGE: $0 [perlroot] [output-file] [embed.fnc]"
  exit 0
}

if [ -z "$1" ]; then
  if isperlroot "../../.."; then
    PERLROOT=../../..
  else
    PERLROOT=.
  fi
else
  PERLROOT=$1
fi

if [ -z "$2" ]; then
  if [ -f "parts/apidoc.fnc" ]; then
    OUTPUT="parts/apidoc.fnc"
  else
    usage
  fi
else
  OUTPUT=$2
fi

if [ -z "$3" ]; then
  if [ -f "parts/embed.fnc" ]; then
    EMBED="parts/embed.fnc"
  else
    usage
  fi
else
  EMBED=$3
fi

if isperlroot $PERLROOT; then
  grep -hr '^=for apidoc' $PERLROOT | sed -e 's/=for apidoc //' | grep '|' | sort | uniq \
     | perl -e'$f=pop;open(F,$f)||die"$f:$!";while(<F>){(split/\|/)[2]=~/(\w+)/;$h{$1}++}
               while(<>){s/[ \t]+$//;(split/\|/)[2]=~/(\w+)/;$h{$1}||print}' $EMBED >$OUTPUT
else
  usage
fi
