package OS2::localMorphPM;
# use strict;

sub new {
  my ($c,$f) = @_;
  OS2::MorphPM($f);
  # print STDERR ">>>>>\n";
  bless [$f], $c
}
sub DESTROY {
  # print STDERR "<<<<<\n";
  OS2::UnMorphPM(shift->[0])
}

package OS2::Process;

BEGIN {
  require Exporter;
  require XSLoader;
  #require AutoLoader;

  our @ISA = qw(Exporter);
  our $VERSION = "1.02";
  XSLoader::load('OS2::Process', $VERSION);
}

# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.
our @EXPORT = qw(
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
	ppid
	ppidOf
	sidOf
	scrsize
	scrsize_set
	kbdChar
	kbdhChar
	kbdStatus
	_kbdStatus_set
	kbdhStatus
	kbdhStatus_set
	vioConfig
	viohConfig
	vioMode
	viohMode
	viohMode_set
	_vioMode_set
	_vioState
	_vioState_set
	vioFont
	vioFont_set
	process_entry
	process_entries
	process_hentry
	process_hentries
	change_entry
	change_entryh
	process_hwnd
	Title_set
	Title
	winTitle_set
	winTitle
	swTitle_set
	bothTitle_set
	WindowText
	WindowText_set
	WindowPos
	WindowPos_set
	hWindowPos
	hWindowPos_set
	WindowProcess
	SwitchToProgram
	DesktopWindow
	ActiveWindow
	ActiveWindow_set
	ClassName
	FocusWindow
	FocusWindow_set
	ShowWindow
	PostMsg
	BeginEnumWindows
	EndEnumWindows
	GetNextWindow
	IsWindow
	ChildWindows
	out_codepage
	out_codepage_set
	process_codepage_set
	in_codepage
	in_codepage_set
	cursor
	cursor_set
	screen
	screen_set
	process_codepages
	QueryWindow
	WindowFromId
	WindowFromPoint
	EnumDlgItem
        EnableWindow
        EnableWindowUpdate
        IsWindowEnabled
        IsWindowVisible
        IsWindowShowing
        WindowPtr
        WindowULong
        WindowUShort
        SetWindowBits
        SetWindowPtr
        SetWindowULong
        SetWindowUShort
	TopLevel
	FocusWindow_set_keep_Zorder

	ActiveDesktopPathname
	InvalidateRect
	CreateFrameControl
	ClipbrdFmtInfo
	ClipbrdOwner
	ClipbrdViewer
	ClipbrdData
	OpenClipbrd
	CloseClipbrd
	ClipbrdData_set
	ClipbrdOwner_set
	ClipbrdViewer_set
	EnumClipbrdFmts
	EmptyClipbrd
	AddAtom
	FindAtom
	DeleteAtom
	AtomUsage
	AtomName
	AtomLength
	SystemAtomTable
	CreateAtomTable
	DestroyAtomTable

	_ClipbrdData_set
	ClipbrdText
	ClipbrdText_set

	_MessageBox
	MessageBox
	_MessageBox2
	MessageBox2
	LoadPointer
	SysPointer
	Alarm
	FlashWindow

	get_title
	set_title
);
our @EXPORT_OK = qw(
	ResetWinError
        MPFROMSHORT
        MPVOID
        MPFROMCHAR
        MPFROM2SHORT
        MPFROMSH2CH
        MPFROMLONG
);

our $AUTOLOAD;

sub AUTOLOAD {
    # This AUTOLOAD is used to 'autoload' constants from the constant()
    # XS function.  If a constant is not found then control is passed
    # to the AUTOLOAD in AutoLoader.

    (my $constname = $AUTOLOAD) =~ s/.*:://;
    my $val = constant($constname, @_ ? $_[0] : 0);
    if ($! != 0) {
	if ($! =~ /Invalid/ || $!{EINVAL}) {
	    die "Unsupported function $AUTOLOAD"
	} else {
	    my ($pack,$file,$line) = caller;
	    die "Your vendor has not defined OS2::Process macro $constname, used at $file line $line.
";
	}
    }
    eval "sub $AUTOLOAD { $val }";
    goto &$AUTOLOAD;
}

sub const_import {
  require OS2::Process::Const;
  my $sym = shift;
  my ($err, $val) = OS2::Process::Const::constant($sym);
  die $err if $err;
  my $p = caller(1);

  # no strict;

  *{"$p\::$sym"} = sub () { $val };
  ();			# needed by import()
}

sub import {
  my $class = shift;
  my $ini = @_;
  @_ = ($class,
	map {
	  /^(HWND|WM|SC|SWP|WC|PROG|QW|EDI|WS|QWS|QWP|QWL|FF|FI|LS|FS|FCF|BS|MS|TBM|CF|CFI|FID|MB|MBID|CF|CFI|SPTR)_/ ? const_import($_) : $_
	} @_);
  goto &Exporter::import if @_ > 1 or $ini == 0;
}

# Preloaded methods go here.

sub Title () { (process_entry())[0] }

# *Title_set = \&sesmgr_title_set;

sub swTitle_set_sw {
  my ($title, @sw) = @_;
  $sw[0] = $title;
  change_entry(@sw);
}

sub swTitle_set ($) {
  my (@sw) = process_entry();
  swTitle_set_sw(shift, @sw);
}

sub winTitle_set_sw {
  my ($title, @sw) = @_;
  my $h = OS2::localMorphPM->new(0);
  WindowText_set $sw[1], $title;
}

sub winTitle_set ($) {
  my (@sw) = process_entry();
  winTitle_set_sw(shift, @sw);
}

sub winTitle () {
  my (@sw) = process_entry();
  my $h = OS2::localMorphPM->new(0);
  WindowText $sw[1];
}

sub bothTitle_set ($) {
  my (@sw) = process_entry();
  my $t = shift;
  winTitle_set_sw($t, @sw);
  swTitle_set_sw($t, @sw);
}

