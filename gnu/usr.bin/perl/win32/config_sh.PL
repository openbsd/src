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

FindExt::scan_ext("../ext");

my %opt;

my $optref = loadopts();
while (@{$optref} && $optref->[0] =~ /^([\w_]+)=(.*)$/) {
    $opt{$1}=$2;
    shift(@{$optref});
}

my @dynamic = FindExt::dynamic_extensions();
my @noxs    = FindExt::noxs_extensions();
my @known   = sort(@dynamic,split(/\s+/,$opt{'staticext'}),@noxs);
$opt{'known_extensions'} = join(' ',@known);

if (!$opt{'use5005threads'} || $opt{'use5005threads'} eq 'undef')
 {
  @dynamic = grep(!/Thread/,@dynamic);
  @known   = grep(!/Thread/,@dynamic);
 }

$opt{'dynamic_ext'} = join(' ',@dynamic);
$opt{'nonxs_ext'}   = join(' ',@noxs);

$opt{'extensions'} = join(' ',@known);

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
$opt{VERSION} = "$opt{PERL_REVISION}.$opt{PERL_VERSION}.$opt{PERL_SUBVERSION}";
$opt{INST_VER} =~ s|~VERSION~|$opt{VERSION}|g;
$opt{'version_patchlevel_string'} = "version $opt{PERL_VERSION} subversion $opt{PERL_SUBVERSION}";
$opt{'version_patchlevel_string'} .= " patchlevel $opt{PERL_PATCHLEVEL}" if exists $opt{PERL_PATCHLEVEL};

$opt{'cf_by'} = $ENV{USERNAME} unless $opt{'cf_by'};
$opt{'cf_email'} = $opt{'cf_by'} . '@' . (gethostbyname('localhost'))[0]
	unless $opt{'cf_email'};
$opt{'usemymalloc'} = 'y' if $opt{'d_mymalloc'} eq 'define';

$opt{libpth} = mungepath($opt{libpth}) if exists $opt{libpth};
$opt{incpath} = mungepath($opt{incpath}) if exists $opt{incpath};

# some functions are not available on Win9x
if (defined(&Win32::IsWin95) && Win32::IsWin95()) {
    $opt{d_flock} = 'undef';
    $opt{d_link} = 'undef';
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
