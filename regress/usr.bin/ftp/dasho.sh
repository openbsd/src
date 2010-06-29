#!/bin/sh

: ${FTP:=ftp}

tmpdir=$(readlink -f temp) &&
mkdir $tmpdir || exit 1

trap 'rm -rf "$tmpdir"' EXIT
trap 'rm -rf "$tmpdir"; exit 1' INT HUP TERM

cd "$tmpdir" || exit 1

mkdir src dest &&
cd dest || exit 1

echo 'DASH' >> ../src/-
echo 'XXXX' >> ../src/X

args=$1
exitcode=$2
stdout=$3
stderr=$4
files=$5

echo "Testing ${FTP} $1"

eval "\"\$FTP\" $1" >../stdout 2>../stderr
echo -n $? > ../exitcode
for a in *; do
	test -e $a || continue
	print -rn -- "[$a] "
	cat ./$a
done >../files

result=0
for a in exitcode stdout stderr files; do
	if ! eval "[ X\"\$$a\" == X\"$(<../$a)\" ]"; then
		echo "*** $a ***"
		echo "expected:"
		eval "print -r -- \"\$$a\"" | sed 's/^/> /'
		echo "got:"
		cat ../$a | sed 's/^/> /'
		echo
		result=1
	fi
done

exit $result
