#   MM_MacOS.pm
#   MakeMaker default methods for MacOS
#   This package is inserted into @ISA of MakeMaker's MM before the
#   built-in ExtUtils::MM_Unix methods if MakeMaker.pm is run under MacOS.
#
#   Author:  Matthias Neeracher <neeracher@mac.com>
#   Maintainer: Chris Nandor <pudge@pobox.com>

package ExtUtils::MM_MacOS;
require ExtUtils::MM_Any;
require ExtUtils::MM_Unix;
@ISA = qw( ExtUtils::MM_Any ExtUtils::MM_Unix );

use vars qw($VERSION);
$VERSION = '1.07';

use Config;
use Cwd 'cwd';
require Exporter;
use File::Basename;
use vars qw(%make_data);

my $Mac_FS = eval { require Mac::FileSpec::Unixish };

use ExtUtils::MakeMaker qw($Verbose &neatvalue);

=head1 NAME

ExtUtils::MM_MacOS - methods to override UN*X behaviour in ExtUtils::MakeMaker

=head1 SYNOPSIS

 use ExtUtils::MM_MacOS; # Done internally by ExtUtils::MakeMaker if needed

=head1 DESCRIPTION

MM_MacOS currently only produces an approximation to the correct Makefile.

=over 4

=cut

sub new {
    my($class,$self) = @_;
    my($key);
    my($cwd) = cwd();

    print STDOUT "Mac MakeMaker (v$ExtUtils::MakeMaker::VERSION)\n" if $Verbose;
    if (-f "MANIFEST" && ! -f "Makefile.mk"){
	ExtUtils::MakeMaker::check_manifest();
    }

    mkdir("Obj", 0777) unless -d "Obj";

    $self = {} unless defined $self;

    check_hints($self);

    my(%initial_att) = %$self; # record initial attributes

    if (defined $self->{CONFIGURE}) {
	if (ref $self->{CONFIGURE} eq 'CODE') {
	    $self = { %$self, %{&{$self->{CONFIGURE}}}};
	} else {
            require Carp;
	    Carp::croak("Attribute 'CONFIGURE' to WriteMakefile() not a code reference\n");
	}
    }

    my $newclass = ++$ExtUtils::MakeMaker::PACKNAME;
    local @ExtUtils::MakeMaker::Parent = @ExtUtils::MakeMaker::Parent;    # Protect against non-local exits
    {
        no strict 'refs';
        print "Blessing Object into class [$newclass]\n" if $Verbose>=2;
        ExtUtils::MakeMaker::mv_all_methods("MY",$newclass);
        bless $self, $newclass;
        push @Parent, $self;
        require ExtUtils::MY;
        @{"$newclass\:\:ISA"} = 'MM';
    }

    $ExtUtils::MakeMaker::Recognized_Att_Keys{$_} = 1
      for map { $_ . 'Optimize' } qw(MWC MWCPPC MWC68K MPW MRC MRC SC);

    if (defined $ExtUtils::MakeMaker::Parent[-2]){
        $self->{PARENT} = $ExtUtils::MakeMaker::Parent[-2];
        my $key;
        for $key (@ExtUtils::MakeMaker::Prepend_parent) {
            next unless defined $self->{PARENT}{$key};
            $self->{$key} = $self->{PARENT}{$key};
            if ($key !~ /PERL$/) {
                $self->{$key} = $self->catdir("..",$self->{$key})
                  unless $self->file_name_is_absolute($self->{$key});
            } else {
                # PERL or FULLPERL will be a command verb or even a
                # command with an argument instead of a full file
                # specification under VMS.  So, don't turn the command
                # into a filespec, but do add a level to the path of
                # the argument if not already absolute.
                my @cmd = split /\s+/, $self->{$key};
                $cmd[1] = $self->catfile('[-]',$cmd[1])
                  unless (@cmd < 2) || $self->file_name_is_absolute($cmd[1]);
                $self->{$key} = join(' ', @cmd);
            }
        }
        if ($self->{PARENT}) {
            $self->{PARENT}->{CHILDREN}->{$newclass} = $self;
            foreach my $opt (qw(POLLUTE PERL_CORE)) {
                if (exists $self->{PARENT}->{$opt}
                    and not exists $self->{$opt})
                    {
                        # inherit, but only if already unspecified
                        $self->{$opt} = $self->{PARENT}->{$opt};
                    }
            }
        }
        my @fm = grep /^FIRST_MAKEFILE=/, @ARGV;
        $self->parse_args(@fm) if @fm;
    } else {
        $self->parse_args(split(' ', $ENV{PERL_MM_OPT} || ''),@ARGV);
    }

    $self->{NAME} ||= $self->guess_name;

    ($self->{NAME_SYM} = $self->{NAME}) =~ s/\W+/_/g;

    $self->init_main();
    $self->init_dirscan();
    $self->init_others();

    push @{$self->{RESULT}}, <<END;
# This Makefile is for the $self->{NAME} extension to perl.
#
# It was generated automatically by MakeMaker version
# $ExtUtils::MakeMaker::VERSION (Revision: $ExtUtils::MakeMaker::Revision) from the contents of
# Makefile.PL. Don't edit this file, edit Makefile.PL instead.
#
#	ANY CHANGES MADE HERE WILL BE LOST!
#
#   MakeMaker Parameters:
END

    foreach $key (sort keys %initial_att){
	my($v) = neatvalue($initial_att{$key});
	$v =~ s/(CODE|HASH|ARRAY|SCALAR)\([\dxa-f]+\)/$1\(...\)/;
	$v =~ tr/\n/ /s;
	push @{$self->{RESULT}}, "#	$key => $v";
    }

    # turn the SKIP array into a SKIPHASH hash
    my (%skip,$skip);
    for $skip (@{$self->{SKIP} || []}) {
	$self->{SKIPHASH}{$skip} = 1;
    }
    delete $self->{SKIP}; # free memory

    # We skip many sections for MacOS, but we don't say anything about it in the Makefile
    for (qw/ const_config tool_autosplit
	    tool_xsubpp tools_other dist macro depend post_constants
	    pasthru c_o xs_c xs_o top_targets linkext 
	    dynamic_bs dynamic_lib static_lib manifypods
	    installbin subdirs dist_basics dist_core
	    distdir dist_test dist_ci install force perldepend makefile
	    staticmake test pm_to_blib selfdocument
	    const_loadlibs const_cccmd
    /)
    {
	$self->{SKIPHASH}{$_} = 2;
    }
    push @ExtUtils::MakeMaker::MM_Sections, "rulez" 
    	unless grep /rulez/, @ExtUtils::MakeMaker::MM_Sections;

    if ($self->{PARENT}) {
	for (qw/install dist dist_basics dist_core distdir dist_test dist_ci/) {
	    $self->{SKIPHASH}{$_} = 1;
	}
    }

    # We run all the subdirectories now. They don't have much to query
    # from the parent, but the parent has to query them: if they need linking!
    unless ($self->{NORECURS}) {
	$self->eval_in_subdirs if @{$self->{DIR}};
    }

    my $section;
    foreach $section ( @ExtUtils::MakeMaker::MM_Sections ){
    	next if defined $self->{SKIPHASH}{$section} &&
                $self->{SKIPHASH}{$section} == 2;
	print "Processing Makefile '$section' section\n" if ($Verbose >= 2);
	$self->{ABSTRACT_FROM} = macify($self->{ABSTRACT_FROM})
		if $self->{ABSTRACT_FROM};
	my($skipit) = $self->skipcheck($section);
	if ($skipit){
	    push @{$self->{RESULT}}, "\n# --- MakeMaker $section section $skipit.";
	} else {
	    my(%a) = %{$self->{$section} || {}};
	    push @{$self->{RESULT}}, "\n# --- MakeMaker $section section:";
	    push @{$self->{RESULT}}, "# " . join ", ", %a if $Verbose && %a;
	    push @{$self->{RESULT}}, $self->nicetext($self->$section( %a ));
	}
    }

    push @{$self->{RESULT}}, "\n# End.";
    pop @Parent;

    $ExtUtils::MM_MacOS::make_data{$cwd} = $self;
    $self;
}

