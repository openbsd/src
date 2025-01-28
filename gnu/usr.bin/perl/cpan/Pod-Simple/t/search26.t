use strict;
use warnings;
use Pod::Simple::Search;
use Test::More tests => 3;

#
#  "kleene" rhymes with "zany".  It's a fact!
#


print "# ", __FILE__,
 ": Testing limit_glob ...\n";

my $x = Pod::Simple::Search->new;
die "Couldn't make an object!?" unless ok defined $x;

$x->inc(0);
$x->shadows(1);

use File::Spec;
use Cwd ();
use File::Basename ();

my $t_dir = File::Basename::dirname(Cwd::abs_path(__FILE__));

my $here1 = File::Spec->catdir($t_dir, 'testlib1');
my $here2 = File::Spec->catdir($t_dir, 'testlib2');
my $here3 = File::Spec->catdir($t_dir, 'testlib3');

print "# OK, found the test corpora\n#  as $here1\n# and $here2\n# and $here3\n#\n";

print $x->_state_as_string;
#$x->verbose(12);

use Pod::Simple;
*pretty = \&Pod::Simple::BlackBox::pretty;

my $glob = '*k';
print "# Limiting to $glob\n";
$x->limit_glob($glob);

my($name2where, $where2name) = $x->survey($here1, $here2, $here3);

my $p = pretty( $where2name, $name2where )."\n";
$p =~ s/, +/,\n/g;
$p =~ s/^/#  /mg;
print $p;

require File::Spec->catfile($t_dir, 'ascii_order.pl');

{
my $names = join "|", sort ascii_order keys %$name2where;
is $names, "Zonk::Pronk|hinkhonk::Glunk|perlzuk|squaa::Glunk|zikzik";
}

{
my $names = join "|", sort ascii_order values %$where2name;
is $names, "Zonk::Pronk|hinkhonk::Glunk|hinkhonk::Glunk|perlzuk|squaa::Glunk|zikzik";
}
