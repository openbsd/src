use ExtUtils::MakeMaker;
use ExtUtils::Constant 0.23 'WriteConstants';
use File::Spec;
use strict;
use warnings;

my $core = grep { $_ eq 'PERL_CORE=1' } @ARGV;

WriteMakefile(
    NAME	    => "B",
    VERSION_FROM    => "B.pm",
    realclean	    => {FILES=> 'const-c.inc const-xs.inc'},
);

my $headerpath;
if ($core) {
    $headerpath = File::Spec->catdir(File::Spec->updir, File::Spec->updir);
} else {
	require Config;
    $headerpath = File::Spec->catdir($Config::Config{archlibexp}, "CORE");
}

my @names = ({ name => 'HEf_SVKEY', macro => 1, type => "IV" },
             qw(SVTYPEMASK SVt_PVGV SVt_PVHV PAD_FAKELEX_ANON PAD_FAKELEX_MULTI));


# First element in each tuple is the file; second is a regex snippet
# giving the prefix to limit the names of symbols to define that come
# from that file.  If none, all symbols will be defined whose values
# match the pattern below.
foreach my $tuple (['cop.h'],
                   ['cv.h', 'CVf'],
                   ['gv.h', 'GVf'],
                   ['op.h'],
                   ['op_reg_common.h','(?:(?:RXf_)?PMf_)'],
                   ['regexp.h','RXf_'],
                   ['sv.h', 'SV(?:[fps]|pad)_'],
                  ) {
    my $file = $tuple->[0];
    my $pfx = $tuple->[1] || '';
    my $path = File::Spec->catfile($headerpath, $file);
    open my $fh, '<', $path or die "Cannot open $path: $!";
    while (<$fh>) {
	push @names, $1 if (/ \#define \s+ ( $pfx \w+ ) \s+
			      ( [()|\dx]+             # Parens, '|', digits, 'x'
			      | \(? \d+ \s* << .*?    # digits left shifted by anything
			      ) \s* (?: $| \/ \* )    # ending at comment or $
			    /x);
    }
    close $fh;
}

# Currently only SVt_PVGV and SVt_PVHV aren't macros, but everything we name
# should exist, so ensure that the C compile breaks if anything does not.
WriteConstants(
    PROXYSUBS => {push => 'EXPORT_OK'},
    NAME => 'B',
    NAMES => [map {ref $_ ? $_ : {name=>$_, macro=>1, type=>"UV"}} @names],
    XS_SUBNAME => undef,
);
