#!/bin/sh
#
# $OpenBSD: xargs-L.sh,v 1.1 2010/03/25 01:43:47 schwarze Exp $
#
# written by Ingo Schwarze <schwarze@openbsd.org> 2010
# and placed in the public domain

test_xargs()
{
	printf 'Testing %13.13s with options "%s"\n' "\"$1\"" "$2"
        expect=`printf "$3"`
	result=`printf "$1" | xargs $2 ./showargs`
	if [ "$result" != "$expect" ]; then
		printf 'Expected "%s", but got "%s"\n' "$expect" "$result"
		exit 1
	fi
}

test_xargs 'a b'         ''        'a|b|'
test_xargs 'a  b'        ''        'a|b|'
test_xargs 'a\nb'        ''        'a|b|'
test_xargs 'a\n\nb'      ''        'a|b|'
test_xargs 'a \nb'       ''        'a|b|'
test_xargs 'a\n b'       ''        'a|b|'
test_xargs 'a \n b'      ''        'a|b|'
test_xargs 'a\n \nb'     ''        'a|b|'
test_xargs 'a \n\nb'     ''        'a|b|'

test_xargs 'a\\ b'       ''        'a b|'
test_xargs 'a\\ \nb'     ''        'a |b|'
test_xargs 'a\n\\ b'     ''        'a| b|'

test_xargs 'a\\\nb'      ''        'a\nb|'
test_xargs 'a\n\\\nb'    ''        'a|\nb|'
test_xargs 'a \\\nb'     ''        'a|\nb|'
test_xargs 'a\\\n b'     ''        'a\n|b|'
test_xargs 'a \\\n b'    ''        'a|\n|b|'

test_xargs 'a b'         '-L 1'    'a|b|'
test_xargs 'a  b'        '-L 1'    'a|b|'
test_xargs 'a\nb'        '-L 1'    'a|\nb|'
test_xargs 'a\n\nb'      '-L 1'    'a|\nb|'
test_xargs 'a \nb'       '-L 1'    'a|b|'
test_xargs 'a\n b'       '-L 1'    'a|\nb|'
test_xargs 'a \n b'      '-L 1'    'a|b|'
test_xargs 'a\n \nb'     '-L 1'    'a|\nb|'
test_xargs 'a \n\nb'     '-L 1'    'a|b|'

test_xargs 'a\\ b'       '-L 1'    'a b|'
test_xargs 'a\\ \nb'     '-L 1'    'a |\nb|'
test_xargs 'a\n\\ b'     '-L 1'    'a|\n b|'

test_xargs 'a\\\nb'      '-L 1'    'a\nb|'
test_xargs 'a\n\\\nb'    '-L 1'    'a|\n\nb|'
test_xargs 'a \\\nb'     '-L 1'    'a|\nb|'
test_xargs 'a\\\n b'     '-L 1'    'a\n|b|'
test_xargs 'a \\\n b'    '-L 1'    'a|\n|b|'

test_xargs 'a b'         '-0'      'a b|'
test_xargs 'a  b'        '-0'      'a  b|'
test_xargs 'a\nb'        '-0'      'a\nb|'
test_xargs 'a\n\nb'      '-0'      'a\n\nb|'
test_xargs 'a \nb'       '-0'      'a \nb|'
test_xargs 'a\n b'       '-0'      'a\n b|'
test_xargs 'a \n b'      '-0'      'a \n b|'
test_xargs 'a\n \nb'     '-0'      'a\n \nb|'
test_xargs 'a \n\nb'     '-0'      'a \n\nb|'

test_xargs 'a\\ b'       '-0'      'a\\ b|'
test_xargs 'a\\ \nb'     '-0'      'a\\ \nb|'
test_xargs 'a\n\\ b'     '-0'      'a\n\\ b|'

test_xargs 'a\\\nb'      '-0'      'a\\\nb|'
test_xargs 'a\n\\\nb'    '-0'      'a\n\\\nb|'
test_xargs 'a \\\nb'     '-0'      'a \\\nb|'
test_xargs 'a\\\n b'     '-0'      'a\\\n b|'
test_xargs 'a \\\n b'    '-0'      'a \\\n b|'

test_xargs 'a b\0c'      '-0 -L 1' 'a b|c|'
test_xargs 'a  b\0c'     '-0 -L 1' 'a  b|c|'
test_xargs 'a\nb\0c'     '-0 -L 1' 'a\nb|\nc|'
test_xargs 'a\n\nb\0c'   '-0 -L 1' 'a\n\nb|\nc|'
test_xargs 'a \nb\0c'    '-0 -L 1' 'a \nb|c|'
test_xargs 'a\n b\0c'    '-0 -L 1' 'a\n b|\nc|'
test_xargs 'a \n b\0c'   '-0 -L 1' 'a \n b|c|'
test_xargs 'a\n \nb\0c'  '-0 -L 1' 'a\n \nb|\nc|'
test_xargs 'a \n\nb\0c'  '-0 -L 1' 'a \n\nb|c|'

test_xargs 'a\\ b\0c'    '-0 -L 1' 'a\\ b|c|'
test_xargs 'a\\ \nb\0c'  '-0 -L 1' 'a\\ \nb|c|'
test_xargs 'a\n\\ b\0c'  '-0 -L 1' 'a\n\\ b|\nc|'

test_xargs 'a\\\nb\0c'   '-0 -L 1' 'a\\\nb|\nc|'
test_xargs 'a\n\\\nb\0c' '-0 -L 1' 'a\n\\\nb|\nc|'
test_xargs 'a \\\nb\0c'  '-0 -L 1' 'a \\\nb|\nc|'
test_xargs 'a\\\n b\0c'  '-0 -L 1' 'a\\\n b|\nc|'
test_xargs 'a \\\n b\0c' '-0 -L 1' 'a \\\n b|\nc|'
