

print "\nGenerating automated scripts for NetWare...\n\n\n";


use File::Basename;
use File::Copy;

chdir '/perl/scripts/';
$DirName = "t";

# These scripts have problems (either abend or hang) as of now (11 May 2001).
# So, they are commented out in the corresponding auto scripts, io.pl and lib.pl
@ScriptsNotUsed = ("t/io/openpid.t", "t/lib/filehandle.t", "t/lib/memoize/t/expire_module_t.t", "t/lib/NEXT/t/next.t", "t/lib/Math/BigInt/t/require.t", "t/ext/B/t/debug.t", "t/lib/IPC/Open3.t", "t/ext/B/t/showlex.t", "t/op/subst_wamp.t", "t/uni/upper.t", "t/lib/Net/t/ftp.t", "t/op/sort.t", "t/ext/POSIX/t/posix.t", "t/lib/CPAN/t/loadme.t", "t/lib/CPAN/t/vcmp.t");

opendir(DIR, $DirName);
@Dirs = readdir(DIR);
close(DIR);
foreach $DirItem(@Dirs)
{
	$DirItem1 = $DirName."/".$DirItem;
	push @DirNames, $DirItem1;	# All items under  $DirName  folder is copied into an array.

	if(-d $DirItem1)
	{	# If an item is a folder, then open it further.

		# Intemediary automated script like base.pl, lib.pl, cmd.pl etc.
		$IntAutoScript = "t/".$DirItem.".pl";

		# Open once in write mode since later files are opened in append mode,
		# and if there already exists a file with the same name, all further opens
		# will append to that file!!
		open(FHW, "> $IntAutoScript") or die "Unable to open the file,  $IntAutoScript  for writing.\n";
		seek(FHW, 0, 0);	# seek to the beginning of the file.
		close FHW;			# close the file.
	}
}


print "Generating  t/nwauto.pl ...\n\n\n";

open(FHWA, "> t/nwauto.pl") or die "Unable to open the file,  t/nwauto.pl  for writing.\n";
seek(FHWA, 0, 0);	# seek to the beginning of the file.

$version = sprintf("%vd",$^V);
print FHWA "\n\nprint \"Automated Unit Testing of Perl$version for NetWare\\n\\n\\n\"\;\n\n\n";


foreach $FileName(@DirNames)
{
	$index = 0;
	if(-d $FileName)
	{	# If an item is a folder, then open it further.

		$dir = dirname($FileName);		# Get the folder name

		foreach $DirItem1(@Dirs)
		{
			$DirItem2 = $DirItem1;
			if($FileName =~ m/$DirItem2/)
			{
				$DirItem = $DirItem1;

				# Intemediary automated script like base.pl, lib.pl, cmd.pl etc.
				$IntAutoScript = "t/".$DirItem.".pl";
			}
		}

		# Write into the intermediary auto script.
		open(FHW, ">> $IntAutoScript") or die "Unable to open the file,  $IntAutoScript  for appending.\n";
		seek(FHW, 0, 2);	# seek to the end of the file.

		$pos = tell(FHW);
		if($pos <= 0)
		{
			print "Generating  $IntAutoScript...\n";
			print FHW "\n\nprint \"Testing  $DirItem  folder:\\n\\n\\n\"\;\n\n\n";
		}

		opendir(SUBDIR, $FileName);
		@SubDirs = readdir(SUBDIR);
		close(SUBDIR);
		foreach $SubFileName(@SubDirs)
		{
			$SubFileName = $FileName."/".$SubFileName;
			if(-d $SubFileName)
			{
				push @DirNames, $SubFileName;	# If sub-folder, push it into the array.
			}
			else
			{
				&Process_File($SubFileName);	# If file, process it.
			}

			$index++;
		}

		close FHW;			# close the file.

		if($index <= 0)
		{
			# The folder is empty and delete the corresponding '.pl' file.
			unlink($IntAutoScript);
			print "Deleted  $IntAutoScript  since it corresponded to an empty folder.\n";
		}
		else
		{
			if($pos <= 0)
			{	# This logic to make sure that it is written only once.
				# Only if something is written into the intermediary auto script,
				# only then make an entry of the intermediary auto script in  nwauto.pl
				print FHWA "print \`perl $IntAutoScript\`\;\n";
				print FHWA "print \"\\n\\n\\n\"\;\n\n";
			}
		}
	}
	else
	{
		if(-f $FileName)
		{
			$dir = dirname($FileName);		# Get the folder name
			$base = basename($FileName);	# Get the base name
			($base, $dir, $ext) = fileparse($FileName, '\..*');	# Get the extension of the file passed.
			
			# Do the processing only if the file has '.t' extension.
			if($ext eq '.t')
			{
				print FHWA "print \`perl $FileName\`\;\n";
				print FHWA "print \"\\n\\n\\n\"\;\n\n";
			}
		}
	}
}


