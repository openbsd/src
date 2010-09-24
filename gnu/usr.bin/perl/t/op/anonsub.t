#!./perl

# Note : we're not using t/test.pl here, because we would need
# fresh_perl_is, and fresh_perl_is uses a closure -- a special
# case of what this program tests for.

chdir 't' if -d 't';
@INC = '../lib';
$Is_VMS = $^O eq 'VMS';
$Is_MSWin32 = $^O eq 'MSWin32';
$Is_NetWare = $^O eq 'NetWare';
$ENV{PERL5LIB} = "../lib" unless $Is_VMS;

$|=1;

undef $/;
@prgs = split "\n########\n", <DATA>;
print "1..", 6 + scalar @prgs, "\n";

$tmpfile = "asubtmp000";
1 while -f ++$tmpfile;
END { if ($tmpfile) { 1 while unlink $tmpfile; } }

for (@prgs){
    my $switch = "";
    if (s/^\s*(-\w+)//){
       $switch = $1;
    }
    my($prog,$expected) = split(/\nEXPECT\n/, $_);
    open TEST, ">$tmpfile";
    print TEST "$prog\n";
    close TEST or die "Could not close: $!";
    my $results = $Is_VMS ?
		`$^X "-I[-.lib]" $switch $tmpfile 2>&1` :
		  $Is_MSWin32 ?
		    `.\\perl -I../lib $switch $tmpfile 2>&1` :
			$Is_NetWare ?
			    `perl -I../lib $switch $tmpfile 2>&1` :
				`./perl $switch $tmpfile 2>&1`;
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

sub test_invalid_decl {
    my ($code,$todo) = @_;
    $todo //= '';
    eval $code;
    if ($@ =~ /^Illegal declaration of anonymous subroutine at/) {
	print "ok ", ++$i, " - '$code' is illegal$todo\n";
    } else {
	print "not ok ", ++$i, " - '$code' is illegal$todo\n# GOT: $@";
    }
}

test_invalid_decl('sub;');
test_invalid_decl('sub ($) ;');
test_invalid_decl('{ $x = sub }');
test_invalid_decl('sub ($) && 1');
test_invalid_decl('sub ($) : lvalue;',' # TODO');

eval "sub #foo\n{print 1}";
if ($@ eq '') {
    print "ok ", ++$i, "\n";
} else {
    print "not ok ", ++$i, "\n# GOT: $@";
}

__END__
sub X {
    my $n = "ok 1\n";
    sub { print $n };
}
my $x = X();
undef &X;
$x->();
EXPECT
ok 1
########
sub X {
    my $n = "ok 1\n";
    sub {
        my $dummy = $n;	# eval can't close on $n without internal reference
	eval 'print $n';
	die $@ if $@;
    };
}
my $x = X();
undef &X;
$x->();
EXPECT
ok 1
########
sub X {
    my $n = "ok 1\n";
    eval 'sub { print $n }';
}
my $x = X();
die $@ if $@;
undef &X;
$x->();
EXPECT
ok 1
########
sub X;
sub X {
    my $n = "ok 1\n";
    eval 'sub Y { my $p = shift; $p->() }';
    die $@ if $@;
    Y(sub { print $n });
}
X();
EXPECT
ok 1
########
print sub { return "ok 1\n" } -> ();
EXPECT
ok 1