sub skipcheck {
    my($self) = shift;
    my($section) = @_;
    return 'skipped' if $self->{SKIPHASH}{$section};
    return '';
}

=item maybe_command

Returns true, if the argument is likely to be a command.

=cut

sub maybe_command {
    my($self,$file) = @_;
    return $file if ! -d $file;
    return;
}

=item guess_name

Guess the name of this package by examining the working directory's
name. MakeMaker calls this only if the developer has not supplied a
NAME attribute.

=cut

sub guess_name {
    my($self) = @_;
    my $name = cwd();
    $name =~ s/.*:// unless ($name =~ s/^.*:ext://);
    $name =~ s#:#::#g;
    $name =~  s#[\-_][\d.\-]+$##;  # this is new with MM 5.00
    $name;
}

=item macify

Translate relative Unix filepaths into Mac names.

=cut

sub macify {
    my($unix) = @_;
    my(@mac);

    foreach (split(/[ \t\n]+/, $unix)) {
	if (m|/|) {
	    if ($Mac_FS) { # should always be true
		$_ = Mac::FileSpec::Unixish::nativize($_);
	    } else {
		s|^\./||;
		s|/|:|g;
		$_ = ":$_";
	    }
	}
	push(@mac, $_);
    }

    return "@mac";
}

=item patternify