sub Title_set ($) {
  my $t = shift;
  return 1 if sesmgr_title_set($t);
  return 0 unless $^E == 372;
  my (@sw) = process_entry();
  winTitle_set_sw($t, @sw);
  swTitle_set_sw($t, @sw);
}

sub process_entry { swentry_expand(process_swentry(@_)) }

our @hentry_fields = qw( title owner_hwnd icon_hwnd 
			 owner_phandle owner_pid owner_sid
			 visible nonswitchable jumpable ptype sw_entry );

sub swentry_hexpand ($) {
  my %h;
  @h{@hentry_fields} = swentry_expand(shift);
  \%h;
}

sub process_hentry { swentry_hexpand(process_swentry(@_)) }
sub process_hwnd { process_hentry()->{owner_hwnd} }

my $swentry_size = swentry_size();

sub sw_entries () {
  my $s = swentries_list();
  my ($c, $s1) = unpack 'La*', $s;
  die "Unconsistent size in swentries_list()" unless 4+$c*$swentry_size == length $s;
  my (@l, $e);
  push @l, $e while $e = substr $s1, 0, $swentry_size, '';
  @l;
}

sub process_entries () {
  map [swentry_expand($_)], sw_entries;
}

sub process_hentries () {
  map swentry_hexpand($_), sw_entries;
}

sub change_entry {
  change_swentry(create_swentry(@_));
}

sub create_swentryh ($) {
  my $h = shift;
  create_swentry(@$h{@hentry_fields});
}

sub change_entryh ($) {
  change_swentry(create_swentryh(shift));
}

# Massage entries into the same order as WindowPos_set:
sub WindowPos ($) {
  my ($fl, $h, $w, $y, $x, $behind, $hwnd, @rest)
	= unpack 'L l4 L4', WindowSWP(shift);
  ($x, $y, $fl, $w, $h, $behind, @rest);
}

# Put them into a hash
sub hWindowPos ($) {
  my %h;
  @h{ qw(flags height width y x behind hwnd reserved1 reserved2) }
	= unpack 'L l4 L4', WindowSWP(shift);
  \%h;
}

my @SWP_keys = ( [qw(width height)],	# SWP_SIZE=1
		 [qw(x y)],		# SWP_MOVE=2
		 [qw(behind)] );	# SWP_ZORDER=3
my %SWP_def;
@SWP_def{ map @$_, @SWP_keys }  = (0) x 20;

# Get them from a hash
sub hWindowPos_set ($$) {
  my $hash = shift;
  my $hwnd = (@_ ? shift : $hash->{hwnd} );
  my $flags;
  if (exists $hash->{flags}) {
    $flags = $hash->{flags};
  } else {			# Set flags according to existing keys in $hash
    $flags = 0;
    for my $bit (0..2) {
      exists $hash->{$_} and $flags |= (1<<$bit) for @{$SWP_keys[$bit]};
    }
  }
  for my $bit (0..2) {		# Check for required keys
    next unless $flags & (1<<$bit);
    exists $hash->{$_}
      or die sprintf "key $_ required for flags=%#x", $flags
	for @{$SWP_keys[$bit]};
  }
  my %h = (%SWP_def, flags => $flags, %$hash);		# Avoid warnings
  my ($x, $y, $fl, $w, $h, $behind) = @h{ qw(x y flags width height behind) };
  WindowPos_set($hwnd, $x, $y, $fl, $w, $h, $behind);
}

sub ChildWindows (;$) {
  my $hm = OS2::localMorphPM->new(0);
  my @kids;
  my $h = BeginEnumWindows(@_ ? shift : 1);	# HWND_DESKTOP
  my $w;
  push @kids, $w while $w = GetNextWindow $h;
  EndEnumWindows $h;
  @kids;
}

sub TopLevel ($) {
  my $d = DesktopWindow;
  my $w = shift;
  while (1) {
    my $p = QueryWindow $w, 5;	# QW_PARENT;
    return $w if not $p or $p == $d;
    $w = $p;
  }
}

sub FocusWindow_set_keep_Zorder ($) {
  my $w = shift;
  my $t = TopLevel $w;
  my $b = hWindowPos($t)->{behind}; # we are behind this
  EnableWindowUpdate($t, 0);
  FocusWindow_set($w);
# sleep 1;    # Make flicker stronger when present
  hWindowPos_set {behind => $b}, $t;
  EnableWindowUpdate($t, 1);
}

sub ClipbrdText (@) {
  my $morph = OS2::localMorphPM->new(0);
  OpenClipbrd();
  my $txt = unpack 'p', pack 'L', ClipbrdData @_;
  CloseClipbrd();
  $txt;
}

sub ClipbrdText_set ($;$) {
  my $morph = OS2::localMorphPM->new(0);
  OpenClipbrd();
  EmptyClipbrd();				# It may contain other types
  my ($txt, $no_convert_nl) = (shift, shift);
  ClipbrdData_set($txt, !$no_convert_nl, @_);
  CloseClipbrd();
}

sub MessageBox ($;$$$$$) {
  my $morph = OS2::localMorphPM->new(0);
  die "MessageBox needs text" unless @_;
  push @_ , ($0 eq '-e' ? "Perl one-liner's message" : "$0 message") if @_ == 1;
  &_MessageBox;
}

my %pointers;

sub get_pointer ($;$$) {
  my $id = $_[0];
  return $pointers{$id} if exists $pointers{$id};
  $pointers{$id} = &SysPointer;
}

