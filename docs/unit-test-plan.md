# Unit-test implementation areas

Work through one area at a time, validate it, and ask the user before starting
the next area. This tracks the repository audit, not measured code coverage.

| Area | Scope | Status |
| --- | --- | --- |
| 1. Breakpoints | Shared CPU/PPU storage, limits, compaction, temporary ownership, sentinel readers | Complete: 14 GoogleTest cases |
| 2. Instruction history | Ring wraparound, ordering, clearing, CPU/PPU separation, interrupt records/names | Complete: 14 GoogleTest cases |
| 3. Snapshots and serialization | File validation, state preservation, uptime/history restoration, Reader/Writer boundaries, little endian helpers | Complete: 55 GoogleTest cases |
| 4. Settings | Decimal/color/binary codecs, invalid input, defaults, isolated persistence | Complete: 81 GoogleTest cases |
| 5. Commands | Console parsing/history/step-over and command-line options | Pending |
| 6. Video decoding | Display tags, palettes, scaling, cursors, text preparation and display-list interpretation | Pending |
| 7. Common utilities | Number formatting, octal parsing, KOI8-R, filenames, named smoke cases | Pending |
| 8. Input | Keyboard queue, mappings, press/release, keyboard hit testing and mouse calculations | Pending |
| 9. Text and subtitles | Subtitle parser/lookup, glyph recognition, screen-to-text layout and buffer contract | Pending |
| 10. Sprites and view calculations | Sprite decoding, address wrapping, memory/debug hit testing, history scrolling | Pending |
| 11. Tape and WAV | Tape state and batched callbacks; malformed WAVs, mode/lifecycle and sample formats | Pending |
| 12. Audio output | Playback rates, stereo packing, buffers and injectable backend failures | Pending |
| 13. Disk-image creation | Sizes, initialization blocks, offsets, zero fill and errors | Pending |
| 14. Processor | HALT instructions, pins, interrupt priority, bus faults, callbacks, flags/addressing combinations | Pending |
| 15. Memory and RAM changes | Port widths/side effects, maps, network/mouse/tape edges, application RAM comparison | Pending |
| 16. Board | Callback scheduling, sound, keyboard scan, tape writes, frame execution with breakpoints | Pending |
| 17. Storage controllers | Floppy transfer/reset/flush failures; HDD command/count/CHS/reset/status sequences | Pending |
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
