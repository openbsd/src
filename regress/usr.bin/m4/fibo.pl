#! /usr/bin/perl
my $n=shift;

$fibo[0] = 'a';
$fibo[1] = 'b';
for (my $i = 2; $i <= $n; $i++) {
	$fibo[$i] = $fibo[$i-1].$fibo[$i-2];
}

print $fibo[$n], "\n";
	
