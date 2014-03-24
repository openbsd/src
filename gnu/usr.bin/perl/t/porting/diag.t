#!/usr/bin/perl
use warnings;
use strict;

BEGIN {
  chdir 't';
  require './test.pl';
}

plan('no_plan');

$|=1;

# --make-exceptions-list outputs the list of strings that don't have
# perldiag.pod entries to STDERR without TAP formatting, so they can
# easily be put in the __DATA__ section of this file.  This was done
# initially so as to not create new test failures upon the initial
# creation of this test file.  You probably shouldn't do it again.
# Just add the documentation instead.
my $make_exceptions_list = ($ARGV[0]||'') eq '--make-exceptions-list';

chdir '..' or die "Can't chdir ..: $!";
BEGIN { defined $ENV{PERL_UNICODE} and push @INC, "lib"; }

my @functions;

open my $func_fh, "<", "embed.fnc" or die "Can't open embed.fnc: $!";

# Look for functions in embed.fnc that look like they could be diagnostic ones.
while (<$func_fh>) {
  chomp;
  s/^\s+//;
  while (s/\s*\\$//) {      # Grab up all continuation lines, these end in \
    my $next = <$func_fh>;
    $next =~ s/^\s+//;
    chomp $next;
    $_ .= $next;
  }
  next if /^:/;     # Lines beginning with colon are comments.
  next unless /\|/; # Lines without a vertical bar are something we can't deal
                    # with
  my @fields = split /\s*\|\s*/;
  next unless $fields[2] =~ /warn|err|(\b|_)die|croak/i;
  push @functions, $fields[2];

  # The flag p means that this function may have a 'Perl_' prefix
  # The flag s means that this function may have a 'S_' prefix
  push @functions, "Perl_$fields[2]", if $fields[0] =~ /p/;
  push @functions, "S_$fields[2]", if $fields[0] =~ /s/;
}

close $func_fh;

my $regcomp_re = "(?<routine>(?:ckWARN(?:\\d+)?reg\\w*|vWARN\\d+))";
my $function_re = join '|', @functions;
my $regcomp_fail_re = '\b(?:(?:Simple_)?v)?FAIL[2-4]?\b';
my $source_msg_re =
   "(?<routine>\\bDIE\\b|$function_re|$regcomp_fail_re)";
