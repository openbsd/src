use strict;
use warnings;
use Test::More tests => 13;

use Pod::Simple::Search;

print "# ", __FILE__,
 ": Testing the scanning of several docroots...\n";

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

my($name2where, $where2name) = $x->survey($here1, $here2, $here3);

my $p = pretty( $where2name, $name2where )."\n";
$p =~ s/, +/,\n/g;
$p =~ s/^/#  /mg;
print $p;

require File::Spec->catfile($t_dir, 'ascii_order.pl');

SKIP: {
    skip '-- case may or may not be preserved', 2
        if $^O eq 'VMS';

    {
        print "# won't show any shadows, since we're just looking at the name2where keys\n";
        my $names = join "|", sort ascii_order keys %$name2where;

        is $names,
            "Blorm|Suzzle|Zonk::Pronk|hinkhonk::Glunk|hinkhonk::Vliff|perlflif|perlthng|perlzoned|perlzuk|squaa|squaa::Glunk|squaa::Vliff|squaa::Wowo|zikzik";
    }

    {
        print "# but here we'll see shadowing:\n";
        my $names = join "|", sort ascii_order values %$where2name;
        is $names,
            "Blorm|Suzzle|Zonk::Pronk|hinkhonk::Glunk|hinkhonk::Glunk|hinkhonk::Vliff|hinkhonk::Vliff|perlflif|perlthng|perlthng|perlzoned|perlzuk|squaa|squaa::Glunk|squaa::Vliff|squaa::Vliff|squaa::Vliff|squaa::Wowo|zikzik";
    }
}

{
my %count;
for(values %$where2name) { ++$count{$_} };
#print pretty(\%count), "\n\n";
delete @count{ grep $count{$_} < 2, keys %count };
my $shadowed = join "|", sort ascii_order keys %count;
is $shadowed, "hinkhonk::Glunk|hinkhonk::Vliff|perlthng|squaa::Vliff";

sub thar { print "# Seen $_[0] :\n", map "#  {$_}\n", sort ascii_order grep $where2name->{$_} eq $_[0],keys %$where2name; return; }

is $count{'perlthng'}, 2;
thar 'perlthng';
is $count{'squaa::Vliff'}, 3;
thar 'squaa::Vliff';
}


like( ($name2where->{'squaa'} || 'huh???'), qr/squaa\.pm$/);

is grep( m/squaa\.pm/, keys %$where2name ), 1;

like( ($name2where->{'perlthng'}    || 'huh???'), qr/[^\^]testlib1/ );
like( ($name2where->{'squaa::Vliff'} || 'huh???'), qr/[^\^]testlib1/ );

SKIP: {
    skip '-- case may or may not be preserved', 1
        if $^O eq 'VMS';

    # Some sanity:
    like
        +($name2where->{'squaa::Wowo'}  || 'huh???'),
        qr/testlib2/;
}

my $in_pods = $x->find('perlzoned', $here2);
like $in_pods, qr{^\Q$here2\E};
like $in_pods, qr{perlzoned.pod$};
