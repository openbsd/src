

		Automated Testing of Perl5 Interpreter for NetWare.



A set of Standard Unit Test Scripts to test all the functionalities of 
Perl5 Interpreter are available along with the CPAN download. They are 
all located under 't' folder. These include sub-folders under 't' such 
as: 'base', 'cmd', 'comp', 'io', lib', 'op', 'pod', 'pragma' and 'run'. 
Each of these sub-folders contain few test scripts ('.t' files) under 
them.

Executing these test scripts on NetWare can be automated as per the 
following:

1. Generate automated scripts like 'base.pl', 'cmd.pl', 'comp.pl', 'io.pl',
'lib.pl', 'op.pl', 'pod.pl', 'pragma.pl', 'run.pl' that execute all the
test scripts ('.t' files) under the corresponding folder.

For example, 'base.pl' to test all the scripts 
              under 'sys:\perl\scripts\t\base' folder,
             'comp.pl' to test all the scripts 
              under 'sys:\perl\scripts\t\comp' folder and so on.

2. Generate an automated script, 'nwauto.pl' that executes all the above 
mentioned '.pl' automated scripts, thus in turn executing all the '.t' 
scripts.

The script, 'NWScripts.pl' available under the 'NetWare\t' folder of the 
CPAN download, is written to generate these automated scripts when 
executed on a NetWare server. It generates 'base.pl', 'cmd.pl', 'comp.pl',
'io.pl', 'lib.pl', 'op.pl', 'pod.pl', 'pragma.pl', 'run.pl' and also 
'nwauto.pl' by including all the corresponding '.t' scripts in them in 
backtick operators.

For example, all the scripts that are under 't\base' folder will be 
entered in 'base.pl' and so on. 'nwauto.pl' includes all these '.pl' 
scripts like 'base.pl', 'comp.pl' etc.


Perform the following steps to execute the automated scripts:

1. Map your NetWare server to "i:"

2. After complete build (building both interpreter and all extensions)
of Perl for NetWare, execute "nmake nwinstall" in the 'NetWare' folder
of the CPAN download. This installs all the library files, perl modules,
the '.pl' files under 'NetWare\t' folder and all the '.t' scripts
under 't' folder, all in appropriate folders onto your server.

3. Execute the command  "perl t\NWModify.pl"  on the console command 
prompt of your server. This script replaces

     "@INC = " with "unshift @INC, "  and
     "push @INC, " with "unshift @INC, "

from all the scripts under 'sys:\perl\scripts\t' folder.

This is done to include the correct path for libraries into the scripts 
when executed on NetWare. If this is not done, some of the scripts will 
not get executed since they cannot locate the corresponding libraries.

4. Execute the command  "perl t\NWScripts.pl"  on the console command 
prompt to generate the automated scripts mentioned above 
under the 'sys:\perl\scripts\t' folder.

5. Execute the command  "perl t\nwauto.pl"  on the server console command 
prompt. This runs all the standard test scripts. If you desire to 
redirect or save the results into a file, say 'nwauto.txt', then the 
console command to execute is:  "perl t\nwauto.pl > nwauto.txt".

6. If you wish to execute only a certain set of scripts, then run the 
corresponding '.pl' file. For example, if you wish to execute only the 
'lib' scripts, then execute 'lib.pl' through the server console command, 
"perl t\lib.pl'. To redirect the results into a file, the console command
 is, "perl t\lib.pl > lib.txt".



Known Issues:

The following scripts are commented out in the corresponding autoscript:

1. 'openpid.t' in 'sys:\perl\scripts\t\io.pl' script
   Reason:
     This either hangs or abends the server when executing through auto 
     scripts. When run individually, the script execution goes through 
     fine.

2. 'argv.t' in 'sys:\perl\scripts\t\io.pl' script
   Reason:
     This either hangs or abends the server when executing through auto 
     scripts. When run individually, the script execution goes through 
     fine.

3. 'filehandle.t' in 'sys:\perl\scripts\t\lib.pl' script
   Reason:
     This hangs in the last test case where it uses FileHandle::Pipe 
     whether run individually or through an auto script.

