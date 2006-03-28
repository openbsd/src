#!../perl
our $DEBUG = @ARGV;
our (%ORD, %SEQ, $NTESTS);
BEGIN {
     if ($ENV{'PERL_CORE'}){
         chdir 't';
         unshift @INC, '../lib';
     }
     require Config; import Config;
     if ($Config{'extensions'} !~ /\bEncode\b/) {
         print "1..0 # Skip: Encode was not built\n";
	 exit 0;
     }
     if ($] <= 5.008 and !$Config{perl_patchlevel}){
	 print "1..0 # Skip: Perl 5.8.1 or later required\n";
	 exit 0;
     }
     # http://smontagu.damowmow.com/utf8test.html
     %ORD = (
	     0x00000080 => 0, # 2.1.2
	     0x00000800 => 0, # 2.1.3
	     0x00010000 => 0, # 2.1.4
	     0x00200000 => 1, # 2.1.5
	     0x00400000 => 1, # 2.1.6
	     0x0000007F => 0, # 2.2.1 -- unmapped okay
	     0x000007FF => 0, # 2.2.2
	     0x0000FFFF => 1, # 2.2.3
	     0x001FFFFF => 1, # 2.2.4
	     0x03FFFFFF => 1, # 2.2.5
	     0x7FFFFFFF => 1, # 2.2.6
	     0x0000D800 => 1, # 5.1.1
	     0x0000DB7F => 1, # 5.1.2
	     0x0000D880 => 1, # 5.1.3
	     0x0000DBFF => 1, # 5.1.4
	     0x0000DC00 => 1, # 5.1.5
	     0x0000DF80 => 1, # 5.1.6
	     0x0000DFFF => 1, # 5.1.7
	     # 5.2 "Paird UTF-16 surrogates skipped
	     # because utf-8-strict raises exception at the first one
	     0x0000FFFF => 1, # 5.3.1
	    );
     $NTESTS +=  scalar keys %ORD;
     %SEQ = (
	     qq/ed 9f bf/    => 0, # 2.3.1
	     qq/ee 80 80/    => 0, # 2.3.2
	     qq/f4 8f bf bf/ => 0, # 2.3.3
	     qq/f4 90 80 80/ => 1, # 2.3.4 -- out of range so NG
	     # "3 Malformed sequences" are checked by perl.
	     # "4 Overlong sequences"  are checked by perl.
	    );
     $NTESTS +=  scalar keys %SEQ;
}
use strict;
use Encode;
use utf8;
use Test::More tests => $NTESTS;

local($SIG{__WARN__}) = sub { $DEBUG and $@ and print STDERR $@ };

my $d = find_encoding("utf-8-strict");
for my $u (sort keys %ORD){
    my $c = chr($u);
    eval { $d->encode($c,1) };
    $DEBUG and $@ and warn $@;
    my $t = $@ ? 1 : 0;
    is($t, $ORD{$u}, sprintf "U+%04X", $u);
}
for my $s (sort keys %SEQ){
    my $o = pack "C*" => map {hex} split /\s+/, $s;
    eval { $d->decode($o,1) };
    $DEBUG and $@ and warn $@;
    my $t = $@ ? 1 : 0;
    is($t, $SEQ{$s}, $s);
}

__END__


