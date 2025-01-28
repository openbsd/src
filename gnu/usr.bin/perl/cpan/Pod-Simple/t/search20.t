use strict;
use warnings;
use Pod::Simple::Search;
use Test::More tests => 9;

print "# ", __FILE__,
 ": Testing the scanning of several (well, two) docroots...\n";

my $x = Pod::Simple::Search->new;
die "Couldn't make an object!?" unless ok defined $x;

$x->inc(0);

$x->callback(sub {
  print "#  ", join("  ", map "{$_}", @_), "\n";
  return;
});

use File::Spec;
use Cwd ();
use File::Basename ();

my $t_dir = File::Basename::dirname(Cwd::abs_path(__FILE__));

my $here1 = File::Spec->catdir($t_dir, 'testlib1');
my $here2 = File::Spec->catdir($t_dir, 'testlib2');

print "# OK, found the test corpora\n#  as $here1\n# and $here2\n";

print $x->_state_as_string;
#$x->verbose(12);

use Pod::Simple;
*pretty = \&Pod::Simple::BlackBox::pretty;

print "# OK, starting run...\n# [[\n";
my($name2where, $where2name) = $x->survey($here1, $here2);
print "# ]]\n#OK, run done.\n";

my $p = pretty( $where2name, $name2where )."\n";
$p =~ s/, +/,\n/g;
$p =~ s/^/#  /mg;
print $p;

require File::Spec->catfile($t_dir, 'ascii_order.pl');

SKIP: {
    skip '-- case may or may not be preserved', 2
        if $^O eq 'VMS';

    {
        my $names = join "|", sort ascii_order values %$where2name;
        is $names,
            "Blorm|Suzzle|Zonk::Pronk|hinkhonk::Glunk|hinkhonk::Vliff|perlflif|perlthng|perlzoned|perlzuk|squaa|squaa::Glunk|squaa::Vliff|squaa::Wowo|zikzik";
    }

    {
        my $names = join "|", sort ascii_order keys %$name2where;
        is $names,
            "Blorm|Suzzle|Zonk::Pronk|hinkhonk::Glunk|hinkhonk::Vliff|perlflif|perlthng|perlzoned|perlzuk|squaa|squaa::Glunk|squaa::Vliff|squaa::Wowo|zikzik";
    }
}

like( ($name2where->{'squaa'} || 'huh???'), qr/squaa\.pm$/);

is grep( m/squaa\.pm/, keys %$where2name ), 1;

###### Now with recurse(0)

$x->recurse(0);

print "# OK, starting run without recurse...\n# [[\n";
($name2where, $where2name) = $x->survey($here1, $here2);
print "# ]]\n#OK, run without recurse done.\n";

$p = pretty( $where2name, $name2where )."\n";
$p =~ s/, +/,\n/g;
$p =~ s/^/#  /mg;
print $p;

SKIP: {
    skip '-- case may or may not be preserved', 2
        if $^O eq 'VMS';

    {
        my $names = join "|", sort ascii_order values %$where2name;
        is $names,
            "Blorm|Suzzle|squaa|zikzik";
    }

    {
        my $names = join "|", sort ascii_order keys %$name2where;
        is $names,
            "Blorm|Suzzle|squaa|zikzik";
    }
}

like( ($name2where->{'squaa'} || 'huh???'), qr/squaa\.pm$/);

is grep( m/squaa\.pm/, keys %$where2name ), 1;
