# Unit-test implementation areas

Work through one area at a time, validate it, and ask the user before starting
the next area. This tracks the repository audit, not measured code coverage.

| Area | Scope | Status |
| --- | --- | --- |
| 1. Breakpoints | Shared CPU/PPU storage, limits, compaction, temporary ownership, sentinel readers | Complete: 14 GoogleTest cases |
| 2. Instruction history | Ring wraparound, ordering, clearing, CPU/PPU separation, interrupt records/names | Complete: 14 GoogleTest cases |
| 3. Snapshots and serialization | File validation, state preservation, uptime/history restoration, Reader/Writer boundaries, little endian helpers | Complete: 55 GoogleTest cases |
| 4. Settings | Decimal/color/binary codecs, invalid input, defaults, isolated persistence | Complete: 81 GoogleTest cases |
| 5. Commands | Console parsing/history/step-over and command-line options | Complete: 98 GoogleTest cases |
| 6. Video decoding | Display tags, palettes, scaling, cursors, text preparation and display-list interpretation | Complete: 34 GoogleTest cases |
| 7. Common utilities | Number formatting, octal parsing, KOI8-R, filenames, named smoke cases | Complete: 42 utility cases; 10 named core smoke cases |
| 8. Input | Keyboard queue, mappings, press/release, keyboard hit testing and mouse calculations | Complete: 23 GoogleTest cases |
| 9. Text and subtitles | Subtitle parser/lookup, glyph recognition, screen-to-text layout and buffer contract | Complete: 26 GoogleTest cases |
| 10. Sprites and view calculations | Sprite decoding, address wrapping, memory/debug hit testing, history scrolling | Complete: 18 GoogleTest cases |
| 11. Tape and WAV | Tape state and batched callbacks; malformed WAVs, mode/lifecycle and sample formats | Complete: 45 GoogleTest cases |
| 12. Audio output | Playback rates, stereo packing, buffers and injectable backend failures | Complete: 16 GoogleTest cases |
| 13. Disk-image creation | Sizes, initialization blocks, offsets, zero fill and errors | Complete: 15 GoogleTest cases |
| 14. Processor | HALT instructions, pins, interrupt priority, bus faults, callbacks, flags/addressing combinations | Complete: 60 GoogleTest cases |
| 15. Memory and RAM changes | Port widths/side effects, maps, network/mouse/tape edges, application RAM comparison | Complete: 26 GoogleTest cases |
| 16. Board | Callback scheduling, sound, keyboard scan, tape writes, frame execution with breakpoints | Complete: 24 GoogleTest cases |
| 17. Storage controllers | Floppy transfer/reset/flush failures; HDD command/count/CHS/reset/status sequences | Complete: 29 GoogleTest cases |
| 18. AY sound | Chunk continuity, span sizes, masking, envelope restart and mid-render changes | Pending |
| 19. Disassembly | Truncated spans, wrapping and semantic assertions alongside corpus hashes | Pending |
| 20. Images and scaling | Bitmap failure/lifecycle/resource cases, complete scaling output and rounding, renderer duplicate implementation | Pending |
| 21. Test infrastructure | Legacy assertion adapter cases; split remaining monolithic RegressionChecks cases | Pending |

OS window creation, menus, clipboard, dynamic renderer loading, physical serial
ports and real audio output need integration tests. Extract deterministic
decisions where useful instead of unit-testing Windows itself.

## Area 1 implementation

`BreakpointSet` owns the compact list for one processor and reserves space for
15 permanent addresses, one temporary address and a terminating sentinel.
`Emulator.cpp` uses separate instances for CPU and PPU, including the spans
passed to the motherboard and the lists read by debugger views.

Tests cover capacity in both insertion orders, repeated failed additions,
first/middle/last removal, address boundaries, duplicates, temporary replacement,
collision with permanent entries, cancellation, clearing and stable storage.
Regression cases prevent stale temporary ownership from removing a subsequently
added permanent entry and prevent adding past capacity while a temporary entry
is active. Loading settings now uses checked insertion; shutdown cancels
temporary targets before persisting the remaining list.

