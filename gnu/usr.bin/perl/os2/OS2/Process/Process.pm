package OS2::Process;

$VERSION = 0.2;

require Exporter;
require DynaLoader;
#require AutoLoader;

@ISA = qw(Exporter DynaLoader);
# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.
@EXPORT = qw(
	P_BACKGROUND
	P_DEBUG
	P_DEFAULT
	P_DETACH
	P_FOREGROUND
	P_FULLSCREEN
	P_MAXIMIZE
	P_MINIMIZE
	P_NOCLOSE
	P_NOSESSION
	P_NOWAIT
	P_OVERLAY
	P_PM
	P_QUOTE
	P_SESSION
	P_TILDE
	P_UNRELATED
	P_WAIT
	P_WINDOWED
	my_type
	file_type
	T_NOTSPEC
	T_NOTWINDOWCOMPAT
	T_WINDOWCOMPAT
	T_WINDOWAPI
	T_BOUND
	T_DLL
	T_DOS
	T_PHYSDRV
	T_VIRTDRV
	T_PROTDLL
	T_32BIT
	process_entry
	set_title
	get_title
);
sub AUTOLOAD {
    # This AUTOLOAD is used to 'autoload' constants from the constant()
    # XS function.  If a constant is not found then control is passed
    # to the AUTOLOAD in AutoLoader.

    local($constname);
    ($constname = $AUTOLOAD) =~ s/.*:://;
    $val = constant($constname, @_ ? $_[0] : 0);
    if ($! != 0) {
	if ($! =~ /Invalid/) {
	    $AutoLoader::AUTOLOAD = $AUTOLOAD;
	    goto &AutoLoader::AUTOLOAD;
	}
	else {
	    ($pack,$file,$line) = caller;
	    die "Your vendor has not defined OS2::Process macro $constname, used at $file line $line.
";
	}
    }
    eval "sub $AUTOLOAD { $val }";
    goto &$AUTOLOAD;
}

bootstrap OS2::Process;

# Preloaded methods go here.

sub get_title () { (process_entry())[0] }

# Autoload methods go after __END__, and are processed by the autosplit program.

1;
__END__

=head1 NAME

OS2::Process - exports constants for system() call on OS2.

=head1 SYNOPSIS

    use OS2::Process;
    $pid = system(P_PM+P_BACKGROUND, "epm.exe");

=head1 DESCRIPTION

the builtin function system() under OS/2 allows an optional first
argument which denotes the mode of the process. Note that this argument is
recognized only if it is strictly numerical.

You can use either one of the process modes:

	P_WAIT (0)	= wait until child terminates (default)
	P_NOWAIT	= do not wait until child terminates
	P_SESSION	= new session
	P_DETACH	= detached
	P_PM		= PM program

and optionally add PM and session option bits:

	P_DEFAULT (0)	= default
	P_MINIMIZE	= minimized
	P_MAXIMIZE	= maximized
	P_FULLSCREEN	= fullscreen (session only)
	P_WINDOWED	= windowed (session only)

	P_FOREGROUND	= foreground (if running in foreground)
	P_BACKGROUND	= background

	P_NOCLOSE	= don't close window on exit (session only)

	P_QUOTE		= quote all arguments
	P_TILDE		= MKS argument passing convention
	P_UNRELATED	= do not kill child when father terminates

=head2 Access to process properties

Additionaly, subroutines my_type(), process_entry() and
C<file_type(file)>, get_title() and C<set_title(newtitle)> are implemented.  
my_type() returns the type of the current process (one of 
"FS", "DOS", "VIO", "PM", "DETACH" and "UNKNOWN"), or C<undef> on error.

=over

=item C<file_type(file)> 

returns the type of the executable file C<file>, or
dies on error.  The bits 0-2 of the result contain one of the values

=over

=item C<T_NOTSPEC> (0)

Application type is not specified in the executable header. 

=item C<T_NOTWINDOWCOMPAT> (1)

Application type is not-window-compatible. 

=item C<T_WINDOWCOMPAT> (2)

Application type is window-compatible. 

=item C<T_WINDOWAPI> (3)

Application type is window-API.

=back

The remaining bits should be masked with the following values to
determine the type of the executable:

=over

=item C<T_BOUND> (8)

Set to 1 if the executable file has been "bound" (by the BIND command)
as a Family API application. Bits 0, 1, and 2 still apply.

=item C<T_DLL> (0x10)

Set to 1 if the executable file is a dynamic link library (DLL)
module. Bits 0, 1, 2, 3, and 5 will be set to 0.

=item C<T_DOS> (0x20)

Set to 1 if the executable file is in PC/DOS format. Bits 0, 1, 2, 3,
and 4 will be set to 0.

=item C<T_PHYSDRV> (0x40)

Set to 1 if the executable file is a physical device driver. 

=item C<T_VIRTDRV> (0x80)

Set to 1 if the executable file is a virtual device driver. 

=item C<T_PROTDLL> (0x100)

Set to 1 if the executable file is a protected-memory dynamic link
library module.

=item C<T_32BIT> (0x4000)

Set to 1 for 32-bit executable files. 

=back

file_type() may croak with one of the strings C<"Invalid EXE
signature"> or C<"EXE marked invalid"> to indicate typical error
conditions.  If given non-absolute path, will look on C<PATH>, will
add extention F<.exe> if no extension is present (add extension F<.>
to suppress).

=item process_entry()

returns a list of the following data:

=over

=item 

Title of the process (in the C<Ctrl-Esc> list);

=item 

window handle of switch entry of the process (in the C<Ctrl-Esc> list);

=item 

window handle of the icon of the process;

=item 

process handle of the owner of the entry in C<Ctrl-Esc> list;

=item 

process id of the owner of the entry in C<Ctrl-Esc> list;

=item 

session id of the owner of the entry in C<Ctrl-Esc> list;

=item 

whether visible in C<Ctrl-Esc> list;

=item

whether item cannot be switched to (note that it is not actually
grayed in the C<Ctrl-Esc> list));

=item 

whether participates in jump sequence;

=item 

program type.  Possible values are: 

     PROG_DEFAULT                       0 
     PROG_FULLSCREEN                    1 
     PROG_WINDOWABLEVIO                 2 
     PROG_PM                            3 
     PROG_VDM                           4 
     PROG_WINDOWEDVDM                   7 

Although there are several other program types for WIN-OS/2 programs,
these do not show up in this field. Instead, the PROG_VDM or
PROG_WINDOWEDVDM program types are used. For instance, for
PROG_31_STDSEAMLESSVDM, PROG_WINDOWEDVDM is used. This is because all
the WIN-OS/2 programs run in DOS sessions. For example, if a program
is a windowed WIN-OS/2 program, it runs in a PROG_WINDOWEDVDM
session. Likewise, if it's a full-screen WIN-OS/2 program, it runs in
a PROG_VDM session.


=back

=item C<set_title(newtitle)> 

- does not work with some windows (if the title is set from the start).  
This is a limitation of OS/2, in such a case $^E is set to 372 (type

  help 372

for a funny - and wrong  - explanation ;-).

=item get_title() 

is a shortcut implemented via process_entry().

=back

=head1 AUTHOR

Andreas Kaiser <ak@ananke.s.bawue.de>, 
Ilya Zakharevich <ilya@math.ohio-state.edu>.

=head1 SEE ALSO

C<spawn*>() system calls.

=cut
