#!./perl

print "1..31\n";

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

sub expected {
    my($object, $package, $type) = @_;
    return "" if (
	ref($object) eq $package
	&& "$object" =~ /^\Q$package\E=(\w+)\(0x([0-9a-f]+)\)$/
	&& $1 eq $type
	# in 64-bit platforms hex warns for 32+ -bit values
	&& do { no warnings 'portable'; hex($2) == $object }
    );
    print "# $object $package $type\n";
    return "not ";
}

# test blessing simple types

$a1 = bless {}, "A";
print expected($a1, "A", "HASH"), "ok 1\n";
$b1 = bless [], "B";
print expected($b1, "B", "ARRAY"), "ok 2\n";
$c1 = bless \(map "$_", "test"), "C";
print expected($c1, "C", "SCALAR"), "ok 3\n";
our $test = "foo"; $d1 = bless \*test, "D";
print expected($d1, "D", "GLOB"), "ok 4\n";
$e1 = bless sub { 1 }, "E";
print expected($e1, "E", "CODE"), "ok 5\n";
$f1 = bless \[], "F";
print expected($f1, "F", "REF"), "ok 6\n";
$g1 = bless \substr("test", 1, 2), "G";
print expected($g1, "G", "LVALUE"), "ok 7\n";

# blessing ref to object doesn't modify object

print expected(bless(\$a1, "F"), "F", "REF"), "ok 8\n";
print expected($a1, "A", "HASH"), "ok 9\n";

# reblessing does modify object

bless $a1, "A2";
print expected($a1, "A2", "HASH"), "ok 10\n";

# local and my
{
    local $a1 = bless $a1, "A3";	# should rebless outer $a1
    local $b1 = bless [], "B3";
    my $c1 = bless $c1, "C3";		# should rebless outer $c1
    our $test2 = ""; my $d1 = bless \*test2, "D3";
    print expected($a1, "A3", "HASH"), "ok 11\n";
    print expected($b1, "B3", "ARRAY"), "ok 12\n";
    print expected($c1, "C3", "SCALAR"), "ok 13\n";
    print expected($d1, "D3", "GLOB"), "ok 14\n";
}
print expected($a1, "A3", "HASH"), "ok 15\n";
print expected($b1, "B", "ARRAY"), "ok 16\n";
print expected($c1, "C3", "SCALAR"), "ok 17\n";
print expected($d1, "D", "GLOB"), "ok 18\n";

# class is magic
"E" =~ /(.)/;
print expected(bless({}, $1), "E", "HASH"), "ok 19\n";
{
    local $! = 1;
    my $string = "$!";
    $! = 2;	# attempt to avoid cached string
    $! = 1;
    print expected(bless({}, $!), $string, "HASH"), "ok 20\n";

# ref is ref to magic
    {
	{
	    package F;
	    sub test { ${$_[0]} eq $string or print "not " }
	}
	$! = 2;
	$f1 = bless \$!, "F";
	$! = 1;
	$f1->test;
	print "ok 21\n";
    }
}

# ref is magic
### example of magic variable that is a reference??

# no class, or empty string (with a warning), or undef (with two)
print expected(bless([]), 'main', "ARRAY"), "ok 22\n";
{
    local $SIG{__WARN__} = sub { push @w, join '', @_ };
    use warnings;

    $m = bless [];
    print expected($m, 'main', "ARRAY"), "ok 23\n";
    print @w ? "not ok 24\t# @w\n" : "ok 24\n";

    @w = ();
    $m = bless [], '';
    print expected($m, 'main', "ARRAY"), "ok 25\n";
    print @w != 1 ? "not ok 26\t# @w\n" : "ok 26\n";

    @w = ();
    $m = bless [], undef;
    print expected($m, 'main', "ARRAY"), "ok 27\n";
    print @w != 2 ? "not ok 28\t# @w\n" : "ok 28\n";
}

# class is a ref
$a1 = bless {}, "A4";
$b1 = eval { bless {}, $a1 };
print $@ ? "ok 29\n" : "not ok 29\t# $b1\n";

# class is an overloaded ref
{
    package H4;
    use overload '""' => sub { "C4" };
}
$h1 = bless {}, "H4";
$c4 = eval { bless \$test, $h1 };
print expected($c4, 'C4', "SCALAR"), "ok 30\n";
print $@ ? "not ok 31\t# $@" : "ok 31\n";
