@echo off
set ASTYLEEXE=c:\bin\astyle.exe
set ASTYLEOPT=-n -Q --options=astyle-cpp-options
%ASTYLEEXE% %ASTYLEOPT% emubase\*.h emubase\*.cpp Util\*.h Util\*.cpp
%ASTYLEEXE% %ASTYLEOPT% *.h --exclude=Version.h --exclude=stdafx.h
%ASTYLEEXE% %ASTYLEOPT% *.cpp --exclude=stdafx.cpp
