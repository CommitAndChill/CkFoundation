# CkPixel authoring tools

CkPixel provides bounded Python XPM3/manifest parsing and deterministic CLI inspection, diff, RGBA PNG, and horizontal sheet export. XPM and JSON files are the canonical source. The native CkPixelSprite parser, indexed assets, importer/reimporter and CPU rendering have passed focused editor gates. GPU resources, runtime playback, native exports and the Slate viewer have passed focused integration gates. A Development cooked client loaded the indexed Character asset, verified authored timing/anchors and advanced the viewport preview; its rendered screenshot was inspected. See Plans/PixelSprites/ACCEPTANCE.md and FULL_GATE.md in the development host for executed evidence and unresolved project-wide test/loader diagnostics. No CLI playback preview exists. Use Python 3.12 or newer, including on Windows so export safety can detect junction paths.

Native integration: [Unreal import, runtime playback and export](UNREAL.md).

## Codex authoring specification

- Choose one canvas for the entire character. Keep head, torso, limb proportions, ground contact, and the named origin anchor consistent. Move limbs within that canvas; do not translate the whole character arbitrarily between frames. Give the same semantic colors the same RGBA values in every frame.
- Draw clear key poses first, then purposeful inbetweens. Idle uses restrained breathing/blinking and longer holds. Walk alternates contact and passing poses on a stable ground line. Attack distinguishes windup, short impact, and longer recovery through silhouette and duration_ms.
- Inspect each frame at native size and nearest-neighbor scale. Diff intended neighbors and review unexpected single-pixel changes, including transparency changes. A palette symbol change alone is harmless when resolved RGBA stays equal.
- Keep frame order explicit in the manifest. Reproduce images and sheets from the same source and scale with the CLI; export does not synthesize inbetweens, move anchors, or resample timing.

Run from this directory:

    python ckpixel.py validate Examples/Robot.xpm
    python ckpixel.py inspect Examples/Robot.xpm
    python ckpixel.py grid Examples/Robot.xpm
    python ckpixel.py render Examples/Robot.xpm robot.png --scale 4
    python ckpixel.py diff Examples/Robot.xpm Examples/Character/idle_1.xpm
    python ckpixel.py validate Examples/Character/character.json
    python ckpixel.py render Examples/Character/character.json idle.png --animation idle --frame 0 --scale 4
    python ckpixel.py sheet Examples/Character/character.json walk.png --animation walk --scale 4

Commands print one JSON object on stdout on success. Failure diagnostics are JSON on stderr with nonzero exit. Render and sheet use nearest-neighbor integral scale 1–32 and cap output at 16,777,216 pixels. Diff compares resolved RGBA colors, returning changed pixel count and an exclusive [left, top, right, bottom] box, or null when identical. Sheets place frames left to right in declared order and write a same-stem JSON sidecar with source names, durations, atlas rectangles, source rectangles, and unscaled source-coordinate anchors.

Exports reject destinations that resolve to the manifest or any referenced XPM, as well as symlink/junction paths and existing hard-linked files. A source XPM named .png is still protected. Sheet export checks both destinations, stages both files beside their targets, then publishes them; if publication fails, it attempts to restore both previous outputs. This covers ordinary command failures, not process crashes or power loss.

## Supported XPM3 subset

An XPM is one char * name[] = { ... }; array of quoted printable-ASCII strings; static and const qualifiers are accepted. The header has four decimal values: width height color_count chars_per_pixel, optionally followed by hotspot_x hotspot_y. Width and height are each 1–4096, their product at most 1,048,576, colors 1–4096, and characters per pixel 1–4. A file is at most 8 MiB. The optional hotspot must be inside the canvas. Exactly color_count palette strings and height rows follow; every row is exactly width × chars_per_pixel characters and uses declared, case-sensitive symbols.

