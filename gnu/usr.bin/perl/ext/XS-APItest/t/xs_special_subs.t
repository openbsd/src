#!perl -w

BEGIN {
    push @INC, "::lib:$MacPerl::Architecture:" if $^O eq 'MacOS';
    require Config; import Config;
    if ($Config{'extensions'} !~ /\bXS\/APItest\b/) {
        print "1..0 # Skip: XS::APItest was not built\n";
        exit 0;
    }
    # Hush the used only once warning.
    $XS::APItest::WARNINGS_ON_BOOTSTRAP = $MacPerl::Architecture;
    $XS::APItest::WARNINGS_ON_BOOTSTRAP = 1;
}

use strict;
use warnings;
my $uc;
BEGIN {
    $uc = $] > 5.009;
}
use Test::More tests => $uc ? 100 : 80;

# Doing this longhand cut&paste makes it clear
# BEGIN and INIT are FIFO, CHECK and END are LIFO
BEGIN {
    print "# First BEGIN\n";
    is($XS::APItest::BEGIN_called, undef, "BEGIN not yet called");
    is($XS::APItest::BEGIN_called_PP, undef, "BEGIN not yet called");
    is($XS::APItest::UNITCHECK_called, undef, "UNITCHECK not yet called")
       if $uc;
    is($XS::APItest::UNITCHECK_called_PP, undef, "UNITCHECK not yet called")
       if $uc;
    is($XS::APItest::CHECK_called, undef, "CHECK not yet called");
    is($XS::APItest::CHECK_called_PP, undef, "CHECK not yet called");
    is($XS::APItest::INIT_called, undef, "INIT not yet called");
    is($XS::APItest::INIT_called_PP, undef, "INIT not yet called");
    is($XS::APItest::END_called, undef, "END not yet called");
    is($XS::APItest::END_called_PP, undef, "END not yet called");
}

CHECK {
    print "# First CHECK\n";
    is($XS::APItest::BEGIN_called, 1, "BEGIN called");
    is($XS::APItest::BEGIN_called_PP, 1, "BEGIN called");
    is($XS::APItest::UNITCHECK_called, 1, "UNITCHECK called") if $uc;
    is($XS::APItest::UNITCHECK_called_PP, 1, "UNITCHECK called") if $uc;
    is($XS::APItest::CHECK_called, 1, "CHECK called");
    is($XS::APItest::CHECK_called_PP, 1, "CHECK called");
    is($XS::APItest::INIT_called, undef, "INIT not yet called");
    is($XS::APItest::INIT_called_PP, undef, "INIT not yet called");
    is($XS::APItest::END_called, undef, "END not yet called");
    is($XS::APItest::END_called_PP, undef, "END not yet called");
}

INIT {
    print "# First INIT\n";
    is($XS::APItest::BEGIN_called, 1, "BEGIN called");
    is($XS::APItest::BEGIN_called_PP, 1, "BEGIN called");
    is($XS::APItest::UNITCHECK_called, 1, "UNITCHECK called") if $uc;
    is($XS::APItest::UNITCHECK_called_PP, 1, "UNITCHECK called") if $uc;
    is($XS::APItest::CHECK_called, 1, "CHECK called");
    is($XS::APItest::CHECK_called_PP, 1, "CHECK called");
    is($XS::APItest::INIT_called, undef, "INIT not yet called");
    is($XS::APItest::INIT_called_PP, undef, "INIT not yet called");
    is($XS::APItest::END_called, undef, "END not yet called");
    is($XS::APItest::END_called_PP, undef, "END not yet called");
}

END {
    print "# First END\n";
    is($XS::APItest::BEGIN_called, 1, "BEGIN called");
    is($XS::APItest::BEGIN_called_PP, 1, "BEGIN called");
    is($XS::APItest::UNITCHECK_called, 1, "UNITCHECK called") if $uc;
    is($XS::APItest::UNITCHECK_called_PP, 1, "UNITCHECK called") if $uc;
    is($XS::APItest::CHECK_called, 1, "CHECK called");
    is($XS::APItest::CHECK_called_PP, 1, "CHECK called");
    is($XS::APItest::INIT_called, 1, "INIT called");
    is($XS::APItest::INIT_called_PP, 1, "INIT called");
    is($XS::APItest::END_called, 1, "END called");
    is($XS::APItest::END_called_PP, 1, "END called");
}

