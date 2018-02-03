# ukncbtl
UKNCBTL -- UKNC Back to Life! emulator, Win32 version.

[![Build status](https://ci.appveyor.com/api/projects/status/xicur65lusd5c3ab?svg=true)](https://ci.appveyor.com/project/nzeemin/ukncbtl)

*UKNCBTL -- UKNC Back to Life!*
-- is cross-platform UKNC emulator for Windows/Linux/Mac OS X.
*UKNC* (*УКНЦ*, Электроника МС-0511) is soviet school computer based on two PDP-11 compatible processors KM1801VM2.

The UKNCBTL project consists of:
* [ukncbtl](https://github.com/nzeemin/ukncbtl/) written for Win32 and works under Windows 2000/2003/2008/XP/Vista/7.
* [ukncbtl-renders](https://github.com/nzeemin/ukncbtl-renders/) -- renderers for UKNCBTL Win32.
* [ukncbtl-qt](https://github.com/nzeemin/ukncbtl-qt/) is based on Qt framework and works under Windows, Linux and Mac OS X.
* [ukncbtl-testbench](https://github.com/nzeemin/ukncbtl-testbench/) -- test bench for regression testing.
* [ukncbtl-utils](https://github.com/nzeemin/ukncbtl-utils/) -- various utilities: rt11dsk, sav2wav, UkncComSender, ESCParser.
* [ukncbtl-doc](https://github.com/nzeemin/ukncbtl-doc/) -- documentation and screenshots.
* Project wiki: https://github.com/nzeemin/ukncbtl-doc/wiki
  * Screenshots: https://github.com/nzeemin/ukncbtl-doc/wiki/Screenshots-ru
  * User's Manual (in Russian): https://github.com/nzeemin/ukncbtl-doc/wiki/Users-Manual-ru

Current status: Beta, under development. Most of software works fine.

Emulated:
 * CPU and PPU
 * Both memory controllers
 * Video controller
 * FDD controller (MZ standard)
 * ROM cartridges
 * Sound
 * Hard disk -- can read/write and boot
 * Tape cassette -- read/write WAV PCM files
 * Serial port (experimental)
 * Parallel port (experimental)

NOT emulated yet: network card, RAM disk.
