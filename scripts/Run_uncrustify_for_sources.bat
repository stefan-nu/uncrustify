@echo off
setlocal

rem
rem Checks if all source files conform
rem to the uncrustify configuration
rem
rem Call this batch script from the top level
rem directory of the uncrustify project.
rem
rem The uncrustify binary can be provided as
rem parameter. If no binary is provided the 
rem path is searched for the uncrustify binary.
rem

set SRC_DIR=src
set OUT_DIR=results
rem set CFG_FILE=forUncrustifySources.cfg
set CFG_FILE=scripts\uncrustify_nuinno.cfg

set DIFF_FILE=lastdiff.txt
set "PWD=%cd%"
set CMP_OPT=/L /N /T

if "%1"=="" (
	rem Check if uncrustify is available in path
	set found=""
	set prog=uncrustify.exe
	for %%i in ("%path:;=";"%") do (
		if exist %%~i\%prog% (
			set found=%%i
			echo found %prog% in %%i
		)
	)
) else (	
	set prog=%1
	where "%prog%" >nul 2>nul
	if %ERRORLEVEL% NEQ 0 goto PROG_MISSING
	set found=.
)

echo using %prog%
rem pause

if %found%=="" goto PROG_MISSING

rem Check if fc (file compare tool) is available in path
set found=""
set CMP=fc.exe
for %%i in ("%path:;=";"%") do (
    if exist %%~i\%CMP% (
		set found=%%i
rem		echo found %prog% in %%i
	)
)
if %found%=="" goto PROG_MISSING

rem build the list of source files
set SRC_LIST=%PWD%\%OUT_DIR%\files.txt
dir /b %SRC_DIR%\*.h   >  %SRC_LIST%
dir /b %SRC_DIR%\*.cpp >> %SRC_LIST%

rem ensure output directory exists
if NOT EXIST %OUT_DIR% md %OUT_DIR%

rem check every source file with uncrustify
rem and compare it with the original file
set DIFF=0
cd .\%SRC_DIR%
for /F %%i in (%SRC_LIST%) do (
	%prog% -q -c ..\%CFG_FILE% -f .\%%i -o ..\%OUT_DIR%\%%i
	cd
rem	%CMP% .\%%i ..\%OUT_DIR%\%%i
rem	> ..\%OUT_DIR%\%DIFF_FILE% 
	echo %Errorlevel%

	if %ERRORLEVEL% NEQ 0 (
		echo "Problem with %%i"
		echo "use: diff %SRC_DIR%\%%i %OUT_DIR%\%%i to find why"
		set DIFF=1
	) else (
rem		del ..\%OUT_DIR%\%%i
rem		del ..\%OUT_DIR%\%DIFF_FILE%
	)
rem pause
)

rem del %SRC_LIST%
cd ..
rem echo input=%SRC_DIR%
rem echo output=%OUT_DIR%
winmergeu /r /e /u /f *.cpp /wr /dl SourceFiles /dr UncrustifiedFiles ./%SRC_DIR% ./%OUT_DIR%

if %DIFF% NEQ 0 (
	echo "some problem(s) are still present"
	exit /b 1
) else (
	echo "all sources are uncrustify-ed"
	exit /b 0
)


:PROG_MISSING
echo.
echo ------------------------------------------------------
echo Error: %prog% not found.
echo Verify that %prog% is correctly installed
echo ------------------------------------------------------ 
echo.

endlocal