Validation: Release application/test build and 311 passing Release CTest cases
(297 existing plus 14 new). No interactive debugger UI test was performed.

## Area 2 implementation

`InstructionHistory` owns both 256-record rings and constructs complete
instruction and interrupt records. The emulator callbacks delegate recording
to this component before trace filtering; the existing debugger API delegates
length, retrieval, clearing and interrupt names to the same implementation.

Fourteen tests cover field preservation and source copying, newest-first order,
exact capacity, first overflow, repeated wraps, CPU/PPU independence, mixed
records, complete replacement of reused slots, clearing/reuse, all named vectors
with every SEL high byte, and unknown/odd vectors falling back to VIRQ.

Validation: Release application and new test target build successfully; all
325 Release CTest cases pass. Tests exercise the extracted history component;
interactive UI rendering and processor callback timing are not tested here.

## Area 3 implementation

`Snapshot.cpp` contains the actual snapshot save/load path used by the emulator.
The application supplies its stop callback; file/header validation finishes
before that callback or any state restoration. Valid loads restore the board
and uptime and clear both instruction histories. Null paths now fail cleanly.
Existing acceptance of trailing bytes and ignored reserved header bytes remains
unchanged.

Twenty-one snapshot cases use temporary files and the real motherboard to test
exact header/file layout, valid state restoration, callback ordering, byte-for-byte
round trips, missing/null paths, locked-file read/write failures, reserved/trailing
bytes, nine truncation lengths and each of the four validated header fields.
Rejected files preserve serialized board state, uptime and both histories.

Thirty-four serialization cases cover 8/16/32/64-bit little-endian reads/writes,
unaligned addresses, guard bytes, zero/max/single-bit round trips, exact and short
buffers for every Reader/Writer operation, unchanged cursors/buffers after
exceptions, mixed fields, array copying and zero/oversized skips.

Validation: Release application and both new test targets build; all 380 Release
CTest cases pass. Tests do not simulate disk-full or close-time failures, test
interactive UI effects, or exercise execution on a big-endian host.

## Area 4 implementation

`SettingsCodec.h` provides checked decimal/color parsing and binary hex
encoding/decoding. Numeric parsing consumes the whole value and rejects signs,
trailing junk and overflow. Colors use RGB text and COLORREF byte order; binary
hex accepts either case. Binary decoding validates the complete exact-length
input before modifying any output bytes.

`Settings_Init` accepts an optional INI path and invalidates scalar, color and
DCB caches when reinitialized. Default application calls still use the INI next
to the executable. String reads distinguish exact fits from truncation; callers
receive failure rather than a valid-looking truncated prefix. Empty INI values
retain the existing missing-value behavior.

Eighty-one tests cover codecs, all 30 scalar setting caches (defaults, malformed
input, setter persistence, external edits, reinitialization and file switching),
all color entries, window rectangles, Unicode device paths, port names,
breakpoint slots/deletion, serial/network defaults and cache persistence, missing
values, invalid buffers and unwritable paths. Persistence tests compile the real
Settings.cpp and use only isolated temporary Unicode INI files.

Validation: Release application and SettingsTests build; all 461 Release CTest
cases pass. Tests do not exercise interactive settings dialogs or simulate
mid-write storage failures. Scalar setters retain their existing behavior of
updating the in-memory cache even when persistence fails.

## Area 5 implementation

`ConsoleCommands.h` holds the command patterns and checked full-input parsing.
ConsoleView dispatches the parsed action to its existing handlers. Register and
octal operands are validated without wrapping; malformed assignments and trailing
junk cannot fall back to executing a shorter command prefix. The view reads the
complete edit-control text rather than executing only its first 31 characters.
A separate PRODUCT test target verifies that trace commands are excluded.

`CommandHistory` implements the existing 32-entry, unique-command history and
navigation. Tests cover empty history, newest-first navigation, bounds, duplicate
promotion, capacity, copied input, and navigation reset when editing. Down while
editing leaves the current line alone. Step-over target tests cover all JMP/BR
variants, ordinary instruction lengths, address wraparound and invalid lengths.

