package ExtUtils::MM_NW5;

=head1 NAME

ExtUtils::MM_NW5 - methods to override UN*X behaviour in ExtUtils::MakeMaker

=head1 SYNOPSIS

 use ExtUtils::MM_NW5; # Done internally by ExtUtils::MakeMaker if needed

=head1 DESCRIPTION

See ExtUtils::MM_Unix for a documentation of the methods provided
there. This package overrides the implementation of these methods, not
the semantics.

=over

=cut 

use strict;
use Config;
use File::Basename;

use vars qw(@ISA $VERSION);
$VERSION = '2.05';

require ExtUtils::MM_Win32;
@ISA = qw(ExtUtils::MM_Win32);

use ExtUtils::MakeMaker qw( &neatvalue );

$ENV{EMXSHELL} = 'sh'; # to run `commands`

my $BORLAND  = 1 if $Config{'cc'} =~ /^bcc/i;
my $GCC      = 1 if $Config{'cc'} =~ /^gcc/i;
my $DMAKE    = 1 if $Config{'make'} =~ /^dmake/i;


sub init_others {
    my ($self) = @_;
    $self->SUPER::init_others(@_);

    # incpath is copied to makefile var INCLUDE in constants sub, here just 
    # make it empty
    my $libpth = $Config{'libpth'};
    $libpth =~ s( )(;);
    $self->{'LIBPTH'} = $libpth;
    $self->{'BASE_IMPORT'} = $Config{'base_import'};
 
    # Additional import file specified from Makefile.pl
    if($self->{'base_import'}) {
        $self->{'BASE_IMPORT'} .= ', ' . $self->{'base_import'};
    }
 
    $self->{'NLM_VERSION'} = $Config{'nlm_version'};
    $self->{'MPKTOOL'}	= $Config{'mpktool'};
    $self->{'TOOLPATH'}	= $Config{'toolpath'};
}


=item constants (o)

Initializes lots of constants and .SUFFIXES and .PHONY

=cut

sub const_cccmd {
    my($self,$libperl)=@_;
    return $self->{CONST_CCCMD} if $self->{CONST_CCCMD};
    return '' unless $self->needs_linking();
    return $self->{CONST_CCCMD} = <<'MAKE_FRAG';
CCCMD = $(CC) $(CCFLAGS) $(INC) $(OPTIMIZE) \
	$(PERLTYPE) $(MPOLLUTE) -o $@ \
	-DVERSION=\"$(VERSION)\" -DXS_VERSION=\"$(XS_VERSION)\"
MAKE_FRAG

}

