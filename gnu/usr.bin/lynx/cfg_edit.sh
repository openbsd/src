#!/bin/sh
# Invoked from cfg_defs.sh as a filter
# Strip leading and trailing whitespace
# Escape any iternal '\'
# Escape any iternal '"'
# Entify any iternal '&', '<' or '>'
# Append a '=' if none present'
# Break into two strings at '='
# Prefix ' { "' and suffix '" },'
sort |
sed	-e 's!^[ 	]*!!' -e 's![ 	]*$!!' \
	-e 's!\\!\\\\!g'        \
	-e 's!"!\\"!g'          \
	-e 's!&!\&amp;!g' -e 's!<!\&lt;!g' -e 's!>!\&gt;!g' \
	-e 's!^[^=]*$!&=!' \
	-e 's!=!",	"!'     \
	-e 's!^!	{ "!' -e 's!$!" },!'
