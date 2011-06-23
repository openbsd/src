#!/bin/sh

# error counter
err=0

# test function; arguments are:
# input string
# regular expression to replace
# wanted output for /g (global substitution)
# wanted output for /1 (substitution of first match) and so on
t() {
	# global substitution
	in=$1
	expr=$2
	want=$3
	shift 3
	out=`echo "$in" | sed -E "s/$expr/x/g"`
	[ "X$out" = "X$want" ] || echo "$in/$expr/g/$want/$out ($((++err)))"

	# substitution with specific index
	num=1
	while [ $# -gt 0 ]; do
		want=$1
		shift
		out=`echo "$in" | sed -E "s/$expr/x/$num"`
		[ "X$out" = "X$want" ] || \
			echo "$in/$expr/$num/$want/$out ($((++err)))"
		num=$((num+1))
	done

	# substitution with excessive index
	out=`echo "$in" | sed -E "s/$expr/x/$num"`
	[ "X$out" = "X$in" ] || echo "$in/$expr/$num/=/$out ($((++err)))"
}

t '' ^ x x
t '' '()' x x
t '' '$' x x
t '' '^|$' x x
t a ^ xa xa
t a '()' xax xa ax
t a '$' ax ax
t a '^|a' x x
t a '^|$' xax xa ax
t a '^|a|$' x x
t a 'a|$' x x
t ab ^ xab xab
t ab '()' xaxbx xab axb abx
t ab '$' abx abx
t ab '^|a' xb xb
t ab '^|b' xax xab ax
t ab '^|$' xabx xab abx
t ab '^|a|$' xbx xb abx
t ab '^|b|$' xax xab ax
t ab '^|a|b|$' xx xb ax
t ab '^|ab|$' x x
t ab 'a|()' xbx xb abx
t ab 'a|$' xbx xb abx
t ab 'ab|$' x x
t ab 'b|()' xax xab ax
t ab 'b|$' ax ax
t abc '^|b' xaxc xabc axc
t abc '^|b|$' xaxcx xabc axc abcx
t abc '^|bc|$' xax xabc ax
t abc 'ab|()' xcx xc abcx
t abc 'ab|$' xcx xc abcx
t abc 'b|()' xaxcx xabc axc abcx
t abc 'bc|()' xax xabc ax
t abc 'b|$' axcx axc abcx
t aa a xx xa ax
t aa 'a|()' xx xa ax
t aa 'a*' x x

exit $err