sub constants {
    my($self) = @_;
    my(@m,$tmp);

# Added LIBPTH, BASE_IMPORT, ABSTRACT, NLM_VERSION BOOT_SYMBOL, NLM_SHORT_NAME
# for NETWARE

    for $tmp (qw/

	      AR_STATIC_ARGS NAME DISTNAME NAME_SYM VERSION
	      VERSION_SYM XS_VERSION INST_BIN INST_LIB
	      INST_ARCHLIB INST_SCRIPT PREFIX  INSTALLDIRS
	      INSTALLPRIVLIB INSTALLARCHLIB INSTALLSITELIB
	      INSTALLSITEARCH INSTALLBIN INSTALLSCRIPT PERL_LIB
	      PERL_ARCHLIB SITELIBEXP SITEARCHEXP LIBPERL_A MYEXTLIB
	      FIRST_MAKEFILE MAKE_APERL_FILE PERLMAINCC PERL_SRC
	      PERL_INC PERL FULLPERL LIBPTH BASE_IMPORT PERLRUN
              FULLPERLRUN PERLRUNINST FULLPERLRUNINST
              FULL_AR PERL_CORE NLM_VERSION MPKTOOL TOOLPATH

	      / ) {
	next unless defined $self->{$tmp};
	push @m, "$tmp = $self->{$tmp}\n";
    }

    (my $boot = $self->{'NAME'}) =~ s/:/_/g;
    $self->{'BOOT_SYMBOL'}=$boot;
    push @m, "BOOT_SYMBOL = $self->{'BOOT_SYMBOL'}\n";

    # If the final binary name is greater than 8 chars,
    # truncate it here.
    if(length($self->{'BASEEXT'}) > 8) {
        $self->{'NLM_SHORT_NAME'} = substr($self->{'BASEEXT'},0,8);
        push @m, "NLM_SHORT_NAME = $self->{'NLM_SHORT_NAME'}\n";
    }

    push @m, qq{
VERSION_MACRO = VERSION
DEFINE_VERSION = -D\$(VERSION_MACRO)=\\\"\$(VERSION)\\\"
XS_VERSION_MACRO = XS_VERSION
XS_DEFINE_VERSION = -D\$(XS_VERSION_MACRO)=\\\"\$(XS_VERSION)\\\"
};

    # Get the include path and replace the spaces with ;
    # Copy this to makefile as INCLUDE = d:\...;d:\;
    (my $inc = $Config{'incpath'}) =~ s/([ ]*)-I/;/g;

    # Get the additional include path from the user through the command prompt
    # and append to INCLUDE
#    $self->{INC} = '';
    push @m, "INC = $self->{'INC'}\n";

    push @m, qq{
INCLUDE = $inc;
};

    # Set the path to CodeWarrior binaries which might not have been set in
    # any other place
    push @m, qq{
PATH = \$(PATH);\$(TOOLPATH)
};

    push @m, qq{
MAKEMAKER = $INC{'ExtUtils/MakeMaker.pm'}
MM_VERSION = $ExtUtils::MakeMaker::VERSION
};

    push @m, q{
# FULLEXT = Pathname for extension directory (eg Foo/Bar/Oracle).
# BASEEXT = Basename part of FULLEXT. May be just equal FULLEXT. (eg Oracle)
# PARENT_NAME = NAME without BASEEXT and no trailing :: (eg Foo::Bar)
# DLBASE  = Basename part of dynamic library. May be just equal BASEEXT.
};

    for $tmp (qw/
	      FULLEXT BASEEXT PARENT_NAME DLBASE VERSION_FROM INC DEFINE OBJECT
	      LDFROM LINKTYPE
	      /	) {
	next unless defined $self->{$tmp};
	push @m, "$tmp = $self->{$tmp}\n";
    }

    push @m, "
# Handy lists of source code files:
XS_FILES= ".join(" \\\n\t", sort keys %{$self->{XS}})."
C_FILES = ".join(" \\\n\t", @{$self->{C}})."
O_FILES = ".join(" \\\n\t", @{$self->{O_FILES}})."
H_FILES = ".join(" \\\n\t", @{$self->{H}})."
MAN1PODS = ".join(" \\\n\t", sort keys %{$self->{MAN1PODS}})."
MAN3PODS = ".join(" \\\n\t", sort keys %{$self->{MAN3PODS}})."
";

    for $tmp (qw/
	      INST_MAN1DIR        INSTALLMAN1DIR MAN1EXT
	      INST_MAN3DIR        INSTALLMAN3DIR MAN3EXT
	      /) {
	next unless defined $self->{$tmp};
	push @m, "$tmp = $self->{$tmp}\n";
    }

    push @m, qq{
.USESHELL :
} if $DMAKE;

    push @m, q{
.NO_CONFIG_REC: Makefile
} if $ENV{CLEARCASE_ROOT};

    # why not q{} ? -- emacs
    push @m, qq{
# work around a famous dec-osf make(1) feature(?):
makemakerdflt: all

.SUFFIXES: .xs .c .C .cpp .cxx .cc \$(OBJ_EXT)

.PHONY: all config static dynamic test linkext manifest

# Where is the Config information that we are using/depend on
CONFIGDEP = \$(PERL_ARCHLIB)\\Config.pm \$(PERL_INC)\\config.h
};

    my @parentdir = split(/::/, $self->{PARENT_NAME});
    push @m, q{
# Where to put things:
INST_LIBDIR      = }. File::Spec->catdir('$(INST_LIB)',@parentdir)        .q{
INST_ARCHLIBDIR  = }. File::Spec->catdir('$(INST_ARCHLIB)',@parentdir)    .q{

INST_AUTODIR     = }. File::Spec->catdir('$(INST_LIB)','auto','$(FULLEXT)')       .q{
INST_ARCHAUTODIR = }. File::Spec->catdir('$(INST_ARCHLIB)','auto','$(FULLEXT)')   .q{
};

    if ($self->has_link_code()) {
	push @m, '
INST_STATIC  = $(INST_ARCHAUTODIR)\$(BASEEXT)$(LIB_EXT)
INST_DYNAMIC = $(INST_ARCHAUTODIR)\$(DLBASE).$(DLEXT)
INST_BOOT    = $(INST_ARCHAUTODIR)\$(BASEEXT).bs
';
    } else {
	push @m, '
INST_STATIC  =
INST_DYNAMIC =
INST_BOOT    =
';
    }

    $tmp = $self->export_list;
    push @m, "
EXPORT_LIST = $tmp
";
    $tmp = $self->perl_archive;
    push @m, "
PERL_ARCHIVE = $tmp
";

    push @m, q{
TO_INST_PM = }.join(" \\\n\t", sort keys %{$self->{PM}}).q{

PM_TO_BLIB = }.join(" \\\n\t", %{$self->{PM}}).q{
};

    join('',@m);
}


=item static_lib (o)

=cut

