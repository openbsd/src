use strict;
use Cwd;
use File::Path;
use File::Find;

my %opts = (
  #defaults
    'verbose' => 1, # verbose level, in range from 0 to 2
    'distdir' => 'distdir',
    'unicode' => 1, # include unicode by default
    'minimal' => 0, # minimal possible distribution.
                    # actually this is just perl.exe and perlXX.dll
		    # but can be extended by additional exts 
		    #  ... (as soon as this will be implemented :)
    'cross-name' => 'wince',
    'strip-pod' => 0, # strip POD from perl modules
    'adaptation' => 1, # do some adaptation, such as stripping such
                       # occurences as "if ($^O eq 'VMS'){...}" for Dynaloader.pm
    'zip' => 0,     # perform zip
    'clean-exts' => 0,
  #options itself
    (map {/^--([\-_\w]+)=(.*)$/} @ARGV),                            # --opt=smth
    (map {/^no-?(.*)$/i?($1=>0):($_=>1)} map {/^--([\-_\w]+)$/} @ARGV),  # --opt --no-opt --noopt
  );

# TODO
#   -- error checking. When something goes wrong, just exit with rc!=0
#   -- may be '--zip' option should be made differently?

my $cwd = cwd;

if ($opts{'clean-exts'}) {
  # unfortunately, unlike perl58.dll and like, extensions for different
  # platforms are built in same directory, therefore we must be able to clean
  # them often
  unlink '../config.sh'; # delete cache config file, which remembers our previous config
  chdir '../ext';
  find({no_chdir=>1,wanted => sub{
        unlink if /((?:\.obj|\/makefile|\/errno\.pm))$/i;
      }
    },'.');
  exit;
}

# zip
if ($opts{'zip'}) {
  if ($opts{'verbose'} >=1) {
    print STDERR "zipping...\n";
  }
  chdir $opts{'distdir'};
  unlink <*.zip>;
  `zip -R perl-$opts{'cross-name'} *`;
  exit;
}

my (%libexclusions, %extexclusions);
my @lfiles;
sub copy($$);

# lib
chdir '../lib';
find({no_chdir=>1,wanted=>sub{push @lfiles, $_ if /\.p[lm]$/}},'.');
chdir $cwd;
# exclusions
@lfiles = grep {!exists $libexclusions{$_}} @lfiles;
#inclusions
#...
#copy them
if ($opts{'verbose'} >=1) {
  print STDERR "Copying perl lib files...\n";
}
for (@lfiles) {
  /^(.*)\/[^\/]+$/;
  mkpath "$opts{distdir}/lib/$1";
  copy "../lib/$_", "$opts{distdir}/lib/$_";
}

#ext
my @efiles;
chdir '../ext';
find({no_chdir=>1,wanted=>sub{push @efiles, $_ if /\.pm$/}},'.');
chdir $cwd;
# exclusions
#...
#inclusions
#...
#copy them
#{s[/(\w+)/\1\.pm][/$1.pm]} @efiles;
if ($opts{'verbose'} >=1) {
  print STDERR "Copying perl core extensions...\n";
}
for (@efiles) {
  if (m#^.*?/lib/(.*)$#) {
    copy "../ext/$_", "$opts{distdir}/lib/$1";
  }
  else {
    /^(.*)\/([^\/]+)\/([^\/]+)$/;
    copy "../ext/$_", "$opts{distdir}/lib/$1/$3";
  }
}
my ($dynaloader_pm);
if ($opts{adaptation}) {
  # let's copy our Dynaloader.pm (make this optional?)
  open my $fhdyna, ">$opts{distdir}/lib/Dynaloader.pm";
  print $fhdyna $dynaloader_pm;
  close $fhdyna;
}

# Config.pm, perl binaries
if ($opts{'verbose'} >=1) {
  print STDERR "Copying Config.pm, perl.dll and perl.exe...\n";
}
copy "../xlib/$opts{'cross-name'}/Config.pm", "$opts{distdir}/lib/Config.pm";
copy "$opts{'cross-name'}/perl.exe", "$opts{distdir}/bin/perl.exe";
copy "$opts{'cross-name'}/perl.dll", "$opts{distdir}/bin/perl.dll";
# how do we know exact name of perl.dll?

# auto
my %aexcl = (socket=>'Socket_1');
# Socket.dll and may be some other conflict with same file in \windows dir
# on WinCE, %aexcl needed to replace it with a different name that however
# will be found by Dynaloader
my @afiles;
chdir "../xlib/$opts{'cross-name'}/auto";
find({no_chdir=>1,wanted=>sub{push @afiles, $_ if /\.(dll|bs)$/}},'.');
chdir $cwd;
if ($opts{'verbose'} >=1) {
  print STDERR "Copying binaries for perl core extensions...\n";
}
for (@afiles) {
  if (/^(.*)\/(\w+)\.dll$/i && exists $aexcl{lc($2)}) {
    copy "../xlib/$opts{'cross-name'}/auto/$_", "$opts{distdir}/lib/auto/$1/$aexcl{lc($2)}.dll";
  }
  else {
    copy "../xlib/$opts{'cross-name'}/auto/$_", "$opts{distdir}/lib/auto/$_";
  }
}

