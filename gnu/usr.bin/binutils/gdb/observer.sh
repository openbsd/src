#!/bin/sh -e

if test $# -ne 3
then
    echo "Usage: $0 <h|inc> <observer.texi> <observer.out>" 1>&2
    exit 0
fi

lang=$1 ; shift
texi=$1 ; shift
o=$1 ; shift
echo "Creating ${o}-tmp" 1>&2
rm -f ${o}-tmp

# Can use any of the following: cat cmp cp diff echo egrep expr false
# grep install-info ln ls mkdir mv pwd rm rmdir sed sleep sort tar
# test touch true

cat <<EOF >>${o}-tmp
/* GDB Notifications to Observers.

   Copyright 2004 Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   --

   This file was generated using observer.sh and observer.texi.  */

EOF


case $lang in
    h) cat <<EOF >>${o}-tmp
#ifndef OBSERVER_H
#define OBSERVER_H

struct observer;
struct bpstats;
struct so_list;
EOF
        ;;
esac


# generate a list of events that can be observed

IFS=:
sed -n '
/@deftypefun void/{
# Save original line for later processing into the actual parameter
    h
# Convert from: @deftypefun void EVENT (TYPE @var{PARAM},...)
# to event and formals: EVENT:TYPE PARAM, ...:
    s/^.* void \([a-z_][a-z_]*\) (\(.*\))$/\1:\2/
    s/@var{//g
    s/}//g
# Switch to held
    x
# Convert from: @deftypefun void FUNC (TYPE @var{PARAM},...)
# to actuals: PARAM, ...
    s/^[^{]*[{]*//
    s/[}]*[^}]*$//
    s/}[^{]*{/, /g
# Combine held (EVENT:TYPE PARAM, ...:) and pattern (PARAM, ...) into
# FUNC:TYPE PARAM, ...:PARAM, ...
    H
    x
    s/\n/:/g
    p
}
' $texi | while read event formal actual
do
  case $lang in
      h) cat <<EOF >>${o}-tmp

/* ${event} notifications.  */

typedef void (observer_${event}_ftype) (${formal});

extern struct observer *observer_attach_${event} (observer_${event}_ftype *f);
extern void observer_detach_${event} (struct observer *observer);
extern void observer_notify_${event} (${formal});
EOF
	;;

      inc)
      	cat <<EOF >>${o}-tmp

/* ${event} notifications.  */

static struct observer_list *${event}_subject = NULL;

struct ${event}_args { `echo "${formal}" | sed -e 's/,/;/g'`; };

static void
observer_${event}_notification_stub (const void *data, const void *args_data)
{
  observer_${event}_ftype *notify = (observer_${event}_ftype *) data;
  const struct ${event}_args *args = args_data;
  notify (`echo ${actual} | sed -e 's/\([a-z0-9_][a-z0-9_]*\)/args->\1/g'`);
}

struct observer *
observer_attach_${event} (observer_${event}_ftype *f)
{
  return generic_observer_attach (&${event}_subject,
				  &observer_${event}_notification_stub,
				  (void *) f);
}

void
observer_detach_${event} (struct observer *observer)
{
  generic_observer_detach (&${event}_subject, observer);
}

void
observer_notify_${event} (${formal})
{
  struct ${event}_args args;
  `echo ${actual} | sed -e 's/\([a-z0-9_][a-z0-9_]*\)/args.\1 = \1/g'`;
  if (observer_debug)
    fprintf_unfiltered (gdb_stdlog, "observer_notify_${event}() called\n");
  generic_observer_notify (${event}_subject, &args);
}
EOF
	;;
    esac
done


case $lang in
    h) cat <<EOF >>${o}-tmp

#endif /* OBSERVER_H */
EOF
esac


echo Moving ${o}-tmp to ${o}
mv ${o}-tmp ${o}