# $button needs to be of the form 'String', ['String'] or ['String', flag].
# If ['String'], it is assumed the default button; same for 'String' if $only
# is set.
sub process_MB2 ($$;$) {
  die "process_MB2() needs 2 arguments, got '@_'" unless @_ == 2 or @_ == 3;
  my ($button, $ret, $only) = @_;
  # default is BS_PUSHBUTTON, add BS_DEFAULT if $only is set
  $button = [$button, $only ? 0x400 : 0] unless ref $button eq 'ARRAY';
  push @$button, 0x400 if @$button == 1; # BS_PUSHBUTTON|BS_DEFAULT
  die "Button needs to be of the form 'String', ['String'] or ['String', flag]"
    unless @$button == 2;
  pack "Z71 x L l", $button->[0], $ret, $button->[1]; # name, retval, flag
}

# If one button, make it the default one even if it is of 'String' => val form.
# If icon is of the form 'SP#<number>', load this via SysPointer.
sub process_MB2_INFO ($;$$$) {
  my $l = 0;
  my $out;
  die "process_MB2_INFO() needs 1..4 arguments" unless @_ and @_ < 5;
  my $buttons = shift;
  die "Buttons array should consist of pairs" if @$buttons % 2;

  push @_, 0 unless @_;		# Icon id (pointer)
  # Box flags (MB_MOVABLE and MB_INFORMATION or MB_CUSTOMICON)
  push @_, ($_[0] ? 0x4080 : 0x4030) unless @_ > 1;
  push @_, 0 unless @_ > 2;	# Notify window

  my ($icon, $style, $notify) = (shift, shift, shift);
  $icon = get_pointer $1 if $icon =~ /^SP#(\d+)\z/;
  $out = pack "L L L L",	# icon, #buttons, style, notify, buttons
      $icon, @$buttons/2, $style, $notify;
  $out .= join '',
    map process_MB2($buttons->[2*$_], $buttons->[2*$_+1], @$buttons == 2),
      0..@$buttons/2-1;
  pack('L', length(pack 'L', 0) + length $out) . $out;
}

# MessageBox2 'Try this', OS2::Process::process_MB2_INFO([['Dismiss', 0] => 0x1000], OS2::Process::get_pointer(22),0x4080,0), 'me', 1, 0, 0
# or the shortcut
# MessageBox2 'Try this', [[['Dismiss', 0] => 0x1000], 'SP#22'], 'me'
# 0x80 means MB_CUSTOMICON (does not focus?!).  This focuses:
# MessageBox2 'Try this', [[['Dismiss',0x400] => 0x1000], 0, 0x4030,0]
# 0x400 means BS_DEFAULT.  This is the same as the shortcut
# MessageBox2 'Try this', [[Dismiss => 0x1000]]
sub MessageBox2 ($;$$$$$) {
  my $morph = OS2::localMorphPM->new(0);
  die "MessageBox needs text" unless @_;
  push @_ , [[Dismiss => 0x1000], # Name, retval (BS_PUSHBUTTON|BS_DEFAULT)
	     #0,		# get_pointer(11),	# SPTR_ICONINFORMATION
	     #0x4030,		# MB_MOVEABLE | MB_INFORMATION
	     #0,		# Notify window; was 1==HWND_DESKTOP
	    ] if @_ == 1;
  push @_ , ($0 eq '-e' ? "Perl one-liner's message" : "$0's message") if @_ == 2;
  $_[1] = &process_MB2_INFO(@{$_[1]}) if ref($_[1]) eq 'ARRAY';
  &_MessageBox2;
}

# backward compatibility
*set_title = \&Title_set;
*get_title = \&Title;

# adapter; display; cbMemory; Configuration; VDHVersion; Flags; HWBufferSize;
# FullSaveSize; PartSaveSize; EMAdaptersOFF; EMDisplaysOFF;
sub vioConfig (;$$) {
  my $data = &_vioConfig;
  my @out = unpack 'x[S]SSLSSSLLLSS', $data;
  # If present, offset points to S/S (with only the first work making sense)
  my (@adaptersEMU, @displayEMU);
  @displaysEMU = unpack("x[$out[10]]S/S", $data), pop @out if @out > 10;
  @adaptersEMU = unpack("x[$out[ 9]]S/S", $data), pop @out if @out > 9;
  $out[9] = $adaptersEMU[0] if @adaptersEMU;
  $out[10] = $displaysEMU[0] if @displaysEMU;
  @out;
}

my @vioConfig = qw(adapter display cbMemory Configuration VDHVersion Flags
		   HWBufferSize FullSaveSize PartSaveSize EMAdapters EMDisplays);

sub viohConfig (;$$) {
  my %h;
  @h{@vioConfig} = &vioConfig;
  %h;
}

# fbType; color; col; row; hres; vres; fmt_ID; attrib; buf_addr; buf_length;
# full_length; partial_length; ext_data_addr;
sub vioMode() {unpack 'x[S]CCSSSSCCLLLLL', _vioMode}

my @vioMode = qw( fbType color col row hres vres fmt_ID attrib buf_addr
		  buf_length full_length partial_length ext_data_addr);

sub viohMode() {
  my %h;
  @h{@vioMode} = vioMode;
  %h;
}

sub viohMode_set {
  my %h = (viohMode, @_);
  my $o = pack 'x[S]CCSSSSCCLLLLL', @h{@vioMode};
  $o = pack 'SCCSSSSCCLLLLL', length $o, @h{@vioMode};
  _vioMode_set($o);
}

sub kbdChar (;$$) {unpack 'CCCCSL', &_kbdChar}

my @kbdChar = qw(ascii scancode status nlsstate shifts time);
sub kbdhChar (;$$) {
  my %h;
  @h{@kbdChar} = &kbdChar;
  %h
}

sub kbdStatus (;$) {unpack 'x[S]SSSS', &_kbdStatus}
my @kbdStatus = qw(state turnChar intCharFlags shifts);
sub kbdhStatus (;$) {
  my %h;
  @h{@kbdStatus} = &kbdStatus;
  %h
}
sub kbdhStatus_set {
  my $h = (@_ % 2 ? shift @_ : 0);
  my %h = (kbdhStatus($h), @_);
  my $o = pack 'x[S]SSSS', @h{@kbdStatus};
  $o = pack 'SSSSS', length $o, @h{@kbdStatus};
  _kbdStatus_set($o,$h);
}