Translate Unix filepaths and shell globs to Mac style.

=cut

sub patternify {
    my($unix) = @_;
    my(@mac);
    use ExtUtils::MakeMaker::bytes; # Non-UTF-8 high bytes below.

    foreach (split(/[ \t\n]+/, $unix)) {
	if (m|/|) {
	    $_ = ":$_";
	    s|/|:|g;
	    s|\*|�|g;
	    $_ = "'$_'" if /[?�]/;
	    push(@mac, $_);
	}
    }

    return "@mac";
}

=item init_main

Initializes some of NAME, FULLEXT, BASEEXT, DLBASE, PERL_SRC,
PERL_LIB, PERL_ARCHLIB, PERL_INC, INSTALLDIRS, INST_*, INSTALL*,
PREFIX, CONFIG, AR, AR_STATIC_ARGS, LD, OBJ_EXT, LIB_EXT, MAP_TARGET,
LIBPERL_A, VERSION_FROM, VERSION, DISTNAME, VERSION_SYM.

=cut

sub init_main {
    my($self) = @_;

    # --- Initialize Module Name and Paths

    # NAME    = The perl module name for this extension (eg DBD::Oracle).
    # FULLEXT = Pathname for extension directory (eg DBD/Oracle).
    # BASEEXT = Basename part of FULLEXT. May be just equal FULLEXT.
    ($self->{FULLEXT} =
     $self->{NAME}) =~ s!::!:!g ;		     #eg. BSD:Foo:Socket
    ($self->{BASEEXT} =
     $self->{NAME}) =~ s!.*::!! ;		             #eg. Socket

    # --- Initialize PERL_LIB, INST_LIB, PERL_SRC

    # *Real* information: where did we get these two from? ...
    my $inc_config_dir = dirname($INC{'Config.pm'});
    my $inc_carp_dir   = dirname($INC{'Carp.pm'});

    unless ($self->{PERL_SRC}){
	my($dir);
	foreach $dir (qw(:: ::: :::: ::::: ::::::)){
	    if (-f "${dir}perl.h") {
		$self->{PERL_SRC}=$dir ;
		last;
	    }
	}
	if (!$self->{PERL_SRC} && -f "$ENV{MACPERL}CORE:perl:perl.h") {
	    # Mac pathnames may be very nasty, so we'll install symlinks
	    unlink(":PerlCore", ":PerlLib");
	    symlink("$ENV{MACPERL}CORE:", "PerlCore");
	    symlink("$ENV{MACPERL}lib:", "PerlLib");
	    $self->{PERL_SRC} = ":PerlCore:perl:" ;
	    $self->{PERL_LIB} = ":PerlLib:";
	}
    }
    if ($self->{PERL_SRC}){
	$self->{PERL_LIB}     ||= $self->catdir("$self->{PERL_SRC}","lib");
	$self->{PERL_ARCHLIB} = $self->{PERL_LIB};
	$self->{PERL_INC}     = $self->{PERL_SRC};
    } else {
# hmmmmmmm ... ?
        $self->{PERL_LIB}    ||= "$ENV{MACPERL}site_perl";
	$self->{PERL_ARCHLIB} =  $self->{PERL_LIB};
	$self->{PERL_INC}     =  $ENV{MACPERL};
        $self->{PERL_SRC}     = '';
    }

    $self->{INSTALLDIRS} = "perl";
    $self->{INST_LIB} = $self->{INST_ARCHLIB} = $self->{PERL_LIB};
    $self->{INST_MAN1DIR} = $self->{INSTALLMAN1DIR} = "none";
    $self->{MAN1EXT} ||= $Config::Config{man1ext};
    $self->{INST_MAN3DIR} = $self->{INSTALLMAN3DIR} = "none";
    $self->{MAN3EXT} ||= $Config::Config{man3ext};
    $self->{MAP_TARGET} ||= "perl";

    # make a simple check if we find Exporter
    # hm ... do we really care?  at all?
#    warn "Warning: PERL_LIB ($self->{PERL_LIB}) seems not to be a perl library directory
#        (Exporter.pm not found)"
#	unless -f $self->catfile("$self->{PERL_LIB}","Exporter.pm") ||
#        $self->{NAME} eq "ExtUtils::MakeMaker";

    # Determine VERSION and VERSION_FROM
    ($self->{DISTNAME}=$self->{NAME}) =~ s#(::)#-#g unless $self->{DISTNAME};
    if ($self->{VERSION_FROM}){
        # XXX replace with parse_version() override
	local *FH;
	open(FH,macify($self->{VERSION_FROM})) or
	    die "Could not open '$self->{VERSION_FROM}' (attribute VERSION_FROM): $!";
	while (<FH>) {
	    chop;
	    next unless /\$([\w:]*\bVERSION)\b.*=/;
	    local $ExtUtils::MakeMaker::module_version_variable = $1;
	    my($eval) = "$_;";
	    eval $eval;
	    die "Could not eval '$eval': $@" if $@;
	    if ($self->{VERSION} = $ {$ExtUtils::MakeMaker::module_version_variable}){
		print "$self->{NAME} VERSION is $self->{VERSION} (from $self->{VERSION_FROM})\n" if $Verbose;
	    } else {
		# XXX this should probably croak
		print "WARNING: Setting VERSION via file '$self->{VERSION_FROM}' failed\n";
	    }
	    last;
	}
	close FH;
    }

    if ($self->{VERSION}) {
	$self->{VERSION} =~ s/^\s+//;
	$self->{VERSION} =~ s/\s+$//;
    }

    $self->{VERSION} = "0.10" unless $self->{VERSION};
    ($self->{VERSION_SYM} = $self->{VERSION}) =~ s/\W/_/g;


    # Graham Barr and Paul Marquess had some ideas how to ensure
    # version compatibility between the *.pm file and the
    # corresponding *.xs file. The bottomline was, that we need an
    # XS_VERSION macro that defaults to VERSION:
    $self->{XS_VERSION} ||= $self->{VERSION};


    $self->{DEFINE} .= " \$(XS_DEFINE_VERSION) \$(DEFINE_VERSION)";

    # Preprocessor definitions may be useful
    $self->{DEFINE} =~ s/-D/-d /g; 

    # UN*X includes probably are not useful
    $self->{DEFINE} =~ s/-I\S+/_include($1)/eg;


    if ($self->{INC}) {
        # UN*X includes probably are not useful
    	$self->{INC} =~ s/-I(\S+)/_include($1)/eg;
    }


    # --- Initialize Perl Binary Locations

    # Find Perl 5. The only contract here is that both 'PERL' and 'FULLPERL'
    # will be working versions of perl 5. miniperl has priority over perl
    # for PERL to ensure that $(PERL) is usable while building ./ext/*
    my ($component,@defpath);
    foreach $component ($self->{PERL_SRC}, $self->path(), $Config::Config{binexp}) {
	push @defpath, $component if defined $component;
    }
    $self->{PERL} = "$self->{PERL_SRC}miniperl";
}

