#!./perl -w

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    no warnings; # Need global -w flag for later tests, but don't want this
    # to warn here:
    push @INC, "::lib:$MacPerl::Architecture:" if $^O eq 'MacOS';
    unless (find PerlIO::Layer 'perlio') {
	print "1..0 # Skip: not perlio\n";
	exit 0;
    }
}

print "1..13\n";

my $grk = "grk$$";
my $utf = "utf$$";
my $fail1 = "fa$$";
my $fail2 = "fb$$";
my $russki = "koi8r$$";

if (open(GRK, ">$grk")) {
    binmode(GRK, ":bytes");
    # alpha beta gamma in ISO 8859-7
    print GRK "\xe1\xe2\xe3";
    close GRK or die "Could not close: $!";
}

{
    use Encode;
    open(my $i,'<:encoding(iso-8859-7)',$grk);
    print "ok 1\n";
    open(my $o,'>:utf8',$utf);
    print "ok 2\n";
    print $o readline($i);
    print "ok 3\n";
    close($o) or die "Could not close: $!";
    close($i);
}

if (open(UTF, "<$utf")) {
    binmode(UTF, ":bytes");
    if (ord('A') == 193) { # EBCDIC
	# alpha beta gamma in UTF-EBCDIC Unicode (0x3b1 0x3b2 0x3b3)
	print "not " unless <UTF> eq "\xb4\x58\xb4\x59\xb4\x62";
    } else {
	# alpha beta gamma in UTF-8 Unicode (0x3b1 0x3b2 0x3b3)
	print "not " unless <UTF> eq "\xce\xb1\xce\xb2\xce\xb3";
    }
    print "ok 4\n";
    close UTF;
}

{
    use Encode;
    open(my $i,'<:utf8',$utf);
    print "ok 5\n";
    open(my $o,'>:encoding(iso-8859-7)',$grk);
    print "ok 6\n";
    print $o readline($i);
    print "ok 7\n";
    close($o) or die "Could not close: $!";
    close($i);
}

if (open(GRK, "<$grk")) {
    binmode(GRK, ":bytes");
    print "not " unless <GRK> eq "\xe1\xe2\xe3";
    print "ok 8\n";
    close GRK;
}

$SIG{__WARN__} = sub {$warn .= $_[0]};

if (open(FAIL, ">:encoding(NoneSuch)", $fail1)) {
    print "not ok 9 # Open should fail\n";
} else {
    print "ok 9\n";
}
if (!defined $warn) {
    print "not ok 10 # warning is undef\n";
} elsif ($warn =~ /^Cannot find encoding "NoneSuch" at/) {
    print "ok 10\n";
} else {
    print "not ok 10 # warning is '$warn'";
}

if (open(RUSSKI, ">$russki")) {
    print RUSSKI "\x3c\x3f\x78";
    close RUSSKI or die "Could not close: $!";
    open(RUSSKI, "$russki");
    binmode(RUSSKI, ":raw");
    my $buf1;
    read(RUSSKI, $buf1, 1);
    # eof(RUSSKI);
    binmode(RUSSKI, ":encoding(koi8-r)");
    my $buf2;
    read(RUSSKI, $buf2, 1);
    my $offset = tell(RUSSKI);
    if (ord($buf1) == 0x3c &&
	ord($buf2) == (ord('A') == 193) ? 0x6f : 0x3f &&
	$offset == 2) {
	print "ok 11\n";
    } else {
	printf "not ok 11 # [%s] [%s] %d\n",
	       join(" ", unpack("H*", $buf1)),
	       join(" ", unpack("H*", $buf2)), $offset;
    }
    close(RUSSKI);
} else {
    print "not ok 11 # open failed: $!\n";
}

undef $warn;

# Check there is no Use of uninitialized value in concatenation (.) warning
# due to the way @latin2iso_num was used to make aliases.
if (open(FAIL, ">:encoding(latin42)", $fail2)) {
    print "not ok 12 # Open should fail\n";
} else {
    print "ok 12\n";
}
if (!defined $warn) {
    print "not ok 13 # warning is undef\n";
} elsif ($warn =~ /^Cannot find encoding "latin42" at.*line \d+\.$/) {
    print "ok 13\n";
} else {
    print "not ok 13 # warning is: \n";
    $warn =~ s/^/# /mg;
    print "$warn";
}

END {
    unlink($grk, $utf, $fail1, $fail2, $russki);
}
