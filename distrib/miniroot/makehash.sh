#!/bin/sh

dir=$1
rev=$2

if [ $# -lt 3 ]; then
	echo makehash.sh: No RELEASEDIR -- skipping hash storage
	exit 0
fi

rel=$3

#echo makehash args $dir $rev $rel

for i in bsd bsd.mp; do
	if [ -f $rel/$i ]; then
		cat $rel/$i | sum -a sha256 > $dir/$i
		#ls -alF $rel/$i
		#echo $i `cat $dir/$i`
	fi
done

if [ $# -gt 3 ]; then
	shift; shift; shift;
	for i in $*; do
		cat $rel/$i | sum -a sha256 > $dir/$i
		#ls -alF $rel/$i
		#echo $i `cat $dir/$i`
	done
fi

for i in base comp etc misc man game ; do
	cat $rel/$i$rev.tgz | sum -a sha256 > $dir/$i$rev.tgz
	#ls -alF $rel/$i$rev.tgz
	#echo $i$rev.tgz `cat $dir/$i$rev.tgz`
done