print "# First body\n";
is($XS::APItest::BEGIN_called, 1, "BEGIN called");
is($XS::APItest::BEGIN_called_PP, 1, "BEGIN called");
is($XS::APItest::UNITCHECK_called, 1, "UNITCHECK called") if $uc;
is($XS::APItest::UNITCHECK_called_PP, 1, "UNITCHECK called") if $uc;
is($XS::APItest::CHECK_called, 1, "CHECK called");
is($XS::APItest::CHECK_called_PP, 1, "CHECK called");
is($XS::APItest::INIT_called, 1, "INIT called");
is($XS::APItest::INIT_called_PP, 1, "INIT called");
is($XS::APItest::END_called, undef, "END not yet called");
is($XS::APItest::END_called_PP, undef, "END not yet called");

use XS::APItest;

print "# Second body\n";
is($XS::APItest::BEGIN_called, 1, "BEGIN called");
is($XS::APItest::BEGIN_called_PP, 1, "BEGIN called");
is($XS::APItest::UNITCHECK_called, 1, "UNITCHECK called") if $uc;
is($XS::APItest::UNITCHECK_called_PP, 1, "UNITCHECK called") if $uc;
is($XS::APItest::CHECK_called, 1, "CHECK called");
is($XS::APItest::CHECK_called_PP, 1, "CHECK called");
is($XS::APItest::INIT_called, 1, "INIT called");
is($XS::APItest::INIT_called_PP, 1, "INIT called");
is($XS::APItest::END_called, undef, "END not yet called");
is($XS::APItest::END_called_PP, undef, "END not yet called");

BEGIN {
    print "# Second BEGIN\n";
    is($XS::APItest::BEGIN_called, 1, "BEGIN called");
    is($XS::APItest::BEGIN_called_PP, 1, "BEGIN called");
    is($XS::APItest::UNITCHECK_called, 1, "UNITCHECK called") if $uc;
    is($XS::APItest::UNITCHECK_called_PP, 1, "UNITCHECK called") if $uc;
    is($XS::APItest::CHECK_called, undef, "CHECK not yet called");
    is($XS::APItest::CHECK_called_PP, undef, "CHECK not yet called");
    is($XS::APItest::INIT_called, undef, "INIT not yet called");
    is($XS::APItest::INIT_called_PP, undef, "INIT not yet called");
    is($XS::APItest::END_called, undef, "END not yet called");
    is($XS::APItest::END_called_PP, undef, "END not yet called");
}

CHECK {
    print "# Second CHECK\n";
    is($XS::APItest::BEGIN_called, 1, "BEGIN called");
    is($XS::APItest::BEGIN_called_PP, 1, "BEGIN called");
    is($XS::APItest::UNITCHECK_called, 1, "UNITCHECK yet called") if $uc;
    is($XS::APItest::UNITCHECK_called_PP, 1, "UNITCHECK yet called") if $uc;
    is($XS::APItest::CHECK_called, undef, "CHECK not yet called");
    is($XS::APItest::CHECK_called_PP, undef, "CHECK not yet called");
    is($XS::APItest::INIT_called, undef, "INIT not yet called");
    is($XS::APItest::INIT_called_PP, undef, "INIT not yet called");
    is($XS::APItest::END_called, undef, "END not yet called");
    is($XS::APItest::END_called_PP, undef, "END not yet called");
}

INIT {
    print "# Second INIT\n";
    is($XS::APItest::BEGIN_called, 1, "BEGIN called");
    is($XS::APItest::BEGIN_called_PP, 1, "BEGIN called");
    is($XS::APItest::UNITCHECK_called, 1, "UNITCHECK called") if $uc;
    is($XS::APItest::UNITCHECK_called_PP, 1, "UNITCHECK called") if $uc;
    is($XS::APItest::CHECK_called, 1, "CHECK called");
    is($XS::APItest::CHECK_called_PP, 1, "CHECK called");
    is($XS::APItest::INIT_called, 1, "INIT called");
    is($XS::APItest::INIT_called_PP, 1, "INIT called");
    is($XS::APItest::END_called, undef, "END not yet called");
    is($XS::APItest::END_called_PP, undef, "END not yet called");
}

END {
    print "# Second END\n";
    is($XS::APItest::BEGIN_called, 1, "BEGIN called");
    is($XS::APItest::BEGIN_called_PP, 1, "BEGIN called");
    is($XS::APItest::UNITCHECK_called, 1, "UNITCHECK called") if $uc;
    is($XS::APItest::UNITCHECK_called_PP, 1, "UNITCHECK called") if $uc;
    is($XS::APItest::CHECK_called, 1, "CHECK called");
    is($XS::APItest::CHECK_called_PP, 1, "CHECK called");
    is($XS::APItest::INIT_called, 1, "INIT called");
    is($XS::APItest::INIT_called_PP, 1, "INIT called");
    is($XS::APItest::END_called, undef, "END not yet called");
    is($XS::APItest::END_called_PP, undef, "END not yet called");
}
