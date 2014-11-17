#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '.';
    push @INC, '../lib';
}

sub do_require {
    %INC = ();
    write_file('bleah.pm',@_);
    eval { require "bleah.pm" };
    my @a; # magic guard for scope violations (must be first lexical in file)
}

# don't make this lexical
$i = 1;

my @files_to_delete = qw (bleah.pm bleah.do bleah.flg urkkk.pm urkkk.pmc
krunch.pm krunch.pmc whap.pm whap.pmc);

# there may be another copy of this test script running, or the files may
# just not have been deleted at the end of the last run; if the former, we
# wait a while so that creating and unlinking these files won't interfere
# with the other process; if the latter, then the delay is harmless.  As
# to why there might be multiple execution of this test file, I don't
# know; but this is an experiment to see if random smoke failures go away.

if (grep -e, @files_to_delete) {
    print "# Sleeping for 20 secs waiting for other process to finish\n";
    sleep 20;
}


my $Is_EBCDIC = (ord('A') == 193) ? 1 : 0;
my $Is_UTF8   = (${^OPEN} || "") =~ /:utf8/;
my $total_tests = 56;
if ($Is_EBCDIC || $Is_UTF8) { $total_tests -= 3; }
print "1..$total_tests\n";

sub write_file {
    my $f = shift;
    open(REQ,">$f") or die "Can't write '$f': $!";
    binmode REQ;
    print REQ @_;
    close REQ or die "Could not close $f: $!";
}

eval {require 5.005};
print "# $@\nnot " if $@;
print "ok ",$i++," - require 5.005 try 1\n";

eval { require 5.005 };
print "# $@\nnot " if $@;
print "ok ",$i++," - require 5.005 try 2\n";

eval { require 5.005; };
print "# $@\nnot " if $@;
print "ok ",$i++," - require 5.005 try 3\n";

eval {
    require 5.005
};
print "# $@\nnot " if $@;
print "ok ",$i++," - require 5.005 try 4\n";

# new style version numbers

eval { require v5.5.630; };
print "# $@\nnot " if $@;
print "ok ",$i++," - require 5.5.630\n";

sub v5 { die }
eval { require v5; };
print "# $@\nnot " if $@;
print "ok ",$i++," - require v5 ignores sub named v5\n";

eval { require 10.0.2; };
print "# $@\nnot " unless $@ =~ /^Perl v10\.0\.2 required/;
print "ok ",$i++," - require 10.0.2\n";

my $ver = 5.005_63;
eval { require $ver; };
print "# $@\nnot " if $@;
print "ok ",$i++," - require 5.005_63\n";

# check inaccurate fp
$ver = 10.2;
eval { require $ver; };
print "# $@\nnot " unless $@ =~ /^Perl v10\.200.0 required/;
print "ok ",$i++," - require 10.2\n";

$ver = 10.000_02;
eval { require $ver; };
print "# $@\nnot " unless $@ =~ /^Perl v10\.0\.20 required/;
print "ok ",$i++," - require 10.000_02\n";

print "not " unless 5.5.1 gt v5.5;
print "ok ",$i++," - 5.5.1 gt v5.5\n";

{
    print "not " unless v5.5.640 eq "\x{5}\x{5}\x{280}";
    print "ok ",$i++," - v5.5.640 eq \\x{5}\\x{5}\\x{280}\n";

    print "not " unless v7.15 eq "\x{7}\x{f}";
    print "ok ",$i++," - v7.15 eq \\x{7}\\x{f}\n";

    print "not "
      unless v1.20.300.4000.50000.600000 eq "\x{1}\x{14}\x{12c}\x{fa0}\x{c350}\x{927c0}";
    print "ok ",$i++," - v1.20.300.4000.50000.600000 eq ...\n";
}

# "use 5.11.0" (and higher) loads strictures.
# check that this doesn't happen with require
eval 'require 5.11.0; ${"foo"} = "bar";';
print "# $@\nnot " if $@;
print "ok ",$i++," - require 5.11.0\n";
eval 'BEGIN {require 5.11.0} ${"foo"} = "bar";';
print "# $@\nnot " if $@;
print "ok ",$i++,"\ - BEGIN { require 5.11.0}\n";

# interaction with pod (see the eof)
write_file('bleah.pm', "print 'ok $i - require bleah.pm\n'; 1;\n");
require "bleah.pm";
$i++;

# run-time failure in require
do_require "0;\n";
print "# $@\nnot " unless $@ =~ /did not return a true/;
print "ok ",$i++," - require returning 0\n";

print "not " if exists $INC{'bleah.pm'};
print "ok ",$i++," - %INC not updated\n";

