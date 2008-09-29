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
	local (*F);
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

FindExt::scan_ext("../ext");
FindExt::scan_ext("ext") if -d 'ext';  # also look in win32/ext/ if it exists
FindExt::set_static_extensions(split ' ', $opt{'static_ext'});

$opt{'nonxs_ext'}        = join(' ',FindExt::nonxs_ext()) || ' ';
$opt{'static_ext'}       = join(' ',FindExt::static_ext()) || ' ';
$opt{'dynamic_ext'}      = join(' ',FindExt::dynamic_ext()) || ' ';
$opt{'extensions'}       = join(' ',FindExt::extensions()) || ' ';
$opt{'known_extensions'} = join(' ',FindExt::known_extensions()) || ' ';

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

$opt{VERSION} = "$opt{PERL_REVISION}.$opt{PERL_VERSION}.$opt{PERL_SUBVERSION}";
$opt{INST_VER} =~ s|~VERSION~|$opt{VERSION}|g;
$opt{'version_patchlevel_string'} = "version $opt{PERL_VERSION} subversion $opt{PERL_SUBVERSION}";
$opt{'version_patchlevel_string'} .= " patchlevel $opt{PERL_PATCHLEVEL}" if exists $opt{PERL_PATCHLEVEL};

my $ver = `ver 2>nul`;
if ($ver =~ /Version (\d+\.\d+)/) {
    $opt{'osvers'} = $1;
}
else {
    $opt{'osvers'} = '4.0';
}

if (exists $opt{cc}) {
    # cl and bcc32 version detection borrowed from Test::Smoke's configsmoke.pl
    if ($opt{cc} eq 'cl') {
        my $output = `cl --version 2>&1`;
        $opt{ccversion} = $output =~ /^.*Version\s+([\d.]+)/ ? $1 : '?';
    }
    elsif ($opt{cc} eq 'bcc32') {
        my $output = `bcc32 --version 2>&1`;
        $opt{ccversion} = $output =~ /([\d.]+)/ ? $1 : '?';
    }
    elsif ($opt{cc} eq 'gcc') {
        chomp($opt{gccversion} = `gcc -dumpversion`);
    }
}

$opt{'cf_by'} = $ENV{USERNAME} unless $opt{'cf_by'};
$opt{'cf_email'} = $opt{'cf_by'} . '@' . (gethostbyname('localhost'))[0]
	unless $opt{'cf_email'};
$opt{'usemymalloc'} = 'y' if $opt{'d_mymalloc'} eq 'define';

$opt{libpth} = mungepath($opt{libpth}) if exists $opt{libpth};
$opt{incpath} = mungepath($opt{incpath}) if exists $opt{incpath};

# some functions are not available on Win9x
unless (defined $ENV{SYSTEMROOT}) { # SystemRoot has been introduced by WinNT
    $opt{d_flock} = 'undef';
    $opt{d_link} = 'undef';
}

if ($opt{uselargefiles} ne 'define') {
    $opt{lseeksize} = 4;
    $opt{lseektype} = 'off_t';
}

if ($opt{useithreads} eq 'define' && $opt{ccflags} =~ /-DPERL_IMPLICIT_SYS\b/) {
    $opt{d_pseudofork} = 'define';
}

while (<>) {
    s/~([\w_]+)~/$opt{$1}/g;
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
