@echo off

rem --- Clean up ---
if exist ZenLib_GNU_Prepare.7z del ZenLib_GNU_Prepare.7z
if exist ZenLib_GNU_Prepare    rmdir ZenLib_GNU_Prepare /S /Q
mkdir ZenLib_GNU_Prepare

rem --- Copying : Sources ---
xcopy ..\Source\ZenLib\*.c   ZenLib_GNU_Prepare\Source\ZenLib\ /S
xcopy ..\Source\ZenLib\*.cpp ZenLib_GNU_Prepare\Source\ZenLib\ /S
xcopy ..\Source\ZenLib\*.h   ZenLib_GNU_Prepare\Source\ZenLib\ /S

rem --- Copying : Projects ---
xcopy ..\Project\GNU\* ZenLib_GNU_Prepare\Project\GNU\ /S

rem --- Copying : Release ---
xcopy *.sh  ZenLib_GNU_Prepare\Release\
xcopy *.txt ZenLib_GNU_Prepare\Release\

rem --- Copying : Information files ---
copy ..\*.txt ZenLib_GNU_Prepare\


rem --- Compressing Archive ---
if "%2"=="SkipCompression" goto SkipCompression
..\..\Shared\Binary\Win32\7-Zip\7z a -r -t7z -mx9 ZenLib_GNU_Prepare.7z ZenLib_GNU_Prepare\*
:SkipCompression

rem --- Clean up ---
if "%1"=="SkipCleanUp" goto SkipCleanUp
rmdir ZenLib_GNU_Prepare /S /Q
:SkipCleanUp
