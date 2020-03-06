#!/usr/bin/perl
#
# $Id: genchars.pl,v 2.22 2005/01/11 21:15:17 jonathan Exp $
#
##############################
$version="1.98";
##############################
use Config;

BEGIN { push @INC, "."; }
use Configure;
use constant SILENT =>
  (defined $ENV{MAKEFLAGS} and $ENV{MAKEFLAGS} =~ /\b(s|silent|quiet)\b/ ? 1 : 0);

#sub report {
#	my($prog)=join(" ",@_);
#
#  my($ccflags, $ldflags, $cc, $rm) = @Config{'ccflags', 'ldflags', 'cc', 'rm'};
#  my($command, $ret);
#
#  $command = $prog;
#  open(F, ">temp$$.c") || die "Can't make temp file temp$$.c! $!\n";
#  print F $command;
#  close F;
#
#  $command  = "$cc $ccflags -o temp$$ temp$$.c $ldfcrs $libcrs $ldflags -lbsd";
#  $command .= " >/dev/null 2>&1";
#  $ret = system $command;
#  #if(!$ret) { system "temp$$" }
#  unlink "temp$$", "temp$$.o", "temp$$.c";
#
#  return $ret;
#}

open(CCHARS,">cchars.h") || die "Fatal error, Unable to write to cchars.h!";

#print "Checking for termio...\n";
#$TERMIO = !report(	"#include <termio.h>\n	struct termios s; main(){}");
#print "	Termio ",($TERMIO?"":"NOT "),"found.\n";

#print "Checking for termios...\n";
#$TERMIOS = !report(	"#include <termios.h>\n	struct termio s;  main(){}");
#print "	Termios ",($TERMIOS?"":"NOT "),"found.\n";

#print "Checking for sgtty...\n";
#$SGTTY = !report(	"#include <sgtty.h>\n	struct sgttyb s;  main(){}");
#print "	Sgtty ",($SGTTY?"":"NOT "),"found.\n";

#print "Termio=$TERMIO, Termios=$TERMIOS, Sgtty=$SGTTY\n";

# Control characters used for termio and termios
%possible = (	VINTR	=>	"INTERRUPT",
		VQUIT	=>	"QUIT",
		VERASE	=>	"ERASE", 
		VKILL	=>	"KILL",
		VEOF	=> 	"EOF",
		VTIME	=>	"TIME",
		VMIN	=>	"MIN",
		VSWTC	=>	"SWITCH",
		VSWTCH	=>	"SWITCH",
		VSTART	=>	"START",
		VSTOP	=>	"STOP",
		VSUSP	=>	"SUSPEND",
		VDSUSP	=>	"DSUSPEND",
		VEOL	=>	"EOL",
		VREPRINT =>	"REPRINT",
		VDISCARD =>	"DISCARD",
		VFLUSH	=>	"DISCARD",
		VWERASE	=>	"ERASEWORD",
		VLNEXT	=>	"QUOTENEXT",
		VQUOTE  => 	"QUOTENEXT",
		VEOL2	=>	"EOL2",
		VSTATUS	=>	"STATUS",
);

# Control characters for sgtty
%possible2 = (	"intrc"	=>	"INTERRUPT",
		"quitc"	=>	"QUIT",
		"eofc"	=> 	"EOF",
		"startc"=>	"START",
		"stopc"	=>	"STOP",
		"brkc"	=>	"EOL",
		"eolc"	=>	"EOL",
		"suspc"	=>	"SUSPEND",
		"dsuspc"=>	"DSUSPEND",
		"rprntc"=>	"REPRINT",
		"flushc"=>	"DISCARD",
		"lnextc"=>	"QUOTENEXT",
		"werasc"=>	"ERASEWORD",
);

print CCHARS "
/* -*- buffer-read-only: t -*-

  This file is auto-generated. ***ANY*** changes here will be lost.
  Written by genchars.pl version $version */

";

print CCHARS "#define HAVE_POLL_H\n" if CheckHeader("poll.h");
print CCHARS "#define HAVE_SYS_POLL_H\n" if CheckHeader("sys/poll.h");

