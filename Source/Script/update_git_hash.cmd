@echo off
REM Script to automatically update the git hash in Version.h
REM Usage: update_git_hash.cmd

setlocal enabledelayedexpansion

set "VERSION_FILE=%~dp0..\..\Source\Project64-core\Version.h"

REM Get the current git hash (7 characters)
for /f %%i in ('git rev-parse --short=7 HEAD') do set GIT_HASH=%%i

echo Updating git hash in %VERSION_FILE% to: %GIT_HASH%

REM Create a temporary file
set "TEMP_FILE=%TEMP%\version_temp.h"

REM Replace GIT_HASH_PLACEHOLDER with the actual git hash
powershell -Command "(Get-Content '%VERSION_FILE%') -replace 'GIT_HASH_PLACEHOLDER', '%GIT_HASH%' | Set-Content '%TEMP_FILE%'"

REM Move the temporary file back to the original location
move "%TEMP_FILE%" "%VERSION_FILE%" >nul

echo Git hash updated successfully!
pause