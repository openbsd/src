#!./perl

# $RCSfile: sort.t,v $$Revision: 4.1 $$Date: 92/08/07 18:28:24 $

print "1..19\n";

sub backwards { $a lt $b ? 1 : $a gt $b ? -1 : 0 }

@harry = ('dog','cat','x','Cain','Abel');
@george = ('gone','chased','yz','punished','Axed');

$x = join('', sort @harry);
print ($x eq 'AbelCaincatdogx' ? "ok 1\n" : "not ok 1\n");
print "# x = '$x'\n";

$x = join('', sort( backwards @harry));
print ($x eq 'xdogcatCainAbel' ? "ok 2\n" : "not ok 2\n");
print "# x = '$x'\n";

$x = join('', sort @george, 'to', @harry);
print ($x eq 'AbelAxedCaincatchaseddoggonepunishedtoxyz'?"ok 3\n":"not ok 3\n");
print "# x = '$x'\n";

@a = ();
@b = reverse @a;
print ("@b" eq "" ? "ok 4\n" : "not ok 4 (@b)\n");

@a = (1);
@b = reverse @a;
print ("@b" eq "1" ? "ok 5\n" : "not ok 5 (@b)\n");

@a = (1,2);
@b = reverse @a;
print ("@b" eq "2 1" ? "ok 6\n" : "not ok 6 (@b)\n");

@a = (1,2,3);
@b = reverse @a;
print ("@b" eq "3 2 1" ? "ok 7\n" : "not ok 7 (@b)\n");

@a = (1,2,3,4);
@b = reverse @a;
print ("@b" eq "4 3 2 1" ? "ok 8\n" : "not ok 8 (@b)\n");

@a = (10,2,3,4);
@b = sort {$a <=> $b;} @a;
print ("@b" eq "2 3 4 10" ? "ok 9\n" : "not ok 9 (@b)\n");

$sub = 'backwards';
$x = join('', sort $sub @harry);
print ($x eq 'xdogcatCainAbel' ? "ok 10\n" : "not ok 10\n");

# literals, combinations

@b = sort (4,1,3,2);
print ("@b" eq '1 2 3 4' ? "ok 11\n" : "not ok 11\n");
print "# x = '@b'\n";

@b = sort grep { $_ } (4,1,3,2);
print ("@b" eq '1 2 3 4' ? "ok 12\n" : "not ok 12\n");
print "# x = '@b'\n";

@b = sort map { $_ } (4,1,3,2);
print ("@b" eq '1 2 3 4' ? "ok 13\n" : "not ok 13\n");
print "# x = '@b'\n";

@b = sort reverse (4,1,3,2);
print ("@b" eq '1 2 3 4' ? "ok 14\n" : "not ok 14\n");
print "# x = '@b'\n";

$^W = 0;
# redefining sort sub inside the sort sub should fail
sub twoface { *twoface = sub { $a <=> $b }; &twoface }
eval { @b = sort twoface 4,1,3,2 };
print ($@ =~ /redefine active sort/ ? "ok 15\n" : "not ok 15\n");

# redefining sort subs outside the sort should not fail
eval { *twoface = sub { &backwards } };
print $@ ? "not ok 16\n" : "ok 16\n";

eval { @b = sort twoface 4,1,3,2 };
print ("@b" eq '4 3 2 1' ? "ok 17\n" : "not ok 17 |@b|\n");

*twoface = sub { *twoface = *backwards; $a <=> $b };
eval { @b = sort twoface 4,1 };
print ($@ =~ /redefine active sort/ ? "ok 18\n" : "not ok 18\n");

*twoface = sub {
                 eval 'sub twoface { $a <=> $b }';
		 die($@ =~ /redefine active sort/ ? "ok 19\n" : "not ok 19\n");
		 $a <=> $b;
	       };
eval { @b = sort twoface 4,1 };
print $@ ? "$@" : "not ok 19\n";
