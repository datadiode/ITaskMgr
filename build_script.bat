@echo off

setlocal

set "PLATFORM=%~1"
set "CONFIGURATION=%~2"

call "itaskmgr_src\GIT-VS-VERSION-GEN.bat" itaskmgr_src "itaskmgr_src\gen-versioninfo.h"

findstr /c:"ActiveCfg = %CONFIGURATION%|%PLATFORM%" /e itaskmgr_src\ITaskMgr_vc9.sln
if errorlevel 1 (
  call "%VS140COMNTOOLS%vsvars32.bat"
  msbuild /t:Rebuild itaskmgr_src\ITaskMgr.vcxproj /p:SolutionDir=%~dp0itaskmgr_src\ /p:Platform="%PLATFORM%" /p:Configuration="%CONFIGURATION%"
) else (
  call "%ProgramFiles(x86)%\Microsoft Visual Studio 9.0\Common7\Tools\vsvars32.bat"
  vcbuild /rebuild itaskmgr_src\ITaskMgr_vc9.sln "%CONFIGURATION%|%PLATFORM%"
)

endlocal
