.RP
.\" .TM "79-1274-xx 79-1273-yy" 39199 39199-11
.ND January 30, 1979
.\" .TM "76-1274-6 76-1273-5" 39199 39199-11
.TL
LEARN \(em Computer-Aided Instruction on UNIX
.br
(Second Edition)
.AU "MH 2C-518" 6021
Brian W. Kernighan
.AU "MH 2C-569" 6377
Michael E. Lesk
.AI
.\" .MH
.\" .OK
.\" CAI
.AB
.PP
This paper describes the
second version of the
.I
learn
.R
program for interpreting CAI
scripts on
the
.UX 
operating system,
and a set of scripts that provide a computerized introduction
to the system.
.PP
Six current scripts cover basic commands and file
handling, the editor, additional file handling commands, the
.I
eqn
.R
program for mathematical
typing,
the ``\-ms'' package of formatting macros,
and an introduction to the C programming language.
These scripts now include a total of
about 530 lessons.
.PP
Many users from a wide variety of backgrounds have used
.I learn
to acquire basic UNIX skills.
Most usage involves the first two scripts,
an introduction to
.UX
files and commands, and
the
.UX
editor.
.PP
The second version of
.I learn
is about four times faster than the previous one
in CPU utilization,
and much faster in perceived time
because of better overlap of computing and printing.
It also requires less file space than the first version.
Many of the lessons have been revised;
new material has been added to reflect changes
and enhancements in 
.UX
itself.
Script-writing is also easier
because of revisions to the script language.
.AE
.\" .CS 11 2 13 4 0 0
.NH
Educational Assumptions and Design.
.PP
First, the way to teach people how to do something
is to have them do it.  Scripts should
not contain long pieces of explanation; they should
instead frequently ask the student to do some task.
So teaching is always by example: the typical
script fragment shows a small example of some
technique and then asks the
user to either repeat that example or
produce a variation on it.
All are intended to be easy enough that most students will get most questions
right, reinforcing the desired behavior.
.PP
Most lessons fall into one of three types.
The simplest presents a lesson and asks for a yes or no
answer to a question.
The student is given a chance to experiment before replying.
The script checks for the correct reply.
Problems of this form are sparingly used.
.PP
The second type asks for a word or number as an answer.
For example a lesson on files might say
.IP
.I
How many files are there in the current directory?
Type ``answer N'', where N is the number of files.
.R
.LP
The student is expected to respond (perhaps after experimenting) with
.LP
.I
	answer 17
.R
.LP
or whatever.
Surprisingly often, however, the idea of a substitutable argument
(i.e., replacing
.I
N
.R
by
17)
is difficult for non-programmer students,
so the first few such lessons need real care.
.PP
The third type of lesson is open-ended \(em
a task is set for the student,
appropriate parts of the input or output are monitored,
and the student types 
.ul
ready
when the task is done.
Figure 1 shows a sample dialog that illustrates the last of these, using two
lessons about the
.I cat
(concatenate, i.e., print) command taken
from early in the script that teaches
file handling.
Most
.I learn
lessons are of this form.
.KF
.TS
box, center;
c.
T{
Figure 1:  Sample dialog from basic files script
.sp
(Student responses
in italics; `$' is the prompt)
.nf
.sp
A file can be printed on your terminal
by using the "cat" command.  Just say
"cat file" where "file" is the file name.
For example, there is a file named
"food" in this directory.  List it
by saying "cat food"; then type "ready".
$ \fIcat food\fR
  this is the file
  named food.
$ \fIready\fR

Good.  Lesson 3.3a (1)

Of course, you can print any file with "cat".
In particular, it is common to first use
"ls" to find the name of a file and then "cat"
to print it.  Note the difference between
"ls", which tells you the name of the file,
and "cat", which tells you the contents.
One file in the current directory is named for
a President.  Print the file, then type "ready".
$ \fIcat President\fR
cat: can't open President
$ \fIready\fR

Sorry, that's not right.  Do you want to try again? \fIyes\fR
Try the problem again.
$ \fIls\fR
\&.ocopy
X1
roosevelt
$ \fIcat roosevelt\fR
  this file is named roosevelt
  and contains three lines of
  text.
$ \fIready\fR

Good.  Lesson 3.3b (0)

The "cat" command can also print several files
at once.  In fact, it is named "cat" as an abbreviation
for "concatenate"....
.fi
T}
.TE
.sp
.KE
.PP
After each correct response the computer congratulates
the student and indicates the lesson number that
has just been completed, permitting the student
to restart the script after that lesson.
If the answer is wrong, the student
is offered a chance to repeat the lesson.
The ``speed'' rating of the student (explained in
section 5) is given after the lesson number when the lesson is completed successfully; it is
printed only for the aid of script authors checking
out possible errors in the lessons.
.br
.PP
It is assumed that there is no foolproof way
to determine if the student truly ``understands''
what he or she is doing;
accordingly,
the current
.I
learn
.R
scripts
only measure performance, not comprehension.
If the student can perform a given task, that is deemed to be ``learning.''
.[
%A B. F. Skinner
%T Why We Need Teaching Machines
%J Harvard Educational Review
%V 31
%P 377-398
%D 1961
.]
.PP
The main point of using the computer is that what the student
does is checked for correctness immediately.
Unlike many CAI scripts, however, these scripts provide
few facilities for dealing with wrong answers.
In practice, if most of the answers are not right the script is
a failure; the universal solution to student error is to provide
a new, easier script.
Anticipating possible wrong answers is an endless job, and it is really
easier as well as better to provide a simpler script.
.PP
Along with this goes the assumption that
anything can be taught to anybody if it can
be broken into sufficiently small pieces.  Anything
not absorbed in a single chunk is just subdivided.
.PP
To avoid boring the faster students,
however,
an effort is made in the files and editor scripts to provide
three tracks of different difficulty.
The fastest sequence of lessons
is aimed at roughly the bulk and speed of a typical tutorial
manual and should be adequate for review and for
well-prepared students.
The next track is intended for most users and is roughly
twice as long.  Typically, for example, the fast track
might present an idea and ask for a variation on the
example shown; the normal track will first
ask the student to repeat the example that was shown
before attempting a variation.
The third and slowest track, which is often
three or four times the length of the fast track,
is intended to be adequate for anyone.
(The lessons of Figure 1 are from the third track.)
The multiple tracks also mean that a student repeating a course is unlikely
to hit the same series of lessons; this makes it profitable for a shaky
user to back up and try again, and many students have done so.
.PP
The tracks are not completely distinct, however.
Depending on the number of correct answers the student has given for the
last few lessons, the program may switch tracks.
The driver is actually capable of following
an arbitrary directed graph of lesson sequences, as discussed in section 5.
Some more structured arrangement, however, is used in all current scripts
to aid the script writer in organizing the material into lessons.
It is sufficiently difficult
to write lessons
that the three-track theory
is not followed very closely
except in
the files and editor scripts.
Accordingly,
in some cases, the fast track is produced merely by skipping
lessons from the slower track.
In others, there is essentially only one track.
.PP
The main reason for using the
.I
learn
.R
program rather than
simply writing the same material as a workbook
is not the selection of tracks, but
actual hands-on experience.
Learning by doing
is much more effective
than pencil and paper exercises.
.PP
.I Learn
also provides a mechanical check on performance.
The first version in fact would not let
the student proceed unless it
received correct answers to the questions
it set and it would not tell a student the right answer.
This somewhat Draconian approach has been moderated
in version 2.
Lessons are sometimes badly worded or even just plain wrong;
in such cases,
the student has no recourse.
But if a student is simply unable to complete one lesson,
that should not prevent access to the rest.
Accordingly, the current version of
.I learn
allows the student to skip
a lesson that he cannot pass;
a ``no'' answer to the ``Do you want to try again?''
question in Figure 1 will pass to the next lesson.
It is still true that 
.I learn
will not tell the student the right answer.
.PP
Of course, there are valid objections to the
assumptions above.
In particular, some students may object to
not understanding
what they are doing;
and the procedure of smashing everything into small pieces may provoke
the retort ``you can't cross a ditch in two jumps.''
Since writing CAI scripts is considerably
more tedious than ordinary manuals, however, it is safe
to assume that there will always be alternatives to the
scripts as a way of learning.
In fact, for a reference manual of 3 or 4 pages it would
not be surprising to have a tutorial manual of 20 pages
and a (multi-track) script of 100 pages.  Thus the reference manual
will exist long before the scripts.
.NH
Scripts.
.PP
As mentioned above, the present scripts try
at most
to follow a three-track theory.  Thus little
of the potential complexity of the possible directed graph
is employed, since
care must be taken in lesson construction to see
that every necessary fact is presented in
every possible path through the units.  In addition,
it is desirable that every unit have alternate successors
to deal with student errors.
.PP
In most existing courses, the first few lessons
are devoted to checking prerequisites.  For example,
before the student is allowed to proceed through the editor
script the script verifies that the student understands files
and is able to type.
It is felt that the sooner lack of student preparation
is detected, the easier it will be on the student.
Anyone proceeding through the scripts
should be getting mostly correct answers; otherwise, the
system will be unsatisfactory both because the wrong
habits are being learned and because the
scripts make little effort to deal with wrong answers.
Unprepared students should not be encouraged
to continue with scripts.
.PP
There are some preliminary items which the student must
know before any scripts can be tried.  In particular,
the student must know how to connect to
a
.UX
system,
set the terminal properly,
log in,
and execute simple commands (e.g.,
.ul
learn
itself).
In addition, the character erase and line kill conventions
(# and @) should be known.
It is hard to see how this much could be taught by
computer-aided instruction, since a student who
does not know these basic skills will not be able
to run the learning program.
A brief description on paper is provided (see Appendix A), although
assistance will be needed for the first few
minutes.  This assistance, however, need not be highly skilled.
.PP
The first script in the current set deals with files.  It assumes
the basic knowledge above and teaches the student about
the
.I ls ,
.I cat ,
.I mv ,
.I rm ,
.I cp
and
.I diff
commands.
.tr ~
It also deals with the abbreviation characters *, ?, and [\ ]
in file names.
It does not cover pipes or I/O redirection,
nor does it present the many options
on the
.ul
ls
command.
.PP
This script contains 31 lessons
in the fast track;
two are
intended as prerequisite checks,
seven are review exercises.
There are a total of 75 lessons in all three tracks,
and the instructional passages typed at the student
to begin each lesson total 4,476 words.  The average
lesson thus begins with a 60-word message.
In general, the fast track lessons have somewhat longer
introductions, and the slow tracks somewhat shorter ones.
The longest message is 144 words and the shortest 14.
.PP
The second script trains students in the use
of the
.UX
context editor
.I ed ,
a sophisticated editor
using regular expressions for searching.
(See section \f2ed\f1 (I).
All editor
features except encryption, mark names and `;' in addressing
are covered.
The fast track contains 2 prerequisite checks,
93 lessons, and a review lesson.
It is supplemented by 146 additional lessons in other tracks.
.PP
A comparison of sizes may be of interest.  The
.ul
ed
description
in the reference manual is 2,572 words long.  The
.ul
ed
tutorial
.[
%A B. W. Kernighan
%T A Tutorial Introduction to the Unix Editor ed
%D 1974
.]
is 6,138 words long.  
The fast track through
the
.ul
ed
script is 7,407 words of explanatory messages, and the
total
.ul
ed
script, 242 lessons, 
has 15,615 words.
The average
.ul
ed
lesson is thus also about 60 words; the largest
is 171 words and the smallest 10.
The
original
.ul
ed
script represents about three man-weeks of effort.
.PP
The advanced file handling script deals with
.ul
ls
options,
I/O diversion, pipes, and supporting programs like
.I pr ,
.I wc ,
.I tail ,
.I spell
and
.I grep .
(The basic file handling script is a prerequisite.)
It is not as refined as the first two scripts;
this is reflected at least partly in the fact that
it provides much less of a full three-track sequence
than they do.
On the other hand,
since it is perceived as ``advanced,''
it is hoped that the student will have somewhat
more sophistication
and be better able to cope with it at a reasonably
high level of performance.
.PP
A fourth script covers the
.ul
eqn
language for typing mathematics.
This script must be run on a terminal capable of printing
mathematics, for instance the DASI 300 and similar Diablo-based
terminals, or the nearly extinct Model 37 teletype.
Again, this script is relatively short of tracks:
of 76 lessons, only 17 are in the second track and 2
in the third track.
Most of these provide additional practice for students
who are having trouble in the first track.
.PP
The
.I \-ms
script for formatting macros
is a short one-track only script.
The macro package it describes is no longer the standard,
so this script will undoubtedly be superseded
in the future.
Furthermore, the linear style of a single learn script is somewhat
inappropriate for the macros, since the macro package is composed of many
independent features, and few users need all of them.
It would be better to have a selection of short lesson
sequences dealing with the features independently.
.PP
The script on C is in a state of transition.
It was originally designed to follow
a tutorial on C,
but that document has since become obsolete.
The current script has been partially converted
to follow the order of presentation in
.ul
The C Programming Language,
.[
%A B. W. Kernighan
%A D. M. Ritchie
%T The C Programming Language
%I Prentice Hall
%D 1978
.]
but this job is not complete.
The C script was never intended to teach C;
rather it is supposed to be a series of exercises
for which the computer provides checking and
(upon success) a suggested solution.
.PP
This combination of scripts covers much of the material which any
.UX
user
will need to know
to make effective use of the system.
With enlargement of the advanced files
course to include more on the command interpreter, there
will be a relatively complete introduction to
.UX
available via
.ul
learn.
Although we make no pretense that 
.ul
learn
will replace other instructional materials,
it should provide a useful supplement to existing tutorials and reference manuals.
.NH
Experience with Students.
.PP
.I
Learn
.R
has been installed on
many different 
.UX
systems.
Most of the usage is on the first two scripts, so these
are more thoroughly debugged and polished.
As a (random) sample of user experience,
the
.I learn
program has been used at Bell Labs at Indian Hill
for 10,500 lessons in a four month period.
About 3600 of these are in the files script,
4100 in the editor,
and 1400 in advanced files.
The passing rate is about 80%,
that is, about 4 lessons are passed for every one
failed.
There have been 86 distinct users of the files script,
and 58 of the editor.
On our system at Murray Hill, there have been nearly 2000 lessons
over two weeks that include
Christmas and New Year.
Users have ranged in age from six up.
.PP
It is difficult to characterize typical sessions with the
scripts;
many instances exist of someone doing one or two lessons
and then logging out, as do instances of someone pausing
in a script for twenty minutes or more.
In the earlier version of
.I learn ,
the average session in the files course took 32 minutes and covered
23 lessons.
The distribution is quite
broad and skewed, however; the longest session was
130 minutes and there were five sessions shorter than
five minutes.
The average lesson took about 80 seconds.
These numbers are roughly typical for non-programmers;
a
.UX
expert can do the scripts at approximately 30 seconds
per lesson, most of which is the system printing.
.PP
At present working through a section of the middle of the files
script took about 1.4 seconds of processor time per lesson,
and a system expert typing quickly took 15 seconds of real time per lesson.
A novice would probably take at least a minute.
Thus a UNIX system could support ten students working simultaneously
with some spare capacity.
.NH
The Script Interpreter.
.PP
The
.I
learn
.R
program itself merely interprets scripts.  It provides
facilities for the script writer to capture student
responses and their effects, and simplifies the job
of passing control to and recovering control from the student.
This section describes the operation and
usage of the driver program,
and indicates what is
required to produce a new script.
Readers only interested in
the existing scripts may skip this section.
.PP
The file structure used by
.I learn
is shown in Figure 2.
There is one parent directory (named \f2lib\f1\^) containing the script data.
Within this directory are subdirectories, one for each
subject in which a course is available,
one for logging (named
.I log ),
and one in which user sub-directories
are created (named
.I play ).
The subject directory contains master copies of all lessons,
plus any supporting material for that subject.
In a given subdirectory,
each lesson is a single text file.
Lessons are usually named systematically;
the file that contains lesson
.I n
is called
.I Ln .
.br
.KF
.sp
.TS
center, box;
c s s s
l l l l.
Figure 2:  Directory structure for \fIlearn\fR
.sp
.nf
lib
.if t .sp .5
	play
		student1
			files for student1...
		student2
			files for student2...
.if t .sp .5
	files
		L0.1a	lessons for files course
		L0.1b
		...
.if t .sp .5
	editor
		...
.if t .sp .5
	(other courses)
.if t .sp .5
	log
.TE
.sp
.KE
.PP
When
.I
learn
.R
is executed, it makes a private directory
for the user to work in,
within the
.I
learn
.R
portion of the file system.
A fresh copy of all the files used in each lesson
(mostly data for the student to operate upon) is made each
time a student starts a lesson,
so the script writer may assume that everything
is reinitialized each time a lesson is entered.
The student directory is deleted after each session; any permanent records
must be kept elsewhere.
.PP
The script writer must provide certain basic items
in each
lesson:
.IP (1)
the text of the lesson;
.IP (2)
the set-up commands to be executed before the user gets control;
.IP (3)
the data, if any, which the user is supposed to edit, transform, or otherwise
process;
.IP (4)
the evaluating commands to be executed after the user
has finished the lesson, to decide whether the answer is right;
and
.IP (5)
a list of possible successor lessons.
.LP
.I
Learn
.R
tries to minimize the work
of bookkeeping and installation, so
that most of the effort involved in
script production is in planning lessons,
writing tutorial paragraphs,
and coding tests of student performance.
.PP
The basic sequence of events is
as follows.
First,
.I learn
creates the working directory.
Then, for each lesson,
.I learn
reads the script for the lesson and processes
it a line at a time.
The lines in the script are:
(1) commands to the script interpreter
to print something, to create a files,
to test something, etc.;
(2) text to be printed or put in a file;
(3) other lines, which are sent to
the shell to be executed.
One line in each lesson turns control over
to the user;
the user can run any 
.UX 
commands.
The user mode terminates when the user
types 
.I yes ,
.I no ,
.I ready ,
or
.I answer .
At this point, the user's work is tested;
if the lesson is passed,
a new lesson is selected, and if not
the old one is repeated.
.PP
Let us illustrate this with the script
for the second lesson of Figure 1;
this is shown in Figure 3.
.KF
.sp
.TS
center, box;
c.
T{
Figure 3:  Sample Lesson
.sp
.nf
#print
Of course, you can print any file with "cat".
In particular, it is common to first use
"ls" to find the name of a file and then "cat"
to print it.  Note the difference between
"ls", which tells you the name of the files,
and "cat", which tells you the contents.
One file in the current directory is named for
a President.  Print the file, then type "ready".
#create roosevelt
  this file is named roosevelt
  and contains three lines of
  text.
#copyout
#user
#uncopyout
tail \-3 .ocopy >X1
#cmp X1 roosevelt
#log
#next
3.2b 2
.fi
T}
.TE
.sp
.KE
.LP
Lines which begin with
# are commands to the
.I  learn 
script interpreter.
For example,
.LP
.ul
	#print
.LP
causes printing of any text that follows,
up to the next line that begins with a sharp.
.LP
.ul
	#print file
.LP
prints the contents of
.I file ;
it
is the same as
.ul
cat file 
but has
less overhead.
Both forms of
.I #print
have the added property that if a lesson is failed,
the
.ul
#print
will not be executed the second time through;
this avoids annoying the student by repeating the preamble
to a lesson.
.LP
.ul
	#create filename
.LP
creates a file of the specified name,
and copies any subsequent text up to a
# to the file.
This is used for creating and initializing working files
and reference data for the lessons.
.LP
.ul
	#user
.LP
gives control to the student;
each line he or she types is passed to the shell
for execution.
The
.I #user
mode
is terminated when the student types one of
.I yes ,
.I no ,
.I ready 
or
.I answer .
At that time, the driver
resumes interpretation of the script.
.LP
.ul
	#copyin
.br
.ul
	#uncopyin
.LP
Anything the student types between these
commands is copied onto a file
called
.ul
\&.copy.
This lets the script writer interrogate the student's
responses upon regaining control.
.LP
.ul
	#copyout
.br
.ul
	#uncopyout
.LP
Between these commands, any material typed at the student
by any program
is copied to the file
.ul
\&.ocopy.
This lets the script writer interrogate the
effect of what the student typed, 
which true believers in the performance theory of learning
usually
prefer to the student's actual input.
.LP
.ul
	#pipe
.br
.ul
	#unpipe
.LP
Normally the student input and the script commands
are fed to the
.UX
command interpreter (the ``shell'') one line at a time. This won't do
if, for example, a sequence of editor commands
is provided,
since the input to the editor must be handed to the editor,
not to the shell.
Accordingly, the material between 
.ul
#pipe
and
.ul
#unpipe
commands
is fed
continuously through a pipe so that such sequences
work.
If
.ul
copyout
is also desired
the
.ul
copyout
brackets must include
the
.ul
pipe
brackets.
.PP
There are several commands for setting status
after the student has attempted the lesson.
.LP
.ul
	#cmp file1 file2
.LP
is an in-line implementation of
.I cmp ,
which compares two files for identity.
.LP
.ul
	#match stuff
.LP
The last line of the student's input
is compared to
.I stuff ,
and the success or fail status is set
according to it.
Extraneous things like the word
.I answer
are stripped before the comparison is made.
There may be several 
.I #match
lines;
this provides a convenient mechanism for handling multiple
``right'' answers.
Any text up to a
# on subsequent lines after a successful
.I #match
is printed; 
this is illustrated in Figure 4, another sample lesson.
.br
.KF
.sp
.TS
center, box;
c.
T{
Figure 4:  Another Sample Lesson
.sp
.nf
#print
What command will move the current line
to the end of the file?  Type 
"answer COMMAND", where COMMAND is the command.
#copyin
#user
#uncopyin
#match m$
#match .m$
"m$" is easier.
#log
#next
63.1d 10
T}
.TE
.sp
.KE
.LP
.ul
	#bad stuff
.LP
This is similar to
.I #match ,
except that it corresponds to specific failure answers;
this can be used to produce hints for particular wrong answers
that have been anticipated by the script writer.
.LP
.ul
	#succeed
.br
.ul
	#fail
.LP
print a message
upon success or failure
(as determined by some previous mechanism).
.PP
When the student types
one of the ``commands''
.I yes ,
.I no ,
.I ready ,
or
.I answer ,
the driver terminates the
.I #user
command,
and evaluation of the student's work can begin.
This can be done either by
the built-in commands above, such as
.I #match
and
.I #cmp ,
or by status returned by normal
.UX 
commands, typically
.I grep
and
.I test .
The last command
should return status true
(0) if the task was done successfully and
false (non-zero) otherwise;
this status return tells the driver
whether or not the student
has successfully passed the lesson.
.PP
Performance can be logged:
.LP
.ul
	#log file
.LP
writes the date, lesson, user name and speed rating, and
a success/failure indication on
.ul
file.
The command
.LP
.ul
	#log
.LP
by itself writes the logging information
in the logging directory
within the
.I learn
hierarchy,
and is the normal form.
.LP
.ul
	#next
.LP
is followed by a few lines, each with a successor
lesson name and an optional speed rating on it.
A typical set might read
.LP
.nf
	25.1a   10
	25.2a   5
	25.3a   2
.fi
.LP
indicating that unit 25.1a is a suitable follow-on lesson
for students with
a speed rating of 10 units,
25.2a for student with speed near 5,
and 25.3a for speed near 2.
Speed ratings are maintained for
each session with a student; the
rating is increased by one each tiee
the student gets a lesson right and decreased
by four each
time the student gets a lesson wrong.
Thus the driver tries to maintain a devel such
that the users get 80% right answers.
The maximum rating is limited to 10 afd the minimum to 0.
The initial rating is zero unless the studeft
specifies a differeft rating when starting
a session.
.PP
If the student passes a lesson,
a new lesson is sedected and the process repeats.
If the student fails, a false status is returned
and the program
reverts to the previous lesson and tries
another alternative.
If it can not find another alternative, it skips forward
a lesson.
.I bye ,
bye,
which causes a graceful exit
from the 
.ul
learn
system.  Hanging up is the usual novice's way out.
.PP
The lessons may form an arbitrary directed graph,
although the present program imposes a limitation on cycles in that
it will not present a lesson twice in the
same session.
If the student is unable to answer one of the exercises
correctly, the driver searches for a previous lesson
with a set of alternatives as successors
(following the
.I #next
line).
From the previous lesson with alternatives one route was taken
earlier; the program simply tries a different one.
.PP
It is perfectly possible
to write sophisticated scripts that evaluate
the student's speed of response, or try to estimate the
elegance of the answer, or provide detailed
analysis of wrong answers.
Lesson writing is so tedious already, however, that most
of these abilities are likely to go unused.
.PP
The driver program depends heavily on features
of
.UX
that are not available on many other operating systems.
These include
the ease of manipulating files and directories,
file redirection,
the ability to use the command interpreter
as just another program (even in a pipeline),
command status testing and branching,
the ability to catch signals like interrupts,
and of course
the pipeline mechanism itself.
Although some parts of 
.ul
learn
might be transferable to other systems,
some generality will probably be lost.
.PP
A bit of history:
The first version of
.I learn
had fewer built-in words
in the driver program,
and made more use of the
facilities of
.UX .
For example,
file comparison was done by creating a
.I cmp
process,
rather than comparing the two files within
.I learn .
Lessons were not stored as text files,
but as archives.
There was no concept of the in-line document;
even 
.I #print
had to be followed by a file name.
Thus the initialization for each lesson
was to extract the archive into the working directory
(typically 4-8 files),
then 
.I #print
the lesson text.
.PP
The combination of such things made
.I learn
slower.
The new version is about 4 or 5 times faster.
Furthermore, it appears even faster to the user
because in a typical lesson,
the printing of the message comes first,
and file setup with
.I #create
can be overlapped with the printng,
so that when the program
finishes printing,
it is really ready for the user
to type at it.
.PP
It is also a great advantage to the script maintainer
that lessons are now just ordinary text files.
They can be edited without any difficulty,
and  
.UX
text manipulation tools can be applied
to them.
The result has been that
there is much less resistance
to going in and fixing substandard lessons.
.NH
Conclusions
.PP
The following observations can be made about
secretaries, typists, and
other non-programmers who have used
.I learn :
.IP (a)
A novice must have assistance with the mechanics
of communicating with the computer to get through to
the first lesson or two;
once the first few lessons are passed people can proceed
on their own.
.IP (b)
The terminology used in the first few lessons
is obscure to those inexperienced with computers.
It would help if there were a low level
reference card for
.UX
to supplement the existing
programmer oriented bulky manual and bulky reference card.
.IP (c)
The concept of ``substitutable argument'' is hard
to grasp, and requires help.
.IP (d)
They enjoy the system for the most part.
Motivation matters a great deal, however.
.LP
It takes an hour or two for a novice to get through
the script on file handling.
The total time for a reasonably intelligent and motivated novice to proceed from ignorance
to a reasonable ability to create new files and manipulate old ones
seems to be a few days, with perhaps half of each day
spent on the machine.
.PP
The normal way of proceeding has been to have students in the same
room with someone who knows
.UX
and the scripts.
Thus the student is not brought to a halt by
difficult questions.  The burden on the counselor, however,
is much lower than that on a teacher of a course.
Ideally, the students should be encouraged to proceed with instruction
immediately prior to their actual use of the computer.
They should exercise the scripts on the same computer and the same
kind of terminal that they will later use
for their real work, and
their first few jobs for the computer should be
relatively easy ones.
Also, both training and initial work should take place on days
when the
.UX
hardware and software
are working reliably.
Rarely is all of this possible, but the closer one comes the better
the result.
For example, if it is known that the hardware is shaky one day, it is better
to attempt to reschedule training for another one.  Students are very
frustrated by machine downtime; when nothing is happening, it takes
some sophistication and experience to distinguish
an infinite loop, a slow but functioning program,
a program waiting for the user, and a broken machine.*
.FS
* We have even known an expert programmer to decide the computer
was broken when he had simply left his terminal in local mode.
Novices have great difficulties with such problems.
.FE
.PP
One disadvantage
of training with
.I
learn
.R
is that students come to depend
completely on the CAI system, and do not try
to read manuals or use other learning aids.
This is unfortunate, not only because of the increased
demands for completeness and accuracy of the
scripts, but because the scripts do not cover all of
the
.UX
system.
New users should have manuals (appropriate for their level) and
read them; the scripts ought to be altered
to recommend suitable documents and urge
students to read them.
.PP
There are several other difficulties which are clearly evident.
From the student's viewpoint,
the most serious is that
lessons still crop up which simply can't be passed.
Sometimes this is due to poor explanations,
but just as often it is some error in the lesson itself
\(em a botched setup, a missing file,
an invalid test for correctness,
or some system facility that doesn't work on the local
system in the same way it did on the development system.
It takes knowledge and a certain healthy arrogance on the part of the user to recognize
that the fault is not his or hers,
but the script writer's.
Permitting the student to get on with the next lesson
regardless does alleviate this somewhat,
and the logging facilities make it easy
to watch for lessons that no one
can pass,
but it is still a problem.
.PP
The biggest problem with the previous
.I learn
was speed (or lack thereof) \(em
it was often excruciatingly slow
and made a significant drain on the system.
The current version so far does not seem
to have that difficulty,
although some scripts,
notably
.I eqn ,
are intrinsically slow.
.I eqn ,
for example,
must do a lot of work even to print its introductions,
let alone check the student responses,
but delay is perceptible in all scripts
from time to time.
.PP
Another potential problem is that it is possible
to break
.ul
learn
inadvertently, by pushing interrupt at the wrong time,
or by removing critical files,
or any number of similar slips.
The defenses against such problems
have steadily been improved, to the point
where most students should not notice difficulties.
Of course, it will always be possible to break
.I
learn
.R
maliciously, but this is not likely to be a problem.
.PP
One area is more fundamental \(em
some
.UX
commands are sufficiently global in their effect
that
.ul
learn
currently
does not allow them to be executed at all.
The most obvious is
.I cd ,
which changes to another directory.
The prospect of a student who is learning about directories
inadvertently moving to some random directory
and removing files has deterred us
from even writing lessons on
.I cd ,
but ultimately lessons on such topics probably should be added.
.NH
Acknowledgments
.PP
We are grateful to all those who have tried
.ul
learn,
for we have benefited greatly from their
suggestions and criticisms.
In particular,
M. E. Bittrich,
J. L. Blue,
S. I. Feldman,
P. A. Fox,
and
M. J. McAlpin
have provided substantial feedback.
Conversations with E. Z. Rothkopf also provided many of the ideas in the system.
We are also indebted to Don Jackowski
for serving as a guinea pig for the second version,
and to Tom Plum for his efforts to improve the C script.
.\" .SG \s-2MH\s0-1273/4-\s-2MEL/BWK\s0-unix
.[
$LIST$
.]
