

print "\nModifying the '.t' files...\n\n";

use File::Basename;
use File::Copy;

## Change the below line to the folder you want to process
$DirName = "/perl/scripts/t";

$FilesTotal = 0;
$FilesRead = 0;
$FilesModified = 0;

opendir(DIR, $DirName);
@Dirs = readdir(DIR);

foreach $DirItem(@Dirs)
{
	$DirItem = $DirName."/".$DirItem;
	push @DirNames, $DirItem;	# All items under  $DirName  folder is copied into an array.
}

foreach $FileName(@DirNames)
{
	if(-d $FileName)
	{	# If an item is a folder, then open it further.

		opendir(SUBDIR, $FileName);
		@SubDirs = readdir(SUBDIR);
		close(SUBDIR);

		foreach $SubFileName(@SubDirs)
		{
			if(-f $SubFileName)
			{
				&Process_File($SubFileName);	# If file, process it.
			}
			else
			{
				$SubFileName = $FileName."/".$SubFileName;
				push @DirNames, $SubFileName;	# If sub-folder, push it into the array.
			}
		}
	}
	else
	{
		if(-f $FileName)
		{
			&Process_File($FileName);	# If file, process it.
		}
	}
}

close(DIR);

print "\n\n\nTotal number of files present = $FilesTotal\n";
print "Total number of '.t' files read = $FilesRead\n";
print "Total number of '.t' files modified = $FilesModified\n\n";




# Process the file.
sub Process_File
{
	local($FileToProcess) = @_;		# File name.
	local($Modified) = 0;

	if(!(-w $FileToProcess)) {
		# If the file is a read-only file, then change its mode to read-write.
		chmod(0777, $FileToProcess);
	}

	## For example:
	## If the value of $FileToProcess is '/perl/scripts/t/pragma/warnings.t', then
		## $dir = '/perl/scripts/t/pragma/'
		## $base = 'warnings'
		## $ext = '.t'
	$dir = dirname($FileToProcess);		# Get the folder name
	$base = basename($FileToProcess);	# Get the base name
	($base, $dir, $ext) = fileparse($FileToProcess, '\..*');	# Get the extension of the file passed.


	# Do the processing only if the file has '.t' extension.
	if($ext eq '.t') {

		open(FH, '+<', $FileToProcess) or die "Unable to open the file,  $FileToProcess  for reading and writing.\n";
		@ARRAY = <FH>;	# Get the contents of the file into an array.

		foreach $Line(@ARRAY)	# Get each line of the file.
		{
			if($Line =~ m/\@INC = /)
			{	# If the line contains the string (@INC = ), then replace it

				# Replace "@INC = " with "unshift @INC, "
				$Line =~ s/\@INC = /unshift \@INC, /;

				$Modified = 1;
			}

			if($Line =~ m/push \@INC, /)
			{	# If the line contains the string (push @INC, ), then replace it

				# Replace "push @INC, " with "unshift @INC, "
				$Line =~ s/push \@INC, /unshift \@INC, /;

				$Modified = 1;
			}
		}

		seek(FH, 0, 0);		# Seek to the beginning.
		print FH @ARRAY;	# Write the changed array into the file.
		close FH;			# close the file.

		$FilesRead++;	# One more file read.

		if($Modified) {
			print "Modified the file,  $FileToProcess\n";
			$Modified = 0;

			$FilesModified++;	# One more file modified.
		}
	}

	$FilesTotal++;	# One more file present.
}

