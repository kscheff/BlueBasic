@echo off
::: Begin set date

for /f "tokens=1-4 delims=/-. " %%i in ('date /t') do (call :set_date %%i %%j %%k %%l)
goto :end_set_date

:set_date
if "%1:~0,1%" gtr "9" shift
for /f "skip=1 tokens=2-4 delims=(-)" %%m in ('echo,^|date') do (set %%m=%1&set %%n=%2&set %%o=%3)
goto :eof

:end_set_date
::: End set date

set hour=%time:~0,2%
if %hour% lss 10 (set hour=0%time:~1,1%)
set timestamp=%yy%%mm%%dd%%hour%%time:~3,2%%time:~6,2%
echo #define BUILD_TIMESTAMP "%2/%timestamp%" > "%3/timestamp.h"
echo %timestamp% > "%1\..\..\..\..\..\hex\BlueBasic-%2.version"
