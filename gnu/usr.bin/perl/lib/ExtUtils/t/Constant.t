#!/usr/bin/perl -w

print "1..51\n";

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't' if -d 't';
        @INC = '../lib';
    }
}

# use warnings;
use strict;
use ExtUtils::MakeMaker;
use ExtUtils::Constant qw (constant_types C_constant XS_constant autoload);
use Config;
use File::Spec::Functions qw(catfile rel2abs);
# Because were are going to be changing directory before running Makefile.PL
my $perl;
$perl = rel2abs( $^X ) unless $] < 5.006; # Hack. Until 5.00503 has rel2abs
# ExtUtils::Constant::C_constant uses $^X inside a comment, and we want to
# compare output to ensure that it is the same. We were probably run as ./perl
# whereas we will run the child with the full path in $perl. So make $^X for
# us the same as our child will see.
$^X = $perl;

print "# perl=$perl\n";
my $runperl = "$perl \"-I../../lib\"";

$| = 1;

my $dir = "ext-$$";
my @files;

print "# $dir being created...\n";
mkdir $dir, 0777 or die "mkdir: $!\n";

my $output = "output";

# For debugging set this to 1.
my $keep_files = 0;

END {
    use File::Path;
    print "# $dir being removed...\n";
    rmtree($dir) unless $keep_files;
}

my $package = "ExtTest";

# Test the code that generates 1 and 2 letter name comparisons.
my %compass = (
N => 0, 'NE' => 45, E => 90, SE => 135, S => 180, SW => 225, W => 270, NW => 315
);

my $parent_rfc1149 =
  'A Standard for the Transmission of IP Datagrams on Avian Carriers';
# Check that 8 bit and unicode names don't cause problems.
my $pound; 
if (ord('A') == 193) {  # EBCDIC platform
    $pound = chr 177; # A pound sign. (Currency)
} else { # ASCII platform
    $pound = chr 163; # A pound sign. (Currency)
}
my $inf = chr 0x221E;
# Check that we can distiguish the pathological case of a string, and the
# utf8 representation of that string.
my $pound_bytes = my $pound_utf8 = $pound . '1';
utf8::encode ($pound_bytes);

my @names = ("FIVE", {name=>"OK6", type=>"PV",},
             {name=>"OK7", type=>"PVN",
              value=>['"not ok 7\\n\\0ok 7\\n"', 15]},
             {name => "FARTHING", type=>"NV"},
             {name => "NOT_ZERO", type=>"UV", value=>"~(UV)0"},
             {name => "OPEN", type=>"PV", value=>'"/*"', macro=>1},
             {name => "CLOSE", type=>"PV", value=>'"*/"',
              macro=>["#if 1\n", "#endif\n"]},
             {name => "ANSWER", default=>["UV", 42]}, "NOTDEF",
             {name => "Yes", type=>"YES"},
             {name => "No", type=>"NO"},
             {name => "Undef", type=>"UNDEF"},
# OK. It wasn't really designed to allow the creation of dual valued constants.
# It was more for INADDR_ANY INADDR_BROADCAST INADDR_LOOPBACK INADDR_NONE
             {name=>"RFC1149", type=>"SV", value=>"sv_2mortal(temp_sv)",
              pre=>"SV *temp_sv = newSVpv(RFC1149, 0); "
              	   . "(void) SvUPGRADE(temp_sv,SVt_PVIV); SvIOK_on(temp_sv); "
                   . "SvIVX(temp_sv) = 1149;"},
             {name=>"perl", type=>"PV",},
);

push @names, $_ foreach keys %compass;

# Automatically compile the list of all the macro names, and make them
# exported constants.
my @names_only = map {(ref $_) ? $_->{name} : $_} @names;

# Exporter::Heavy (currently) isn't able to export these names:
push @names, ({name=>"*/", type=>"PV", value=>'"CLOSE"', macro=>1},
              {name=>"/*", type=>"PV", value=>'"OPEN"', macro=>1},
              {name=>$pound, type=>"PV", value=>'"Sterling"', macro=>1},
              {name=>$inf, type=>"PV", value=>'"Infinity"', macro=>1},
              {name=>$pound_utf8, type=>"PV", value=>'"1 Pound"', macro=>1},
              {name=>$pound_bytes, type=>"PV", value=>'"1 Pound (as bytes)"',
               macro=>1},
             );

