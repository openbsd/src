use strict;
use warnings;
use Pod::Simple::Search;
use Test::More tests => 3;

print "# ", __FILE__,
 ": Testing forced case sensitivity ...\n";

my $x = Pod::Simple::Search->new;
die "Couldn't make an object!?" unless ok defined $x;

$x->inc(0);
$x->is_case_insensitive(0);

use File::Spec;
use Cwd ();
use File::Basename ();

my $t_dir = File::Basename::dirname(Cwd::abs_path(__FILE__));

my $A = File::Spec->catdir($t_dir, 'search60', 'A');
my $B = File::Spec->catdir($t_dir, 'search60', 'B');

print "# OK, found the test corpora\n#  as $A\n# and $B\n#\n";

my($name2where, $where2name) = $x->survey($A, $B);
like ($name2where->{x}, qr{^\Q$A\E[\\/]x\.pod$});

like ($name2where->{X}, qr{^\Q$B\E[\\/]X\.pod$});
