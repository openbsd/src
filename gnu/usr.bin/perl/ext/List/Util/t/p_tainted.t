#!./perl -T

# force perl-only version to be tested
$List::Util::TESTING_PERL_ONLY = $List::Util::TESTING_PERL_ONLY = 1;

(my $f = __FILE__) =~ s/p_//;
do "./$f";
