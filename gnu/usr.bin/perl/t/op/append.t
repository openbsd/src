#!./perl

# $RCSfile: append.t,v $$Revision: 4.1 $$Date: 92/08/07 18:27:36 $

print "1..13\n";

$a = 'ab' . 'c';	# compile time
$b = 'def';

$c = $a . $b;
print "#1\t:$c: eq :abcdef:\n";
if ($c eq 'abcdef') {print "ok 1\n";} else {print "not ok 1\n";}

$c .= 'xyz';
print "#2\t:$c: eq :abcdefxyz:\n";
if ($c eq 'abcdefxyz') {print "ok 2\n";} else {print "not ok 2\n";}

$_ = $a;
$_ .= $b;
print "#3\t:$_: eq :abcdef:\n";
if ($_ eq 'abcdef') {print "ok 3\n";} else {print "not ok 3\n";}

# test that when right argument of concat is UTF8, and is the same
# variable as the target, and the left argument is not UTF8, it no
# longer frees the wrong string.
{
    sub r2 {
	my $string = '';
	$string .= pack("U0a*", 'mnopqrstuvwx');
	$string = "abcdefghijkl$string";
    }

    r2() and print "ok $_\n" for qw/ 4 5 /;
}

# test that nul bytes get copied
{
    my ($a, $ab)   = ("a", "a\0b");
    my ($ua, $uab) = map pack("U0a*", $_), $a, $ab;

    my $ub = pack("U0a*", 'b');

    my $t1 = $a; $t1 .= $ab;

    print $t1 =~ /b/ ? "ok 6\n" : "not ok 6\t# $t1\n";
    
    my $t2 = $a; $t2 .= $uab;
    
    print eval '$t2 =~ /$ub/' ? "ok 7\n" : "not ok 7\t# $t2\n";
    
    my $t3 = $ua; $t3 .= $ab;
    
    print $t3 =~ /$ub/ ? "ok 8\n" : "not ok 8\t# $t3\n";
    
    my $t4 = $ua; $t4 .= $uab;
    
    print eval '$t4 =~ /$ub/' ? "ok 9\n" : "not ok 9\t# $t4\n";
    
    my $t5 = $a; $t5 = $ab . $t5;
    
    print $t5 =~ /$ub/ ? "ok 10\n" : "not ok 10\t# $t5\n";
    
    my $t6 = $a; $t6 = $uab . $t6;
    
    print eval '$t6 =~ /$ub/' ? "ok 11\n" : "not ok 11\t# $t6\n";
    
    my $t7 = $ua; $t7 = $ab . $t7;
    
    print $t7 =~ /$ub/ ? "ok 12\n" : "not ok 12\t# $t7\n";
    
    my $t8 = $ua; $t8 = $uab . $t8;
    
    print eval '$t8 =~ /$ub/' ? "ok 13\n" : "not ok 13\t# $t8\n";
}
