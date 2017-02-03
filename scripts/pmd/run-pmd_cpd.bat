@echo off
setlocal

rem Run this script from the top level directory of the project
rem
rem Usage instructions for CPD
rem https://pmd.github.io/latest/usage/cpd-usage.html
rem
rem so see CPD help run "cpd.bat -h"

echo ------------------------------------------------------
@echo Running Copy Paste detector (CPD from PMD) to find similar code ...

rem Check if SIMINAN program is available
set prog=cpd.bat
for %%i in ("%path:;=";"%") do (
    if exist %%~i\%prog% (
		set found=%%i
		echo found %prog% in %found%
	)
)
if %found%=="" goto PROG_MISSING

rem ensure expected directories are present
if NOT EXIST tests md tests
if NOT EXIST tests\pmd md tests\pmd

@echo CPD from PMD started

rem save results to file
rem cpd.bat --language Cpp --minimum-tokens 30 --format xml --files src/*.* --files include/*.* > tests\pmd\cpd_results.xml

rem display results directly in console of eclipse
cpd.bat --language Cpp --minimum-tokens 50 --ignore-literals --ignore-identifiers --format txt --files src/*.* --files include/*.* --exclude include\char_table.h

goto END

:PROG_MISSING
echo.
echo ------------------------------------------------------
echo SIMIAN Error: %prog% not found.
echo Verify that simian is correctly installed and the 
echo directory that holds simian.exe was added to the PATH.
echo ------------------------------------------------------ 
echo.
goto END 


:END
echo Simian script finished