=item init_others

Initializes LDLOADLIBS, LIBS

=cut

sub init_others {	# --- Initialize Other Attributes
    my($self) = shift;

    if ( !$self->{OBJECT} ) {
	# init_dirscan should have found out, if we have C files
	$self->{OBJECT} = "";
	$self->{OBJECT} = "$self->{BASEEXT}.c" if @{$self->{C}||[]};
    } else {
    	$self->{OBJECT} =~ s/\$\(O_FILES\)/@{$self->{C}||[]}/;
    }
    my($src);
    foreach (split(/[ \t\n]+/, $self->{OBJECT})) {
    	if (/^$self->{BASEEXT}\.o(bj)?$/) {
	    $src .= " $self->{BASEEXT}.c";
	} elsif (/^(.*\..*)\.o$/) {
	    $src .= " $1";
	} elsif (/^(.*)(\.o(bj)?|\$\(OBJ_EXT\))$/) {
	    if (-f "$1.cp") {
	    	$src .= " $1.cp";
	    } else {
	    	$src .= " $1.c";
	    }
	} else {
	    $src .= " $_";
	}
    }
    $self->{SOURCE} = $src;
    $self->{FULLPERL} = "$self->{PERL_SRC}perl";
    $self->{MAKEFILE}       = "Makefile.mk";
    $self->{FIRST_MAKEFILE} = $self->{MAKEFILE};
    $self->{MAKEFILE_OLD}   = $self->{MAKEFILE}.'.old';

    $self->{'DEV_NULL'} ||= ' \xB3 Dev:Null';

    return 1;
}

