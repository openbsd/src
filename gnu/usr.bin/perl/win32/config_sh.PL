#!perl -w
use strict;
use FindExt;

# take a semicolon separated path list and turn it into a quoted
# list of paths that Text::Parsewords will grok
sub mungepath {
    my $p = shift;
    # remove leading/trailing semis/spaces
    $p =~ s/^[ ;]+//;
    $p =~ s/[ ;]+$//;
    $p =~ s/'/"/g;
    my @p = map { $_ = "\"$_\"" if /\s/ and !/^".*"$/; $_ } split /;/, $p;
    return join(' ', @p);
}

# generate an array of option strings from command-line args
# or an option file
#    -- added by BKS, 10-17-1999 to fix command-line overflow problems
sub loadopts {
    if ($ARGV[0] =~ /--cfgsh-option-file/) {
	shift @ARGV;
	my $optfile = shift @ARGV;
	local (*OPTF);
	open OPTF, $optfile or die "Can't open $optfile: $!\n";
	my @opts;
	chomp(my $line = <OPTF>);
	my @vars = split(/\t+~\t+/, $line);
	for (@vars) {
	    push(@opts, $_) unless (/^\s*$/);
	}
	close OPTF;
	return \@opts;
    }
    else {
	return \@ARGV;
    }
}

my %opt;

my $optref = loadopts();
while (@{$optref} && $optref->[0] =~ /^([\w_]+)=(.*)$/) {
    $opt{$1}=$2;
    shift(@{$optref});
}

FindExt::scan_ext("../cpan");
FindExt::scan_ext("../dist");
FindExt::scan_ext("../ext");
FindExt::set_static_extensions(split ' ', $opt{static_ext});

$opt{nonxs_ext}        = join(' ',FindExt::nonxs_ext()) || ' ';
$opt{static_ext}       = join(' ',FindExt::static_ext()) || ' ';
$opt{dynamic_ext}      = join(' ',FindExt::dynamic_ext()) || ' ';
$opt{extensions}       = join(' ',FindExt::extensions()) || ' ';
$opt{known_extensions} = join(' ',FindExt::known_extensions()) || ' ';

my $pl_h = '../patchlevel.h';

if (-e $pl_h) {
    open PL, "<$pl_h" or die "Can't open $pl_h: $!";
    while (<PL>) {
	if (/^#\s*define\s+(PERL_\w+)\s+([\d.]+)/) {
	    $opt{$1} = $2;
	}
    }
    close PL;
}
else {
    die "Can't find $pl_h: $!";
}

my $patch_file = '../.patch';

if (-e $patch_file) {
    open my $fh, "<", $patch_file or die "Can't open $patch_file: $!";
    chomp($opt{PERL_PATCHLEVEL} = <$fh>);
    close $fh;
}

$opt{version} = "$opt{PERL_REVISION}.$opt{PERL_VERSION}.$opt{PERL_SUBVERSION}";
$opt{version_patchlevel_string} = "version $opt{PERL_VERSION} subversion $opt{PERL_SUBVERSION}";
$opt{version_patchlevel_string} .= " patch $opt{PERL_PATCHLEVEL}" if exists $opt{PERL_PATCHLEVEL};

my $ver = `ver 2>nul`;
if ($ver =~ /Version (\d+\.\d+)/) {
    $opt{osvers} = $1;
}
else {
    $opt{osvers} = '4.0';
}

if (exists $opt{cc}) {
    # cl version detection borrowed from Test::Smoke's configsmoke.pl
    if ($opt{cc} =~ /\b(?:cl|icl)/) { #MSVC can come as clarm.exe, icl=Intel C
        my $output = `$opt{cc} --version 2>&1`;
        $opt{ccversion} = $output =~ /^.*Version\s+([\d.]+)/ ? $1 : '?';
    }
    elsif ($opt{cc} =~ /\bgcc\b/) {
        chomp($opt{gccversion} = `$opt{cc} -dumpversion`);
    }
}

$opt{cf_by} = $ENV{USERNAME} unless $opt{cf_by};
if (!$opt{cf_email}) {
    my $computername = eval{(gethostbyname('localhost'))[0]};
# gethostbyname might not be implemented in miniperl
    $computername = $ENV{COMPUTERNAME} if $@;    
    $opt{cf_email} = $opt{cf_by} . '@' . $computername;
}
$opt{usemymalloc} = 'y' if $opt{d_mymalloc} eq 'define';

$opt{libpth} = mungepath($opt{libpth}) if exists $opt{libpth};
$opt{incpath} = mungepath($opt{incpath}) if exists $opt{incpath};

my($int64, $int64f);
if ($opt{cc} =~ /\b(?:cl|icl)/) {
    $int64  = '__int64';
    $int64f = 'I64';
}
elsif ($opt{cc} =~ /\bgcc\b/) {
    $int64  = 'long long';
    $int64f = 'I64';
}

