
BEGIN {
        chdir 't' if -d 't';
        @INC = '../lib';
        require Test; import Test;
        plan(tests => 154);
}

# Test that Pod::LaTeX works
# This test relies on the DATA filehandle
# DATA contains the latex that is used for comparison
# and the pod that was used to generate it. The two
# are separated by '=pod'
# Note that if the translator is adjusted the output tex
# will probably not match what is currently there. You
# will need to adjust it to match (assuming it is correct).

use strict;

use Pod::LaTeX;

# Set up an END block to remove the test output file
END { unlink "test.tex" };

ok(1);

# First thing to do is to read the expected output from
# the DATA filehandle and store it in a scalar.
# Do this until we read an =pod
my @reference;
while (my $line = <DATA>) {
  last if $line =~ /^=pod/;
  push(@reference,$line);
}

# Create a new parser
my $parser = Pod::LaTeX->new;
ok($parser);
$parser->Head1Level(1);
# Add the preamble but remember not to compare the timestamps
$parser->AddPreamble(1);
$parser->AddPostamble(1);

# For a laugh add a table of contents
$parser->TableOfContents(1);

# Create an output file
open(OUTFH, "> test.tex" ) or die "Unable to open test tex file: $!\n";

# Read from the DATA filehandle and write to a new output file
# Really want to write this to a scalar
$parser->parse_from_filehandle(\*DATA,\*OUTFH);

close(OUTFH) or die "Error closing OUTFH test.tex: $!\n";

# Now read in OUTFH and compare
open(INFH, "< test.tex") or die "Unable to read test tex file: $!\n";
my @output = <INFH>;

ok(@output, @reference);
for my $i (0..$#reference) {
  next if $reference[$i] =~ /^%%/; # skip timestamp comments
  ok($output[$i], $reference[$i]);
}

close(INFH) or die "Error closing INFH test.tex: $!\n";


__DATA__
\documentclass{article}

%%  Latex generated from POD in document ...
%%  Using the perl module Pod::LaTeX
%%  Converted on Tue Nov 20 20:43:05 2001


\usepackage{makeidx}
\makeindex


\begin{document}

\tableofcontents

\section{Introduction\label{Introduction}\index{Introduction}}
\begin{itemize}

\item 

Always check the return codes of system calls. Good error messages should
go to STDERR, include which program caused the problem, what the failed
system call and arguments were, and (\textbf{very important}) should contain
the standard system error message for what went wrong. Here's a simple
but sufficient example:

\begin{verbatim}
        opendir(D, $dir) or die "can't opendir $dir: $!";
\end{verbatim}

\item 

Line up your transliterations when it makes sense:

\begin{verbatim}
        tr [abc]
           [xyz];
\end{verbatim}


The above should be aligned since it includes an embedded tab.


\item 

Think about reusability. Why waste brainpower on a one-shot when you
might want to do something like it again? Consider generalizing your
code. Consider writing a module or object class. Consider making your
code run cleanly with \texttt{use strict} and \texttt{-w} (or \texttt{use warnings} in
Perl 5.6) in effect. Consider giving away your code. Consider changing
your whole world view. Consider... oh, never mind.


\item 

Be consistent.


\item 

Be nice.

\end{itemize}
\section{Links\label{Links}\index{Links}}


This link should just include one word: \textsf{Pod::LaTeX}



This link should include the text \texttt{test} even though
it refers to \texttt{Pod::LaTeX}: \textsf{test}.



Standard link: the \emph{Pod::LaTeX} manpage.



Now refer to an external section: the section on \textsf{sec} in the \emph{Pod::LaTeX} manpage

\section{Lists\label{Lists}\index{Lists}}


Test description list with long lines

\begin{description}

\item[Some short text] \mbox{}

Some additional para.

\begin{itemize}

\item 

Nested itemized list


\item 

Second item

\end{itemize}

\item[some longer text than that] \mbox{}

and again.


\item[this text is even longer and greater than] \textbf{40 characters}

Some more content for the item.


\item[this is some text with \textit{something across}] \textbf{the 40 char boundary}

This is item content.

\end{description}
\section{Escapes\label{Escapes}\index{Escapes}}


Test some normal escapes such as $<$ (lt) and $>$ (gt) and $|$ (verbar) and
\texttt{\~{}} (tilde) and \& (amp) as well as $<$ (Esc lt) and $|$ (Esc
verbar) and / (Esc sol) and $>$ (Esc gt) and \& (Esc amp)
and " (Esc quot) and even $\alpha$ (Esc alpha).

\section{For blocks\label{For_blocks}\index{For blocks}}
  Some latex code \textbf{here}.



Some text that should appear.



Some more text that should appear

Some latex in a \textsf{begin block}

and some more

\begin{equation}
a = \frac{3}{2}
\end{equation}



Back to pod.

\printindex

\end{document}
=pod

=head1 Introduction

=over 4

=item *

Always check the return codes of system calls. Good error messages should
go to STDERR, include which program caused the problem, what the failed
system call and arguments were, and (B<very important>) should contain
the standard system error message for what went wrong. Here's a simple
but sufficient example:

        opendir(D, $dir) or die "can't opendir $dir: $!";

=item *

Line up your transliterations when it makes sense:

        tr [abc]
  	   [xyz];

The above should be aligned since it includes an embedded tab.

=item *

Think about reusability. Why waste brainpower on a one-shot when you
might want to do something like it again? Consider generalizing your
code. Consider writing a module or object class. Consider making your
code run cleanly with C<use strict> and C<-w> (or C<use warnings> in
Perl 5.6) in effect. Consider giving away your code. Consider changing
your whole world view. Consider... oh, never mind.

=item *

Be consistent.

=item *

Be nice.

=back

=head1 Links

This link should just include one word: L<Pod::LaTeX|Pod::LaTeX>

This link should include the text C<test> even though
it refers to C<Pod::LaTeX>: L<test|Pod::LaTeX>.

Standard link: L<Pod::LaTeX>.

Now refer to an external section: L<Pod::LaTeX/"sec">


=head1 Lists

Test description list with long lines

=over 4

=item Some short text

Some additional para.

=over 4

=item *

Nested itemized list

=item *

Second item

=back

=item some longer text than that

and again.

=item this text is even longer and greater than 40 characters

Some more content for the item.

=item this is some text with I<something across> the 40 char boundary

This is item content.

=back

=head1 Escapes

Test some normal escapes such as < (lt) and > (gt) and | (verbar) and
~ (tilde) and & (amp) as well as E<lt> (Esc lt) and E<verbar> (Esc
verbar) and E<sol> (Esc sol) and E<gt> (Esc gt) and E<amp> (Esc amp)
and E<quot> (Esc quot) and even E<alpha> (Esc alpha).

=head1 For blocks

=for latex
  Some latex code \textbf{here}.

Some text that should appear.

=for comment
  Should not print anything

Some more text that should appear

=begin latex

Some latex in a \textsf{begin block}

and some more

\begin{equation}
a = \frac{3}{2}
\end{equation}

=end latex

Back to pod.

=cut