=item init_platform

Add MACPERL_SRC MACPERL_LIB

=item platform_constants

Add MACPERL_SRC MACPERL_LIB MACLIBS_68K MACLIBS_PPC MACLIBS_SC MACLIBS_MRC
MACLIBS_ALL_68K MACLIBS_ALL_PPC MACLIBS_SHARED

XXX Few are initialized.  How many of these are ever used?

=cut

sub init_platform {
    my $self = shift;

    $self->{MACPERL_SRC}  = $self->catdir("$self->{PERL_SRC}","macos:");
    $self->{MACPERL_LIB}  ||= $self->catdir("$self->{MACPERL_SRC}","lib");
    $self->{MACPERL_INC}  = $self->{MACPERL_SRC};
}



sub platform_constants {
    my $self = shift;

    foreach my $macro (qw(MACPERL_SRC MACPERL_LIB MACLIBS_68K MACLIBS_PPC 
                          MACLIBS_SC  MACLIBS_MRC MACLIBS_ALL_68K 
                          MACLIBS_ALL_PPC MACLIBS_SHARED))
    {
        next unless defined $self->{$macro};
        $make_frag .= "$macro = $self->{$macro}\n";
    }

    return $make_frag;
}


=item init_dirscan

Initializes DIR, XS, PM, C, O_FILES, H, PL_FILES, MAN*PODS, EXE_FILES.

=cut

