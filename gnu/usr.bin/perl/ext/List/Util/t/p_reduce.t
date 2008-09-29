#!./perl

# force perl-only version to be tested
$List::Util::TESTING_PERL_ONLY = $List::Util::TESTING_PERL_ONLY = 1;

(my $f = __FILE__) =~ s/p_//;
$::PERL_ONLY = $::PERL_ONLY = 1; # Mustn't use it only once!
do $f; die $@ if $@;