`CommandLineOptions` parses tokenized arguments into owned values before Main
applies them to settings. Tests cover every boolean alias, help and boot forms,
last-valid-option precedence, all disk/cartridge/HDD slots, malformed options,
owned paths and Windows tokenization of quoted Unicode paths. `/boot` and
`/boot1` through `/boot7` remain supported; malformed suffixes are ignored.

Validation: Release application and both command test targets build; all 559
Release CTest cases pass (98 new). Tests cover parsing and decision logic, not
interactive command-handler UI effects or live processor step-over execution.

## Area 6 implementation

`VideoDecoder` provides a shared display-list cursor and headless RGB/text
rendering. Both renderers, DisplayListView and the console display-list dump now
use the same two-/four-word tag interpretation. Emulator wrappers retain their
initialization guard. Rendering retains the existing initial black palette,
19 hidden lines, 288 visible lines, palette/parameter persistence, cursor timing
and text-mode behavior (no horizontal expansion; unused row tails untouched).

Thirty-four tests use synthetic motherboard RAM to verify tag fields and link
flags, links at the end of RAM, all four scales over entire frames, all eight
brightness banks and palette nibbles, symbolic/graphical cursor positioning,
blanking-line state changes, cursor toggles, palette/scale transitions, pixel
address wraparound across all planes, bounded cyclic lists, exact/undersized
spans, output guard regions and unchanged motherboard state.

Validation: Release application and VideoDecoderTests build; all 593 Release
CTest cases pass. Tests validate decoded pixels and tags, not interactive drawing
or the display-list views' text formatting. Renderer-DLL scaling remains Area 20.

## Area 7 implementation

The existing number-formatting, UKNC KOI8-R translation and filename functions
now live in `util/CommonText.cpp`, compiled into both the application and its
unit-test target. Their behavior is unchanged, including lowercase hex output,
fixed-width zero padding, the machine-specific graphics mapping, and filename
extraction using backslash only (returning a pointer into the original input).

Forty-two utility cases cover fixed examples and buffer guards, all 65,536 words
against standard octal/hex/binary formatting and octal round trips, exact parse
errors, long leading-zero inputs, non-null-terminated views, embedded nulls,
ASCII/control preservation, both Cyrillic alphabets, representative UKNC graphics
symbols and filename/path boundaries including Unicode and UNC paths.

The former combined CoreSmokeTests.RegressionChecks is now ten named cases with
direct assertions and fixture cleanup, retaining disassembly, jump, RAM,
unaligned snapshot, device-list, stepping and disk regressions. Its four octal
checks are covered by the dedicated utility tests; its short serialization
checks are covered by Area 3's parameterized tests.

Validation: Release application, CommonTextTests and CoreSmokeTests build;
all 644 Release CTest cases pass (42 new utility cases plus nine net cases from
the smoke-test split). Win32 text drawing and clipboard ownership remain outside
these pure utility tests; the supported Unicode build is tested.

## Area 8 implementation

`Input.cpp` contains the shared keyboard queue, host-key mappings, key transitions,
Enter/numpad normalization, keyboard rectangle hit testing and mouse calculations.
ScreenView and KeyboardView delegate to these functions. Keyboard releases keep
using the layout saved on key-down, and the 32-event queue retains its existing
policy of dropping the newest event when full. Zero events are rejected because
zero denotes an empty queue.

Twenty-three cases cover queue capacity/order/repeated wraps, independent queues,
alphabet/digit/keypad/control mappings, every key's press/repeat/release across
layout changes, toggle bits, Enter combinations, hit-test edges/offsets/overlaps,
invalid rectangles, mouse scaling/truncation/acceleration/clamping, stationary
cursors, invalid dimensions and all mouse button state words with/without focus.

Mouse deltas now use wide arithmetic before clamping, preventing large movements
from wrapping. Zero-sized displays produce zero movement. Mouse buttons use the
current down bit rather than treating the pressed-since-last-query bit as held.

Validation: Release application and InputTests build; all 667 Release CTest cases
pass (23 new). OS keyboard polling, focus transitions, window messages and drawing
remain integration-test concerns; existing Enter normalization is preserved.
## Area 9 implementation

