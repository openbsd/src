#!/bin/sh

# na.phone:
# Area Code : City : State/Province : State/Province Abbrev.

# areas:
# Area Code : State/Province|State/Province Abbrev. : City

if [ X"$1" == X"" ]; then
	exit 1
fi

AC=0
grep -v '\#' $1 | grep -v '\$' | \
while T= read -r line; do
	AC_LAST=$AC
	AC=`echo $line | cut -d: -f1`

	# skip line if area code isn't numeric
	CMD=`echo $AC | grep "^[0-9]*$"`
	if [ $? -eq 1 ]; then
		continue
	fi

	# skip line if area code is a duplicate
	if [ $AC -eq $AC_LAST ]; then
		continue
	fi

	C=`echo $line | cut -d: -f2`
	SP=`echo $line | cut -d: -f3`
	SPA=`echo $line | cut -d: -f4`

	if [ X"$SPA" == X"" ]; then
		echo "$AC:$SP:$C"
	else
		echo "$AC:$SP|$SPA:$C"
	fi
done