my $flag_file = 'bleah.flg';
# run-time error in require
for my $expected_compile (1,0) {
    write_file($flag_file, 1);
    print "not " unless -e $flag_file;
    print "ok ",$i++," - exp $expected_compile; bleah.flg\n";
    write_file('bleah.pm', "unlink '$flag_file' or die; \$a=0; \$b=1/\$a; 1;\n");
    print "# $@\nnot " if eval { require 'bleah.pm' };
    print "ok ",$i++," - exp $expected_compile; require bleah.pm with flag file\n";
    print "not " unless -e $flag_file xor $expected_compile;
    print "ok ",$i++," - exp $expected_compile; -e flag_file\n";
    print "not " unless exists $INC{'bleah.pm'};
    print "ok ",$i++," - exp $expected_compile; exists \$INC{'bleah.pm}\n";
}

# compile-time failure in require
do_require "1)\n";
# bison says 'parse error' instead of 'syntax error',
# various yaccs may or may not capitalize 'syntax'.
print "# $@\nnot " unless $@ =~ /(syntax|parse) error/mi;
print "ok ",$i++," - syntax error\n";

# previous failure cached in %INC
print "not " unless exists $INC{'bleah.pm'};
print "ok ",$i++," - cached %INC\n";
write_file($flag_file, 1);
write_file('bleah.pm', "unlink '$flag_file'; 1");
print "# $@\nnot " if eval { require 'bleah.pm' };
print "ok ",$i++," - eval { require 'bleah.pm' }\n";
print "# $@\nnot " unless $@ =~ /Compilation failed/i;
print "ok ",$i++," - Compilation failed\n";
print "not " unless -e $flag_file;
print "ok ",$i++," - -e flag_file\n";
print "not " unless exists $INC{'bleah.pm'};
print "ok ",$i++," - \$INC{'bleah.pm'}\n";

# successful require
do_require "1";
print "# $@\nnot " if $@;
print "ok ",$i++," - do_require '1';\n";

# do FILE shouldn't see any outside lexicals
my $x = "ok $i - bleah.do\n";
write_file("bleah.do", <<EOT);
\$x = "not ok $i - bleah.do\\n";
EOT
do "bleah.do" or die $@;
dofile();
sub dofile { do "bleah.do" or die $@; };
print $x;

# Test that scalar context is forced for require

write_file('bleah.pm', <<'**BLEAH**'
print "not " if !defined wantarray || wantarray ne '';
print "ok $i - require() context\n";
1;
**BLEAH**
);
                              delete $INC{"bleah.pm"}; ++$::i;
$foo = eval q{require bleah}; delete $INC{"bleah.pm"}; ++$::i;
@foo = eval q{require bleah}; delete $INC{"bleah.pm"}; ++$::i;
       eval q{require bleah}; delete $INC{"bleah.pm"}; ++$::i;
       eval q{$_=$_+2;require bleah}; delete $INC{"bleah.pm"}; ++$::i;
       eval q{return require bleah}; delete $INC{"bleah.pm"}; ++$::i;
$foo = eval  {require bleah}; delete $INC{"bleah.pm"}; ++$::i;
@foo = eval  {require bleah}; delete $INC{"bleah.pm"}; ++$::i;
       eval  {require bleah};

# Test for fix of RT #24404 : "require $scalar" may load a directory
my $r = "threads";
eval { require $r };
$i++;
if($@ =~ /Can't locate threads in \@INC/) {
    print "ok $i - RT #24404\n";
} else {
    print "not ok - RT #24404$i\n";
}

# require CORE::foo
eval ' require CORE::lc "THREADS" ';
$i++;
if($@ =~ /Can't locate threads in \@INC/) {
    print "ok $i - [perl #24482] require CORE::foo\n";
} else {
    print "not ok - [perl #24482] require CORE::foo\n";
}


write_file('bleah.pm', qq(die "This is an expected error";\n));
delete $INC{"bleah.pm"}; ++$::i;
eval { CORE::require bleah; };
if ($@ =~ /^This is an expected error/) {
    print "ok $i - expected error\n";
} else {
    print "not ok $i - expected error\n";
}

sub write_file_not_thing {
    my ($file, $thing, $test) = @_;
    write_file($file, <<"EOT");
    print "not ok $test - write_file_not_thing $file\n";
    die "The $thing file should not be loaded";
EOT
}

