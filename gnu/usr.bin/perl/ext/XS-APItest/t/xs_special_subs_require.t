#!perl -w
BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
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
use Test::More tests => $uc ? 103 : 83;

# Doing this longhand cut&paste makes it clear
# BEGIN and INIT are FIFO, CHECK and END are LIFO
BEGIN {
    print "# First BEGIN\n";
    is($XS::APItest::BEGIN_called, undef, "BEGIN not yet called");
    is($XS::APItest::BEGIN_called_PP, undef, "BEGIN not yet called");
    is($XS::APItest::UNITCHECK_called, undef, "UNITCHECK not yet called")
       if $uc;
    is($XS::APItest::UNITCHECK_called_PP, undef, "UNITCHECK not called")
       if $uc;
    is($XS::APItest::CHECK_called, undef, "CHECK not called");
    is($XS::APItest::CHECK_called_PP, undef, "CHECK not called");
    is($XS::APItest::INIT_called, undef, "INIT not called");
    is($XS::APItest::INIT_called_PP, undef, "INIT not called");
    is($XS::APItest::END_called, undef, "END not yet called");
    is($XS::APItest::END_called_PP, undef, "END not yet called");
}

CHECK {
    print "# First CHECK\n";
    is($XS::APItest::BEGIN_called, undef, "BEGIN not yet called");
    is($XS::APItest::BEGIN_called_PP, undef, "BEGIN not yet called");
    is($XS::APItest::UNITCHECK_called, undef, "UNITCHECK not yet called")
       if $uc;
    is($XS::APItest::UNITCHECK_called_PP, undef, "UNITCHECK not called")
       if $uc;
    is($XS::APItest::CHECK_called, undef, "CHECK not called (too late)");
    is($XS::APItest::CHECK_called_PP, undef, "CHECK not called (too late)");
    is($XS::APItest::INIT_called, undef, "INIT not called");
    is($XS::APItest::INIT_called_PP, undef, "INIT not called");
    is($XS::APItest::END_called, undef, "END not yet called");
    is($XS::APItest::END_called_PP, undef, "END not yet called");
}

INIT {
    print "# First INIT\n";
    is($XS::APItest::BEGIN_called, undef, "BEGIN not yet called");
    is($XS::APItest::BEGIN_called_PP, undef, "BEGIN not yet called");
    is($XS::APItest::UNITCHECK_called, undef, "UNITCHECK not yet called")
       if $uc;
    is($XS::APItest::UNITCHECK_called_PP, undef, "UNITCHECK not called")
       if $uc;
    is($XS::APItest::CHECK_called, undef, "CHECK not called (too late)");
    is($XS::APItest::CHECK_called_PP, undef, "CHECK not called (too late)");
    is($XS::APItest::INIT_called, undef, "INIT not called");
    is($XS::APItest::INIT_called_PP, undef, "INIT not called");
    is($XS::APItest::END_called, undef, "END not yet called");
    is($XS::APItest::END_called_PP, undef, "END not yet called");
}

END {
    print "# First END\n";
    is($XS::APItest::BEGIN_called, 1, "BEGIN called");
    is($XS::APItest::BEGIN_called_PP, 1, "BEGIN called");
    is($XS::APItest::UNITCHECK_called, 1, "UNITCHECK called") if $uc;
    is($XS::APItest::UNITCHECK_called_PP, 1, "UNITCHECK called") if $uc;
    is($XS::APItest::CHECK_called, undef, "CHECK not called (too late)");
    is($XS::APItest::CHECK_called_PP, undef, "CHECK not called (too late)");
    is($XS::APItest::INIT_called, undef, "INIT not called (too late)");
    is($XS::APItest::INIT_called_PP, undef, "INIT not called (too late)");
    is($XS::APItest::END_called, 1, "END called");
    is($XS::APItest::END_called_PP, 1, "END called");
}

print "# First body\n";
is($XS::APItest::BEGIN_called, undef, "BEGIN not yet called");
is($XS::APItest::BEGIN_called_PP, undef, "BEGIN not yet called");
is($XS::APItest::UNITCHECK_called, undef, "UNITCHECK not yet called") if $uc;
is($XS::APItest::UNITCHECK_called_PP, undef, "UNITCHECK not called") if $uc;
is($XS::APItest::CHECK_called, undef, "CHECK not called (too late)");
is($XS::APItest::CHECK_called_PP, undef, "CHECK not called (too late)");
is($XS::APItest::INIT_called, undef, "INIT not called (too late)");
is($XS::APItest::INIT_called_PP, undef, "INIT not called (too late)");
is($XS::APItest::END_called, undef, "END not yet called");
is($XS::APItest::END_called_PP, undef, "END not yet called");

