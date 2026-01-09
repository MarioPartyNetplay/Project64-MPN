@ECHO OFF
SETLOCAL

for /f "delims=" %%a in ('WHERE 7z 2^>nul') do set "zip=%%a"

if "%zip%" == "" (
	if exist "C:\Program Files\7-Zip\7z.exe" (
		set "zip=C:\Program Files\7-Zip\7z.exe"
	) else (
		echo can not find 7z.exe
		goto :EndErr
	)
)

set ZipFileName=Project64-Netplay
if not "%1" == "" set ZipFileName=%1
if "%~2" == "x64" set VSPlatform=64


SET current_dir=%cd%
cd /d %~dp0
SET base_dir=%cd%
cd /d %current_dir%

IF EXIST "%base_dir%\Package" rmdir /S /Q "%base_dir%\Package"
IF %ERRORLEVEL% NEQ 0 GOTO EndErr
IF NOT EXIST "%base_dir%\Package" mkdir "%base_dir%\Package"
IF %ERRORLEVEL% NEQ 0 GOTO EndErr

rd "%base_dir%\Bin\Package" /Q /S > NUL 2>&1
md "%base_dir%\Bin\Package"
md "%base_dir%\Bin\Package\Config\Backup"
md "%base_dir%\Bin\Package\Extras\Mario Party 3 Costume Party v1.1 Patcher"
md "%base_dir%\Bin\Package\Games"
md "%base_dir%\Bin\Package\Plugin%VSPlatform%\Audio"
md "%base_dir%\Bin\Package\Plugin%VSPlatform%\GFX\hires_texture\MarioParty"
md "%base_dir%\Bin\Package\Plugin%VSPlatform%\GFX\hires_texture\MarioParty2"
md "%base_dir%\Bin\Package\Plugin%VSPlatform%\GFX\hires_texture\MarioParty3"

md "%base_dir%\Bin\Package\Plugin%VSPlatform%\Input"
md "%base_dir%\Bin\Package\Plugin%VSPlatform%\RSP"
md "%base_dir%\Bin\Package\Plugin%VSPlatform%\Updater"
md "%base_dir%\Bin\Package\Save"

copy "%base_dir%\Bin\Release%VSPlatform%\Project64-MPN.exe" "%base_dir%\Bin\Package"

copy "%base_dir%\Source\UserFiles\Config\*.*" "%base_dir%\Bin\Package\Config"
copy "%base_dir%\Source\UserFiles\Config\Backup\*.*" "%base_dir%\Bin\Package\Config\Backup\"

copy "%base_dir%\Source\UserFiles\Extras\*.*" "%base_dir%\Bin\Package\Extras"
copy "%base_dir%\Source\UserFiles\Extras\Mario Party 3 Costume Party v1.1 Patcher\*.*" "%base_dir%\Bin\Package\Extras\Mario Party 3 Costume Party v1.1 Patcher\"

copy "%base_dir%\Source\UserFiles\Games\*.*" "%base_dir%\Bin\Package\Games"

copy "%base_dir%\Source\UserFiles\Plugin\Audio\*" "%base_dir%\Bin\Package\Plugin%VSPlatform%\Audio"
copy "%base_dir%\Source\UserFiles\Plugin\GFX\*" "%base_dir%\Bin\Package\Plugin%VSPlatform%\GFX"
copy "%base_dir%\Source\UserFiles\Plugin\GFX\hires_texture\MarioParty\*.*" "%base_dir%\Bin\Package\Plugin%VSPlatform%\GFX\hires_texture\MarioParty\"
copy "%base_dir%\Source\UserFiles\Plugin\GFX\hires_texture\MarioParty2\*.*" "%base_dir%\Bin\Package\Plugin%VSPlatform%\GFX\hires_texture\MarioParty2\"
copy "%base_dir%\Source\UserFiles\Plugin\GFX\hires_texture\MarioParty3\*.*" "%base_dir%\Bin\Package\Plugin%VSPlatform%\GFX\hires_texture\MarioParty3\"

copy "%base_dir%\Source\UserFiles\Plugin\Input\*" "%base_dir%\Bin\Package\Plugin%VSPlatform%\Input"
copy "%base_dir%\Bin\Release%VSPlatform%\Plugin\RSP\RSP 1.7.dll" "%base_dir%\Bin\Package\Plugin%VSPlatform%\RSP"

cd %base_dir%\Bin\Package
"%zip%" a -tzip -r "%base_dir%\Package\%ZipFileName%" *
cd /d %current_dir%

echo Package %ZipFileName% created

copy "%base_dir%\Package\Project64-MPN.zip" "Bin\Release\"

goto :end


:EndErr
ENDLOCAL
echo Build failed
exit /B 1

:End
ENDLOCAL
exit /B 0