sub init_dirscan {	# --- File and Directory Lists (.xs .pm .pod etc)
    my($self) = @_;
    my($name, %dir, %xs, %c, %h, %ignore, %pl_files, %manifypods);
    local(%pm); #the sub in find() has to see this hash

    # in case we don't find it below!
    if ($self->{VERSION_FROM}) {
        my $version_from = macify($self->{VERSION_FROM});
        $pm{$version_from} = $self->catfile('$(INST_LIBDIR)',
            $version_from);
    }

    $ignore{'test.pl'} = 1;
    foreach $name ($self->lsdir(":")){
	next if ($name =~ /^\./ or $ignore{$name});
	next unless $self->libscan($name);
	if (-d $name){
            next if $self->{NORECURS};
	    $dir{$name} = $name if (-f ":$name:Makefile.PL");
	} elsif ($name =~ /\.xs$/){
	    my($c); ($c = $name) =~ s/\.xs$/.c/;
	    $xs{$name} = $c;
	    $c{$c} = 1;
	} elsif ($name =~ /\.c(p|pp|xx|c)?$/i){  # .c .C .cpp .cxx .cc .cp
	    $c{$name} = 1
		unless $name =~ m/perlmain\.c/; # See MAP_TARGET
	} elsif ($name =~ /\.h$/i){
	    $h{$name} = 1;
	} elsif ($name =~ /\.(p[ml]|pod)$/){
	    $pm{$name} = $self->catfile('$(INST_LIBDIR)',$name);
	} elsif ($name =~ /\.PL$/ && $name ne "Makefile.PL") {
	    ($pl_files{$name} = $name) =~ s/\.PL$// ;
	}
    }

    # Some larger extensions often wish to install a number of *.pm/pl
    # files into the library in various locations.

    # The attribute PMLIBDIRS holds an array reference which lists
    # subdirectories which we should search for library files to
    # install. PMLIBDIRS defaults to [ 'lib', $self->{BASEEXT} ].  We
    # recursively search through the named directories (skipping any
    # which don't exist or contain Makefile.PL files).

    # For each *.pm or *.pl file found $self->libscan() is called with
    # the default installation path in $_[1]. The return value of
    # libscan defines the actual installation location.  The default
    # libscan function simply returns the path.  The file is skipped
    # if libscan returns false.

    # The default installation location passed to libscan in $_[1] is:
    #
    #  ./*.pm		=> $(INST_LIBDIR)/*.pm
    #  ./xyz/...	=> $(INST_LIBDIR)/xyz/...
    #  ./lib/...	=> $(INST_LIB)/...
    #
    # In this way the 'lib' directory is seen as the root of the actual
    # perl library whereas the others are relative to INST_LIBDIR
    # This is a subtle distinction but one that's important for nested 
    # modules.

    $self->{PMLIBDIRS} = ['lib', $self->{BASEEXT}]
	unless $self->{PMLIBDIRS};

    #only existing directories that aren't in $dir are allowed

    my (@pmlibdirs) = map { macify ($_) } @{$self->{PMLIBDIRS}};
    my ($pmlibdir);
    @{$self->{PMLIBDIRS}} = ();
    foreach $pmlibdir (@pmlibdirs) {
	-d $pmlibdir && !$dir{$pmlibdir} && push @{$self->{PMLIBDIRS}}, $pmlibdir;
    }

    if (@{$self->{PMLIBDIRS}}){
	print "Searching PMLIBDIRS: @{$self->{PMLIBDIRS}}\n"
	    if ($Verbose >= 2);
	require File::Find;
	File::Find::find(sub {
	    if (-d $_){
	        unless ($self->libscan($_)){
		    $File::Find::prune = 1;
		}
		return;
	    }
	    my($path, $prefix) = ($File::Find::name, '$(INST_LIBDIR)');
	    my($striplibpath,$striplibname);
	    $prefix =  '$(INST_LIB)' if (($striplibpath = $path) =~ s:^(\W*)lib\W:$1:);
	    ($striplibname,$striplibpath) = fileparse($striplibpath);
	    my($inst) = $self->catfile($prefix,$striplibpath,$striplibname);
	    local($_) = $inst; # for backwards compatibility
	    $inst = $self->libscan($inst);
	    print "libscan($path) => '$inst'\n" if ($Verbose >= 2);
	    return unless $inst;
	    $pm{$path} = $inst;
	}, @{$self->{PMLIBDIRS}});
    }

    $self->{DIR} = [sort keys %dir] unless $self->{DIR};
    $self->{XS}  = \%xs             unless $self->{XS};
    $self->{PM}  = \%pm             unless $self->{PM};
    $self->{C}   = [sort keys %c]   unless $self->{C};
    $self->{H}   = [sort keys %h]   unless $self->{H};
    $self->{PL_FILES} = \%pl_files unless $self->{PL_FILES};

    # Set up names of manual pages to generate from pods
    unless ($self->{MAN1PODS}) {
    	$self->{MAN1PODS} = {};
    }
    unless ($self->{MAN3PODS}) {
    	$self->{MAN3PODS} = {};
    }
}


