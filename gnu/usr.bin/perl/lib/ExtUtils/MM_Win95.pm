package ExtUtils::MM_Win95;

use vars qw($VERSION @ISA);
$VERSION = 0.02;

require ExtUtils::MM_Win32;
@ISA = qw(ExtUtils::MM_Win32);
use Config;

=head1 NAME

ExtUtils::MM_Win95 - method to customize MakeMaker for Win9X

=head1 SYNOPSIS

  You should not be using this module directly.

=head1 DESCRIPTION

This is a subclass of ExtUtils::MM_Win32 containing changes necessary
to get MakeMaker playing nice with command.com and other Win9Xisms.

=cut

sub dist_test {
    my($self) = shift;
    return q{
disttest : distdir
	cd $(DISTVNAME)
	$(ABSPERLRUN) Makefile.PL
	$(MAKE) $(PASTHRU)
	$(MAKE) test $(PASTHRU)
	cd ..
};
}

sub xs_c {
    my($self) = shift;
    return '' unless $self->needs_linking();
    '
.xs.c:
	$(PERL) -I$(PERL_ARCHLIB) -I$(PERL_LIB) $(XSUBPP) \\
	    $(XSPROTOARG) $(XSUBPPARGS) $*.xs > $*.c
	'
}

sub xs_cpp {
    my($self) = shift;
    return '' unless $self->needs_linking();
    '
.xs.cpp:
	$(PERL) -I$(PERL_ARCHLIB) -I$(PERL_LIB) $(XSUBPP) \\
	    $(XSPROTOARG) $(XSUBPPARGS) $*.xs > $*.cpp
	';
}

# many makes are too dumb to use xs_c then c_o
sub xs_o {
    my($self) = shift;
    return '' unless $self->needs_linking();
    # having to choose between .xs -> .c -> .o and .xs -> .o confuses dmake
    return '' if $Config{make} eq 'dmake';
    '
.xs$(OBJ_EXT):
	$(PERL) -I$(PERL_ARCHLIB) -I$(PERL_LIB) $(XSUBPP) \\
	    $(XSPROTOARG) $(XSUBPPARGS) $*.xs > $*.c
	$(CCCMD) $(CCCDLFLAGS) -I$(PERL_INC) $(DEFINE) $*.c
	';
}

1;
