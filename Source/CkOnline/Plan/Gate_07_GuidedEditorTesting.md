# Guided editor testing

Written 2026-10-04. Evidence/status live in ../PROGRESS.md. Retire this test-tooling contract when the online gyms are replaced by a consuming game's integration gate.

The user reports the prior Lobby/gameplay round trip works, but wants fewer manual steps and a readable information panel. This gate improves CkTests only; no online wire, authentication, engine, CkAuto, or two-role automation change is included.

1. Extend the existing shared row/Slate face with explicit semantic tones, optional compact status/wrapping, and details-only rows. Preserve neutral defaults, row order, input indices, warning/error visibility and disabled input semantics. Online HUDs start guided; F2 toggles details through the existing input route. D remains movement.
   -> Verify the existing native CompactRows test for old behavior, opt-in filtering, critical details, defaults/equality; extend the existing AS numbered-shortcut test to exercise the reflected row fields/builders and preserved dispatch.
2. Reuse the native gym helper for one-key clipboard Join and Enter-to-send chat. Reject invalid clipboard/foreign ownership before draft/admission mutation. Escape/window close must cancel, and rejected send must retain draft/error. Keep explicit save-only/manual paths.
   -> Verify incremental compilation and compatible Direct/socket/AS smoke coverage. [EDITOR-VERIFY] Host J/P, Client K join; B/Enter delivers once; B/Escape delivers nothing and restores normal controls.
3. Replace the default diagnostic wall with a live next-step hint, connection, readable roster/readiness/chat and gameplay proof cues. Full IDs/invites/counters remain under F2, errors remain visible, and local green state is not an automatic whole-test PASS. Every network guide repeats setup and joining.
   -> [EDITOR-VERIFY] Compare guided and full views, see semantic colors/text, then ready both, Host G, Client G, client P, client X and host X; return to guided Lobby with the same control state. Instructions: CkTests/Script/CkOnline/ONLINE_PANEL_EDITOR_TESTING.md.

The known-red same-process Iris automation retains its verdict. Separate-process human acceptance, renderer appearance/focus, and Blueprint UI interaction remain explicit observations. All work stays in the selected checkout and retains the two temporary research commits and unrelated dirt.

Verification, 2026-10-04: incremental build and seven selected Direct tests passed in OnlineGuidedPanelGate-01; native CompactRows and AS NumberedShiftShortcuts passed in OnlineGuidedPanelControls-01, reusing the build. Both exited 0, with zero selected failures/skips/contamination and no ensures/script errors. Three fresh boots covered discovery and the two families. The Direct startup recorded one asset-registry cache write error while discovery wrote that cache concurrently; its exact OS reason is unknown. Panel-control startup has no Error records. Renderer-only coverage and the new three-check editor UI observation remain pending; the same-process Iris fixture was excluded and remains red.
