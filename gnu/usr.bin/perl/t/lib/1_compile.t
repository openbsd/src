#!./perl

# Modules should have their own tests.  For historical reasons, some
# do not.  This does basic compile tests on modules that have no tests
# of their own.

BEGIN {
    chdir 't';
    @INC = '../lib';
    require './test.pl';
}

use warnings;
use File::Spec::Functions;

# Okay, this is the list.

my @Core_Modules = grep /\S/, <DATA>;
chomp @Core_Modules;

if (eval { require Socket }) {
  # Two Net:: modules need the Convert::EBCDIC if in EBDCIC.
  if (ord("A") != 193 || eval { require Convert::EBCDIC }) {
      push @Core_Modules, qw(Net::Cmd Net::POP3);
  }
}

@Core_Modules = sort @Core_Modules;

plan tests => 1+@Core_Modules;

cmp_ok(@Core_Modules, '>', 0, "All modules should have tests");
note("http://www.xray.mpe.mpg.de/mailing-lists/perl5-porters/2001-04/msg01223.html");
note("20010421230349.P2946\@blackrider.blackstar.co.uk");

foreach my $module (@Core_Modules) {
    if ($module eq 'ByteLoader' && $^O eq 'VMS') {
        TODO: {
            local $TODO = "$module needs porting on $^O";
            ok(compile_module($module), "compile $module");
        }
    }
    else {
        ok(compile_module($module), "compile $module");
    }
}

# We do this as a separate process else we'll blow the hell
# out of our namespace.
sub compile_module {
    my ($module) = $_[0];

    my $compmod = catfile(curdir(), 'lib', 'compmod.pl');
    my $lib     = '-I' . catdir(updir(), 'lib');

    my $out = scalar `$^X $lib $compmod $module`;
    return $out =~ /^ok/;
}

# These modules have no tests of their own.
# Keep up to date with
# http://perl-qa.hexten.net/wiki/index.php/Untested_Core_Modules
# and vice-versa.  The list should only shrink.
__DATA__
