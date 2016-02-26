@echo off
c:\bin\astyle.exe -n -Q --options=astyle-cpp-options emubase\*.h emubase\*.cpp
c:\bin\astyle.exe -n -Q --options=astyle-cpp-options Util\*.h Util\*.cpp
c:\bin\astyle.exe -n -Q --options=astyle-cpp-options *.h --exclude=Resource.h --exclude=Version.h --exclude=stdafx.h
c:\bin\astyle.exe -n -Q --options=astyle-cpp-options *.cpp --exclude=stdafx.cpp
