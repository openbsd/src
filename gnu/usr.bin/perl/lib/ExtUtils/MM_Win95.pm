package ExtUtils::MM_Win95;

use vars qw($VERSION @ISA);
$VERSION = 0.03_01;

require ExtUtils::MM_Win32;
@ISA = qw(ExtUtils::MM_Win32);

use Config;
my $DMAKE = $Config{'make'} =~ /^dmake/i;
my $NMAKE = $Config{'make'} =~ /^nmake/i;


=head1 NAME

ExtUtils::MM_Win95 - method to customize MakeMaker for Win9X

=head1 SYNOPSIS

  You should not be using this module directly.

=head1 DESCRIPTION

This is a subclass of ExtUtils::MM_Win32 containing changes necessary
to get MakeMaker playing nice with command.com and other Win9Xisms.

=head2 Overriden methods

Most of these make up for limitations in the Win9x command shell.
Namely the lack of && and that a chdir is global, so you have to chdir
back at the end.

=over 4

=item dist_test

&& and chdir problem.

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

=item subdir_x

&& and chdir problem.

Also, dmake has an odd way of making a command series silent.

=cut

sub subdir_x {
    my($self, $subdir) = @_;

    # Win-9x has nasty problem in command.com that can't cope with
    # &&.  Also, Dmake has an odd way of making a commandseries silent:
    if ($DMAKE) {
      return sprintf <<'EOT', $subdir;

subdirs ::
@[
	cd %s
	$(MAKE) all $(PASTHRU)
	cd ..
]
EOT
    }
    else {
        return sprintf <<'EOT', $subdir;

subdirs ::
	$(NOECHO)cd %s
	$(NOECHO)$(MAKE) all $(PASTHRU)
	$(NOECHO)cd ..
EOT
    }
}

=item xs_c

The && problem.

=cut

sub xs_c {
    my($self) = shift;
    return '' unless $self->needs_linking();
    '
.xs.c:
	$(PERLRUN) $(XSUBPP) $(XSPROTOARG) $(XSUBPPARGS) $*.xs > $*.c
	'
}


=item xs_cpp

The && problem

=cut

sub xs_cpp {
    my($self) = shift;
    return '' unless $self->needs_linking();
    '
.xs.cpp:
	$(PERLRUN) $(XSUBPP) $(XSPROTOARG) $(XSUBPPARGS) $*.xs > $*.cpp
	';
}

=item xs_o 

The && problem.

=cut

sub xs_o {
    my($self) = shift;
    return '' unless $self->needs_linking();
    # Having to choose between .xs -> .c -> .o and .xs -> .o confuses dmake.
    return '' if $DMAKE;
    '
.xs$(OBJ_EXT):
	$(PERLRUN) $(XSUBPP) $(XSPROTOARG) $(XSUBPPARGS) $*.xs > $*.c
	$(CCCMD) $(CCCDLFLAGS) -I$(PERL_INC) $(DEFINE) $*.c
	';
}

=item clean_subdirs_target

&& and chdir problem.

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
	cd %s
	$(TEST_F) $(FIRST_MAKEFILE)
	$(MAKE) clean
	cd ..
MAKE_FRAG
    }

    return $clean;
}


=item realclean_subdirs_target

&& and chdir problem.

=cut

sub realclean_subdirs_target {
    my $self = shift;

    return <<'NOOP_FRAG' unless @{$self->{DIR}};
realclean_subdirs :
	$(NOECHO)$(NOOP)
NOOP_FRAG

    my $rclean = "realclean_subdirs :\n";

    foreach my $dir (@{$self->{DIR}}){
        $rclean .= sprintf <<'RCLEAN', $dir;
	-cd %s
	-$(PERLRUN) -e "exit unless -f shift; system q{$(MAKE) realclean}" $(FIRST_MAKEFILE)
	-cd ..
RCLEAN

    }

    return $rclean;
}


=item os_flavor

Win95 and Win98 and WinME are collectively Win9x and Win32

=cut

sub os_flavor {
    my $self = shift;
    return ($self->SUPER::os_flavor, 'Win9x');
}


=back


=head1 AUTHOR

Code originally inside MM_Win32.  Original author unknown.  

Currently maintained by Michael G Schwern <schwern@pobox.com>.

Send patches and ideas to <F<makemaker@perl.org>>.

See http://www.makemaker.org.

=cut


1;
