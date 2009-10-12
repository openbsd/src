#!./perl -T

use File::Spec;

# force perl-only version to be tested
$List::Util::TESTING_PERL_ONLY = $List::Util::TESTING_PERL_ONLY = 1;

(my $f = __FILE__) =~ s/p_//;
my $filename = $^O eq 'MSWin32'
             ? File::Spec->rel2abs(File::Spec->catfile(".", $f))
             : File::Spec->catfile(".", $f);
do $filename; die $@ if $@;
