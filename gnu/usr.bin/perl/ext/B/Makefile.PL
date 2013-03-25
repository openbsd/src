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

my @names = qw(CVf_ANON CVf_CLONE CVf_CLONED CVf_CONST CVf_LVALUE CVf_METHOD
	       CVf_NODEBUG CVf_UNIQUE CVf_WEAKOUTSIDE
	       GVf_IMPORTED_AV GVf_IMPORTED_CV GVf_IMPORTED_HV GVf_IMPORTED_SV
	       HEf_SVKEY
	       SVTYPEMASK SVt_PVGV SVt_PVHV
	       SVf_FAKE SVf_IOK SVf_IVisUV SVf_NOK SVf_POK SVf_READONLY
	       SVf_ROK SVp_IOK SVp_NOK SVp_POK SVpad_OUR SVs_RMG SVs_SMG
	       PAD_FAKELEX_ANON PAD_FAKELEX_MULTI);

if ($] >= 5.009) {
    push @names, 'CVf_ISXSUB';
} else {
    # Constant not present after 5.8.x
    push @names, 'AVf_REAL';
    # This is only present in 5.10, but it's useful to B::Deparse to be able
    # to import a dummy value from B
    push @names, {name=>"OPpPAD_STATE", default=>["IV", "0"]};
}  

if ($] < 5.011) {
    # Constant not present after 5.10.x
    push @names, 'CVf_LOCKED';
}

# First element in each tuple is the file; second is a regex snippet
# giving the prefix to limit the names of symbols to define that come
# from that file.  If none, all symbols will be defined whose values
# match the pattern below.
foreach my $tuple (['op_reg_common.h','(?:(?:RXf_)?PMf_)'],
		   ['op.h'],
		   ['cop.h'],
		   ['regexp.h','RXf_']) {
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
    NAMES => [map {ref $_ ? $_ : {name=>$_, macro=>1}} @names],
    XS_SUBNAME => undef,
);
