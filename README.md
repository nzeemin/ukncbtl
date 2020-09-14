# ukncbtl
UKNCBTL — UKNC Back to Life! emulator, Win32 version.

[![License: LGPL v3](https://img.shields.io/badge/License-LGPL%20v3-blue.svg)](https://www.gnu.org/licenses/lgpl-3.0)
[![Build status](https://ci.appveyor.com/api/projects/status/xicur65lusd5c3ab?svg=true)](https://ci.appveyor.com/project/nzeemin/ukncbtl)
[![CodeFactor](https://www.codefactor.io/repository/github/nzeemin/ukncbtl/badge)](https://www.codefactor.io/repository/github/nzeemin/ukncbtl)

### На русском / In Russian
UKNCBTL — UKNC Back to Life! — это эмулятор компьютера Электроника МС-0511 (УКНЦ).
УКНЦ это советский домашний/учебный компьютер, построенный на двух процессорах КМ1801ВМ2, совместимых по системе команд с Электроника-60, ДВК и др.

Текущее состояние эмулятора: в разработке. Большая часть программного обеспечения запускается и работает.

Эмулируются:
 * Центральный и периферийный процессоры
 * Оба контроллера памяти
 * Видеоконтроллер
 * Контроллер гибких дисков (стандарт MZ)
 * Картриджи ПЗУ (24 КБ на картридж)
 * Звук
 * Контроллер жёсткого диска IDE
 * Работа с кассетным магнитофоном — эмулируется через работу с WAV PCM файлами
 * Последовательный порт (Стык С2) — экспериментально
 * Параллельный порт — через запись вывода в файл, для просмотра результата можно использовать утилит ESCParser

Пока что НЕ эмулируются: сетевая карта, RAM-диски, джойстики.

Огромное спасибо всем тем, кто внёс свой вклад в создание и развитие эмулятора, особенно:
 * Феликс Лазарев — проделал огромную работу на начальном этапе развития эмулятора
 * Алексей Кислый (Alex_K) — оказал неоценимую помощь во всех вопросах, касающихся деталей работы УКНЦ

### In English
*UKNCBTL* is cross-platform UKNC emulator for Windows/Linux/Mac OS X.
*UKNC* (*УКНЦ*, Электроника МС-0511) is soviet school computer based on two PDP-11 compatible processors KM1801VM2.

Current status: under development. Most of software works fine.

Emulated:
 * CPU and PPU
 * Both memory controllers
 * Video controller
 * FDD controller (MZ standard, 400K/800K .dsk/.trd files)
 * ROM cartridges (24K .bin files)
 * Sound
 * Hard disk (IDE, .img files) — can read/write and boot
 * Tape cassette — read/write WAV PCM files
 * Serial port (experimental)
 * Parallel port — just dump output to file, use ESCParser to visualize the result

NOT emulated yet: network card, RAM disks, joysticks.

-----
The UKNCBTL project consists of:
* [ukncbtl](https://github.com/nzeemin/ukncbtl/) written for Win32 and works under Windows 2000/2003/2008/XP/Vista/7/8/10.
* [ukncbtl-renders](https://github.com/nzeemin/ukncbtl-renders/) — renderers for UKNCBTL Win32.
* [ukncbtl-qt](https://github.com/nzeemin/ukncbtl-qt/) is based on Qt framework and works under Windows, Linux and Mac OS X.
* [ukncbtl-testbench](https://github.com/nzeemin/ukncbtl-testbench/) — test bench for regression and performance testing.
* [ukncbtl-utils](https://github.com/nzeemin/ukncbtl-utils/) — various utilities: rt11dsk, sav2wav, ESCParser etc.
* [ukncbtl-doc](https://github.com/nzeemin/ukncbtl-doc/) — documentation and screenshots.
* Project wiki: https://github.com/nzeemin/ukncbtl-doc/wiki
  * Screenshots: https://github.com/nzeemin/ukncbtl-doc/wiki/Screenshots-ru
  * User's Manual (in Russian): https://github.com/nzeemin/ukncbtl-doc/wiki/Users-Manual-ru
