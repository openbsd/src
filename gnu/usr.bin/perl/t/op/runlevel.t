#!./perl

##
## all of these tests are from Michael Schroeder
## <Michael.Schroeder@informatik.uni-erlangen.de>
##
## The more esoteric failure modes require Michael's
## stack-of-stacks patch (so we don't test them here,
## and they are commented out before the __END__).
##
## The remaining tests pass with a simpler fix
## intended for 5.004
##
## Gurusamy Sarathy <gsar@umich.edu> 97-02-24
##

chdir 't' if -d 't';
@INC = "../lib";
$Is_VMS = $^O eq 'VMS';
$Is_MSWin32 = $^O eq 'MSWin32';
$ENV{PERL5LIB} = "../lib" unless $Is_VMS;

$|=1;

undef $/;
@prgs = split "\n########\n", <DATA>;
print "1..", scalar @prgs, "\n";

$tmpfile = "runltmp000";
1 while -f ++$tmpfile;
END { if ($tmpfile) { 1 while unlink $tmpfile; } }

for (@prgs){
    my $switch;
    if (s/^\s*(-\w+)//){
       $switch = $1;
    }
    my($prog,$expected) = split(/\nEXPECT\n/, $_);
    open TEST, ">$tmpfile";
    print TEST "$prog\n";
    close TEST;
    my $results = $Is_VMS ?
		  `MCR $^X "-I[-.lib]" $switch $tmpfile` :
		      $Is_MSWin32 ?  
			  `.\\perl -I../lib $switch $tmpfile 2>&1` :
			      `sh -c './perl $switch $tmpfile' 2>&1`;
    my $status = $?;
    $results =~ s/\n+$//;
    # allow expected output to be written as if $prog is on STDIN
    $results =~ s/runltmp\d+/-/g;
    $results =~ s/\n%[A-Z]+-[SIWEF]-.*$// if $Is_VMS;  # clip off DCL status msg
    $expected =~ s/\n+$//;
    if ($results ne $expected) {
       print STDERR "PROG: $switch\n$prog\n";
       print STDERR "EXPECTED:\n$expected\n";
       print STDERR "GOT:\n$results\n";
       print "not ";
    }
    print "ok ", ++$i, "\n";
}

=head2 stay out of here (the real tests are after __END__)

##
## these tests don't pass yet (need the full stack-of-stacks patch)
## GSAR 97-02-24
##

########
# sort within sort
sub sortfn {
  (split(/./, 'x'x10000))[0];
  my (@y) = ( 4, 6, 5);
  @y = sort { $a <=> $b } @y;
  print "sortfn ".join(', ', @y)."\n";
  return $_[0] <=> $_[1];
}
@x = ( 3, 2, 1 );
@x = sort { &sortfn($a, $b) } @x;
print "---- ".join(', ', @x)."\n";
EXPECT
sortfn 4, 5, 6
---- 1, 2, 3
########
# trapping eval within sort (doesn't work currently because
# die does a SWITCHSTACK())
@a = (3, 2, 1);
@a = sort { eval('die("no way")') ,  $a <=> $b} @a;
print join(", ", @a)."\n";
EXPECT
1, 2, 3
########
# this actually works fine, but results in a poor error message
@a = (1, 2, 3);
foo:
{
  @a = sort { last foo; } @a;
}
EXPECT
cannot reach destination block at - line 2.
########
package TEST;
 
sub TIESCALAR {
  my $foo;
  return bless \$foo;
}
sub FETCH {
  next;
  return "ZZZ";
}
sub STORE {
}
 
package main;
 
tie $bar, TEST;
{
  print "- $bar\n";
}
print "OK\n";
EXPECT
cannot reach destination block at - line 8.
########
package TEST;
 
sub TIESCALAR {
  my $foo;
  return bless \$foo;
}
sub FETCH {
  goto bbb;
  return "ZZZ";
}
 
package main;
 
tie $bar, TEST;
print "- $bar\n";
exit;
bbb:
print "bbb\n";
EXPECT
bbb
########
# trapping eval within sort (doesn't work currently because
# die does a SWITCHSTACK())
sub foo {
  $a <=> $b unless eval('$a == 0 ? die("foo\n") : ($a <=> $b)');
}
@a = (3, 2, 0, 1);
@a = sort foo @a;
print join(', ', @a)."\n";
EXPECT
0, 1, 2, 3
########
package TEST;
sub TIESCALAR {
  my $foo;
  next;
  return bless \$foo;
}
package main;
{
tie $bar, TEST;
}
EXPECT
cannot reach destination block at - line 4.
########
# large stack extension causes realloc, and segfault
package TEST;
sub TIESCALAR {
  my $foo;
  return bless \$foo;
}
sub FETCH {
  return "fetch";
}
sub STORE {
(split(/./, 'x'x10000))[0];
}
package main;
tie $bar, TEST;
$bar = "x";

=cut

##
##
## The real tests begin here
##
##

__END__
@a = (1, 2, 3);
{
  @a = sort { last ; } @a;
}
EXPECT
Can't "last" outside a block at - line 3.
########
package TEST;
 
sub TIESCALAR {
  my $foo;
  return bless \$foo;
}
sub FETCH {
  eval 'die("test")';
  print "still in fetch\n";
  return ">$@<";
}
package main;
 
tie $bar, TEST;
print "- $bar\n";
EXPECT
still in fetch
- >test at (eval 1) line 1.
<
########
package TEST;
 
sub TIESCALAR {
  my $foo;
  eval('die("foo\n")');
  print "after eval\n";
  return bless \$foo;
}
sub FETCH {
  return "ZZZ";
}
 
package main;
 
tie $bar, TEST;
print "- $bar\n";
print "OK\n";
EXPECT
after eval
- ZZZ
OK
########
package TEST;
 
sub TIEHANDLE {
  my $foo;
  return bless \$foo;
}
sub PRINT {
print STDERR "PRINT CALLED\n";
(split(/./, 'x'x10000))[0];
eval('die("test\n")');
}
 
package main;
 
open FH, ">&STDOUT";
tie *FH, TEST;
print FH "OK\n";
print STDERR "DONE\n";
EXPECT
PRINT CALLED
DONE
########
sub warnhook {
  print "WARNHOOK\n";
  eval('die("foooo\n")');
}
$SIG{'__WARN__'} = 'warnhook';
warn("dfsds\n");
print "END\n";
EXPECT
WARNHOOK
END
########
package TEST;
 
use overload
     "\"\""   =>  \&str
;
 
sub str {
  eval('die("test\n")');
  return "STR";
}
 
package main;
 
$bar = bless {}, TEST;
print "$bar\n";
print "OK\n";
EXPECT
STR
OK
########
sub foo {
  $a <=> $b unless eval('$a == 0 ? bless undef : ($a <=> $b)');
}
@a = (3, 2, 0, 1);
@a = sort foo @a;
print join(', ', @a)."\n";
EXPECT
0, 1, 2, 3
########
sub foo {
  goto bar if $a == 0 || $b == 0;
  $a <=> $b;
}
@a = (3, 2, 0, 1);
@a = sort foo @a;
print join(', ', @a)."\n";
exit;
bar:
print "bar reached\n";
EXPECT
Can't "goto" outside a block at - line 2.