Palette rows need a c color selector; m, g, g4, and s may also appear. None is transparent. Opaque colors support #RGB, #RRGGBB, #RRRGGGBBB, #RRRRGGGGBBBB; case-insensitive black, white, red, green, blue, yellow, cyan, magenta, gray, grey; and gray0–gray100 / grey0–grey100. String escapes support backslash, double quote, single quote, question mark, one-to-three digit octal, and hexadecimal byte escapes. Decoded values must remain printable ASCII. Adjacent C string literals, extra arrays, and XPM extensions are unsupported.

## Manifest and timing

A version 1 manifest is JSON with required root fields version: 1, canvas: {width, height}, and a nonempty animations object. Root events (array) and metadata (object) are optional. Each named animation requires playback (loop, once, or ping-pong) and a nonempty ordered frames array; animation events and metadata are optional. Each frame requires source and duration_ms; anchors, events, and metadata are optional. Anchors map names to integer [x, y] source coordinates within the canvas. Every referenced XPM must match the shared canvas. Unknown fields are rejected; layers specifically reports unsupported composition.

Bounds: canvas dimensions 1–4096 with at most 1,048,576 pixels; 1–1024 animations with names 1–128 characters; at most 4096 frames and 16,777,216 aggregate frame pixels; finite duration_ms greater than zero and at most 86,400,000; at most 256 anchors per frame with names 1–128 characters. Manifest and individual XPM sources are each at most 8 MiB; distinct referenced XPM files total at most 64 MiB. Frame source is at most 1024 characters, a forward-slash relative .xpm path within the manifest root. Absolute paths, backslashes, colons, empty/dot/dot-dot segments, and paths resolving outside that root are rejected.

The Python manifest parser permits a source link that resolves inside the manifest root. Native loading requires ordinary existing files and directories and rejects symlinks or junctions in every manifest/source path component, even links whose targets stay inside the root. It assumes a stable local authoring tree during loading and does not establish safety against concurrent filesystem replacement. Native source caching uses exact normalized path spelling: repeated identical references share one decoded grid, while spellings that differ only by case are distinct entries and each counts toward the 64 MiB aggregate source-byte limit.

Native duration_ms must also produce a positive, representable double-precision value after conversion to seconds. An extremely small positive JSON duration that the Python parser accepts can therefore receive a native `duration` diagnostic. Native schema names and fields reject decoded NUL in structural keys, animation or anchor names, source paths, and playback values. Escaped NUL remains permitted inside metadata/events extras; raw NUL bytes are invalid JSON.

The parser's evaluate_frame timing helper uses authored durations with half-open frame intervals. Loop wraps, once holds the final frame, and ping-pong reverses without repeating either endpoint. Optional sample_ms floors elapsed time to its sampling interval before selection. Optional positive integer source decimation selects the preceding retained source frame (active_index - active_index % decimation) while preserving the original timeline and durations. For consumer playback speed 2×, pass twice real elapsed time to the helper; 0× holds the selected time. The CLI exports authored frames and timings only; it has no playback speed, sampling, or decimation flags.

The sample Examples/Character/character.json has 16×16 robot frames, a common origin at [8,15], and variable timing for idle (3 frames), walk (4), and attack (3). Fixtures/Manifest/conformance.json indexes 6 valid and 32 invalid manifests for Python/native conformance, with small local XPM sources. The corpus covers timing, ordering, metadata, source reuse, escaped NUL values and keys, negative-zero anchors, reserved paths, and structured rejection. Run python -B -m unittest discover -s tests -v for parser and CLI checks, including decoded PNG pixels and source immutability. The independent Python suite passed 28 tests in 3.131 seconds. NativeManifestGate4 built and passed all 15 focused native tests, including manifest conformance and authored decimal timing. Native ordinary-file and junction fixtures passed; symbolic-link fixtures were skipped for missing Windows privileges. See the host Plans/PixelSprites/PROGRESS.md for current Unreal asset, viewer and runtime acceptance evidence.
