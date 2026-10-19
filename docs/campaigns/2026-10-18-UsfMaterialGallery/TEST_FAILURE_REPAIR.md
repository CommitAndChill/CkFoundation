# CkUsf test failure repair

2026-10-18, America/Los_Angeles. Baseline: the real-renderer CkUsf publication
run passed 51/57, with six failures and zero contamination. Source evidence is in
`Saved/Logs/Test-UsfGallery-Publish.log` in the host checkout.

Final result: 57/57 CkUsf tests pass in one fresh real-renderer editor, with
known-red exemptions disabled, zero skipped/contaminated tests, in 2m23s.
The new browser-policy regression separately passes 1/1 in 29 seconds.

| Failure | Cause and repair |
| --- | --- |
| GymShadingModelsCompileAllLegacyVariants | The legacy catalog explicitly rejects Substrate. On a Substrate host, assert the real atomic rejection and unchanged catalog; retain all compilation checks on supported legacy hosts. |
| ToonRejectsUnsupportedAuthoringAtomically | Contract resolution now rejects translucent Toon before the older validator message. Isolate the domain fixture and assert the current rejection, unchanged resolver output and absence of package creation. |
| ToonReservedOutputNamesAreScoped | Toon.ush now declares its model in source, overriding the test's attempted DefaultLit setting. Use header-free RimGlow with matching argument types and assert the resolved models before checking reserved names. |
| ToonLightingReloadUpdatesExistingMID | The earlier wiring-only test left an uncompiled ThinTranslucent UObject alive after its strong owner expired. Global recompilation visits non-garbage materials and observed its stale Refraction cache. Retire wiring/transition fixtures through the existing cleanup helper on every exit; assert they are marked garbage. No production reload change. |
| ToonAuthoringGeneratesNativeMaster | An accelerated CEF browser created before automation failed its hardware check after GIsAutomationTesting changed. Separate stable D3D capability from the software-paint policy for new automation/build-machine browsers. Inherited popups validate hardware, preserving their parent's existing paint mode. |
| GeneratesUsableMasters | DebugScene used Opacity as both a Custom input and generated output. Rename only the authored input to FillOpacity and generate the two missing shipped masters through the production generator. Keep O.Opacity and strict validation intact. |

The browser repair is in the dependent engine's WebBrowser module, with a focused
automation-policy regression. It covers stable hardware eligibility across the
automation transition and software selection for new automation/build-machine
browsers. Existing paint/popup lifetime assertions remain enabled. The final
rendered CkUsf batch supplies the original browser-crash regression path.

The two debug masters are prepared outside the tests. Generation tests retain
their isolated package-root rule and do not populate shipped content themselves.
CkDebugScene continues to use its existing legacy materials; consumer migration
is outside this repair.

The incremental Toolbox build passed (746.31 seconds). Both shipped debug masters
were generated with FillOpacity. The initial invocation incorrectly combined
ExecutePythonScript with automation: that flag exited after preparing assets,
so no browser-test verdict was produced and Toolbox retried. That run was stopped.
A subsequent no-script run found a stale cached renderer-test list (zero matches);
explicit fresh discovery was used for the new engine regression. Neither
attempt is counted as a passing test. The final serial real-renderer CkUsf batch
ran wiring and global reload in one editor. No cache/artifact cleanup.
The user subsequently authorized direct dev publication and engine main-ck delivery.
The unrelated Iris edit is preserved; opening SHA256:
`69813456749AB1E6ACE335A81231761F589552ADBF5BDEB00949D7F2FE990C92`.

## Final verification

- `Saved/Logs/UsfFailures-BrowserFresh.log`: browser-policy regression 1/1,
  zero failed/skipped/contaminated, D3D12 SM6. Predicate coverage, not a synthetic
  paint/popup lifecycle test; the full CkUsf run covers the original crash path.
- `Saved/Logs/UsfFailures-FinalSerial.log`: raw CkUsf 57/57 in one editor,
  `--no-live --no-nullrhi --parallel=1 --known-reds=off` and
  `--editor-args=-CkUsfSubstrateProof`. All six baseline failures report Success.
- PID 15548 ran wiring before global reload. Both Toon reload fixtures retained
  their MID identities; lighting gain zero darkened pixels and exact source
  restoration recovered them. No refraction ensure, browser assertion or
  AngelScript compile failure occurred in this run.
- The obsolete GeneratesUsableMasters known-red entry is removed after its raw
  pass in the plugin's CkPlugins home host. Other entries are unchanged.
- Verified before publication. No other game host or non-Substrate
  renderer boot was run; the legacy host retains its original compilation branch.
- Final diff checks pass. Both production Toon HLSL files are unchanged after
  edit/recovery tests, and the closing Iris checksum matches the opening value.
- Publication preflight fetched all four repositories: their HEADs matched the
  intended origin/dev or origin/main-ck tips, with no incoming commits. The
  existing green gates therefore cover the published code without a new boot.
