@echo off
rem ! This is a batch file to delete all the files on its
rem ! command line, to work around command.com's del command's
rem ! braindeadness
rem !
rem !    -- BKS, 11-11-2000

:nextfile
set file=%1
shift
if "%file%"=="" goto end
del %file%
goto nextfile
:end

@echo off
rem ! This is a batch file to delete all the files on its
rem ! command line, to work around command.com's del command's
rem ! braindeadness
rem !
rem !    -- BKS, 11-11-2000

:nextfile
set file=%1
shift
if "%file%"=="" goto end
del %file%
goto nextfile
:end
