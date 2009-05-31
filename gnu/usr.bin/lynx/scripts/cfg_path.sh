#!/bin/sh
# Use this script for substituting the configured path into lynx.cfg -
# not all paths begin with a slash.
SECOND=`echo "$2" | sed -e 's,^/,,'`
sed -e "/^[abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_]*:file:/s,/PATH_TO/$1,/$SECOND,"
