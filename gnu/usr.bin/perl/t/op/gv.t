#!./perl

#
# various typeglob tests
#

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

use warnings;

require './test.pl';
plan( tests => 61 );

# type coersion on assignment
$foo = 'foo';
$bar = *main::foo;
$bar = $foo;
is(ref(\$bar), 'SCALAR');
$foo = *main::bar;

# type coersion (not) on misc ops

ok($foo);
is(ref(\$foo), 'GLOB');

unlike ($foo, qr/abcd/);
is(ref(\$foo), 'GLOB');

is($foo, '*main::bar');
is(ref(\$foo), 'GLOB');

# type coersion on substitutions that match
$a = *main::foo;
$b = $a;
$a =~ s/^X//;
is(ref(\$a), 'GLOB');
$a =~ s/^\*//;
is($a, 'main::foo');
is(ref(\$b), 'GLOB');

# typeglobs as lvalues
substr($foo, 0, 1) = "XXX";
is(ref(\$foo), 'SCALAR');
is($foo, 'XXXmain::bar');

# returning glob values
sub foo {
  local($bar) = *main::foo;
  $foo = *main::bar;
  return ($foo, $bar);
}

($fuu, $baa) = foo();
ok(defined $fuu);
is(ref(\$fuu), 'GLOB');


ok(defined $baa);
is(ref(\$baa), 'GLOB');

# nested package globs
# NOTE:  It's probably OK if these semantics change, because the
#        fact that %X::Y:: is stored in %X:: isn't documented.
#        (I hope.)

{ package Foo::Bar; no warnings 'once'; $test=1; }
ok(exists $Foo::{'Bar::'});
is($Foo::{'Bar::'}, '*Foo::Bar::');


# test undef operator clearing out entire glob
$foo = 'stuff';
@foo = qw(more stuff);
%foo = qw(even more random stuff);
undef *foo;
is ($foo, undef);
is (scalar @foo, 0);
is (scalar %foo, 0);

{
    # test warnings from assignment of undef to glob
    my $msg = '';
    local $SIG{__WARN__} = sub { $msg = $_[0] };
    use warnings;
    *foo = 'bar';
    is($msg, '');
    *foo = undef;
    like($msg, qr/Undefined value assigned to typeglob/);
}

my $test = curr_test();
# test *glob{THING} syntax
$x = "ok $test\n";
++$test;
@x = ("ok $test\n");
++$test;
%x = ("ok $test" => "\n");
++$test;
sub x { "ok $test\n" }
print ${*x{SCALAR}}, @{*x{ARRAY}}, %{*x{HASH}}, &{*x{CODE}};
# This needs to go here, after the print, as sub x will return the current
# value of test
++$test;
format x =
XXX This text isn't used. Should it be?
.
curr_test($test);

is (ref *x{FORMAT}, "FORMAT");
*x = *STDOUT;
is (*{*x{GLOB}}, "*main::STDOUT");

{
    my $test = curr_test();

    print {*x{IO}} "ok $test\n";
    ++$test;

    my $warn;
    local $SIG{__WARN__} = sub {
	$warn .= $_[0];
    };
    my $val = *x{FILEHANDLE};
    print {*x{IO}} ($warn =~ /is deprecated/
		    ? "ok $test\n" : "not ok $test\n");
    curr_test(++$test);
}


{
    # test if defined() doesn't create any new symbols

    my $a = "SYM000";
    ok(!defined *{$a});

    ok(!defined @{$a});
    ok(!defined *{$a});

    ok(!defined %{$a});
    ok(!defined *{$a});

    ok(!defined ${$a});
    ok(!defined *{$a});

    ok(!defined &{$a});
    ok(!defined *{$a});

    my $state = "not";
    *{$a} = sub { $state = "ok" };
    ok(defined &{$a});
    ok(defined *{$a});
    &{$a};
    is ($state, 'ok');
}

{
    # although it *should* if you're talking about magicals

    my $a = "]";
    ok(defined ${$a});
    ok(defined *{$a});

    $a = "1";
    "o" =~ /(o)/;
    ok(${$a});
    ok(defined *{$a});
    $a = "2";
    ok(!${$a});
    ok(defined *{$a});
    $a = "1x";
    ok(!defined ${$a});
    ok(!defined *{$a});
    $a = "11";
    "o" =~ /(((((((((((o)))))))))))/;
    ok(${$a});
    ok(defined *{$a});
}

# [ID 20010526.001] localized glob loses value when assigned to

$j=1; %j=(a=>1); @j=(1); local *j=*j; *j = sub{};

is($j, 1);
is($j{a}, 1);
is($j[0], 1);

{
    # does pp_readline() handle glob-ness correctly?
    my $g = *foo;
    $g = <DATA>;
    is ($g, "Perl\n");
}

{
    my $w = '';
    $SIG{__WARN__} = sub { $w = $_[0] };
    sub abc1 ();
    local *abc1 = sub { };
    is ($w, '');
    sub abc2 ();
    local *abc2;
    *abc2 = sub { };
    is ($w, '');
    sub abc3 ();
    *abc3 = sub { };
    like ($w, qr/Prototype mismatch/);
}

{
    # [17375] rcatline to formerly-defined undef was broken. Fixed in
    # do_readline by checking SvOK. AMS, 20020918
    my $x = "not ";
    $x  = undef;
    $x .= <DATA>;
    is ($x, "Rules\n");
}

__END__
Perl
Rules
