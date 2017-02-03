@echo off
setlocal

echo ------------------------------------------------------
@echo Running Dr. memory to find memory faults ...

rem Check if CPPCHECK program is available
set prog=drmemory.exe
for %%i in ("%path:;=";"%") do (
    if exist %%~i\%prog% (
		set found=%%i
		echo found %prog% in %found%
	)
)
if %found%=="" goto PROG_MISSING

if NOT EXIST tests md tests
if NOT EXIST tests\drmemory md tests\drmemory

rem --check-library --library=scripts/cppcheck.type 
rem --check-config

@echo drmemory started
%prog% Debug/uncrustify.exe

echo 1
goto END

:PROG_MISSING
echo.
echo ------------------------------------------------------
echo Error: %prog% not found.
echo Verify that program is correctly installed and the 
echo directory that holds cppcheck.exe was added to the PATH.
echo ------------------------------------------------------ 
echo.
goto END

:END
echo Dr. Memory script finished
echo ------------------------------------------------------
endlocal