my $text_re = '"(?<text>(?:\\\\"|[^"]|"\s*[A-Z_]+\s*")*)"';
my $source_msg_call_re = qr/$source_msg_re(?:_nocontext)? \s*
    \(aTHX_ \s*
    (?:packWARN\d*\((?<category>.*?)\),)? \s*
    $text_re /x;
my $bad_version_re = qr{BADVERSION\([^"]*$text_re};
   $regcomp_fail_re = qr/$regcomp_fail_re\([^"]*$text_re/;
my $regcomp_call_re = qr/$regcomp_re.*?$text_re/;

my %entries;

# Get the ignores that are compiled into this file
my $reading_categorical_exceptions;
while (<DATA>) {
  chomp;
  $entries{$_}{$reading_categorical_exceptions ? 'cattodo' : 'todo'}=1;
  /__CATEGORIES__/ and ++$reading_categorical_exceptions;
}

my $pod = "pod/perldiag.pod";
my $cur_entry;
open my $diagfh, "<", $pod
  or die "Can't open $pod: $!";

my $category_re = qr/ [a-z0-9_:]+?/;    # Note: requires an initial space
my $severity_re = qr/ . (?: \| . )* /x; # A severity is a single char, but can
                                        # be of the form 'S|P|W'
my @same_descr;
while (<$diagfh>) {
  if (m/^=item (.*)/) {
    $cur_entry = $1;

    # Allow multi-line headers
    while (<$diagfh>) {
      if (/^\s*$/) {
        last;
      }

      $cur_entry .= $_;
    }

    $cur_entry =~ s/\n/ /gs; # Fix multi-line headers if they have \n's
    $cur_entry =~ s/\s+\z//;

    if (exists $entries{$cur_entry} && $entries{$cur_entry}{todo}) {
        TODO: {
            local $::TODO = "Remove the TODO entry \"$cur_entry\" from DATA as it is already in $pod near line $.";
            ok($cur_entry);
        }
    }
    # Make sure to init this here, so an actual entry in perldiag
    # overwrites one in DATA.
    $entries{$cur_entry}{todo} = 0;
    $entries{$cur_entry}{line_number} = $.;
  }

  next if ! defined $cur_entry;

  if (! $entries{$cur_entry}{severity}) {
    if (/^ \( ( $severity_re )

        # Can have multiple categories separated by commas
        ( $category_re (?: , $category_re)* )? \) /x)
    {
      $entries{$cur_entry}{severity} = $1;
      $entries{$cur_entry}{category} =
        $2 && join ", ", sort split " ", $2 =~ y/,//dr;

      # Record it also for other messages sharing the same description
      @$_{qw<severity category>} =
        @{$entries{$cur_entry}}{qw<severity category>}
       for @same_descr;
    }
    elsif (! $entries{$cur_entry}{first_line} && $_ =~ /\S/) {

      # Keep track of first line of text if doesn't contain a severity, so
      # that can later examine it to determine if that is ok or not
      $entries{$cur_entry}{first_line} = $_;
    }
    if (/\S/) {
      @same_descr = ();
    }
    else {
      push @same_descr, $entries{$cur_entry};
    }
  }
}

foreach my $cur_entry ( keys %entries) {
    next if $entries{$cur_entry}{todo}; # If in this file, won't have a severity
    if (! exists $entries{$cur_entry}{severity}

            # If there is no first line, it was two =items in a row, so the
            # second one is the one with with text, not this one.
        && exists $entries{$cur_entry}{first_line}

            # If the first line refers to another message, no need for severity
        && $entries{$cur_entry}{first_line} !~ /^See/)
    {
        fail($cur_entry);
        diag(
            "   $pod entry at line $entries{$cur_entry}{line_number}\n"
          . "       \"$cur_entry\"\n"
          . "   is missing a severity and/or category"
        );
    }
}

# List from perlguts.pod "Formatted Printing of IVs, UVs, and NVs"
# Convert from internal formats to ones that the readers will be familiar
# with, while removing any format modifiers, such as precision, the
# presence of which would just confuse the pod's explanation
my %specialformats = (IVdf => 'd',
		      UVuf => 'd',
		      UVof => 'o',
		      UVxf => 'x',
		      UVXf => 'X',
		      NVef => 'f',
		      NVff => 'f',
		      NVgf => 'f',
		      HEKf256=>'s',
		      HEKf => 's',
		      SVf256=>'s',
		      SVf32=> 's',
		      SVf  => 's');
my $format_modifiers = qr/ [#0\ +-]*              # optional flags
			  (?: [1-9][0-9]* | \* )? # optional field width
			  (?: \. \d* )?           # optional precision
			  (?: h|l )?              # optional length modifier
			/x;

my $specialformats =
 join '|', sort { length $b cmp length $a } keys %specialformats;
my $specialformats_re = qr/%$format_modifiers"\s*($specialformats)(\s*")?/;

open my $fh, '<', 'MANIFEST' or die "Can't open MANIFEST: $!";
while (my $file = <$fh>) {
    chomp $file;
    $file =~ s/\s+.*//;
    next unless $file =~ /\.(?:c|cpp|h|xs|y)\z/ or $file =~ /^perly\./;
    # OS/2 extensions have never been migrated to ext/, hence the special case:
    next if $file =~ m!\A(?:ext|dist|cpan|lib|t|os2/OS2)/!
            && $file !~ m!\Aext/DynaLoader/!;
    check_file($file);
}
close $fh or die $!;

# Standardize messages with variants into the form that appears
# in perldiag.pod -- useful for things without a diag_listed_as annotation
sub standardize {
  my ($name) = @_;

  if    ( $name =~ m/^(Invalid strict version format) \([^\)]*\)/ ) {
    $name = "$1 (\%s)";
  }
  elsif ( $name =~ m/^(Invalid version format) \([^\)]*\)/ ) {
    $name = "$1 (\%s)";
  }
  elsif ($name =~ m/^panic: /) {
    $name = "panic: \%s";
  }

  return $name;
}

sub check_file {
  my ($codefn) = @_;

  print "# Checking $codefn\n";

  open my $codefh, "<", $codefn
    or die "Can't open $codefn: $!";

  my $listed_as;
  my $listed_as_line;
  my $sub = 'top of file';
  while (<$codefh>) {
    chomp;
    # Getting too much here isn't a problem; we only use this to skip
    # errors inside of XS modules, which should get documented in the
    # docs for the module.
    if (m<^[^#\s]> and $_ !~ m/^[{}]*$/) {
      $sub = $_;
    }
    next if $sub =~ m/^XS/;
    if (m</\*\s*diag_listed_as: (.*?)\s*\*/>) {
      $listed_as = $1;
      $listed_as_line = $.+1;
    }
    next if /^#/;

    my $multiline = 0;
    # Loop to accumulate the message text all on one line.
    if (m/(?:$source_msg_re(?:_nocontext)?|$regcomp_re)\s*\(/) {
      while (not m/\);$/) {
        my $nextline = <$codefh>;
        # Means we fell off the end of the file.  Not terribly surprising;
        # this code tries to merge a lot of things that aren't regular C
        # code (preprocessor stuff, long comments).  That's OK; we don't
        # need those anyway.
        last if not defined $nextline;
        chomp $nextline;
        $nextline =~ s/^\s+//;
        $_ =~ s/\\$//;
        # Note that we only want to do this where *both* are true.
        if ($_ =~ m/"$/ and $nextline =~ m/^"/) {
          $_ =~ s/"$//;
          $nextline =~ s/^"//;
        }
        $_ .= $nextline;
        ++$multiline;
      }
    }
    # This should happen *after* unwrapping, or we don't reformat the things
    # in later lines.

    s/$specialformats_re/"%$specialformats{$1}" .  (defined $2 ? '' : '"')/ge;

    # Remove any remaining format modifiers, but not in %%
    s/ (?<!%) % $format_modifiers ( [dioxXucsfeEgGp] ) /%$1/xg;

    # The %"foo" thing needs to happen *before* this regex.
    # diag($_);
    # DIE is just return Perl_die
    my ($name, $category, $routine);
    if (/$source_msg_call_re/) {
      ($name, $category, $routine) = ($+{'text'}, $+{'category'}, $+{'routine'});
      # Sometimes the regexp will pick up too much for the category
      # e.g., WARN_UNINITIALIZED), PL_warn_uninit_sv ... up to the next )
      $category && $category =~ s/\).*//s;
    }
    elsif (/$bad_version_re/) {
      ($name, $category) = ($+{'text'}, undef);
    }
    elsif (/$regcomp_fail_re/) {
      #  FAIL("foo") -> "foo in regex m/%s/"
      # vFAIL("foo") -> "foo in regex; marked by <-- HERE in m/%s/"
      ($name, $category) = ($+{'text'}, undef);
      $name .=
        " in regex" . ("; marked by <-- HERE in" x /vFAIL/) . " m/%s/";
    }
    elsif (/$regcomp_call_re/) {
      # vWARN/ckWARNreg("foo") -> "foo in regex; marked by <-- HERE in m/%s/
      ($name, $category, $routine) = ($+{'text'}, undef, $+{'routine'});
      $name .= " in regex; marked by <-- HERE in m/%s/";
      $category = 'WARN_REGEXP';
      if ($routine =~ /dep/) {
        $category .= ',WARN_DEPRECATED';
      }
    }
    else {
      next;
    }

    # Try to guess what the severity should be.  In the case of
    # Perl_ck_warner and other _ck_ functions, we can tell whether it is
    # a severe/default warning or no by the _d suffix.  In the case of
    # other warn functions we cannot tell, because Perl_warner may be pre-
    # ceded by if(ckWARN) or if(ckWARN_d).
    my $severity = !$routine                   ? '[PFX]'
                 :  $routine =~ /warn.*_d\z/   ? '[DS]'
                 :  $routine =~ /ck_warn/      ?  'W'
                 :  $routine =~ /warn/         ? '[WDS]'
                 :  $routine =~ /ckWARN.*dep/  ?  'D'
                 :  $routine =~ /ckWARN\d*reg/ ?  'W'
                 :  $routine =~ /vWARN\d/      ? '[WDS]'
                 :                             '[PFX]';
    my $categories;
    if (defined $category) {
      $category =~ s/__/::/g;
      $categories =
        join ", ",
              sort map {s/^WARN_//; lc $_} split /\s*[|,]\s*/, $category;
    }
    if ($listed_as and $listed_as_line == $. - $multiline) {
      $name = $listed_as;
    } else {
      # The form listed in perldiag ignores most sorts of fancy printf
      # formatting, or makes it more perlish.
      $name =~ s/%%/%/g;
      $name =~ s/%l[ud]/%d/g;
      $name =~ s/%\.(\d+|\*)s/\%s/g;
      $name =~ s/(?:%s){2,}/%s/g;
      $name =~ s/(\\")|("\s*[A-Z_]+\s*")/$1 ? '"' : '%s'/egg;
      $name =~ s/\\t/\t/g;
      $name =~ s/\\n/\n/g;
      $name =~ s/\s+$//;
      $name =~ s/(\\)\\/$1/g;
    }

    # Extra explanatory info on an already-listed error, doesn't
    # need it's own listing.
    next if $name =~ m/^\t/;

    # Happens fairly often with PL_no_modify.
    next if $name eq '%s';

    # Special syntax for magic comment, allows ignoring the fact
    # that it isn't listed.  Only use in very special circumstances,
    # like this script failing to notice that the Perl_croak call is
    # inside an #if 0 block.
    next if $name eq 'SKIPME';

    next if $name=~/\[TESTING\]/; # ignore these as they are works in progress

    check_message(standardize($name),$codefn,$severity,$categories);
  }
}

sub check_message {
    my($name,$codefn,$severity,$categories,$partial) = @_;
    my $key = $name =~ y/\n/ /r;
    my $ret;

    # Try to reduce printf() formats to simplest forms
    # Really this should be matching %s, etc like diagnostics.pm does

    # Kill flags
    $key =~ s/%[#0\-+]/%/g;

    # Kill width
    $key =~ s/\%(\d+|\*)/%/g;

    # Kill precision
    $key =~ s/\%\.(\d+|\*)/%/g;

    if (exists $entries{$key}) {
      $ret = 1;
      if ( $entries{$key}{seen}++ ) {
        # no need to repeat entries we've tested
      } elsif ($entries{$key}{todo}) {
        TODO: {
          no warnings 'once';
          local $::TODO = 'in DATA';
          # There is no listing, but it is in the list of exceptions.  TODO FAIL.
          fail($key);
          diag(
            "    Message '$name'\n    from $codefn line $. is not listed in $pod\n".
            "    (but it wasn't documented in 5.10 either, so marking it TODO)."
          );
        }
      } else {
        # We found an actual valid entry in perldiag.pod for this error.
        pass($key);

        # Now check the category and severity

        # Cache our severity qr thingies
        use 5.01;
        state %qrs;
        my $qr = $qrs{$severity} ||= qr/$severity/;

        return $ret
          if $entries{$key}{cattodo};

        like $entries{$key}{severity}, $qr,
          $severity =~ /\[/
            ? "severity is one of $severity for $key"
            : "severity is $severity for $key";

        is $entries{$key}{category}, $categories,
           ($categories ? "categories are [$categories]" : "no category")
             . " for $key";
      }
      # Later, should start checking that the severity is correct, too.
    } elsif ($partial) {
      # noop
    } else {
      my $ok;
      if ($name =~ /\n/) {
        $ok = 1;
        check_message($_,$codefn,$severity,$categories,1) or $ok = 0, last
          for split /\n/, $name;
      }
      if ($ok) {
        # noop
      } elsif ($make_exceptions_list) {
        # We're making an updated version of the exception list, to
        # stick in the __DATA__ section.  I honestly can't think of
        # a situation where this is the right thing to do, but I'm
        # leaving it here, just in case one of my descendents thinks
        # it's a good idea.
        print STDERR "$key\n";
      } else {
        # No listing found, and no excuse either.
        # Find the correct place in perldiag.pod, and add a stanza beginning =item $name.
        fail($name);
        diag("    Message '$name'\n    from $codefn line $. is not listed in $pod");
      }
      # seen it, so only fail once for this message
      $entries{$name}{seen}++;
    }

    die if $name =~ /%$/;
    return $ret;
}

# Lists all missing things as of the inauguration of this script, so we
# don't have to go from "meh" to perfect all at once.
# 
# PLEASE DO NOT ADD TO THIS LIST.  Instead, write an entry in
# pod/perldiag.pod for your new (warning|error).

# Entries after __CATEGORIES__ are those that are in perldiag but fail the
# severity/category test.

# Also FIXME this test, as the first entry in TODO *is* covered by the
# description: Malformed UTF-8 character (%s)
__DATA__
Malformed UTF-8 character (unexpected non-continuation byte 0x%x, immediately after start byte 0x%x)

'%c' allowed only after types %s in %s
bad top format reference
Cannot apply "%s" in non-PerlIO perl
Can't %s big-endian %ss on this
Can't call mro_isa_changed_in() on anonymous symbol table
Can't call mro_method_changed_in() on anonymous symbol table
Can't coerce readonly %s to string
Can't coerce readonly %s to string in %s
Can't find string terminator %c%s%c anywhere before EOF
Can't fix broken locale name "%s"
Can't get short module name from a handle
Can't locate object method "%s" via package "%s" (perhaps you forgot to load "%s"?)
Can't pipe "%s": %s
Can't spawn: %s
Can't spawn "%s": %s
Can't %s script `%s' with ARGV[0] being `%s'
Can't %s "%s": %s
Can't %s `%s' with ARGV[0] being `%s' (looking for executables only, not found)
Can't use string ("%s"%s) as a subroutine ref while "strict refs" in use
\%c better written as $%c
Character(s) in '%c' format wrapped in %s
chown not implemented!
clear %s
Code missing after '/' in pack
Code missing after '/' in unpack
'%c' outside of string in pack
Debug leaking scalars child failed%s with errno %d: %s
'/' does not take a repeat count in %s
Don't know how to get file name
Don't know how to handle magic of type \%o
-Dp not implemented on this platform
Error reading "%s": %s
execl not implemented!
EVAL without pos change exceeded limit in regex
Filehandle opened only for %sput
Filehandle %s opened only for %sput
Filehandle STD%s reopened as %s only for input
filter_del can only delete in reverse order (currently)
YOU HAVEN'T DISABLED SET-ID SCRIPTS IN THE KERNEL YET! FIX YOUR KERNEL, PUT A C WRAPPER AROUND THIS SCRIPT, OR USE -u AND UNDUMP!
fork() not implemented!
free %s
Free to wrong pool %p not %p
get %s %p %p %p
gethostent not implemented!
getpwnam returned invalid UIC %o for user "%s"
glob failed (can't start child: %s)
glob failed (child exited with status %d%s)
Goto undefined subroutine
Goto undefined subroutine &%s
Got signal %d
()-group starts with a count in %s
Illegal binary digit '%c' ignored
Illegal character %sin prototype for %s : %s
Illegal hexadecimal digit '%c' ignored
Illegal octal digit '%c' ignored
Infinite recursion in regex
internal %<num>p might conflict with future printf extensions
Invalid argument to sv_cat_decode
Invalid range "%c-%c" in transliteration operator
Invalid separator character %c%c%c in PerlIO layer specification %s
Invalid TOKEN object ignored
Invalid type '%c' in pack
Invalid type '%c' in %s
Invalid type '%c' in unpack
Invalid type ',' in %s
ioctlsocket not implemented!
'j' not supported on this platform
'J' not supported on this platform
killpg not implemented!
length() used on %s (did you mean "scalar(%s)"?)
length() used on %hash (did you mean "scalar(keys %hash)"?)
length() used on @array (did you mean "scalar(@array)"?)
List form of pipe open not implemented
Malformed integer in [] in %s
Malformed UTF-8 character (fatal)
Missing (suid) fd script name
More than one argument to open
More than one argument to open(,':%s')
mprotect for %p %u failed with %d
mprotect RW for %p %u failed with %d
No %s allowed while running setgid
No %s allowed with (suid) fdscript
No such class field "%s"
Not an XSUB reference
Operator or semicolon missing before %c%s
Pattern subroutine nesting without pos change exceeded limit in regex
Perl %s required--this is only %s, stopped
PerlApp::TextQuery: no arguments, please
POSIX syntax [%c %c] is reserved for future extensions in regex; marked by <-- HERE in m/%s/
ptr wrong %p != %p fl=%x nl=%p e=%p for %d
Recompile perl with -DDEBUGGING to use -D switch (did you mean -d ?)
Regexp modifier "%c" may appear a maximum of twice in regex; marked by <-- HERE in m/%s/
Regexp modifier "%c" may not appear twice in regex; marked by <-- HERE in m/%s/
Regexp modifiers "%c" and "%c" are mutually exclusive in regex; marked by <-- HERE in m/%s/
Regexp *+ operand could be empty in regex; marked by <-- HERE in m/%s/
Repeated format line will never terminate (~~ and @#)
Reversed %c= operator
%s(%f) failed
%sCompilation failed in require
Sequence (?%c...) not implemented in regex; marked by <-- HERE in m/%s/
Sequence (%s...) not recognized in regex; marked by <-- HERE in m/%s/
Sequence %s... not terminated in regex; marked by <-- HERE in m/%s/
Sequence (?%c... not terminated in regex; marked by <-- HERE in m/%s/
Sequence (?(%c... not terminated in regex; marked by <-- HERE in m/%s/
Sequence (?R) not terminated in regex m/%s/
set %s %p %p %p
%s free() ignored (RMAGIC, PERL_CORE)
%s has too many errors.
SIG%s handler "%s" not defined.
%s in %s
Size magic not implemented
%s number > %s non-portable
%srealloc() %signored
%s in regex m/%s/
%s on %s %s
socketpair not implemented!
Starting Full Screen process with flag=%d, mytype=%d
Starting PM process with flag=%d, mytype=%d
sv_2iv assumed (U_V(fabs((double)SvNVX(sv))) < (UV)IV_MAX) but SvNVX(sv)=%f U_V is 0x%x, IV_MAX is 0x%x
SWASHNEW didn't return an HV ref
switching effective gid is not implemented
switching effective uid is not implemented
System V IPC is not implemented on this machine
-T and -B not implemented on filehandles
Terminating on signal SIG%s(%d)
The crypt() function is not implemented on NetWare
The flock() function is not implemented on NetWare
The rewinddir() function is not implemented on NetWare
The seekdir() function is not implemented on NetWare
The telldir() function is not implemented on NetWare
Too deeply nested ()-groups in %s
Too many args on %s line of "%s"
U0 mode on a byte string
unable to find VMSPIPE.COM for i/o piping
Unknown Unicode option value %d
Unrecognized character %s; marked by <-- HERE after %s<-- HERE near column %d
Unstable directory path, current directory changed unexpectedly
Unterminated compressed integer in unpack
Unterminated \g... pattern in regex; marked by <-- HERE in m/%s/
Usage: CODE(0x%x)(%s)
Usage: %s(%s)
Usage: %s::%s(%s)
Usage: File::Copy::rmscopy(from,to[,date_flag])
Usage: VMS::Filespec::candelete(spec)
Usage: VMS::Filespec::fileify(spec)
Usage: VMS::Filespec::pathify(spec)
Usage: VMS::Filespec::rmsexpand(spec[,defspec])
Usage: VMS::Filespec::unixify(spec)
Usage: VMS::Filespec::unixpath(spec)
Usage: VMS::Filespec::unixrealpath(spec)
Usage: VMS::Filespec::vmsify(spec)
Usage: VMS::Filespec::vmspath(spec)
Usage: VMS::Filespec::vmsrealpath(spec)
Use of inherited AUTOLOAD for non-method %s::%s() is deprecated
utf8 "\x%X" does not map to Unicode
Value of logical "%s" too long. Truncating to %i bytes
waitpid: process %x is not a child of process %x
Wide character
Wide character in $/
Within []-length '*' not allowed in %s
Within []-length '%c' not allowed in %s
Wrong syntax (suid) fd script name "%s"
'X' outside of string in %s
'X' outside of string in unpack

__CATEGORIES__
Code point 0x%X is not Unicode, all \p{} matches fail; all \P{} matches succeed
Code point 0x%X is not Unicode, may not be portable
Illegal character \%o (carriage return)
Missing argument in %s
Unicode non-character U+%X is illegal for open interchange
Operation "%s" returns its argument for non-Unicode code point 0x%X
Operation "%s" returns its argument for UTF-16 surrogate U+%X
Unicode surrogate U+%X is illegal in UTF-8
UTF-16 surrogate U+%X
False [] range "%s" in regex; marked by <-- HERE in m/%s/
\N{} in character class restricted to one character in regex; marked by <-- HERE in m/%s/
Zero length \N{} in regex; marked by <-- HERE in m/%s/
Expecting '(?flags:(?[...' in regex; marked by <-- HERE in m/%s/
