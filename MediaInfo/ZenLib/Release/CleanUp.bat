@echo off
rem --- General ---
pushd ..
del /Q /S     *.~* *.obj *.o *.tds *.dcu *.ddp *.opt *.ncb *.suo *.ilk *.idb *.pdb *.pch *.plg *.aps *.user *.win *.layout *.local *.depend *.identcache *.tgs *.tgw *.sdf
del /AH /Q /S *.~* *.obj *.o *.tds *.dcu *.ddp *.opt *.ncb *.suo *.ilk *.idb *.pdb *.pch *.plg *.aps *.user *.win *.layout *.local *.depend *.identcache *.tgs *.tgw *.sdf
popd

rem Borland Developper Studio ---
pushd ..\Project\BCB
call CleanUp
popd

rem Code::Blocks ---
pushd ..\Project\CodeBlocks
call CleanUp
popd

rem MS Visual Studio ---
pushd ..\Project\MSVC2012
call CleanUp
popd
pushd ..\Project\MSVC2010
call CleanUp
popd
pushd ..\Project\MSVC2008
call CleanUp
popd
pushd ..\Project\MSVC2005
call CleanUp
popd

rem Release ---
del /Q /S *.zip *.gz *.bz2 *.7z
