BEGIN {
    if($ENV{PERL_CORE}) {
        chdir 't';
        @INC = '../lib';
    }
}

use strict;
use Pod::Simple::Search;
use Test;
BEGIN { plan tests => 7 }

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
use Cwd;
my $cwd = cwd();
print "# CWD: $cwd\n";

sub source_path {
    my $file = shift;
    if ($ENV{PERL_CORE}) {
        my $updir = File::Spec->updir;
        my $dir = File::Spec->catdir($updir, 'lib', 'Pod', 'Simple', 't');
        return File::Spec->catdir ($dir, $file);
    } else {
        return $file;
    }
}

my($here1, $here2);
if(        -e ($here1 = source_path('testlib1'))) {
  die "But where's $here2?"
    unless -e ($here2 = source_path('testlib2'));
} elsif(   -e ($here1 = File::Spec->catdir($cwd, 't', 'testlib1'      ))) {
  die "But where's $here2?"
    unless -e ($here2 = File::Spec->catdir($cwd, 't', 'testlib2'));
} else {
  die "Can't find the test corpora";
}
print "# OK, found the test corpora\n#  as $here1\n# and $here2\n";
ok 1;

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

{
my $names = join "|", sort values %$where2name;
skip $^O eq 'VMS' ? '-- case may or may not be preserved' : 0, 
     $names, 
     "Blorm|Suzzle|Zonk::Pronk|hinkhonk::Glunk|hinkhonk::Vliff|perlflif|perlthng|perlzuk|squaa|squaa::Glunk|squaa::Vliff|squaa::Wowo|zikzik";
}

{
my $names = join "|", sort keys %$name2where;
skip $^O eq 'VMS' ? '-- case may or may not be preserved' : 0, 
     $names, 
     "Blorm|Suzzle|Zonk::Pronk|hinkhonk::Glunk|hinkhonk::Vliff|perlflif|perlthng|perlzuk|squaa|squaa::Glunk|squaa::Vliff|squaa::Wowo|zikzik";
}

ok( ($name2where->{'squaa'} || 'huh???'), '/squaa\.pm$/');

ok grep( m/squaa\.pm/, keys %$where2name ), 1;

ok 1;

__END__

