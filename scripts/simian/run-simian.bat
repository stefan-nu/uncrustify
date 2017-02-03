@echo off
setlocal

echo ------------------------------------------------------
@echo Running Simian to find similar code ...

rem Check if SIMINAN program is available
set prog=simian.exe
for %%i in ("%path:;=";"%") do (
    if exist %%~i\%prog% (
		set found=%%i
		echo found %prog% in %found%
	)
)
if %found%=="" goto PROG_MISSING

rem ensure expected directories are present
if NOT EXIST tests md tests
if NOT EXIST tests\simian md tests\simian

@echo simian started
simian -formatter=xml -threshold=3 +ignoreNumbers ..\..\src\*.c > tests\simian\simian-result.xml

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