print "\n" unless SILENT;
if(1) {
	@values = sort { $possible{$a} cmp $possible{$b} or $a cmp $b } keys %possible;

	print "Writing termio/termios section of cchars.h... " unless SILENT;
	print CCHARS "

#ifdef CC_TERMIOS
# define TermStructure struct termios
# ifdef NCCS
#  define LEGALMAXCC NCCS
# else
#  ifdef NCC
#   define LEGALMAXCC NCC
#  endif
# endif
#else
# ifdef CC_TERMIO
#  define TermStructure struct termio
#  ifdef NCC
#   define LEGALMAXCC NCC
#  else
#   ifdef NCCS
#    define LEGALMAXCC NCCS
#   endif
#  endif
# endif
#endif

#if !defined(LEGALMAXCC)
# define LEGALMAXCC 126
#endif

#ifdef XS_INTERNAL
#  define TRTXS(a) XS_INTERNAL(a)
#else
#  define TRTXS(a) XS(a)
#endif

#if defined(CC_TERMIO) || defined(CC_TERMIOS)

STATIC const char	* const cc_names[] = {	".join('',map("
#if defined($_) && ($_ < LEGALMAXCC)
	\"$possible{$_}\",	"."
#else				"."
	\"\",			"."
#endif				", @values ))."
};

STATIC const int MAXCC = 0	",join('',map("
#if defined($_)  && ($_ < LEGALMAXCC)
	+1		/* $possible{$_} */
#endif			", @values ))."
	;

TRTXS(XS_Term__ReadKey_GetControlChars)
{
	dXSARGS;
	if (items < 0 || items > 1) {
		croak(\"Usage: Term::ReadKey::GetControlChars()\");
	}
	SP -= items;
	{
                PerlIO * file;
		TermStructure s;
	        if (items < 1)
	            file = STDIN;
	        else {
	            file = IoIFP(sv_2io(ST(0)));
	        }

#ifdef CC_TERMIOS 
		if(tcgetattr(PerlIO_fileno(file),&s))
#else
# ifdef CC_TERMIO
		if(ioctl(PerlIO_fileno(file),TCGETA,&s))
# endif
#endif
			croak(\"Unable to read terminal settings in GetControlChars\");
		else {
			EXTEND(sp,MAXCC*2);		".join('',map("
#if defined($values[$_]) && ($values[$_] < LEGALMAXCC)	"."
PUSHs(sv_2mortal(newSVpv(cc_names[$_],strlen(cc_names[$_])))); /* $possible{$values[$_]} */
PUSHs(sv_2mortal(newSVpv((char*)&s.c_cc[$values[$_]],1))); 	"."
#endif			"				,0..$#values))."
			
		}
		PUTBACK;
		return;
	}
}

TRTXS(XS_Term__ReadKey_SetControlChars)
{
	dXSARGS;
	/*if ((items % 2) != 0) {
		croak(\"Usage: Term::ReadKey::SetControlChars(%charpairs,file=STDIN)\");
	}*/
	SP -= items;
	{
		TermStructure s;
		PerlIO * file;
	        if ((items % 2) == 1)
	            file = IoIFP(sv_2io(ST(items-1)));
	        else {
	            file = STDIN;
	        }

#ifdef CC_TERMIOS
		if(tcgetattr(PerlIO_fileno(file),&s))
#else
# ifdef CC_TERMIO
		if(ioctl(PerlIO_fileno(file),TCGETA,&s))
# endif
#endif
			croak(\"Unable to read terminal settings in SetControlChars\");
		else {
			int i;
			char * name, value;
			for(i=0;i+1<items;i+=2) {
				name = SvPV(ST(i),PL_na);
				if( SvIOKp(ST(i+1)) || SvNOKp(ST(i+1)) )/* If Int or Float */
					value = (char)SvIV(ST(i+1));         /* Store int value */
				else                                    /* Otherwise */
					value = SvPV(ST(i+1),PL_na)[0];          /* Use first char of PV */

	if (0) ;					".join('',map("
#if defined($values[$_]) && ($values[$_] < LEGALMAXCC)	"."
	else if(strcmp(name,cc_names[$_])==0) /* $possible{$values[$_]} */ 
		s.c_cc[$values[$_]] = value;		"."
#endif							",0..$#values))."
	else
		croak(\"Invalid control character passed to SetControlChars\");
				
			}
#ifdef CC_TERMIOS
		if(tcsetattr(PerlIO_fileno(file),TCSANOW,&s))
#else
# ifdef CC_TERMIO
		if(ioctl(PerlIO_fileno(file),TCSETA,&s))
# endif
#endif
			croak(\"Unable to write terminal settings in SetControlChars\");
		}
	}
	XSRETURN(1);
}


#endif

";

	print "Done.\n" unless SILENT;

}

undef %billy;

if(@ARGV) { # If any argument is supplied on the command-line don't check sgtty
	$SGTTY=0; #skip tests
}  else {
	print "Checking for sgtty...\n" unless SILENT;

	$SGTTY = CheckStructure("sgttyb","sgtty.h");
#	$SGTTY = !Compile("
##include <sgtty.h>
#struct sgttyb s;
#main(){
#ioctl(0,TIOCGETP,&s);
#}");

#}

#	$SGTTY = !report("
##include <sgtty.h>
#struct sgttyb s;
#main(){
#ioctl(0,TIOCGETP,&s);
#}");

	print "	Sgtty ",($SGTTY?"":"NOT "),"found.\n" unless SILENT;
}

$billy{"ERASE"} = "s1.sg_erase";
$billy{"KILL"} = "s1.sg_kill";
$tchars=$ltchars=0;

if($SGTTY) {

	print "Checking sgtty...\n" unless SILENT;

	$tchars = CheckStructure("tchars","sgtty.h");
#	$tchars = !report(	'
##include <sgtty.h>
#struct tchars t;  
#main() { ioctl(0,TIOCGETC,&t); }
#');
	print "	tchars structure found.\n" if $tchars and !SILENT;

	$ltchars = CheckStructure("ltchars","sgtty.h");
#	$ltchars = !report(	'
##include <sgtty.h>
#struct ltchars t;  
#main() { ioctl(0,TIOCGLTC,&t); }
#');

	print "	ltchars structure found.\n" if $ltchars and !SILENT;


	print "Checking symbols\n" unless SILENT;


	for $c (sort keys %possible2) {

#		if($tchars and !report("
##include <sgtty.h>
#struct tchars s2;
#main () { char c = s2.t_$c; }
#")) {
		if($tchars and CheckField("tchars","t_$c","sgtty.h")) {

			print "	t_$c ($possible2{$c}) found in tchars\n" unless SILENT;
			$billy{$possible2{$c}} = "s2.t_$c";
		}

#		elsif($ltchars and !report("
##include <sgtty.h>
#struct ltchars s3;
#main () { char c = s3.t_$c; }
#")) {
		elsif($ltchars and CheckField("ltchars","t_$c","sgtty.h")) {
			print "	t_$c ($possible2{$c}) found in ltchars\n" unless SILENT;
			$billy{$possible2{$c}} = "s3.t_$c";
		}

	}


	#undef @names;
	#undef @values;
	#for $v (sort keys %billy) {
	#	push(@names,$billy{$v});
	#	push(@values,$v);
	#}

	#$numchars = keys %billy;

}

@values = sort keys %billy;

	$struct = "
struct termstruct {
	struct sgttyb s1;
";
	$struct .= "
	struct tchars s2;
"	if $tchars;
	$struct .= "
	struct ltchars s3;
"	if $ltchars;
	$struct .= "
};";

print "Writing sgtty section of cchars.h... " unless SILENT;

	print CCHARS "

#ifdef CC_SGTTY
$struct
#define TermStructure struct termstruct

STATIC const char	* const cc_names[] = {	".join('',map("
	\"$_\",			", @values ))."
};

#define MAXCC	". ($#values+1)."

TRTXS(XS_Term__ReadKey_GetControlChars)
{
	dXSARGS;
	if (items < 0 || items > 1) {
		croak(\"Usage: Term::ReadKey::GetControlChars()\");
	}
	SP -= items;
	{
		PerlIO * file;
		TermStructure s;
	        if (items < 1)
	            file = STDIN;
	        else {
	            file = IoIFP(sv_2io(ST(0)));
	        }
        if(ioctl(fileno(PerlIO_file),TIOCGETP,&s.s1) ".($tchars?"
 	||ioctl(fileno(PerlIO_file),TIOCGETC,&s.s2)  ":'').($ltchars?"
        ||ioctl(fileno(PerlIO_file),TIOCGLTC,&s.s3)  ":'')."
			)
			croak(\"Unable to read terminal settings in GetControlChars\");
		else {
			int i;
			EXTEND(sp,MAXCC*2);		".join('',map("
PUSHs(sv_2mortal(newSVpv(cc_names[$_],strlen(cc_names[$_])))); /* $values[$_] */
PUSHs(sv_2mortal(newSVpv(&s.$billy{$values[$_]},1))); 	",0..$#values))."
			
		}
		PUTBACK;
		return;
	}
}

TRTXS(XS_Term__ReadKey_SetControlChars)
{
	dXSARGS;
	/*if ((items % 2) != 0) {
		croak(\"Usage: Term::ReadKey::SetControlChars(%charpairs,file=STDIN)\");
	}*/
	SP -= items;
	{
		PerlIO * file;
		TermStructure s;
	        if ((items%2)==0)
	            file = STDIN;
	        else {
	            file = IoIFP(sv_2io(ST(items-1)));
	        }

	        if(ioctl(PerlIO_fileno(file),TIOCGETP,&s.s1) ".($tchars?"
	 	||ioctl(fileno(PerlIO_file),TIOCGETC,&s.s2)  ":'').($ltchars?"
	        ||ioctl(fileno(PerlIO_file),TIOCGLTC,&s.s3)  ":'')."
			)
			croak(\"Unable to read terminal settings in SetControlChars\");
		else {
			int i;
			char * name, value;
			for(i=0;i+1<items;i+=2) {
				name = SvPV(ST(i),PL_na);
				if( SvIOKp(ST(i+1)) || SvNOKp(ST(i+1)) )/* If Int or Float */
					value = (char)SvIV(ST(i+1));         /* Store int value */
				else                                    /* Otherwise */
					value = SvPV(ST(i+1),PL_na)[0];          /* Use first char of PV */

	if (0) ;					".join('',map("
	else if(strcmp(name,cc_names[$_])==0) /* $values[$_] */ 
		s.$billy{$values[$_]} = value;		",0..$#values))."
	else
		croak(\"Invalid control character passed to SetControlChars\");
				
			}
	        if(ioctl(fileno(PerlIO_file),TIOCSETN,&s.s1) ".($tchars?"
	        ||ioctl(fileno(PerlIO_file),TIOCSETC,&s.s2) ":'').($ltchars?"
	        ||ioctl(fileno(PerlIO_file),TIOCSLTC,&s.s3) ":'')."
			) croak(\"Unable to write terminal settings in SetControlChars\");
		}
	}
	XSRETURN(1);
}

#endif

#if !defined(CC_TERMIO) && !defined(CC_TERMIOS) && !defined(CC_SGTTY)
#define TermStructure int
TRTXS(XS_Term__ReadKey_GetControlChars)
{
	dXSARGS;
	if (items <0 || items>1) {
		croak(\"Usage: Term::ReadKey::GetControlChars([FileHandle])\");
	}
	SP -= items;
	{
		ST(0) = sv_newmortal();
		PUTBACK;
		return;
	}
}

TRTXS(XS_Term__ReadKey_SetControlChars)
{
	dXSARGS;
	if (items < 0 || items > 1) {
		croak(\"Invalid control character passed to SetControlChars\");
	}
	SP -= items;
	XSRETURN(1);
}

#endif

/* ex: set ro: */
";

print "Done.\n" unless SILENT;




	
