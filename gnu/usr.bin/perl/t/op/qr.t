#!./perl -w

use strict;

BEGIN {
    chdir 't' if -d 't';
    require './test.pl';
}

plan(tests => 34);

sub r {
    return qr/Good/;
}

my $a = r();
object_ok($a, 'Regexp');
my $b = r();
object_ok($b, 'Regexp');

my $b1 = $b;

isnt($a + 0, $b + 0, 'Not the same object');

bless $b, 'Pie';

object_ok($b, 'Pie');
object_ok($a, 'Regexp');
object_ok($b1, 'Pie');

my $c = r();
like("$c", qr/Good/);
my $d = r();
like("$d", qr/Good/);

my $d1 = $d;

isnt($c + 0, $d + 0, 'Not the same object');

$$d = 'Bad';

like("$c", qr/Good/);
is($$d, 'Bad');
is($$d1, 'Bad');

# Assignment to an implicitly blessed Regexp object retains the class
# (No different from direct value assignment to any other blessed SV

object_ok($d, 'Regexp');
like("$d", qr/\ARegexp=SCALAR\(0x[0-9a-f]+\)\z/);

# As does an explicitly blessed Regexp object.

my $e = bless qr/Faux Pie/, 'Stew';

object_ok($e, 'Stew');
$$e = 'Fake!';

is($$e, 'Fake!');
object_ok($e, 'Stew');
like("$e", qr/\Stew=SCALAR\(0x[0-9a-f]+\)\z/);

# [perl #96230] qr// should not have the reuse-last-pattern magic
"foo" =~ /foo/;
like "bar",qr//,'[perl #96230] =~ qr// does not reuse last successful pat';
"foo" =~ /foo/;
$_ = "bar";
$_ =~ s/${qr||}/baz/;
is $_, "bazbar", '[perl #96230] s/$qr// does not reuse last pat';

{
    my $x = 1.1; $x = ${qr//};
    pass 'no assertion failure when upgrading NV to regexp';
}

sub TIESCALAR{bless[]}
sub STORE { is ref\pop, "REGEXP", "stored regexp" }
tie my $t, "";
$t = ${qr||};
ok tied $t, 'tied var is still tied after regexp assignment';

bless \my $t2;
$t2 = ${qr||};
is ref \$t2, 'main', 'regexp assignment is not maledictory';

{
    my $w;
    local $SIG{__WARN__}=sub{$w=$_[0]};
    $_ = 1.1;
    $_ = ${qr//};
    is 0+$_, 0, 'double upgraded to regexp';
    like $w, qr/numeric/, 'produces non-numeric warning';
    undef $w;
    $_ = 1;
    $_ = ${qr//};
    is 0+$_, 0, 'int upgraded to regexp';
    like $w, qr/numeric/, 'likewise produces non-numeric warning';
}

sub {
    $_[0] = ${qr=crumpets=};
    is ref\$_[0], 'REGEXP', 'PVLVs';
    # Don’t use like() here, as we would no longer be testing a PVLV.
    ok " crumpets " =~ $_[0], 'using a regexpvlv as regexp';
    my $x = $_[0];
    is ref\$x, 'REGEXP', 'copying a regexpvlv';
    $_[0] = ${qr//};
    my $str = "".qr//;
    $_[0] .= " ";
    is $_[0], "$str ", 'stringifying regexpvlv in place';
}
 ->((\my%hash)->{key});

# utf8::upgrade on an SVt_REGEXP should be a NOOP.
# RT #131821

{
    my $r1 = qr/X/i;
    utf8::upgrade($$r1);
    like "xxx", $r1, "RT #131821 utf8::upgrade: case insensitive";
}

# after v5.27.2-30-gdf6b4bd, this was double-freeing the PVX buffer
# and would crash under valgrind or similar. The eval ensures that the
# regex any children are freed.

{
    my %h;
    eval q{
        sub {
           my $r = qr/abc/;
           $_[0] = $$r;
        }->($h{foo});
        1;
    };
}
pass("PVLV-as-REGEXP double-free of PVX");