sub copy($$) {
  my ($fnfrom, $fnto) = @_;
  open my $fh, "<$fnfrom" or die "can not open $fnfrom: $!";
  binmode $fh;
  local $/;
  my $ffrom = <$fh>;
  if ($opts{'strip-pod'}) {
    # actually following regexp is suspicious to not work everywhere.
    # but we've checked on our set of modules, and it's fit for our purposes
    $ffrom =~ s/^=\w+.*?^=cut(?:\n|\Z)//msg;
    unless ($ffrom=~/\bAutoLoader\b/) {
      # this logic actually strip less than could be stripped, but we're
      # not risky. Just strip only of no mention of AutoLoader
      $ffrom =~ s/^__END__.*\Z//msg;
    }
  }
  mkpath $1 if $fnto=~/^(.*)\/([^\/]+)$/;
  open my $fhout, ">$fnto";
  binmode $fhout;
  print $fhout $ffrom;
  if ($opts{'verbose'} >=2) {
    print STDERR "copying $fnfrom=>$fnto\n";
  }
}

BEGIN {
%libexclusions = map {$_=>1} split/\s/, <<"EOS";
abbrev.pl bigfloat.pl bigint.pl bigrat.pl cacheout.pl complete.pl ctime.pl
dotsh.pl exceptions.pl fastcwd.pl flush.pl ftp.pl getcwd.pl getopt.pl
getopts.pl hostname.pl look.pl newgetopt.pl pwd.pl termcap.pl
EOS
%extexclusions = map {$_=>1} split/\s/, <<"EOS";
EOS
$dynaloader_pm=<<'EOS';
# This module designed *only* for WinCE
# if you encounter a problem with this file, try using original Dynaloader.pm
# from perl distribution, it's larger but essentially the same.
package DynaLoader;
our $VERSION = 1.04;

$dl_debug ||= 0;

@dl_require_symbols = ();       # names of symbols we need

#@dl_librefs = (); # things we have loaded
#@dl_modules = (); # Modules we have loaded

boot_DynaLoader('DynaLoader') if defined(&boot_DynaLoader) && !defined(&dl_error);

print STDERR "DynaLoader not linked into this perl\n"
  unless defined(&boot_DynaLoader);

1; # End of main code

sub croak{require Carp;Carp::croak(@_)}
sub bootstrap_inherit {
    my $module = $_[0];
    local *isa = *{"$module\::ISA"};
    local @isa = (@isa, 'DynaLoader');
    bootstrap(@_);
}
sub bootstrap {
    # use local vars to enable $module.bs script to edit values
    local(@args) = @_;
    local($module) = $args[0];
    local(@dirs, $file);

    unless ($module) {
	require Carp;
	Carp::confess("Usage: DynaLoader::bootstrap(module)");
    }

    croak("Can't load module $module, dynamic loading not available in this perl.\n")
	unless defined(&dl_load_file);

    my @modparts = split(/::/,$module);
    my $modfname = $modparts[-1];
    my $modpname = join('/',@modparts);

    for (@INC) {
	my $dir = "$_/auto/$modpname";
	next unless -d $dir;
	my $try = "$dir/$modfname.dll";
	last if $file = ( (-f $try) && $try);
	
	$try = "$dir/${modfname}_1.dll";
	last if $file = ( (-f $try) && $try);
	push @dirs, $dir;
    }
    $file = dl_findfile(map("-L$_",@dirs,@INC), $modfname) unless $file;

    croak("Can't locate loadable object for module $module in \@INC (\@INC contains: @INC)")
	unless $file;

    (my $bootname = "boot_$module") =~ s/\W/_/g;
    @dl_require_symbols = ($bootname);

    # optional '.bootstrap' perl script
    my $bs = $file;
    $bs =~ s/(\.\w+)?(;\d*)?$/\.bs/;
    if (-s $bs) { # only read file if it's not empty
        eval { do $bs; };
        warn "$bs: $@\n" if $@;
    }

    my $libref = dl_load_file($file, 0) or
	croak("Can't load '$file' for module $module: ".dl_error());

    push(@dl_librefs,$libref);  # record loaded object

    my @unresolved = dl_undef_symbols();
    if (@unresolved) {
	require Carp;
	Carp::carp("Undefined symbols present after loading $file: @unresolved\n");
    }

    my $boot_symbol_ref = dl_find_symbol($libref, $bootname) or
         croak("Can't find '$bootname' symbol in $file\n");

    push(@dl_modules, $module);

  boot:
    my $xs = dl_install_xsub("${module}::bootstrap", $boot_symbol_ref, $file);
    &$xs(@args);
}

sub dl_findfile {
    my (@args) = @_;
    my (@dirs,  $dir);
    my (@found);

    arg: foreach(@args) {
        if (m:/: && -f $_) {
	    push(@found,$_);
	    last arg unless wantarray;
	    next;
	}

        if (s:^-L::) {push(@dirs, $_); next;}
        if (m:/: && -d $_) {push(@dirs, $_); next;}

        for $dir (@dirs) {
            next unless -d $dir;
            for my $name (/\.dll$/i?($_):("$_.dll",$_)) {
                print STDERR " checking in $dir for $name\n" if $dl_debug;
        	if (-f "$dir/$name") {
                    push(@found, "$dir/$name");
                    next arg;
                }
            }
        }
    }
    return $found[0] unless wantarray;
    @found;
}
EOS
}