=pod

The above set of names seems to produce a suitably bad set of compile
problems on a Unicode naive version of ExtUtils::Constant (ie 0.11):

nick@thinking-cap 15439-32-utf$ PERL_CORE=1 ./perl lib/ExtUtils/t/Constant.t
1..33
# perl=/stuff/perl5/15439-32-utf/perl
# ext-30370 being created...
Wide character in print at lib/ExtUtils/t/Constant.t line 140.
ok 1
ok 2
# make = 'make'
ExtTest.xs: In function `constant_1':
ExtTest.xs:80: warning: multi-character character constant
ExtTest.xs:80: warning: case value out of range
ok 3

=cut

my $types = {};
my $constant_types = constant_types(); # macro defs
my $C_constant = join "\n",
  C_constant ($package, undef, "IV", $types, undef, undef, @names);
my $XS_constant = XS_constant ($package, $types); # XS for ExtTest::constant

################ Header
my $header = catfile($dir, "test.h");
push @files, "test.h";
open FH, ">$header" or die "open >$header: $!\n";
print FH <<"EOT";
#define FIVE 5
#define OK6 "ok 6\\n"
#define OK7 1
#define FARTHING 0.25
#define NOT_ZERO 1
#define Yes 0
#define No 1
#define Undef 1
#define RFC1149 "$parent_rfc1149"
#undef NOTDEF
#define perl "rules"
EOT

while (my ($point, $bearing) = each %compass) {
  print FH "#define $point $bearing\n"
}
close FH or die "close $header: $!\n";

################ XS
my $xs = catfile($dir, "$package.xs");
push @files, "$package.xs";
open FH, ">$xs" or die "open >$xs: $!\n";

print FH <<'EOT';
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
EOT

print FH "#include \"test.h\"\n\n";
print FH $constant_types;
print FH $C_constant, "\n";
print FH "MODULE = $package		PACKAGE = $package\n";
print FH "PROTOTYPES: ENABLE\n";
print FH $XS_constant;
close FH or die "close $xs: $!\n";

################ PM
my $pm = catfile($dir, "$package.pm");
push @files, "$package.pm";
open FH, ">$pm" or die "open >$pm: $!\n";
print FH "package $package;\n";
print FH "use $];\n";

print FH <<'EOT';

use strict;
EOT
printf FH "use warnings;\n" unless $] < 5.006;
print FH <<'EOT';
use Carp;

require Exporter;
require DynaLoader;
use vars qw ($VERSION @ISA @EXPORT_OK $AUTOLOAD);

$VERSION = '0.01';
@ISA = qw(Exporter DynaLoader);
@EXPORT_OK = qw(
EOT

# Print the names of all our autoloaded constants
print FH "\t$_\n" foreach (@names_only);
print FH ");\n";
# Print the AUTOLOAD subroutine ExtUtils::Constant generated for us
print FH autoload ($package, $]);
print FH "bootstrap $package \$VERSION;\n1;\n__END__\n";
close FH or die "close $pm: $!\n";

################ test.pl
my $testpl = catfile($dir, "test.pl");
push @files, "test.pl";
open FH, ">$testpl" or die "open >$testpl: $!\n";

print FH "use strict;\n";
print FH "use $package qw(@names_only);\n";
print FH <<"EOT";

use utf8;

print "1..1\n";
if (open OUTPUT, ">$output") {
  print "ok 1\n";
  select OUTPUT;
} else {
  print "not ok 1 # Failed to open '$output': $!\n";
  exit 1;
}
EOT

print FH << 'EOT';

# What follows goes to the temporary file.
# IV
my $five = FIVE;
if ($five == 5) {
  print "ok 5\n";
} else {
  print "not ok 5 # $five\n";
}

# PV
print OK6;

# PVN containing embedded \0s
$_ = OK7;
s/.*\0//s;
print;

# NV
my $farthing = FARTHING;
if ($farthing == 0.25) {
  print "ok 8\n";
} else {
  print "not ok 8 # $farthing\n";
}

# UV
my $not_zero = NOT_ZERO;
if ($not_zero > 0 && $not_zero == ~0) {
  print "ok 9\n";
} else {
  print "not ok 9 # \$not_zero=$not_zero ~0=" . (~0) . "\n";
}

# Value includes a "*/" in an attempt to bust out of a C comment.
# Also tests custom cpp #if clauses
my $close = CLOSE;
if ($close eq '*/') {
  print "ok 10\n";
} else {
  print "not ok 10 # \$close='$close'\n";
}

# Default values if macro not defined.
my $answer = ANSWER;
if ($answer == 42) {
  print "ok 11\n";
} else {
  print "not ok 11 # What do you get if you multiply six by nine? '$answer'\n";
}

# not defined macro
my $notdef = eval { NOTDEF; };
if (defined $notdef) {
  print "not ok 12 # \$notdef='$notdef'\n";
} elsif ($@ !~ /Your vendor has not defined ExtTest macro NOTDEF/) {
  print "not ok 12 # \$@='$@'\n";
} else {
  print "ok 12\n";
}

# not a macro
my $notthere = eval { &ExtTest::NOTTHERE; };
if (defined $notthere) {
  print "not ok 13 # \$notthere='$notthere'\n";
} elsif ($@ !~ /NOTTHERE is not a valid ExtTest macro/) {
  chomp $@;
  print "not ok 13 # \$@='$@'\n";
} else {
  print "ok 13\n";
}

# Truth
my $yes = Yes;
if ($yes) {
  print "ok 14\n";
} else {
  print "not ok 14 # $yes='\$yes'\n";
}

# Falsehood
my $no = No;
if (defined $no and !$no) {
  print "ok 15\n";
} else {
  print "not ok 15 # \$no=" . defined ($no) ? "'$no'\n" : "undef\n";
}

# Undef
my $undef = Undef;
unless (defined $undef) {
  print "ok 16\n";
} else {
  print "not ok 16 # \$undef='$undef'\n";
}


# invalid macro (chosen to look like a mix up between No and SW)
$notdef = eval { &ExtTest::So };
if (defined $notdef) {
  print "not ok 17 # \$notdef='$notdef'\n";
} elsif ($@ !~ /^So is not a valid ExtTest macro/) {
  print "not ok 17 # \$@='$@'\n";
} else {
  print "ok 17\n";
}

# invalid defined macro
$notdef = eval { &ExtTest::EW };
if (defined $notdef) {
  print "not ok 18 # \$notdef='$notdef'\n";
} elsif ($@ !~ /^EW is not a valid ExtTest macro/) {
  print "not ok 18 # \$@='$@'\n";
} else {
  print "ok 18\n";
}

my %compass = (
EOT

while (my ($point, $bearing) = each %compass) {
  print FH "'$point' => $bearing, "
}

print FH <<'EOT';

);

my $fail;
while (my ($point, $bearing) = each %compass) {
  my $val = eval $point;
  if ($@) {
    print "# $point: \$@='$@'\n";
    $fail = 1;
  } elsif (!defined $bearing) {
    print "# $point: \$val=undef\n";
    $fail = 1;
  } elsif ($val != $bearing) {
    print "# $point: \$val=$val, not $bearing\n";
    $fail = 1;
  }
}
if ($fail) {
  print "not ok 19\n";
} else {
  print "ok 19\n";
}

EOT

print FH <<"EOT";
my \$rfc1149 = RFC1149;
if (\$rfc1149 ne "$parent_rfc1149") {
  print "not ok 20 # '\$rfc1149' ne '$parent_rfc1149'\n";
} else {
  print "ok 20\n";
}

if (\$rfc1149 != 1149) {
  printf "not ok 21 # %d != 1149\n", \$rfc1149;
} else {
  print "ok 21\n";
}

EOT

print FH <<'EOT';
# test macro=>1
my $open = OPEN;
if ($open eq '/*') {
  print "ok 22\n";
} else {
  print "not ok 22 # \$open='$open'\n";
}
EOT

# Do this in 7 bit in case someone is testing with some settings that cause
# 8 bit files incapable of storing this character.
my @values
 = map {"'" . join (",", unpack "U*", $_) . "'"}
 ($pound, $inf, $pound_bytes, $pound_utf8);
# Values is a list of strings, such as ('194,163,49', '163,49')

print FH <<'EOT';

# I can see that this child test program might be about to use parts of
# Test::Builder

my $test = 23;
my ($pound, $inf, $pound_bytes, $pound_utf8) = map {eval "pack 'U*', $_"}
EOT

print FH join ",", @values;

print FH << 'EOT';
;

foreach (["perl", "rules", "rules"],
	 ["/*", "OPEN", "OPEN"],
	 ["*/", "CLOSE", "CLOSE"],
	 [$pound, 'Sterling', []],
         [$inf, 'Infinity', []],
	 [$pound_utf8, '1 Pound', '1 Pound (as bytes)'],
	 [$pound_bytes, '1 Pound (as bytes)', []],
        ) {
  # Flag an expected error with a reference for the expect string.
  my ($string, $expect, $expect_bytes) = @$_;
  (my $name = $string) =~ s/([^ -~])/sprintf '\x{%X}', ord $1/ges;
  print "# \"$name\" => \'$expect\'\n";
  # Try to force this to be bytes if possible.
  utf8::downgrade ($string, 1);
EOT

print FH  "my (\$error, \$got) = ${package}::constant (\$string);\n";

print FH <<'EOT';
  if ($error or $got ne $expect) {
    print "not ok $test # error '$error', got '$got'\n";
  } else {
    print "ok $test\n";
  }
  $test++;
  print "# Now upgrade '$name' to utf8\n";
  utf8::upgrade ($string);
EOT

print FH  "my (\$error, \$got) = ${package}::constant (\$string);\n";

print FH <<'EOT';
  if ($error or $got ne $expect) {
    print "not ok $test # error '$error', got '$got'\n";
  } else {
    print "ok $test\n";
  }
  $test++;
  if (defined $expect_bytes) {
    print "# And now with the utf8 byte sequence for name\n";
    # Try the encoded bytes.
    utf8::encode ($string);
EOT

print FH "my (\$error, \$got) = ${package}::constant (\$string);\n";

print FH <<'EOT';
    if (ref $expect_bytes) {
      # Error expected.
      if ($error) {
        print "ok $test # error='$error' (as expected)\n";
      } else {
        print "not ok $test # expected error, got no error and '$got'\n";
      }
    } elsif ($got ne $expect_bytes) {
      print "not ok $test # error '$error', expect '$expect_bytes', got '$got'\n";
    } else {
      print "ok $test\n";
    }
    $test++;
  }
}
EOT

close FH or die "close $testpl: $!\n";

# This is where the test numbers carry on after the test number above are
# relayed
my $test = 44;

################ Makefile.PL
# We really need a Makefile.PL because make test for a no dynamic linking perl
# will run Makefile.PL again as part of the "make perl" target.
my $makefilePL = catfile($dir, "Makefile.PL");
push @files, "Makefile.PL";
open FH, ">$makefilePL" or die "open >$makefilePL: $!\n";
print FH <<"EOT";
#!$perl -w
use ExtUtils::MakeMaker;
WriteMakefile(
              'NAME'		=> "$package",
              'VERSION_FROM'	=> "$package.pm", # finds \$VERSION
              (\$] >= 5.005 ?
               (#ABSTRACT_FROM => "$package.pm", # XXX add this
                AUTHOR     => "$0") : ())
             );
EOT

close FH or die "close $makefilePL: $!\n";

################ MANIFEST
# We really need a MANIFEST because make distclean checks it.
my $manifest = catfile($dir, "MANIFEST");
push @files, "MANIFEST";
open FH, ">$manifest" or die "open >$manifest: $!\n";
print FH "$_\n" foreach @files;
close FH or die "close $manifest: $!\n";

chdir $dir or die $!; push @INC,  '../../lib';
END {chdir ".." or warn $!};

my @perlout = `$runperl Makefile.PL PERL_CORE=1`;
if ($?) {
  print "not ok 1 # $runperl Makefile.PL failed: $?\n";
  print "# $_" foreach @perlout;
  exit($?);
} else {
  print "ok 1\n";
}


my $makefile = ($^O eq 'VMS' ? 'descrip' : 'Makefile');
my $makefile_ext = ($^O eq 'VMS' ? '.mms' : '');
if (-f "$makefile$makefile_ext") {
  print "ok 2\n";
} else {
  print "not ok 2\n";
}

# Renamed by make clean
my $makefile_rename = $makefile . ($^O eq 'VMS' ? '.mms' : '.old');

my $make = $Config{make};

$make = $ENV{MAKE} if exists $ENV{MAKE};

if ($^O eq 'MSWin32' && $make eq 'nmake') { $make .= " -nologo"; }

my @makeout;

if ($^O eq 'VMS') { $make .= ' all'; }
print "# make = '$make'\n";
@makeout = `$make`;
if ($?) {
  print "not ok 3 # $make failed: $?\n";
  print "# $_" foreach @makeout;
  exit($?);
} else {
  print "ok 3\n";
}

if ($^O eq 'VMS') { $make =~ s{ all}{}; }

if ($Config{usedl}) {
  print "ok 4\n";
} else {
  my $makeperl = "$make perl";
  print "# make = '$makeperl'\n";
  @makeout = `$makeperl`;
  if ($?) {
    print "not ok 4 # $makeperl failed: $?\n";
  print "# $_" foreach @makeout;
    exit($?);
  } else {
    print "ok 4\n";
  }
}

my $maketest = "$make test";
print "# make = '$maketest'\n";

@makeout = `$maketest`;

if (open OUTPUT, "<$output") {
  print while <OUTPUT>;
  close OUTPUT or print "# Close $output failed: $!\n";
} else {
  # Harness will report missing test results at this point.
  print "# Open <$output failed: $!\n";
}

if ($?) {
  print "not ok $test # $maketest failed: $?\n";
  print "# $_" foreach @makeout;
} else {
  print "ok $test - maketest\n";
}
$test++;


# -x is busted on Win32 < 5.6.1, so we emulate it.
my $regen;
if( $^O eq 'MSWin32' && $] <= 5.006001 ) {
    open(REGENTMP, ">regentmp") or die $!;
    open(XS, "$package.xs")     or die $!;
    my $saw_shebang;
    while(<XS>) {
        $saw_shebang++ if /^#!.*/i ;
        print REGENTMP $_ if $saw_shebang;
    }
    close XS;  close REGENTMP;
    $regen = `$runperl regentmp`;
    unlink 'regentmp';
}
else {
    $regen = `$runperl -x $package.xs`;
}
if ($?) {
  print "not ok $test # $runperl -x $package.xs failed: $?\n";
} else {
  print "ok $test - regen\n";
}
$test++;

my $expect = $constant_types . $C_constant .
  "\n#### XS Section:\n" . $XS_constant;

if ($expect eq $regen) {
  print "ok $test - regen worked\n";
} else {
  print "not ok $test - regen worked\n";
  # open FOO, ">expect"; print FOO $expect;
  # open FOO, ">regen"; print FOO $regen; close FOO;
}
$test++;

my $makeclean = "$make clean";
print "# make = '$makeclean'\n";
@makeout = `$makeclean`;
if ($?) {
  print "not ok $test # $make failed: $?\n";
  print "# $_" foreach @makeout;
} else {
  print "ok $test\n";
}
$test++;

sub check_for_bonus_files {
  my $dir = shift;
  my %expect = map {($^O eq 'VMS' ? lc($_) : $_), 1} @_;

  my $fail;
  opendir DIR, $dir or die "opendir '$dir': $!";
  while (defined (my $entry = readdir DIR)) {
    $entry =~ s/\.$// if $^O eq 'VMS';  # delete trailing dot that indicates no extension
    next if $expect{$entry};
    print "# Extra file '$entry'\n";
    $fail = 1;
  }

  closedir DIR or warn "closedir '.': $!";
  if ($fail) {
    print "not ok $test\n";
  } else {
    print "ok $test\n";
  }
  $test++;
}

check_for_bonus_files ('.', @files, $output, $makefile_rename, '.', '..');

rename $makefile_rename, $makefile
 or die "Can't rename '$makefile_rename' to '$makefile': $!";

unlink $output or warn "Can't unlink '$output': $!";

# Need to make distclean to remove ../../lib/ExtTest.pm
my $makedistclean = "$make distclean";
print "# make = '$makedistclean'\n";
@makeout = `$makedistclean`;
if ($?) {
  print "not ok $test # $make failed: $?\n";
  print "# $_" foreach @makeout;
} else {
  print "ok $test\n";
}
$test++;

check_for_bonus_files ('.', @files, '.', '..');

unless ($keep_files) {
  foreach (@files) {
    unlink $_ or warn "unlink $_: $!";
  }
}

check_for_bonus_files ('.', '.', '..');