`Subtitles.cpp` provides bounded parsing and lookup, with owned Unicode comment
strings. The disassembly view uses this implementation. Lookup no longer walks
past the vector looking for a nonexistent sentinel, and supports unsorted input.
Duplicate addresses retain first-matching-item behavior for each type mask.
Parsing preserves optional LE BOMs, null termination, column-zero addresses and
block comments, last-block-line selection, and the existing delimiter grammar.
Final directives and block comments attached to final addresses now work without
a trailing newline. Invalid final addresses are checked rather than skipped.

Seventeen subtitle cases cover empty/BE input, optional BOMs, CR/LF/CRLF, EOF,
directives and mixed comment/data flags, block association/cancellation, address
limits/overflow/invalid digits, bounded views, embedded nulls, Unicode ownership,
copying, empty/missing/past-end lookup, duplicates, unsorted input and type masks.

`ScreenText.cpp` shares glyph recognition and screen-to-text layout with the view.
The screen-to-text API now takes a span, rejects undersized output before reading
the screen, and writes the promised trailing zero. Nine cases cover all 224
characters from each font, tie order, pixel bit order/stride, brightness threshold
and alpha, dark-pixel penalties, short inputs with unchanged output, all 26 rows,
indicator gaps, CRLF, terminator and output guards.

Validation: Release application, SubtitleTests and ScreenTextTests build; all
693 Release CTest cases pass (26 new). File dialogs, clipboard ownership and live
PPU font-memory reads remain integration concerns. Recognition scoring is
unchanged; the tests use synthetic fonts and frames rather than visual OCR quality.
## Area 10 implementation

`SpriteDecoder.h` contains the sprite renderer used by SpriteView, with an injected
aligned CPU-word reader. Eight cases cover every output pixel for all five modes
and all widths 1 through 32, bottom-up DIB layout, plane palettes and bit order,
column separators and unused pixels, odd/even starting addresses, 16-bit address
wrapping, memory-read order, output guards and rejected dimensions/modes/spans.
Page size now counts the same columns as rendering, including a final column
that fits exactly (four width-8 columns fill the 262-pixel bitmap).

`ViewCalculations` shares memory hit testing, debug row selection and history
scroll calculations with their views. Ten cases cover all memory columns,
multiple rows and wrapped addresses, header/margin clamping, bottom boundaries,
missing metrics, debug register/saved-state/blank rows, history row counts,
empty/full pages, line/page/top/bottom commands, reversed thumb positions,
32-bit track positions, wheel direction, partial notches and endpoint clamping.
Uninitialized font metrics no longer cause division by zero; the debug top
margin no longer selects register zero through integer truncation.

Validation: Release application, SpriteTests and ViewCalculationTests build;
all 711 Release CTest cases pass (18 new). Tests exercise shared calculations and
synthetic sprite memory. Window painting, font measurement, scrollbar messaging
and the live CPU memory-controller mapping remain integration concerns.
## Area 11 implementation

`TapeTransport.h` shares insertion/play/stop/eject state, batched sample processing,
graph updates and position calculations with TapeView. Thirteen cases cover both
transport modes, repeat transitions, zero-sized and oversized batches, exact EOF,
partial EOF, read/write errors, threshold/last-sample semantics, graph continuity,
empty graph spans, time conversion and position-update thresholds/rewind.
The graph now scrolls by the number of successfully processed samples rather than
the requested count, preserving recent history on early EOF or I/O failure.
Injected stream failures exercise the same batch functions used by WAV callbacks.

The former combined WAV RegressionChecks case is replaced by 32 named cases.
They cover 8/16/32-bit PCM signed boundaries and little endian data, mono/stereo/
four-channel input (first channel used), high rates, seeking/rewinding, every
truncated fixture prefix, malformed fields/chunks, missing/duplicate/out-of-order
chunks, unknown odd chunks, extended formats, trailing bytes, empty and legacy
unpadded data, invalid arguments, open failure, recording overwrite/append,
move ownership, destructor finalization, wrong-mode/closed operations, idempotent
close, physical file sizes and odd-byte padding. Files use isolated temporary paths.