{
    my @trap;
    local $SIG{__WARN__} = sub { push @trap, join "!", @_ };
    require XS::APItest;

    @trap = sort @trap;
    is(scalar @trap, 2, "There were 2 warnings");
    is($trap[0], "Too late to run CHECK block.\n");
    is($trap[1], "Too late to run INIT block.\n");
}

print "# Second body\n";
is($XS::APItest::BEGIN_called, 1, "BEGIN called");
is($XS::APItest::BEGIN_called_PP, 1, "BEGIN called");
is($XS::APItest::UNITCHECK_called, 1, "UNITCHECK called") if $uc;
is($XS::APItest::UNITCHECK_called_PP, 1, "UNITCHECK called") if $uc;
is($XS::APItest::CHECK_called, undef, "CHECK not called (too late)");
is($XS::APItest::CHECK_called_PP, undef, "CHECK not called (too late)");
is($XS::APItest::INIT_called, undef, "INIT not called (too late)");
is($XS::APItest::INIT_called_PP, undef, "INIT not called (too late)");
is($XS::APItest::END_called, undef, "END not yet called");
is($XS::APItest::END_called_PP, undef, "END not yet called");

BEGIN {
    print "# Second BEGIN\n";
    is($XS::APItest::BEGIN_called, undef, "BEGIN not yet called");
    is($XS::APItest::BEGIN_called_PP, undef, "BEGIN not yet called");
    is($XS::APItest::UNITCHECK_called, undef, "UNITCHECK not yet called")
	if $uc;
    is($XS::APItest::UNITCHECK_called_PP, undef, "UNITCHECK not called")
	if $uc;
    is($XS::APItest::CHECK_called, undef, "CHECK not called");
    is($XS::APItest::CHECK_called_PP, undef, "CHECK not called");
    is($XS::APItest::INIT_called, undef, "INIT not called");
    is($XS::APItest::INIT_called_PP, undef, "INIT not called");
    is($XS::APItest::END_called, undef, "END not yet called");
    is($XS::APItest::END_called_PP, undef, "END not yet called");
}

CHECK {
    print "# Second CHECK\n";
    is($XS::APItest::BEGIN_called, undef, "BEGIN not yet called");
    is($XS::APItest::BEGIN_called_PP, undef, "BEGIN not yet called");
    is($XS::APItest::UNITCHECK_called, undef, "UNITCHECK not yet called")
	if $uc;
    is($XS::APItest::UNITCHECK_called_PP, undef, "UNITCHECK not yet called")
	if $uc;
    is($XS::APItest::CHECK_called, undef, "CHECK not called");
    is($XS::APItest::CHECK_called_PP, undef, "CHECK not called");
    is($XS::APItest::INIT_called, undef, "INIT not called");
    is($XS::APItest::INIT_called_PP, undef, "INIT not called");
    is($XS::APItest::END_called, undef, "END not yet called");
    is($XS::APItest::END_called_PP, undef, "END not yet called");
}

INIT {
    print "# Second INIT\n";
    is($XS::APItest::BEGIN_called, undef, "BEGIN not yet called");
    is($XS::APItest::BEGIN_called_PP, undef, "BEGIN not yet called");
    is($XS::APItest::UNITCHECK_called, undef, "UNITCHECK not yet called")
	if $uc;
    is($XS::APItest::UNITCHECK_called_PP, undef, "UNITCHECK not yet called")
	if $uc;
    is($XS::APItest::CHECK_called, undef, "CHECK not called (too late)");
    is($XS::APItest::CHECK_called_PP, undef, "CHECK not called (too late)");
    is($XS::APItest::INIT_called, undef, "INIT not called (too late)");
    is($XS::APItest::INIT_called_PP, undef, "INIT not called (too late)");
    is($XS::APItest::END_called, undef, "END not yet called");
    is($XS::APItest::END_called_PP, undef, "END not yet called");
}

END {
    print "# Second END\n";
    is($XS::APItest::BEGIN_called, 1, "BEGIN called");
    is($XS::APItest::BEGIN_called_PP, 1, "BEGIN called");
    is($XS::APItest::UNITCHECK_called, 1, "UNITCHECK called") if $uc;
    is($XS::APItest::UNITCHECK_called_PP, 1, "UNITCHECK called") if $uc;
    is($XS::APItest::CHECK_called, undef, "CHECK not called (too late)");
    is($XS::APItest::CHECK_called_PP, undef, "CHECK not called (too late)");
    is($XS::APItest::INIT_called, undef, "INIT not called (too late)");
    is($XS::APItest::INIT_called_PP, undef, "INIT not called (too late)");
    is($XS::APItest::END_called, 1, "END called");
    is($XS::APItest::END_called_PP, 1, "END called");
}
