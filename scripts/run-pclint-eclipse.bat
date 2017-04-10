rem set to on for debugging
@echo off
setlocal

rem Run this script from the project root directory

echo ------------------------------------------------------
echo Start pcLint analysis to check code quality ...

rem Check if pcLint program is available
set prog=lint-nt.exe
for %%i in ("%path:;=";"%") do (
rem echo %%~i
    if exist %%~i\%prog% (
		set found=%%i
		echo found %prog% in %%i
	)
)
if %found%=="" goto PROG_MISSING

if NOT EXIST test        md test
if NOT EXIST test\pclint md test\pclint

rem create list of all C source files
rem FIXME: works only if there are no spaces in the paths 

dir /s/b lnt\*.lnt				>  .\test\pclint\exceptions.lnt
dir /s/b src\*.c 				>  .\test\pclint\files.lnt
dir /s/b src\*.cpp 				>  .\test\pclint\files.lnt


rem "D:\PROGRA~1\pclint\Lint-nt"  -v  -passes(4) -static_depth(2) -i"D:\PROGRA~1\pclint"  -i".\include" std.lnt env-xml.lnt  -os(_LINT.TMP) %1 %2 %3 %4 %5 %6 %7 %8 %9
@echo pclint started
rem lint-nt .\scripts\pclint_cfg_eclipse.lnt .\test\pclint\files.lnt > test\pclint\pclint-results.xml
lint-nt .\scripts\pclint_cfg_eclipse.lnt .\test\pclint\exceptions.lnt .\test\pclint\files.lnt

rem type test\pclint\pclint-results.xml | more
rem type test\pclint\pclint-results.xml
rem echo pcLint output placed in test\pclint\pclint-results.xml

goto END

:PROG_MISSING
echo.
echo ------------------------------------------------------
echo pcLint Error: %prog% not found.
echo Verify that PCLINT is correctly installed, the 
echo installation was added to the PATH and the
echo environment variable PCLINT_HOME was set to its path. 
echo ------------------------------------------------------ 
echo.
goto END

:END 
echo pcLint finished
echo ------------------------------------------------------
endlocal