# Autoload methods go after __END__, and are processed by the autosplit program.

1;
__END__

=head1 NAME

OS2::Process - exports constants for system() call, and process control on OS2.

=head1 SYNOPSIS

    use OS2::Process;
    $pid = system(P_PM | P_BACKGROUND, "epm.exe");

=head1 DESCRIPTION

=head2 Optional argument to system()

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

On OS/2 processes have the usual I<parent/child> semantic;
additionally, there is a hierarchy of sessions with their own
I<parent/child> tree.  A session is either a FS session, or a windowed
pseudo-session created by PM.  A session is a "unit of user
interaction", a change to in/out settings in one of them does not
affect other sessions.

=over

=item my_type()

returns the type of the current process (one of
"FS", "DOS", "VIO", "PM", "DETACH" and "UNKNOWN"), or C<undef> on error.

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
add extension F<.exe> if no extension is present (add extension F<.>
to suppress).

=item C<@list = process_codepages()>

the first element is the currently active codepage, up to 2 additional
entries specify the system's "prepared codepages": the codepages the
user can switch to.  The active codepage of a process is one of the
prepared codepages of the system (if present).

=item C<process_codepage_set($cp)>

sets the currently active codepage.  [Affects printer output, in/out
codepages of sessions started by this process, and the default
codepage for drawing in PM; is inherited by kids.  Does not affect the
out- and in-codepages of the session.]

=item ppid()

returns the PID of the parent process.

=item C<ppidOf($pid = $$)>

returns the PID of the parent process of $pid.  -1 on error.

=item C<sidOf($pid = $$)>

returns the session id of the process id $pid.  -1 on error.

=back

=head2 Control of VIO sessions

VIO applications are applications running in a text-mode session.

=over

=item out_codepage()

gets code page used for screen output (glyphs).  -1 means that a user font
was loaded.

=item C<out_codepage_set($cp)>

sets code page used for screen output (glyphs).  -1 switches to a preloaded
user font.  -2 switches off the preloaded user font.

=item in_codepage()

gets code page used for keyboard input.  0 means that a hardware codepage
is used.

=item C<in_codepage_set($cp)>

sets code page used for keyboard input.

=item C<($w, $h) = scrsize()>

width and height of the given console window in character cells.

=item C<scrsize_set([$w, ] $h)>

set height (and optionally width) of the given console window in
character cells.  Use 0 size to keep the old size.

=item C<($s, $e, $w, $a) = cursor()>

gets start/end lines of the blinking cursor in the charcell, its width
(1 on text modes) and attribute (-1 for hidden, in text modes other
values mean visible, in graphic modes color).

=item C<cursor_set($s, $e, [$w [, $a]])>

sets start/end lines of the blinking cursor in the charcell.  Negative
values mean percents of the character cell height.

=item screen()

gets a buffer with characters and attributes of the screen.

=item C<screen_set($buffer)>

restores the screen given the result of screen().  E.g., if the file
C<$file> contains the screen contents, then

  open IN, $file or die;
  binmode IN;
  read IN, $in, -s IN;
  $s = screen;
  $in .= qq(\0) x (length($s) - length $in);
  substr($in, length $s) = '';
  screen_set $in;

will restore the screen content even if the height of the window
changed (if the width changed, more manipulation is needed).

=back

=head2 Control of the process list

With the exception of Title_set(), all these calls require that PM is
running, they would not work under alternative Session Managers.

=over

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

=item

switch-entry handle.

=back

Optional arguments: the pid and the window-handle of the application running
in the OS/2 session to query.

=item process_hentry()

similar to process_entry(), but returns a hash reference, the keys being

  title owner_hwnd icon_hwnd owner_phandle owner_pid owner_sid
  visible nonswitchable jumpable ptype sw_entry

(a copy of the list of keys is in @hentry_fields).

=item process_entries()

similar to process_entry(), but returns a list of array reference for all
the elements in the switch list (one controlling C<Ctrl-Esc> window).

=item process_hentries()

similar to process_hentry(), but returns a list of hash reference for all
the elements in the switch list (one controlling C<Ctrl-Esc> window).

=item change_entry()

changes a process entry, arguments are the same as process_entry() returns.

=item change_entryh()

Similar to change_entry(), but takes a hash reference as an argument.

=item process_hwnd()

returns the C<owner_hwnd> of the process entry (for VIO windowed processes
this is the frame window of the session).

=item Title()

returns the text of the task switch menu entry of the current session.
(There is no way to get this info in non-standard Session Managers.  This
implementation is a shortcut via process_entry().)

=item C<Title_set(newtitle)>

tries two different interfaces.  The Session Manager one does not work
with some windows (if the title is set from the start).
This is a limitation of OS/2, in such a case $^E is set to 372 (type

  help 372

for a funny - and wrong  - explanation ;-).  In such cases a
direct-manipulation of low-level entries is used (same as bothTitle_set()).
Keep in mind that some versions of OS/2 leak memory with such a manipulation.

=item winTitle()

returns text of the titlebar of the current process' window.

=item C<winTitle_set(newtitle)>

sets text of the titlebar of the current process' window.  The change does not
affect the text of the switch entry of the current window.

=item C<swTitle_set(newtitle)>

sets text of the task switch menu entry of the current process' window.  [There
is no API to query this title.]  Does it via SwitchEntry interface,
not Session manager interface.  The change does not affect the text of the
titlebar of the current window.

=item C<bothTitle_set(newtitle)>

sets text of the titlebar and task switch menu of the current process' window
via direct manipulation of the windows' texts.

