@echo off

for /f "tokens=2 delims==" %%a in ('wmic OS Get localdatetime /value') do set "dt=%%a"
set "YY=%dt:~2,2%" & set "YYYY=%dt:~0,4%" & set "MM=%dt:~4,2%" & set "DD=%dt:~6,2%"
REM echo Year: %YYYY%

REM set YYYY=2020
REM git log --tags --simplify-by-decoration --pretty="format:%%cI %%d" | find /c "%YYYY%"

set VERMINOR=
for /f "tokens=* USEBACKQ" %%a in (
  `git log --tags --simplify-by-decoration --pretty^=^"format^:%%cI^" ^| find ^/c ^"%YYYY%^"`
) do set VERMINOR=%%a
REM echo Version minor: %VERMINOR%
set /A VERMINOR=%VERMINOR%+1
REM echo %VERMINOR%

echo Version: %YYYY%.%VERMINOR%

for /f %%i in ('git rev-list HEAD --count') do set REVISION=%%i
echo Revision: %REVISION%

echo. > Version.h
echo #define APP_VERSION_STRING "%YYYY%.%VERMINOR%" >> Version.h
echo. >> Version.h
echo #define APP_REVISION %REVISION% >> Version.h
