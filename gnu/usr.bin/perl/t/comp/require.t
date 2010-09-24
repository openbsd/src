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

my @fjles_to_delete = qw (bleah.pm bleah.do bleah.flg urkkk.pm urkkk.pmc
krunch.pm krunch.pmc whap.pm whap.pmc);


my $Is_EBCDIC = (ord('A') == 193) ? 1 : 0;
my $Is_UTF8   = (${^OPEN} || "") =~ /:utf8/;
my $total_tests = 49;
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
print "ok ",$i++,"\n";

eval { require 5.005 };
print "# $@\nnot " if $@;
print "ok ",$i++,"\n";

eval { require 5.005; };
print "# $@\nnot " if $@;
print "ok ",$i++,"\n";

eval {
    require 5.005
};
print "# $@\nnot " if $@;
print "ok ",$i++,"\n";

# new style version numbers

eval { require v5.5.630; };
print "# $@\nnot " if $@;
print "ok ",$i++,"\n";

eval { require 10.0.2; };
print "# $@\nnot " unless $@ =~ /^Perl v10\.0\.2 required/;
print "ok ",$i++,"\n";

my $ver = 5.005_63;
eval { require $ver; };
print "# $@\nnot " if $@;
print "ok ",$i++,"\n";

# check inaccurate fp
$ver = 10.2;
eval { require $ver; };
print "# $@\nnot " unless $@ =~ /^Perl v10\.200.0 required/;
print "ok ",$i++,"\n";

$ver = 10.000_02;
eval { require $ver; };
print "# $@\nnot " unless $@ =~ /^Perl v10\.0\.20 required/;
print "ok ",$i++,"\n";

print "not " unless 5.5.1 gt v5.5;
print "ok ",$i++,"\n";

{
    print "not " unless v5.5.640 eq "\x{5}\x{5}\x{280}";
    print "ok ",$i++,"\n";

    print "not " unless v7.15 eq "\x{7}\x{f}";
    print "ok ",$i++,"\n";

    print "not "
      unless v1.20.300.4000.50000.600000 eq "\x{1}\x{14}\x{12c}\x{fa0}\x{c350}\x{927c0}";
    print "ok ",$i++,"\n";
}

# "use 5.11.0" (and higher) loads strictures.
# check that this doesn't happen with require
eval 'require 5.11.0; ${"foo"} = "bar";';
print "# $@\nnot " if $@;
print "ok ",$i++,"\n";

# interaction with pod (see the eof)
write_file('bleah.pm', "print 'ok $i\n'; 1;\n");
require "bleah.pm";
$i++;

# run-time failure in require
do_require "0;\n";
print "# $@\nnot " unless $@ =~ /did not return a true/;
print "ok ",$i++,"\n";

print "not " if exists $INC{'bleah.pm'};
print "ok ",$i++,"\n";

my $flag_file = 'bleah.flg';
# run-time error in require
for my $expected_compile (1,0) {
    write_file($flag_file, 1);
    print "not " unless -e $flag_file;
    print "ok ",$i++,"\n";
    write_file('bleah.pm', "unlink '$flag_file' or die; \$a=0; \$b=1/\$a; 1;\n");
    print "# $@\nnot " if eval { require 'bleah.pm' };
    print "ok ",$i++,"\n";
    print "not " unless -e $flag_file xor $expected_compile;
    print "ok ",$i++,"\n";
    print "not " unless exists $INC{'bleah.pm'};
    print "ok ",$i++,"\n";
}

# compile-time failure in require
do_require "1)\n";
# bison says 'parse error' instead of 'syntax error',
# various yaccs may or may not capitalize 'syntax'.
print "# $@\nnot " unless $@ =~ /(syntax|parse) error/mi;
print "ok ",$i++,"\n";

# previous failure cached in %INC
print "not " unless exists $INC{'bleah.pm'};
print "ok ",$i++,"\n";
write_file($flag_file, 1);
write_file('bleah.pm', "unlink '$flag_file'; 1");
print "# $@\nnot " if eval { require 'bleah.pm' };
print "ok ",$i++,"\n";
print "# $@\nnot " unless $@ =~ /Compilation failed/i;
print "ok ",$i++,"\n";
print "not " unless -e $flag_file;
print "ok ",$i++,"\n";
print "not " unless exists $INC{'bleah.pm'};
print "ok ",$i++,"\n";

# successful require
do_require "1";
print "# $@\nnot " if $@;
print "ok ",$i++,"\n";

# do FILE shouldn't see any outside lexicals
my $x = "ok $i\n";
write_file("bleah.do", <<EOT);
\$x = "not ok $i\\n";
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
    print "ok $i\n";
} else {
    print "not ok $i\n";
}


write_file('bleah.pm', qq(die "This is an expected error";\n));
delete $INC{"bleah.pm"}; ++$::i;
eval { CORE::require bleah; };
if ($@ =~ /^This is an expected error/) {
    print "ok $i\n";
} else {
    print "not ok $i\n";
}

sub write_file_not_thing {
    my ($file, $thing, $test) = @_;
    write_file($file, <<"EOT");
    print "not ok $test\n";
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
	write_file('urkkk.pm', qq(print "ok $simple\n"));
	write_file('whap.pmc', qq(die "This is not an expected error"));

	print "# Sleeping for 2 seconds before creating some more files\n";
	sleep 2;

	write_file('krunch.pm', qq(print "ok $pmc_older\n"));
	write_file_not_thing('urkkk.pmc', '.pmc', $simple);
	write_file('whap.pm', qq(die "This is an expected error"));
    } else {
	print "# .pmc files should be loaded, so test that\n";
	write_file('krunch.pmc', qq(print "ok $pmc_older\n";));
	write_file_not_thing('urkkk.pm', '.pm', $simple);
	write_file('whap.pmc', qq(die "This is an expected error"));

	print "# Sleeping for 2 seconds before creating some more files\n";
	sleep 2;

	write_file_not_thing('krunch.pm', '.pm', $pmc_older);
	write_file('urkkk.pmc', qq(print "ok $simple\n";));
	write_file_not_thing('whap.pm', '.pm', $pmc_dies);
    }
    require urkkk;
    require krunch;
    eval {CORE::require whap; 1} and die;

    if ($@ =~ /^This is an expected error/) {
	print "ok $pmc_dies\n";
    } else {
	print "not ok $pmc_dies\n";
    }
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
    foreach my $file (@fjles_to_delete) {
	1 while unlink $file;
    }
}

# ***interaction with pod (don't put any thing after here)***

=pod