# set large files options
if ($opt{uselargefiles} eq 'define') {
    $opt{lseeksize} = 8;
    $opt{lseektype} = $int64;
}
else {
    $opt{lseeksize} = 4;
    $opt{lseektype} = 'long';
}

# set 64-bit options
if ($opt{WIN64} eq 'define') {
    $opt{d_atoll} = 'define';
    $opt{d_strtoll} = 'define';
    $opt{d_strtoull} = 'define';
    $opt{ptrsize} = 8;
    $opt{sizesize} = 8;
    $opt{ssizetype} = $int64;
    $opt{st_ino_size} = 8;
}
else {
    $opt{d_atoll} = 'undef';
    $opt{d_strtoll} = 'undef';
    $opt{d_strtoull} = 'undef';
    $opt{ptrsize} = 4;
    $opt{sizesize} = 4;
    $opt{ssizetype} = 'int';
    $opt{st_ino_size} = 4;
}
if ($opt{use64bitint} eq 'define') {
    $opt{d_nv_preserves_uv} = 'undef';
    $opt{ivdformat} = qq{"${int64f}d"};
    $opt{ivsize} = 8;
    $opt{ivtype} = $int64;
    $opt{nv_preserves_uv_bits} = 53;
    $opt{sPRIXU64} = qq{"${int64f}X"};
    $opt{sPRId64} = qq{"${int64f}d"};
    $opt{sPRIi64} = qq{"${int64f}i"};
    $opt{sPRIo64} = qq{"${int64f}o"};
    $opt{sPRIu64} = qq{"${int64f}u"};
    $opt{sPRIx64} = qq{"${int64f}x"};
    $opt{uvXUformat} = qq{"${int64f}X"};
    $opt{uvoformat} = qq{"${int64f}o"};
    $opt{uvsize} = 8;
    $opt{uvtype} = qq{unsigned $int64};
    $opt{uvuformat} = qq{"${int64f}u"};
    $opt{uvxformat} = qq{"${int64f}x"};
}
else {
    $opt{d_nv_preserves_uv} = 'define';
    $opt{ivdformat} = '"ld"';
    $opt{ivsize} = 4;
    $opt{ivtype} = 'long';
    $opt{nv_preserves_uv_bits} = 32;
    $opt{sPRIXU64} = '"lX"';
    $opt{sPRId64} = '"ld"';
    $opt{sPRIi64} = '"li"';
    $opt{sPRIo64} = '"lo"';
    $opt{sPRIu64} = '"lu"';
    $opt{sPRIx64} = '"lx"';
    $opt{uvXUformat} = '"lX"';
    $opt{uvoformat} = '"lo"';
    $opt{uvsize} = 4;
    $opt{uvtype} = 'unsigned long';
    $opt{uvuformat} = '"lu"';
    $opt{uvxformat} = '"lx"';
}

# change the s{GM|LOCAL}TIME_{min|max} for VS2005 (aka VC 8) and
# VS2008 (aka VC 9) or higher (presuming that later versions will have
# at least the range of that).
if ($opt{cc} =~ /\bcl/ and $opt{ccversion} =~ /^(\d+)/) {
    my $ccversion = $1;
    if ($ccversion >= 14) {
	$opt{sGMTIME_max} = 32535291599;
	$opt{sLOCALTIME_max} = 32535244799;
    }
    if($ccversion < 13) { #VC6
	$opt{ar} ='lib';
    }
}
#find out which MSVC this ICC is using
elsif ($opt{cc} =~ /\bicl/) {
    my $output = `cl --version 2>&1`;
    my $num_ver = $output =~ /^.*Version\s+([\d.]+)/ ? $1 : '?';
    if($num_ver =~ /^(\d+)/ && $1 >= 14) {
	$opt{sGMTIME_max} = 32535291599;
	$opt{sLOCALTIME_max} = 32535244799;
    }
    $opt{ar} ='xilib';
}

if ($opt{useithreads} eq 'define' && $opt{ccflags} =~ /-DPERL_IMPLICIT_SYS\b/) {
    $opt{d_pseudofork} = 'define';
}

#if the fields above are defined, they override the defaults in the premade
#config file
while (<>) {
    s/~([\w_]+)~/exists $opt{$1} ? $opt{$1} : ''/eg;
    if (/^([\w_]+)=(.*)$/) {
	my($k,$v) = ($1,$2);
	# this depends on cf_time being empty in the template (or we'll
	# get a loop)
	if ($k eq 'cf_time') {
	    $_ = "$k='" . localtime(time) . "'\n" if $v =~ /^\s*'\s*'/;
	}
	elsif (exists $opt{$k}) {
	    $_ = "$k='$opt{$k}'\n";
	}
    }
    print;
}