Validation: Release application, TapeTests and WavPcmFileTests build; all 755
Release CTest cases pass (45 area cases replacing one prior combined case, net 44).
No WAV codec behavior was changed. OS dialogs, motherboard callback scheduling,
physical tape signals, disk-full and close-time I/O failures remain outside these
tests; callback scheduling is tracked in Area 16.
## Area 12 implementation

`SoundBackend.h` provides a replaceable WinMM function table; normal application
calls still use WinMM. Backend selection is rejected while the device is open
or when any required function is missing. Tests compile the real SoundGen.cpp
and replace device operations and waits with deterministic callbacks, without
opening an audio device or sleeping.

Sixteen cases cover inactive operations, PCM format and initial volume,
idempotent initialization/finalization, failed-open retry, channel volume packing,
all 65,536 speed values, nonfatal control failures, stereo little-endian samples,
exact block boundaries, repeated eight-buffer wraps, deferred completion and
queue backpressure, draining on finalize, prepare/write failure recovery,
unprepare failures, close retry and partial-buffer reset after reinitialization.
Open/close notifications do not release queued buffers; successful completion
callbacks do. Failed writes restore the free-buffer count.

A failed unprepare now drops the pending batch without overwriting device-owned
storage. Finalization retains device state when unprepare or close fails, allowing
a later finalize call to retry rather than forgetting a live handle. Cleanup
iterates the fixed buffer array instead of using the callback counter as a bound.

Validation: Release application and SoundOutputTests build; all 771 Release CTest
cases pass (16 new). Audible output, actual device capabilities and concurrent
WinMM callback timing remain integration concerns. Existing wait-until-completion
behavior is retained; a driver that never returns buffers can still block shutdown.
## Area 13 implementation

`DiskImage.cpp` builds and writes the disk images used by the creation dialog.
Only the supported 40-/80-track geometries are accepted, producing 409,600 and
819,200 bytes. The existing initialization blocks remain at offsets 0, 0x03D2
and 0x0C00. The full image is explicitly zero-initialized and written, avoiding
reliance on extended-file contents. Invalid arguments are rejected before opening
or truncating the destination. Extra output-span bytes are preserved.

Fifteen cases verify complete images against the original initialization bytes,
zero-filled regions, exact sizes, buffer guards, invalid tracks and short spans,
real file output, overwrite/truncation, Unicode paths, invalid-input preservation,
locked files and missing parents. An injected file backend covers open/write/close
failures, successful short writes, handle cleanup, original write-error retention
when close also fails, and incomplete backend rejection.

Validation: Release application and DiskImageTests build; all 786 Release CTest
cases pass (15 new). Interactive save dialogs and physical disk-full conditions
were not exercised; write failures are injected. Existing replacement semantics
are retained, so a failed write may leave a partial destination file.
## Area 14 implementation

`ProcessorEdgeTests` exercises the real processor and memory-access code with a
controlled memory controller. Separate HALT/USER RAM planes expose mode mistakes;
selected denied addresses trigger real bus-error requests. No production processor
code was changed. Expected instruction results and state transitions are asserted
directly rather than compared to captured emulator output.

Sixty cases cover all 24 privileged opcode aliases rejecting USER execution,
HALT copy-register access, RUN/STEP, SEL, MFUS/MTUS, normalization, HALT/FIS vectors,
DCLO/ACLO/HALT pin transitions, interrupt masks and priority, VIRQ queue selection,
fetch/source/destination/stack/vector faults, double faults and recovery, callback
arguments and timing, callback removal and CPU/PPU installation, WAIT and internal
execution ticks. Register byte writes, saved PC/PSW behavior and condition setters
are checked, including all 65,536 input values to SetPSW.

The MOV/MOVB matrix covers all 64 source/destination addressing-mode pairs, six
boundary operands and all 16 incoming condition-code combinations (12,288 checks).
It verifies data, byte sign extension/preservation, flags, PC extension words and
register increments/decrements, complementing the existing arithmetic suites.