## Below adds the ending comments into all the intermediary auto scripts:

opendir(DIR, $DirName);
@Dirs = readdir(DIR);
close(DIR);
foreach $DirItem(@Dirs)
{
	$index = 0;

	$FileName = $DirName."/".$DirItem;
	if(-d $FileName)
	{	# If an item is a folder, then open it further.

		opendir(SUBDIR, $FileName);
		@SubDirs = readdir(SUBDIR);
		close(SUBDIR);

		# To not to write into the file if the corresponding folder was empty.
		foreach $SubDir(@SubDirs)
		{
			$index++;
		}

		if($index > 0)
		{
			# The folder not empty.

			# Intemediary automated script like base.pl, lib.pl, cmd.pl etc.
			$IntAutoScript = "t/".$DirItem.".pl";

			# Write into the intermediary auto script.
			open(FHW, ">> $IntAutoScript") or die "Unable to open the file,  $IntAutoScript  for appending.\n";
			seek(FHW, 0, 2);	# seek to the end of the file.

			# Write into the intermediary auto script.
			print FHW "\nprint \"Testing of  $DirItem  folder done!\\n\\n\"\;\n\n";

			close FHW;			# close the file.
		}
	}
}


# Write into  nwauto.pl
print FHWA "\nprint \"Automated Unit Testing of Perl$version for NetWare done!\\n\\n\"\;\n\n";

close FHWA;			# close the file.

print "\n\nGeneration of  t/nwauto.pl  Done!\n\n";

print "\nGeneration of automated scripts for NetWare DONE!\n";




# Process the file.
sub Process_File
{
	local($FileToProcess) = @_;		# File name.
	local($Script) = 0;
	local($HeadCut) = 0;

	## For example:
	## If the value of $FileToProcess is '/perl/scripts/t/pragma/warnings.t', then
		## $dir1 = '/perl/scripts/t/pragma/'
		## $base1 = 'warnings'
		## $ext1 = '.t'
	$dir1 = dirname($FileToProcess);	# Get the folder name
	$base1 = basename($FileToProcess);	# Get the base name
	($base1, $dir1, $ext1) = fileparse($FileToProcess, '\..*');	# Get the extension of the file passed.

	# Do the processing only if the file has '.t' extension.
	if($ext1 eq '.t')
	{
		foreach $Script(@ScriptsNotUsed)
		{
			# The variables are converted to lower case before they are compared.
			# This is done to remove the case-sensitive comparison done by 'eq'.
			$Script1 = lc($Script);
			$FileToProcess1 = lc($FileToProcess);
			if($Script1 eq $FileToProcess1)
			{
				$HeadCut = 1;
			}
		}

		if($HeadCut)
		{
			# Write into the intermediary auto script.
			print FHW "=head\n";
		}

		# Write into the intermediary auto script.
		print FHW "print \"Testing  $base1"."$ext1:\\n\\n\"\;\n";
		print FHW "print \`perl $FileToProcess\`\;\n";	# Write the changed array into the file.
		print FHW "print \"\\n\\n\\n\"\;\n";

		if($HeadCut)
		{
			# Write into the intermediary auto script.
			print FHW "=cut\n";
		}

		$HeadCut = 0;
		print FHW "\n";
	}
}