sub static_lib {
    my($self) = @_;

    return '' unless $self->has_link_code;

    my $m = <<'END';
$(INST_STATIC): $(OBJECT) $(MYEXTLIB) $(INST_ARCHAUTODIR)\.exists
	$(RM_RF) $@
END

    # If this extension has it's own library (eg SDBM_File)
    # then copy that to $(INST_STATIC) and add $(OBJECT) into it.
    $m .= <<'END'  if $self->{MYEXTLIB};
	$self->{CP} $(MYEXTLIB) $@
END

    my $ar_arg;
    if( $BORLAND ) {
        $ar_arg = '$@ $(OBJECT:^"+")';
    }
    elsif( $GCC ) {
        $ar_arg = '-ru $@ $(OBJECT)';
    }
    else {
        $ar_arg = '-type library -o $@ $(OBJECT)';
    }

    $m .= sprintf <<'END', $ar_arg;
	$(AR) %s
	$(NOECHO)echo "$(EXTRALIBS)" > $(INST_ARCHAUTODIR)\extralibs.ld
	$(CHMOD) 755 $@
END

    $m .= <<'END' if $self->{PERL_SRC};
	$(NOECHO)echo "$(EXTRALIBS)" >> $(PERL_SRC)\ext.libs
    
    
END
    $m .= $self->dir_target('$(INST_ARCHAUTODIR)');
    return $m;
}

=item dynamic_lib (o)

Defines how to produce the *.so (or equivalent) files.

=cut

sub dynamic_lib {
    my($self, %attribs) = @_;
    return '' unless $self->needs_linking(); #might be because of a subdir

    return '' unless $self->has_link_code;

    my($otherldflags) = $attribs{OTHERLDFLAGS} || ($BORLAND ? 'c0d32.obj': '');
    my($inst_dynamic_dep) = $attribs{INST_DYNAMIC_DEP} || "";
    my($ldfrom) = '$(LDFROM)';

    (my $boot = $self->{NAME}) =~ s/:/_/g;

    my $m = <<'MAKE_FRAG';
# This section creates the dynamically loadable $(INST_DYNAMIC)
# from $(OBJECT) and possibly $(MYEXTLIB).
OTHERLDFLAGS = '.$otherldflags.'
INST_DYNAMIC_DEP = '.$inst_dynamic_dep.'

# Create xdc data for an MT safe NLM in case of mpk build
$(INST_DYNAMIC): $(OBJECT) $(MYEXTLIB) $(BOOTSTRAP)
	@echo Export boot_$(BOOT_SYMBOL) > $(BASEEXT).def
	@echo $(BASE_IMPORT) >> $(BASEEXT).def
	@echo Import @$(PERL_INC)\perl.imp >> $(BASEEXT).def
MAKE_FRAG


    if ( $self->{CCFLAGS} =~ m/ -DMPK_ON /) {
        $m .= <<'MAKE_FRAG';
	$(MPKTOOL) $(XDCFLAGS) $(BASEEXT).xdc
	@echo xdcdata $(BASEEXT).xdc >> $(BASEEXT).def
MAKE_FRAG
    }

    # Reconstruct the X.Y.Z version.
    my $version = join '.', map { sprintf "%d", $_ }
                              $] =~ /(\d)\.(\d{3})(\d{2})/;
    $m .= sprintf '	$(LD) $(LDFLAGS) $(OBJECT:.obj=.obj) -desc "Perl %s Extension ($(BASEEXT))  XS_VERSION: $(XS_VERSION)" -nlmversion $(NLM_VERSION)', $version;

    # Taking care of long names like FileHandle, ByteLoader, SDBM_File etc
    if($self->{NLM_SHORT_NAME}) {
        # In case of nlms with names exceeding 8 chars, build nlm in the 
        # current dir, rename and move to auto\lib.
        $m .= q{ -o $(NLM_SHORT_NAME).$(DLEXT)}
    } else {
        $m .= q{ -o $(INST_AUTODIR)\\$(BASEEXT).$(DLEXT)}
    }

    # Add additional lib files if any (SDBM_File)
    $m .= q{ $(MYEXTLIB) } if $self->{MYEXTLIB};

    $m .= q{ $(PERL_INC)\Main.lib -commandfile $(BASEEXT).def}."\n";

    if($self->{NLM_SHORT_NAME}) {
        $m .= <<'MAKE_FRAG';
	if exist $(INST_AUTODIR)\$(NLM_SHORT_NAME).$(DLEXT) del $(INST_AUTODIR)\$(NLM_SHORT_NAME).$(DLEXT) 
	move $(NLM_SHORT_NAME).$(DLEXT) $(INST_AUTODIR)
MAKE_FRAG
    }

    $m .= <<'MAKE_FRAG';

	$(CHMOD) 755 $@
MAKE_FRAG

    $m .= $self->dir_target('$(INST_ARCHAUTODIR)');

    return $m;
}


1;
__END__

=back

=cut 