Validation: Release application and ProcessorEdgeTests build; all 846 Release
CTest cases pass (60 new). The fixture deliberately controls memory translation;
physical CPU/PPU device mapping and board callback scheduling remain in Areas 15
and 16. These regression tests do not claim exhaustive instruction-set or hardware
cycle-timing conformance.
## Area 15 implementation

`RamChanges.h` shares the application's RAM comparison and change-word readers.
Six cases cover every byte including 0xFFFF, XOR bit flags, baseline replacement,
unchanged/reverted data, short spans, guards, independent planes, every aligned/
odd word address and the CPU's interleaved planes through the end of HALT memory.
The application previously skipped the final byte of each plane, used unchecked
unaligned word loads and suppressed interleaved change flags above octal 170000.
It now compares all bytes, aligns word reads within the plane, and covers all
interleaved RAM addresses selected by the memory controller.

Twenty memory-controller cases add full CPU mapping checks, PPU window priority
and boundaries across all 256 low-byte control values, execution-denied I/O,
CPU address-watch mode matching and debugger suppression, plane-register byte
lanes, serial/network overflow and acknowledgements, transmitter masks, keyboard
read side effects, mouse signed chunking/clamping/consumption, tape enable/edge/
output behavior and background-pixel plane packing.

A regression demonstrated mouse accumulation wrapping before saturation. Mouse
arithmetic now clamps in an int before narrowing to int16_t; both positive and
negative extremes are tested. Normal movement and port behavior remain unchanged.

Validation: Release targets rebuilt and all 872 Release CTest cases pass (26 new).
Tests exercise real board/controller code and synthetic memory/port inputs;
physical serial/network devices and application debugger painting are not tested.
Board callback scheduling remains Area 16.

## Area 16 implementation

`BoardFrameTests` exercises complete frames on the real motherboard with captured
host callbacks. Twenty-four cases cover sound cadence/removal, stereo and Covox
mixing, tape output levels, fractional sample carry and batching through INT_MAX
rates, callback replacement/self-removal, invalid rates and tape timer events.
Serial/network cases verify polling cadence, delivery, receive/transmit interrupts,
ready flags and loopback. Parallel strobe/acknowledgement, Covox exclusion and
terminal channel selection are covered separately.

Keyboard tests check acknowledgement stalls, held rows, release reporting and
interrupts. CPU/PPU breakpoints verify the stopping order; a nonmatching-breakpoint
frame must produce the same saved machine state as the fast execution path.

A constant sound signal previously counted every sample as a new rising edge.
The mixer now updates its previous signal value; the regression checks sustained
high/low signals and transitions across frames.

Validation: full Release build and all 896 Release CTest cases pass (24 new).
Callbacks use synthetic host data; physical devices, audible output and hardware
cycle-timing conformance are not tested. Storage-controller sequences remain
Area 17.

## Area 17 implementation

`StorageControllerTests` adds 29 cases using generated temporary images and the
real floppy/HDD controllers. HDD tests cover command aliases, transfer delays,
zero-count 256-sector reads and writes, data boundaries, CHS rollover and invalid
locations, end-of-image errors and recovery, reset during partial/scheduled writes,
IDENTIFY completion/replacement, detach/re-attach and commands without an image.
Complete image comparisons check both written sectors and untouched regions.

Floppy cases cover write-register queuing/overwrite, read cancellation, marker
propagation, reset flushing and malformed-track recovery. A valid read-only FILE
injects write failures into reset, side/track/drive changes, detach and attach.
Each operation must retain the selected dirty track and original file; reopening
the stream writable verifies that retry saves the retained data.

Regressions exposed HDD transfer timers surviving detach and replacement commands,
and IDENTIFY scheduling an unintended disk read after its response. Detach now
cancels transfers, WRITE/IDENTIFY cancel superseded timers, IDENTIFY has no trailing
sector transfer, and disk I/O checks for an attached file before accessing it.
No floppy production code changes were needed.

Validation: full Release build and all 925 Release CTest cases pass (29 new).
Failure injection exercises stream write rejection, not physical disk-full or
device faults. Existing CRC stubs and unsupported ATA commands are unchanged;
these tests do not establish hardware protocol conformance.
