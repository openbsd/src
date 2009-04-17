#!/bin/sh

dir=$1
rev=$2

if [ $# -lt 3 ]; then
	echo makehash.sh: No RELEASEDIR -- skipping hash storage
	exit 0
fi

rel=$3

#echo makehash args $dir $rev $rel

cat $rel/bsd | sum -a sha256 > $dir/bsd
#ls -alF $rel/bsd
#echo bsd `cat $dir/bsd`

if [ -f $rel/bsd.mp ]; then
	cat $rel/bsd.mp | sum -a sha256 > $dir/bsd.mp
	#ls -alF $rel/bsd.mp
	#echo bsd.mp `cat $dir/bsd.mp`
fi

for i in base comp etc misc man game ; do
	cat $rel/$i$rev.tgz | sum -a sha256 > $dir/$i$rev.tgz
	#ls -alF $rel/$i$rev.tgz
	#echo $i$rev.tgz `cat $dir/$i$rev.tgz`
done
