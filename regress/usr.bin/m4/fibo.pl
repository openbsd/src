#! /usr/bin/perl
#	$OpenBSD: fibo.pl,v 1.2 2001/01/29 02:05:59 niklas Exp $

my $n=shift;

$fibo[0] = 'a';
$fibo[1] = 'b';
for (my $i = 2; $i <= $n; $i++) {
	$fibo[$i] = $fibo[$i-1].$fibo[$i-2];
}

print $fibo[$n], "\n";
	