=item C<SwitchToProgram([$sw_entry])>

switch to session given by a switch list handle (defaults to the entry of our process).

Use of this function causes another window (and its related windows)
of a PM session to appear on the front of the screen, or a switch to
another session in the case of a non-PM program. In either case,
the keyboard (and mouse for the non-PM case) input is directed to
the new program.

=back

=head2 Control of the PM windows

Some of these API's require sending a message to the specified window.
In such a case the process needs to be a PM process, or to be morphed
to a PM process via OS2::MorphPM().

For a temporary morphing to PM use L<OS2::localMorphPM class>.

Keep in mind that PM windows are engaged in 2 "orthogonal" window
trees, as well as in the z-order list.

One tree is given by the I<parent/child> relationship.  This
relationship affects drawing (child is drawn relative to its parent
(lower-left corner), and the drawing is clipped by the parent's
boundary; parent may request that I<it's> drawing is clipped to be
confined to the outsize of the childs and/or siblings' windows);
hiding; minimizing/restoring; and destroying windows.

Another tree (not necessarily connected?) is given by I<ownership>
relationship.  Ownership relationship assumes cooperation of the
engaged windows via passing messages on "important events"; e.g.,
scrollbars send information messages when the "bar" is moved, menus
send messages when an item is selected; frames
move/hide/unhide/minimize/restore/change-z-order-of owned frames when
the owner is moved/etc., and destroy the owned frames (even when these
frames are not descendants) when the owner is destroyed; etc.  [An
important restriction on ownership is that owner should be created by
the same thread as the owned thread, so they engage in the same
message queue.]

Windows may be in many different state: Focused (take keyboard events) or not,
Activated (=Frame windows in the I<parent/child> tree between the root and
the window with the focus; usually indicate such "active state" by titlebar
highlights, and take mouse events) or not, Enabled/Disabled (this influences
the ability to update the graphic, and may change appearance, as for 
enabled/disabled buttons), Visible/Hidden, Minimized/Maximized/Restored, Modal
or not, etc.

The APIs below all die() on error with the message being $^E.

=over

=item C<WindowText($hwnd)>

gets "a text content" of a window.  Requires (morphing to) PM.

=item C<WindowText_set($hwnd, $text)>

sets "a text content" of a window.  Requires (morphing to) PM.

=item C<($x, $y, $flags, $width, $height, $behind, @rest) = WindowPos($hwnd)>

gets window position info as 8 integers (of C<SWP>), in the order suitable
for WindowPos_set().  @rest is marked as "reserved" in PM docs.  $flags
is a combination of C<SWP_*> constants.

=item C<$hash = hWindowPos($hwnd)>

gets window position info as a hash reference; the keys are C<flags width
height x y behind hwnd reserved1 reserved2>.

Example:

  exit unless $hash->{flags} & SWP_MAXIMIZE;	# Maximized

=item C<WindowPos_set($hwnd, $x, $y, $flags = SWP_MOVE, $width = 0, $height = 0, $behind = HWND_TOP)>

Set state of the window: position, size, zorder, show/hide, activation,
minimize/maximize/restore etc.  Which of these operations to perform
is governed by $flags.

=item C<hWindowPos_set($hash, [$hwnd])>

Same as C<WindowPos_set>, but takes the position from keys C<fl width height
x y behind hwnd> of the hash referenced by $hash.  If $hwnd is explicitly
specified, it overrides C<$hash->{hwnd}>.  If $hash->{flags} is not specified,
it is calculated basing on the existing keys of $hash.  Requires (morphing to) PM.

Example:

  hWindowPos_set {flags => SWP_MAXIMIZE}, $hwnd; # Maximize

=item C<($pid, $tid) = WindowProcess($hwnd)>

gets I<PID> and I<TID> of the process associated to the window.

=item C<ClassName($hwnd)>

returns the class name of the window.

If this window is of any of the preregistered WC_* classes the class
name returned is in the form "#nnnnn", where "nnnnn" is a group
of up to five digits that corresponds to the value of the WC_* class name
constant.

=item FocusWindow()

returns the handle of the focus window.  Optional argument for specifying
the desktop to use.

=item C<FocusWindow_set($hwnd)>

set the focus window by handle.  Optional argument for specifying the desktop
to use.  E.g, the first entry in program_entries() is the C<Ctrl-Esc> list.
To show an application, use either one of

       WinShowWindow( $hwnd, 1 );
       FocusWindow_set( $hwnd );
       SwitchToProgram($switch_handle);

(Which work with alternative focus-to-front policies?)  Requires
(morphing to) PM.

Switching focus to currently-unfocused window moves the window to the
front in Z-order; use FocusWindow_set_keep_Zorder() to avoid this.

=item C<FocusWindow_set_keep_Zorder($hwnd)>

same as FocusWindow_set(), but preserves the Z-order of windows.

=item C<ActiveWindow([$parentHwnd])>

gets the active subwindow's handle for $parentHwnd or desktop.
Returns FALSE if none.

=item C<ActiveWindow_set($hwnd, [$parentHwnd])>

sets the active subwindow's handle for $parentHwnd or desktop.  Requires (morphing to) PM.

=item C<ShowWindow($hwnd [, $show])>

Set visible/hidden flag of the window.  Default: $show is TRUE.

=item C<EnableWindowUpdate($hwnd [, $update])>

Set window visibility state flag for the window for subsequent drawing.
No actual drawing is done at this moment.  Use C<ShowWindow($hwnd, $state)>
when redrawing is needed.  While update is disabled, changes to the "window
state" do not change the appearance of the window.  Default: $update is TRUE.

(What is manipulated is the bit C<WS_VISIBLE> of the window style.)

=item C<EnableWindow($hwnd [, $enable])>

Set the window enabled state.  Default: $enable is TRUE.

Results in C<WM_ENABLED> message sent to the window.  Typically, this
would change the appearence of the window.  If at the moment of disabling
focus is in the window (or a descendant), focus is lost (no focus anywhere).
If focus is needed, it can be reassigned explicitly later.

=item IsWindowEnabled(), IsWindowVisible(), IsWindowShowing()

these functions take $hwnd as an argument.  IsWindowEnabled() queries
the state changed by EnableWindow(), IsWindowVisible() the state changed
by ShowWindow(), IsWindowShowing() is true if there is a part of the window
visible on the screen.

=item C<PostMsg($hwnd, $msg, $mp1, $mp2)>

post message to a window.  The meaning of $mp1, $mp2 is specific for each
message id $msg, they default to 0.  E.g.,

  use OS2::Process qw(:DEFAULT WM_SYSCOMMAND WM_CONTEXTMENU
		      WM_SAVEAPPLICATION WM_QUIT WM_CLOSE
		      SC_MAXIMIZE SC_RESTORE);
  $hwnd = process_hentry()->{owner_hwnd};
  # Emulate choosing `Restore' from the window menu:
  PostMsg $hwnd, WM_SYSCOMMAND, MPFROMSHORT(SC_RESTORE); # Not immediate

  # Emulate `Show-Contextmenu' (Double-Click-2), two ways:
  PostMsg ActiveWindow, WM_CONTEXTMENU;
  PostMsg FocusWindow, WM_CONTEXTMENU;

  /* Emulate `Close' */
  PostMsg ActiveWindow, WM_CLOSE;

  /* Same but with some "warnings" to the application */
  $hwnd = ActiveWindow;
  PostMsg $hwnd, WM_SAVEAPPLICATION;
  PostMsg $hwnd, WM_CLOSE;
  PostMsg $hwnd, WM_QUIT;

In fact, MPFROMSHORT() may be omitted above.

For messages to other processes, messages which take/return a pointer are
not supported.

=item C<MP*()>

The functions MPFROMSHORT(), MPVOID(), MPFROMCHAR(), MPFROM2SHORT(),
MPFROMSH2CH(), MPFROMLONG() can be used the same way as from C.  Use them
to construct parameters $m1, $m2 to PostMsg().

These functions are not exported by default.

=item C<$eh = BeginEnumWindows($hwnd)>

starts enumerating immediate child windows of $hwnd in z-order.  The
enumeration reflects the state at the moment of BeginEnumWindows() calls;
use IsWindow() to be sure.  All the functions in this group require (morphing to) PM.

=item C<$kid_hwnd = GetNextWindow($eh)>

gets the next kid in the list.  Gets 0 on error or when the list ends.

=item C<EndEnumWindows($eh)>

End enumeration and release the list.

=item C<@list = ChildWindows([$hwnd])>

returns the list of child windows at the moment of the call.  Same remark
as for enumeration interface applies.  Defaults to HWND_DESKTOP.
Example of usage:

  sub l {
    my ($o,$h) = @_;
    printf ' ' x $o . "%#x\n", $h;
    l($o+2,$_) for ChildWindows $h;
  }
  l 0, $HWND_DESKTOP

=item C<IsWindow($hwnd)>

true if the window handle is still valid.

=item C<QueryWindow($hwnd, $type)>

gets the handle of a related window.  $type should be one of C<QW_*> constants.

=item C<IsChild($hwnd, $parent)>

return TRUE if $hwnd is a descendant of $parent.

=item C<WindowFromId($hwnd, $id)>

return a window handle of a child of $hwnd with the given $id.

  hwndSysMenu = WinWindowFromID(hwndDlg, FID_SYSMENU);
  WinSendMsg(hwndSysMenu, MM_SETITEMATTR,
      MPFROM2SHORT(SC_CLOSE, TRUE),
      MPFROM2SHORT(MIA_DISABLED, MIA_DISABLED));

=item C<WindowFromPoint($x, $y [, $hwndParent [, $descedantsToo]])>

gets a handle of a child of $hwndParent at C<($x,$y)>.  If $descedantsToo
(defaulting to 1) then children of children may be returned too.  May return
$hwndParent (defaults to desktop) if no suitable children are found,
or 0 if the point is outside the parent.

$x and $y are relative to $hwndParent.

=item C<EnumDlgItem($dlgHwnd, $type [, $relativeHwnd])>

gets a dialog item window handle for an item of type $type of $dlgHwnd
relative to $relativeHwnd, which is descendant of $dlgHwnd.
$relativeHwnd may be specified if $type is EDI_FIRSTTABITEM or
EDI_LASTTABITEM.

The return is always an immediate child of hwndDlg, even if hwnd is
not an immediate child window.  $type may be

=over

=item EDI_FIRSTGROUPITEM

First item in the same group.

=item EDI_FIRSTTABITEM

First item in dialog with style WS_TABSTOP. hwnd is ignored.

=item EDI_LASTGROUPITEM

Last item in the same group.

=item EDI_LASTTABITEM

Last item in dialog with style WS_TABSTOP. hwnd is ignored.

=item EDI_NEXTGROUPITEM

Next item in the same group. Wraps around to beginning of group when
the end of the group is reached.

=item EDI_NEXTTABITEM

Next item with style WS_TABSTOP. Wraps around to beginning of dialog
item list when end is reached.

=item EDI_PREVGROUPITEM

Previous item in the same group. Wraps around to end of group when the
start of the group is reached. For information on the WS_GROUP style,
see Window Styles.

=item EDI_PREVTABITEM

Previous item with style WS_TABSTOP. Wraps around to end of dialog
item list when beginning is reached.

=back

=item DesktopWindow()

gets the actual window handle of the PM desktop; most APIs accept the
pseudo-handle C<HWND_DESKTOP> instead.  Keep in mind that the WPS
desktop (one with WindowText() being C<"Desktop">) is a different beast?!

=item TopLevel($hwnd)

gets the toplevel window of $hwnd.

=item ResetWinError()

Resets $^E.  One may need to call it before the C<Win*>-class APIs which may
return 0 during normal operation.  In such a case one should check both
for return value being zero and $^E being non-zero.  The following APIs
do ResetWinError() themselves, thus do not need an explicit one:

  WindowPtr
  WindowULong
  WindowUShort
  WindowTextLength
  ActiveWindow
  PostMsg

This function is normally not needed.  Not exported by default.

=back

=head2 Control of the PM data

=over

=item ActiveDesktopPathname()

gets the path of the directory which corresponds to Desktop.

=item ClipbrdText()

gets the content of the clipboard.  An optional argument is the format
of the data in the clipboard (defaults to C<CF_TEXT>).

Note that the usual convention is to have clipboard data with
C<"\r\n"> as line separators.

=item ClipbrdText_set($txt)

sets the text content of the clipboard.  Unless the optional argument
is TRUE, will convert newlines to C<"\r\n">.  Another optional
argument is the format of the data in the clipboard (defaults to
C<CF_TEXT>).

=item 	InvalidateRect

=item	CreateFrameControl

=item	ClipbrdFmtInfo

=item	ClipbrdOwner

=item	ClipbrdViewer

=item	ClipbrdData

=item	OpenClipbrd

=item	CloseClipbrd

=item	ClipbrdData_set

=item	ClipbrdOwner_set

=item	ClipbrdViewer_set

=item	EnumClipbrdFmts

=item	EmptyClipbrd

=item	AddAtom

=item	FindAtom

=item	DeleteAtom

=item	AtomUsage

=item	AtomName

=item	AtomLength

=item	SystemAtomTable

=item	CreateAtomTable

=item	DestroyAtomTable

Low-level methods to access clipboard and the atom table(s).

=back

=head1 OS2::localMorphPM class

This class morphs the process to PM for the duration of the given scope.

  {
    my $h = OS2::localMorphPM->new(0);
    # Do something
  }

The argument has the same meaning as one to OS2::MorphPM().  Calls can
nest with internal ones being NOPs.

=head1 TODO

Add tests for:

	SwitchToProgram
	ClassName
	out_codepage
	out_codepage_set
	in_codepage
	in_codepage_set
	cursor
	cursor_set
	screen
	screen_set
	process_codepages
	QueryWindow
	EnumDlgItem
        WindowPtr
        WindowULong
        WindowUShort
        SetWindowBits
        SetWindowPtr
        SetWindowULong
        SetWindowUShort
	my_type
	file_type
	scrsize
	scrsize_set

Document and test: Query/SetWindowULong/Short/Ptr, SetWindowBits.
InvalidateRect, CreateFrameControl, ClipbrdFmtInfo ClipbrdOwner
ClipbrdViewer ClipbrdData OpenClipbrd CloseClipbrd ClipbrdData_set
ClipbrdOwner_set ClipbrdViewer_set EnumClipbrdFmts EmptyClipbrd
AddAtom FindAtom DeleteAtom AtomUsage AtomName AtomLength
SystemAtomTable CreateAtomTable DestroyAtomTable

Implement SOMETHINGFROMMR.


  >But I wish to change the default button if the user enters some
  >text into an entryfield.  I can detect the entry ok, but can't
  >seem to get the button to change to default.
  >
  >No matter what message I send it, it's being ignored.

  You need to get the style of the buttons using WinQueryWindowULong/QWL_STYLE,
  set and reset the BS_DEFAULT bits as appropriate and then use
  WinSetWindowULong/QWL_STYLE to set the button style.
  Something like this:
    hwnd1 = WinWindowFromID (hwnd, id1);
    hwnd2 = WinWindowFromID (hwnd, id2);
    style1 = WinQueryWindowULong (hwnd1, QWL_STYLE);
    style2 = WinQueryWindowULong (hwnd2, QWL_STYLE);
    style1 |= style2 & BS_DEFAULT;
    style2 &= ~BS_DEFAULT;
    WinSetWindowULong (hwnd1, QWL_STYLE, style1);
    WinSetWindowULong (hwnd2, QWL_STYLE, style2);

 > How to do query and change a frame creation flags for existing window?

 Set the style bits that correspond to the FCF_* flag for the frame
 window and then send a WM_UPDATEFRAME message with the appropriate FCF_*
 flag in mp1.

 ULONG ulFrameStyle;
 ulFrameStyle = WinQueryWindowULong( WinQueryWindow(hwnd, QW_PARENT),
 QWL_STYLE );
 ulFrameStyle = (ulFrameStyle & ~FS_SIZEBORDER) | FS_BORDER;
 WinSetWindowULong(   WinQueryWindow(hwnd, QW_PARENT),
                      QWL_STYLE,
                      ulFrameStyle );
 WinSendMsg( WinQueryWindow(hwnd, QW_PARENT),
             WM_UPDATEFRAME,
             MPFROMP(FCF_SIZEBORDER),
             MPVOID );

 If the FCF_* flags you want to change does not have a corresponding FS_*
 style (i.e. the FCF_* flag corresponds to the presence/lack of a frame
 control rather than a property of the frame itself) then you create or
 destroy the appropriate control window using the correct FID_* window
 identifier and then send the WM_UPDATEFRAME message with the appropriate
 FCF_* flag in mp1.

 /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*
  |  SetFrameBorder()                                                          |
  |    Changes a frame window's border to the requested type.                  |
  |                                                                            |
  |  Parameters on entry:                                                      |
  |    hwndFrame     -> Frame window whose border is to be changed.            |
  |    ulBorderStyle -> Type of border to change to.                           |
  |                                                                            |
  |  Returns:                                                                  |
  |    BOOL          -> Success indicator.                                     |
  |                                                                            |
  * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
 BOOL SetFrameBorder( HWND hwndFrame, ULONG ulBorderType )  {
   ULONG  ulFrameStyle;
   BOOL   fSuccess = TRUE;

   ulFrameStyle = WinQueryWindowULong( hwndFrame, QWL_STYLE );

   switch ( ulBorderType )  {
     case FS_SIZEBORDER :
       ulFrameStyle = (ulFrameStyle & ~(FS_DLGBORDER | FS_BORDER))
                      | FS_SIZEBORDER;
       break;

     case FS_DLGBORDER :
       ulFrameStyle = (ulFrameStyle & ~(FS_SIZEBORDER | FS_BORDER))
                      | FS_DLGBORDER;
       break;

     case FS_BORDER :
       ulFrameStyle = (ulFrameStyle & ~(FS_SIZEBORDER | FS_DLGBORDER))
                      | FS_BORDER;
       break;

     default :
       fSuccess = FALSE;
       break;
   }  // end switch

   if ( fSuccess )  {
     fSuccess = WinSetWindowULong( hwndFrame, QWL_STYLE, ulFrameStyle );

     if ( fSuccess )  {
       fSuccess = (BOOL)WinSendMsg( hwndFrame, WM_UPDATEFRAME, 0, 0 );
       if ( fSuccess )
         fSuccess = WinInvalidateRect( hwndFrame, NULL, TRUE );
     }
   }

   return ( fSuccess );

 }  // End SetFrameBorder()

         hwndMenu=WinLoadMenu(hwndParent,NULL,WND_IMAGE);
         WinSetWindowUShort(hwndMenu,QWS_ID,FID_MENU);
         ulStyle=WinQueryWindowULong(hwndMenu,QWL_STYLE);
         WinSetWindowULong(hwndMenu,QWL_STYLE,ulStyle|MS_ACTIONBAR);
         WinSendMsg(hwndParent,WM_UPDATEFRAME,MPFROMSHORT(FCF_MENU),0L);

  OS/2-windows have another "parent" called the *owner*,
  which must be set separately - to get a close relationship:

    WinSetOwner (hwndFrameChild, hwndFrameMain);

  Now your child should move with your main window!
  And always stays on top of it....

  To avoid this, for example for dialogwindows, you can
  also "disconnect" this relationship with:

    WinSetWindowBits (hwndFrameChild, QWL_STYLE
                      , FS_NOMOVEWITHOWNER
                      , FS_NOMOVEWITHOWNER);

 Adding a button icon later:

 /* switch the button style to BS_MINIICON */
 WinSetWindowBits(hwndBtn, QWL_STYLE, BS_MINIICON, BS_MINIICON) ;

 /* set up button control data */
 BTNCDATA    bcd;
 bcd.cb = sizeof(BTNCDATA);
 bcd.hImage = WinLoadPointer(HWND_DESKTOP, dllHandle, ID_ICON_BUTTON1) ;
 bcd.fsCheckState = bcd.fsHiliteState = 0 ;


 WNDPARAMS   wp;
 wp.fsStatus = WPM_CTLDATA;
 wp.pCtlData = &bcd;

 /* add the icon on the button */
 WinSendMsg(hwndBtn, WM_SETWINDOWPARAMS, (MPARAM)&wp, NULL);

 MO> Can anyone tell what OS/2 expects of an application to be properly
 MO> minimized to the desktop?
 case WM MINMAXFRAME :
 {
   BOOL  fShow = ! (((PSWP) mp1)->fl & SWP MINIMIZE);
   HENUM henum;

   HWND  hwndChild;

   WinEnableWindowUpdate ( hwnd, FALSE );

   for (henum=WinBeginEnumWindows(hwnd);
        (hwndChild = WinGetNextWindow (henum)) != 0; )
   WinShowWindow ( hwndChild, fShow );

   WinEndEnumWindows ( henum );
   WinEnableWindowUpdate ( hwnd, TRUE );
 }
 break;

Why C<hWindowPos DesktopWindow> gives C<< behind => HWND_TOP >>?

=head1 $^E

the majority of the APIs of this module set $^E on failure (no matter
whether they die() on failure or not).  By the semantic of PM API
which returns something other than a boolean, it is impossible to
distinguish failure from a "normal" 0-return.  In such cases C<$^E ==
0> indicates an absence of error.

=head1 EXPORTS

In addition to symbols described above, the following constants (available
also via module C<OS2::Process::Const>) are exportable.  Note that these
symbols live in package C<OS2::Process::Const>, they are not available
by full name through C<OS2::Process>!

  HWND_*		Standard (abstract) window handles
  WM_*			Message ids
  SC_*			WM_SYSCOMMAND flavor
  SWP_*			Size/move etc flag
  WC_*			Standard window classes
  PROG_*		Program category (PM, VIO etc)
  QW_*			Query-Window flag
  EDI_*			Enumerate-Dialog-Item code
  WS_*			Window Style flag
  QWS_*			Query-window-UShort offsets
  QWP_*			Query-window-pointer offsets
  QWL_*			Query-window-ULong offsets
  FF_*			Frame-window state flags
  FI_*			Frame-window information flags
  LS_*			List box styles
  FS_*			Frame style
  FCF_*			Frame creation flags
  BS_*			Button style
  MS_*			Menu style
  TBM_*			Title bar messages?
  CF_*			Clipboard formats
  CFI_*			Clipboard storage type
  FID_*			ids of subwindows of frames

=head1 BUGS

whether a given API dies or returns FALSE/empty-list on error may be
confusing.  This may change in the future.

=head1 AUTHOR

Andreas Kaiser <ak@ananke.s.bawue.de>,
Ilya Zakharevich <ilya@math.ohio-state.edu>.

=head1 SEE ALSO

C<spawn*>() system calls, L<OS2::Proc> and L<OS2::WinObject> modules.

=cut