{
    # Right. We really really need Config here.
    require Config;
    die "Failed to load Config for some reason"
	unless $Config::Config{version};
    my $ccflags = $Config::Config{ccflags};
    die "Failed to get ccflags for some reason" unless defined $ccflags;

    my $simple = ++$i;
    my $pmc_older = ++$i;
    my $pmc_dies = ++$i;
    if ($ccflags =~ /(?:^|\s)-DPERL_DISABLE_PMC\b/) {
	print "# .pmc files are ignored, so test that\n";
	write_file_not_thing('krunch.pmc', '.pmc', $pmc_older);
	write_file('urkkk.pm', qq(print "ok $simple - urkkk.pm branch A\n"));
	write_file('whap.pmc', qq(die "This is not an expected error"));

	print "# Sleeping for 2 seconds before creating some more files\n";
	sleep 2;

	write_file('krunch.pm', qq(print "ok $pmc_older - krunch.pm branch A\n"));
	write_file_not_thing('urkkk.pmc', '.pmc', $simple);
	write_file('whap.pm', qq(die "This is an expected error"));
    } else {
	print "# .pmc files should be loaded, so test that\n";
	write_file('krunch.pmc', qq(print "ok $pmc_older - krunch.pm branch B\n";));
	write_file_not_thing('urkkk.pm', '.pm', $simple);
	write_file('whap.pmc', qq(die "This is an expected error"));

	print "# Sleeping for 2 seconds before creating some more files\n";
	sleep 2;

	write_file_not_thing('krunch.pm', '.pm', $pmc_older);
	write_file('urkkk.pmc', qq(print "ok $simple - urkkk.pm branch B\n";));
	write_file_not_thing('whap.pm', '.pm', $pmc_dies);
    }
    require urkkk;
    require krunch;
    eval {CORE::require whap; 1} and die;

    if ($@ =~ /^This is an expected error/) {
	print "ok $pmc_dies - pmc_dies\n";
    } else {
	print "not ok $pmc_dies - pmc_dies\n";
    }
}


{
    # if we 'require "op"', since we're in the t/ directory and '.' is the
    # first thing in @INC, it will try to load t/op/; it should fail and
    # move onto the next path; however, the previous value of $! was
    # leaking into implementation if it was EACCES and we're accessing a
    # directory.

    $! = eval 'use Errno qw(EACCES); EACCES' || 0;
    eval q{require 'op'};
    $i++;
    print "not " if $@ =~ /Permission denied/;
    print "ok $i - require op\n";
}

# Test "require func()" with abs path when there is no .pmc file.
++$::i;
if (defined &DynaLoader::boot_DynaLoader) {
    require Cwd;
    require File::Spec::Functions;
    eval {
     CORE::require(File::Spec::Functions::catfile(Cwd::getcwd(),"bleah.pm"));
    };
    if ($@ =~ /^This is an expected error/) {
	print "ok $i - require(func())\n";
    } else {
	print "not ok $i - require(func())\n";
    }
} else {
    print "ok $i # SKIP Cwd may not be available in miniperl\n";
}

{
    BEGIN { ${^OPEN} = ":utf8\0"; }
    %INC = ();
    write_file('bleah.pm',"package F; \$x = '\xD1\x9E';\n");
    eval { require "bleah.pm" };
    $i++;
    my $not = $F::x eq "\xD1\x9E" ? "" : "not ";
    print "${not}ok $i - require ignores I/O layers\n";
}

{
    BEGIN { ${^OPEN} = ":utf8\0"; }
    %INC = ();
    write_file('bleah.pm',"require re; re->import('/x'); 1;\n");
    my $not = eval 'use bleah; "ab" =~ /a b/' ? "" : "not ";
    $i++;
    print "${not}ok $i - require does not localise %^H at run time\n";
}

##########################################
# What follows are UTF-8 specific tests. #
# Add generic tests before this point.   #
##########################################

# UTF-encoded things - skipped on EBCDIC machines and on UTF-8 input

if ($Is_EBCDIC || $Is_UTF8) { exit; }

my %templates = (
		 'UTF-8'    => 'C0U',
		 'UTF-16BE' => 'n',
		 'UTF-16LE' => 'v',
		);

sub bytes_to_utf {
    my ($enc, $content, $do_bom) = @_;
    my $template = $templates{$enc};
    die "Unsupported encoding $enc" unless $template;
    return pack "$template*", ($do_bom ? 0xFEFF : ()), unpack "C*", $content;
}

foreach (sort keys %templates) {
    $i++; do_require(bytes_to_utf($_, qq(print "ok $i # $_\\n"; 1;\n), 1));
    if ($@ =~ /^(Unsupported script encoding \Q$_\E)/) {
	print "ok $i # skip $1\n";
    }
}

END {
    foreach my $file (@files_to_delete) {
	1 while unlink $file;
    }
}

# ***interaction with pod (don't put any thing after here)***

=pod
