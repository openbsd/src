#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

print "1..10\n";

@oops = @ops = <op/*>;

if ($^O eq 'MSWin32') {
  map { $files{lc($_)}++ } <op/*>;
  map { delete $files{"op/$_"} } split /[\s\n]/, `dir /b /l op & dir /b /l /ah op 2>nul`,
}
elsif ($^O eq 'VMS') {
  map { $files{lc($_)}++ } <[.op]*>;
  map { s/;.*$//; delete $files{lc($_)}; } split /[\n]/, `directory/noheading/notrailing/versions=1 [.op]`,
}
elsif ($^O eq 'MacOS') {
  @oops = @ops = <:op:*>;
  map { $files{$_}++ } <:op:*>;
  map { delete $files{$_} } split /[\s\n]/, `echo :op:\xc5`;
}
else {
  map { $files{$_}++ } <op/*>;
  map { delete $files{$_} } split /[\s\n]/, `echo op/*`;
}
if (keys %files) {
	print "not ok 1\t(",join(' ', sort keys %files),"\n";
} else { print "ok 1\n"; }

print $/ eq "\n" ? "ok 2\n" : "not ok 2\n";

if ($^O eq 'MacOS') {
    while (<jskdfjskdfj* :op:* jskdjfjkosvk*>) {
	$not = "not " unless $_ eq shift @ops;
	$not = "not at all " if $/ eq "\0";
    }
} else {
    while (<jskdfjskdfj* op/* jskdjfjkosvk*>) {
	$not = "not " unless $_ eq shift @ops;
	$not = "not at all " if $/ eq "\0";
    }
}
print "${not}ok 3\n";

print $/ eq "\n" ? "ok 4\n" : "not ok 4\n";

# test the "glob" operator
$_ = $^O eq 'MacOS' ? ":op:*" : "op/*";
@glops = glob $_;
print "@glops" eq "@oops" ? "ok 5\n" : "not ok 5\n";

@glops = glob;
print "@glops" eq "@oops" ? "ok 6\n" : "not ok 6\n";

# glob should still work even after the File::Glob stash has gone away
# (this used to dump core)
my $i = 0;
for (1..2) {
    eval "<.>";
    undef %File::Glob::;
    ++$i;
}
print $i == 2 ? "ok 7\n" : "not ok 7\n";

# ... while ($var = glob(...)) should test definedness not truth

if( $INC{'File/Glob.pm'} ) {
    my $ok = "not ok 8\n";
    $ok = "ok 8\n" while my $var = glob("0");
    print $ok;
}
else {
    print "ok 8 # skip: File::Glob emulated Unixism\n";
}


# The formerly-broken test for the situation above would accidentally
# test definedness for an assignment with a LOGOP on the right:
my $f=0;
$ok="ok 9\n";
$ok="not ok 9\n", undef $f while $x = $f||$f;
print $ok;

# Better check that glob actually returned some entries
{
   my $not = (scalar @oops > 0) ? '' : 'not ';
   print "${not}ok 10\n";
}