=item init_VERSION (o)

Change DEFINE_VERSION and XS_DEFINE_VERSION

=cut

sub init_VERSION {
    my $self = shift;

    $self->SUPER::init_VERSION;

    $self->{DEFINE_VERSION}    = '-d $(VERSION_MACRO)="�"$(VERSION)�""';
    $self->{XS_DEFINE_VERSION} = '-d $(XS_VERSION_MACRO)="�"$(XS_VERSION)�""';
}


=item special_targets (o)

Add .INCLUDE

=cut

sub special_targets {
    my $self = shift;

    my $make_frag = $self->SUPER::special_targets;

    return $make_frag . <<'MAKE_FRAG';
.INCLUDE : $(MACPERL_SRC)BuildRules.mk $(MACPERL_SRC)ExtBuildRules.mk

MAKE_FRAG

}

=item static (o)

Defines the static target.

=cut

sub static {
# --- Static Loading Sections ---

    my($self) = shift;
    my($extlib) = $self->{MYEXTLIB} ? "\nstatic :: myextlib\n" : "";
    '
all :: static

install :: do_install_static

install_static :: do_install_static
' . $extlib;
}

=item dlsyms (o)

Used by MacOS to define DL_FUNCS and DL_VARS and write the *.exp
files.

=cut

sub dlsyms {
    my($self,%attribs) = @_;

    return '' unless !$self->{SKIPHASH}{'dynamic'};

    my($funcs) = $attribs{DL_FUNCS} || $self->{DL_FUNCS} || {};
    my($vars)  = $attribs{DL_VARS} || $self->{DL_VARS} || [];
    my(@m);

    push(@m,"
dynamic :: $self->{BASEEXT}.exp

") unless $self->{SKIPHASH}{'dynamic'};

    my($extlib) = $self->{MYEXTLIB} ? " myextlib" : "";

    push(@m,"
$self->{BASEEXT}.exp: Makefile.PL$extlib
", qq[\t\$(PERL) "-I\$(PERL_LIB)" -e 'use ExtUtils::Mksymlists; ],
        'Mksymlists("NAME" => "',$self->{NAME},'", "DL_FUNCS" => ',
	neatvalue($funcs),', "DL_VARS" => ', neatvalue($vars), ');\'
');

    join('',@m);
}

=item dynamic (o)

Defines the dynamic target.

=cut

sub dynamic {
# --- dynamic Loading Sections ---

    my($self) = shift;
    '
all :: dynamic

install :: do_install_dynamic

install_dynamic :: do_install_dynamic
';
}


=item clean (o)

Defines the clean target.

=cut

sub clean {
# --- Cleanup and Distribution Sections ---

    my($self, %attribs) = @_;
    my(@m,$dir);
    push(@m, '
# Delete temporary files but do not touch installed files. We don\'t delete
# the Makefile here so a later make realclean still has a makefile to use.

clean :: clean_subdirs
');

    my(@otherfiles) = values %{$self->{XS}}; # .c files from *.xs files
    push(@otherfiles, patternify($attribs{FILES})) if $attribs{FILES};
    push @m, "\t\$(RM_RF) @otherfiles\n";
    # See realclean and ext/utils/make_ext for usage of Makefile.old
    push(@m,
	 "\t\$(MV) \$(FIRST_MAKEFILE) \$(MAKEFILE_OLD)\n");
    push(@m,
	 "\t$attribs{POSTOP}\n")   if $attribs{POSTOP};
    join("", @m);
}

=item clean_subdirs_target

MacOS semantics for changing directories and checking for existence
very different than everyone else.

=cut

sub clean_subdirs_target {
    my($self) = shift;

    # No subdirectories, no cleaning.
    return <<'NOOP_FRAG' unless @{$self->{DIR}};
clean_subdirs :
	$(NOECHO)$(NOOP)
NOOP_FRAG


    my $clean = "clean_subdirs :\n";

    for my $dir (@{$self->{DIR}}) {
        $clean .= sprintf <<'MAKE_FRAG', $dir;
	Set OldEcho {Echo}
	Set Echo 0
	Directory %s
	If "`Exists -f $(FIRST_MAKEFILE)`" != ""
	    $(MAKE) clean
	End
	Set Echo {OldEcho}
	
MAKE_FRAG
    }

    return $clean;
}


=item realclean (o)

Defines the realclean target.

=cut

sub realclean {
    my($self, %attribs) = @_;
    my(@m);
    push(@m,'
# Delete temporary files (via clean) and also delete installed files
realclean purge ::  clean
');

    my(@otherfiles) = ('$(FIRST_MAKEFILE)', '$(MAKEFILE_OLD)'); # Makefiles last
    push(@otherfiles, patternify($attribs{FILES})) if $attribs{FILES};
    push(@m, "\t\$(RM_RF) @otherfiles\n") if @otherfiles;
    push(@m, "\t$attribs{POSTOP}\n")       if $attribs{POSTOP};
    join("", @m);
}


=item realclean_subdirs_target

MacOS semantics for changing directories and checking for existence
very different than everyone else.

=cut

sub realclean_subdirs_target {
    my $self = shift;

    return <<'NOOP_FRAG' unless @{$self->{DIR}};
realclean_subdirs :
	$(NOECHO)$(NOOP)
NOOP_FRAG

    my $rclean = "realclean_subdirs :\n";

    foreach my $dir (@{$self->{DIR}}){
        $rclean .= sprintf <<'RCLEAN', $dir, 
	Set OldEcho \{Echo\}
	Set Echo 0
	Directory %s
	If \"\`Exists -f $(FIRST_MAKEFILE)\`\" != \"\"
	    \$(MAKE) realclean
	End
	Set Echo \{OldEcho\}

RCLEAN

    }

    return $rclean;
}


=item rulez (o)

=cut

sub rulez {
    my($self) = shift;
    qq'
install install_static install_dynamic :: 
\t\$(MACPERL_SRC)PerlInstall -l \$(PERL_LIB)

.INCLUDE : \$(MACPERL_SRC)BulkBuildRules.mk
';
}


=item processPL (o)

Defines targets to run *.PL files.

=cut

sub processPL {
    my($self) = shift;
    return "" unless $self->{PL_FILES};
    my(@m, $plfile);
    foreach $plfile (sort keys %{$self->{PL_FILES}}) {
        my $list = ref($self->{PL_FILES}->{$plfile})
		? $self->{PL_FILES}->{$plfile}
		: [$self->{PL_FILES}->{$plfile}];
	foreach $target (@$list) {
	push @m, "
ProcessPL :: $target
\t$(NOECHO)\$(NOOP)

$target :: $plfile
\t\$(PERL) -I\$(MACPERL_LIB) -I\$(PERL_LIB) $plfile $target
";
	}
    }
    join "", @m;
}

sub cflags {
    my($self,$libperl) = @_;
    my $optimize = '';

    for (map { $_ . "Optimize" } qw(MWC MWCPPC MWC68K MPW MRC MRC SC)) {
        $optimize .= "$_ = $self->{$_}" if exists $self->{$_};
    }

    return $self->{CFLAGS} = $optimize;
}


sub _include {  # for Unix-style includes, with -I instead of -i
	my($inc) = @_;
	require File::Spec::Unix;

	# allow only relative paths
	if (File::Spec::Unix->file_name_is_absolute($inc)) {
		return '';
	} else {
		return '-i ' . macify($inc);
	}
}

=item os_flavor

MacOS Classic is MacOS and MacOS Classic.

=cut

sub os_flavor {
    return('MacOS', 'MacOS Classic');
}

=back

=cut

1;
