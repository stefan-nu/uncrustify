@echo off
setlocal

echo ------------------------------------------------------
@echo Running CppCheck to find code smells ...

rem Check if CPPCHECK program is available
set prog=cppcheck.exe
for %%i in ("%path:;=";"%") do (
    if exist %%~i\%prog% (
		set found=%%i
		echo found %prog% in %found%
	)
)
if %found%=="" goto PROG_MISSING

if NOT EXIST tests md tests
if NOT EXIST tests\cppcheck md tests\cppcheck

rem --check-library --library=scripts/cppcheck.type 
rem --check-config

@echo cppcheck started
%prog% --library=scripts/cppcheck/cppcheck.cfg --enable=warning,performance --xml-version=2 src

rem %prog%  --suppress=missingIncludeSystem --enable=warning,performance -UGCC -DVDSP=1 -D_ADI_COMPILER=1 -USIMULATOR=0 p ou2G.cfg --xml --enable=style -Iinclude -Iinc src/ 2>test\cppcheck\cppcheck-results.xml

rem %prog% --library=scripts\cppcheck\cppcheck.cfg --suppressions scripts\cppcheck\cppcheck.sup 2>test\cppcheck\cppcheck-results.xml

echo 1
goto END

:PROG_MISSING
echo.
echo ------------------------------------------------------
echo CPPCHECK Error: %prog% not found.
echo Verify that cppcheck is correctly installed and the 
echo directory that holds cppcheck.exe was added to the PATH.
echo ------------------------------------------------------ 
echo.
goto END

:END
echo CppCheck script finished
echo ------------------------------------------------------
endlocal