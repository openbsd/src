#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

plan (108);

sub expected {
    my($object, $package, $type) = @_;
    print "# $object $package $type\n";
    is(ref($object), $package);
    my $r = qr/^\Q$package\E=(\w+)\(0x([0-9a-f]+)\)$/;
    like("$object", $r);
    if ("$object" =~ $r) {
	is($1, $type);
	# in 64-bit platforms hex warns for 32+ -bit values
	cmp_ok(do {no warnings 'portable'; hex($2)}, '==', $object);
    }
    else {
	fail(); fail();
    }
}

# test blessing simple types

$a1 = bless {}, "A";
expected($a1, "A", "HASH");
$b1 = bless [], "B";
expected($b1, "B", "ARRAY");
$c1 = bless \(map "$_", "test"), "C";
expected($c1, "C", "SCALAR");
our $test = "foo"; $d1 = bless \*test, "D";
expected($d1, "D", "GLOB");
$e1 = bless sub { 1 }, "E";
expected($e1, "E", "CODE");
$f1 = bless \[], "F";
expected($f1, "F", "REF");
$g1 = bless \substr("test", 1, 2), "G";
expected($g1, "G", "LVALUE");

# blessing ref to object doesn't modify object

expected(bless(\$a1, "F"), "F", "REF");
expected($a1, "A", "HASH");

# reblessing does modify object

bless $a1, "A2";
expected($a1, "A2", "HASH");

# local and my
{
    local $a1 = bless $a1, "A3";	# should rebless outer $a1
    local $b1 = bless [], "B3";
    my $c1 = bless $c1, "C3";		# should rebless outer $c1
    our $test2 = ""; my $d1 = bless \*test2, "D3";
    expected($a1, "A3", "HASH");
    expected($b1, "B3", "ARRAY");
    expected($c1, "C3", "SCALAR");
    expected($d1, "D3", "GLOB");
}
expected($a1, "A3", "HASH");
expected($b1, "B", "ARRAY");
expected($c1, "C3", "SCALAR");
expected($d1, "D", "GLOB");

# class is magic
"E" =~ /(.)/;
expected(bless({}, $1), "E", "HASH");
{
    local $! = 1;
    my $string = "$!";
    $! = 2;	# attempt to avoid cached string
    $! = 1;
    expected(bless({}, $!), $string, "HASH");

# ref is ref to magic
    {
	{
	    package F;
	    sub test { main::is(${$_[0]}, $string) }
	}
	$! = 2;
	$f1 = bless \$!, "F";
	$! = 1;
	$f1->test;
    }
}

# ref is magic
### example of magic variable that is a reference??

# no class, or empty string (with a warning), or undef (with two)
expected(bless([]), 'main', "ARRAY");
{
    local $SIG{__WARN__} = sub { push @w, join '', @_ };
    use warnings;

    $m = bless [];
    expected($m, 'main', "ARRAY");
    is (scalar @w, 0);

    @w = ();
    $m = bless [], '';
    expected($m, 'main', "ARRAY");
    is (scalar @w, 1);

    @w = ();
    $m = bless [], undef;
    expected($m, 'main', "ARRAY");
    is (scalar @w, 2);
}

# class is a ref
$a1 = bless {}, "A4";
$b1 = eval { bless {}, $a1 };
isnt ($@, '', "class is a ref");

# class is an overloaded ref
{
    package H4;
    use overload '""' => sub { "C4" };
}
$h1 = bless {}, "H4";
$c4 = eval { bless \$test, $h1 };
is ($@, '', "class is an overloaded ref");
expected($c4, 'C4', "SCALAR");

{
    my %h = 1..2;
    my($k) = keys %h; 
    my $x=\$k;
    bless $x, 'pam';
    is(ref $x, 'pam');

    my $a = bless \(keys %h), 'zap';
    is(ref $a, 'zap');
}
