#!./perl

#
# grep() and map() tests
#

print "1..38\n";

$test = 1;

sub ok {
    my ($got,$expect) = @_;
    print "# expected [$expect], got [$got]\nnot " if $got ne $expect;
    print "ok $test\n";
}

{
   my @lol = ([qw(a b c)], [], [qw(1 2 3)]);
   my @mapped = map  {scalar @$_} @lol;
   ok "@mapped", "3 0 3";
   $test++;

   my @grepped = grep {scalar @$_} @lol;
   ok "@grepped", "$lol[0] $lol[2]";
   $test++;

   @grepped = grep { $_ } @mapped;
   ok "@grepped", "3 3";
   $test++;
}

{
   print map({$_} ("ok $test\n"));
   $test++;
   print map
            ({$_} ("ok $test\n"));
   $test++;
   print((map({a => $_}, ("ok $test\n")))[0]->{a});
   $test++;
   print((map
            ({a=>$_},
	     ("ok $test\n")))[0]->{a});
   $test++;
   print map { $_ } ("ok $test\n");
   $test++;
   print map
            { $_ } ("ok $test\n");
   $test++;
   print((map {a => $_}, ("ok $test\n"))[0]->{a});
   $test++;
   print((map
            {a=>$_},
	     ("ok $test\n"))[0]->{a});
   $test++;
   my $x = "ok \xFF\xFF\n";
   print map($_&$x,("ok $test\n"));
   $test++;
   print map
            ($_ & $x, ("ok $test\n"));
   $test++;
   print map { $_ & $x } ("ok $test\n");
   $test++;
   print map
             { $_&$x } ("ok $test\n");
   $test++;

   print grep({$_} ("ok $test\n"));
   $test++;
   print grep
            ({$_} ("ok $test\n"));
   $test++;
   print grep({a => $_}->{a}, ("ok $test\n"));
   $test++;
   print grep
	     ({a => $_}->{a},
	     ("ok $test\n"));
   $test++;
   print grep { $_ } ("ok $test\n");
   $test++;
   print grep
             { $_ } ("ok $test\n");
   $test++;
   print grep {a => $_}->{a}, ("ok $test\n");
   $test++;
   print grep
	     {a => $_}->{a},
	     ("ok $test\n");
   $test++;
   print grep($_&"X",("ok $test\n"));
   $test++;
   print grep
            ($_&"X", ("ok $test\n"));
   $test++;
   print grep { $_ & "X" } ("ok $test\n");
   $test++;
   print grep
             { $_ & "X" } ("ok $test\n");
   $test++;
}

# Tests for "for" in "map" and "grep"
# Used to dump core, bug [perl #17771]

{
    my @x;
    my $y = '';
    @x = map { $y .= $_ for 1..2; 1 } 3..4;
    print "# @x,$y\n";
    print "@x,$y" eq "1 1,1212" ? "ok $test\n" : "not ok $test\n";
    $test++;
    $y = '';
    @x = map { $y .= $_ for 1..2; $y .= $_ } 3..4;
    print "# @x,$y\n";
    print "@x,$y" eq "123 123124,123124" ? "ok $test\n" : "not ok $test\n";
    $test++;
    $y = '';
    @x = map { for (1..2) { $y .= $_ } $y .= $_ } 3..4;
    print "# @x,$y\n";
    print "@x,$y" eq "123 123124,123124" ? "ok $test\n" : "not ok $test\n";
    $test++;
    $y = '';
    @x = grep { $y .= $_ for 1..2; 1 } 3..4;
    print "# @x,$y\n";
    print "@x,$y" eq "3 4,1212" ? "ok $test\n" : "not ok $test\n";
    $test++;
    $y = '';
    @x = grep { for (1..2) { $y .= $_ } 1 } 3..4;
    print "# @x,$y\n";
    print "@x,$y" eq "3 4,1212" ? "ok $test\n" : "not ok $test\n";
    $test++;

    # Add also a sample test from [perl #18153].  (The same bug).
    $a = 1; map {if ($a){}} (2);
    print "ok $test\n"; # no core dump is all we need
    $test++;
}

{
    sub add_an_x(@){
        map {"${_}x"} @_;
    };
    ok join("-",add_an_x(1,2,3,4)), "1x-2x-3x-4x";
    $test++;
}

{
    my $gimme;

    sub gimme {
	my $want = wantarray();
	if (defined $want) {
	    $gimme = $want ? 'list' : 'scalar';
	} else {
	    $gimme = 'void';
	}
    }

    my @list = 0..9;

    undef $gimme; gimme for @list;      ok($gimme, 'void');   $test++;
    undef $gimme; grep { gimme } @list; ok($gimme, 'scalar'); $test++;
    undef $gimme; map { gimme } @list;  ok($gimme, 'list');   $test++;
}

{
    # This shouldn't loop indefinitively.
    my @empty = map { while (1) {} } ();
    ok("@empty", '');
}
