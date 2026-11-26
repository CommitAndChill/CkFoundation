# CkVoiceChat

> **Status (2026-11-26):** feature-complete and unit/net-tested; a real microphone has been heard through the
> loopback gym, but proximity voice between two players has not yet been auditioned by a human. The original
> design spec, CTO review and campaign log are lost; this file and the code are the design of record.
> `PushToTalk` and `OpenMic` are distinct (see Key API, Transmit mode); neither has been heard by a human yet.

**Purpose:** Proximity voice chat — microphone capture, Opus encoding, server-routed transport
with server-side interest management, and spatialized playback, as three composable ECS features:
**VoiceTalker** (an entity that can speak), **VoiceChannel** (a context speech happens in — child
entity under a host, per-channel spatialization policy/attenuation/effect chain), **VoiceListener**
(the local ears — per-talker client mute + receive volume). Zero coupling to OnlineSubsystem,
sessions, or external services.

**Depends on (as of P4):** `CkActorRelay`, `CkCore`, `CkEcs`, `CkEcsExt`, `CkLabel`, `CkLog`,
`CkRecord`, `CkResourceLoader` (per-channel audio asset resolution), `CkSettings`, `CkShapes`,
`CkSpatialQuery` (the routing probes). UE: `AudioMixer`, `DeveloperSettings`, `GameplayTags`,
`NetCore`, `Voice`. Never `CkRelationship`.

**Playback config (P4).** A channel's authored `_Attenuation`/`_SourceEffectChain`/`_SoundClass` soft refs
resolve through CkResourceLoader at channel Setup on every machine (spatializing channels with
none authored resolve the module default attenuation; there is no default SoundClass);
`Get_ResolvedAttenuation`/`Get_ResolvedSourceEffectChain`/`Get_ResolvedSoundClass` expose them. Each receiving
machine's synth adopts the config of the **highest-`_Priority` channel currently delivering that talker's stream**
(one synth per talker per machine — the single-playback dedupe) and applies spatialization per policy. The
channel's SoundClass is the synth's for every policy, near or far (a game's voice volume slider overrides that
class); none authored, or no channel yet, leaves the engine's default SoundClass. **Attach:** the synth attaches
to the talker Spec's `_PlaybackAttachComponent` when set, else to the bridged actor's root, at
`_PlaybackAttachSocketName`. Each machine's Spec names its own copy of the component (the Spec is built on every
machine). Caller errors, each an ensure with a fallback: a component owned by another actor (the root instead),
and a socket name the target lacks (the target's origin instead). **Volume:** the synth takes this machine's
`FFragment_VoiceListener::_TalkerVolumes` entry for the talker (`Request_SetTalkerVolume`; 1.0 with no local
listener or no entry) at creation and on the first receive pass after the value changes, with no bundle needed;
the applied value is kept on `FFragment_VoiceTalker_Playout` so an unchanged volume costs no engine call. Volume
never travels upstream. **HybridRadio** renders per recipient: spatialized proximity speech
inside the channel's `AudibleRange`, flat radio through the channel's effect chain outside it,
with the same hysteresis asymmetry the routing probes use (near inside range, held to
range + margin). Remote `Get_CurrentAmplitude` mirrors the wire header while bundles flow and
releases to zero once the stream idles past the sender's stale-drop age.

---

## Key API (P0 surface — grows per phase)

- `UCk_Utils_VoiceTalker_UE::Add(Handle, Params)` - compose talker fragments directly on a
  transform-bearing entity. Params: transmit mode (PushToTalk default), input gain (1.2), VAD
  threshold (0.07), gate in push-to-talk (Disable), auto-join channel tags, playback attach component and socket,
  loopback. Mode, gain, threshold and the push-to-talk gate switch seed `FFragment_VoiceTalker_Tunables` at `Add`
  and change only by request (`Request_SetTransmitMode` / `_SetInputGain` / `_SetVadThreshold` / `_SetGateInPushToTalk`,
  read back with the matching `Get_*`); all four are local to the machine and never travel.
  - **Transmit mode:** the modes differ in who opens the transmit and whether the voice gate applies, nothing else.
    No wire, replication or routing difference; the mode is local to each machine.

    | | `PushToTalk` | `OpenMic` | `Disabled` |
    |---|---|---|---|
    | Who opens the transmit | The caller: `Request_StartTransmit` / `Request_StopTransmit` | The module, once per arming, on the talker this machine's player controls. A caller's Start / Stop still work (unowned talkers, tests) | Nobody: Start is refused (ensure) |
    | Voice gate | Bypassed unless `_GateInPushToTalk`: every captured frame is encoded from the first one (no gate update, no attack delay); `Get_IsSpeaking` is true while transmitting unmuted. With the switch enabled, the same gate as `OpenMic` (TeamSpeak's "voice activation while push-to-talk": a tuned gate that should hold in a noisy room) | Applies: `_VadThreshold`, 20 ms attack, 200 ms release | - |

    - *Tuning the gate.* `Request_SetVadThreshold` (clamped 0..1) lands on the next captured frame, mid-transmit
      included, and leaves the gate open or closed as it was. A change that makes the gate start applying to a
      transmitting talker (`_GateInPushToTalk` enabled in `PushToTalk`, or `PushToTalk -> OpenMic` with it disabled)
      starts the gate closed, as a transmit's start does; `Get_IsSpeaking` then follows the gate from the next frame.
      The loopback gym's control panel drives all three by key.

    - *Arming.* A talker is armed (`FTag_VoiceTalker_OpenMicPending`) when its mode becomes `OpenMic`: at `Add` from
      the Spec, or by `Request_SetTransmitMode` while it is not transmitting (`OpenMic -> OpenMic` re-arms).
      `FProcessor_VoiceTalker_OpenMic` (after Setup, before HandleRequests, every net mode) acts on a pending talker
      the first time `UCk_Utils_Net_UE::Get_IsEntityLocallyControlled_ByPlayer` reports `IsLocallyControlled` - a
      possessed, player-controlled pawn bridged to the talker's own entity: it disarms and issues the ordinary
      `Request_StartTransmit`, so every rule of that handler applies. A talker that is not controlled here stays
      pending, so late possession converges; a talker with no bridged actor is disarmed (it can never be controlled).
      An owner chain that is not a possessed pawn (an actor owned by a PlayerController) never opens.
    - *One attempt per arming.* A Start that cannot open capture reports through the capture-availability line below
      and is not retried until the talker is armed again. An explicit `Request_StopTransmit` stands until re-armed;
      it also cancels an arming not yet acted on.
    - *Leaving `OpenMic`.* `SetTransmitMode` to `PushToTalk` or `Disabled` disarms and closes an open transmit through
      the same path as StopTransmit (`FProcessor_VoiceTalker_HandleRequests::Stop_Transmit`); so does `PushToTalk ->
      Disabled`. A microphone is never left open in `PushToTalk` without a Start, or in `Disabled` at all.
      `PushToTalk -> OpenMic` while transmitting keeps the caller's transmit and arms nothing. Setting the mode the
      talker already has changes nothing, except the `OpenMic` re-arm above.
    - *Losing local control.* A transmit the open mic opened (`FFragment_VoiceTalker_Capture::_OpenedByOpenMic`) closes
      and re-arms when the talker stops being controlled here (checked by the capture processor), so an unpossess
      never sends frames the server's spoof guard would refuse. A caller's transmit is not touched.

    `FCk_Request_VoiceTalker_StartTransmit::_ChannelTags` is not read: a transmit goes to every channel the talker is
    a `CanTalk` member of.
  - **Capture availability:** `Request_StartTransmit` opens capture BEFORE creating codecs. No device, a device
    that will not start, a process that renders no audio (`FApp::CanEverRenderAudio()` false: `-nosound`, a
    dedicated server, a commandlet), or an unattended process (`FApp::IsUnattended()`: automation lanes, CI) is an
    operational outcome: the request completes Failed, a Log line names the talker, and `Get_CaptureStartResult`
    returns the `ECk_VoiceChat_CaptureStartResult` until the next attempt. The engine source decides this before it
    creates any capture, so an unattended run - including a headless gate lane, which does render audio - never
    opens a real microphone, even when the game starts transmit at spawn. Only a disabled engine Voice module
    (`[Voice] bEnabled=true` missing) ensures. An injected capture source (`Debug_InjectCaptureSource`) is
    unaffected by all of this and is never replaced by the engine microphone; injection refuses a transmitting talker.
  - **Per transmit:** the voice gate, buffered PCM, the outbound queue and the no-progress window reset at
    StopTransmit, so every gated transmit starts with the gate closed.
  - **No-progress report:** at most once per transmit. Progress is a frame **leaving the machine** - handed to the
    player's relay RPC, or placed in the listen host's own routing inbox - not a frame encoded. For a loopback talker
    (`_Loopback` enabled, as in the gym) a frame reaching its own loopback playout counts too, so working loopback is
    never reported. When a transmit has run 5 s (not counting time self-muted or `Disabled`) with no such frame, one
    Log line names the talker, how long, and the stage where the frames stopped: no audio from the source; audio that
    never reached the voice-gate threshold (wherever the gate applies, with the peak RMS, gain and the threshold at
    report time) or, with the gate bypassed, never filled a whole frame; gate-open frames the encoder
    produced nothing for; or frames encoded but none sent, with the reason `Send_OutboundFrames` returned - no channel
    registry in this world, not a `CanTalk` member of any of the N channels registered here (on a client: the control
    plane has not delivered its membership), no relay resolved for the local player yet, the host inbox full, every
    frame stale, or the byte budget. A transmit that has sent any frame never reports - silence after speech (a quiet
    room, a closed gate between sentences) is not a stall.
  - **Codecs:** encoder, loopback decoder and receive decoder are all created through one checked path that
    ensures (naming the `[Voice]` fix) when the engine returns null; a receiver without a decoder drops and counts
    the bundles (`VoiceChat Receive Dropped (No Decoder)`).
- `UCk_Utils_VoiceChannel_UE::Add(HostHandle, Params)` - create a channel child entity under a
  long-lived host, labeled by its `VoiceChat.Channel.*` tag, connected via a Record on the host.
  `TryGet_VoiceChannel(Host, Tag)` resolves by label.
  - **Auto-join:** on the authority, a talker whose Spec lists a channel name in `_AutoJoinChannels` joins every
    registered channel of exactly that name whose Spec enables `_AutoJoinTalkers`, whichever was composed first
    (`FProcessor_VoiceTalker_AutoJoin`, level-triggered on the registry's generation). Each pair is joined once,
    through the ordinary Join request; an explicit Leave afterwards stands. Clients never join - they learn
    membership from the control plane.
  - **Membership integrity:** a talker that begins teardown on the authority leaves every channel through the
    ordinary Leave request (`FProcessor_VoiceTalker_LeaveChannels`, prompt); each channel also sweeps member and
    server-mute keys whose entity no longer exists (`FProcessor_VoiceChannel_SweepMembers`, eventual, logs a
    Warning, no MemberLeft for a key whose talker is gone). Both re-push the control plane, so clients converge.
    Before this, routing past a destroyed member ensured in `Get_MemberFlags` and in the ownership-chain walk.
  - **Proximity probes** compose the frame a Positional3D member has a Transform; a member without one keeps its
    reconcile pending (one Log line when the wait starts) rather than never getting probes.
- `UCk_Utils_VoiceListener_UE::Add(Handle)` — compose local ears state (mute set + volume map).
- `UCk_Utils_VoiceChat_Settings_UE` — project settings getters with constexpr fallbacks
  (`ck::voice_chat::defaults`): codec (48 kHz/24 kbps/20 ms), transport (frames per RPC, byte
  budget), routing (MaxAudibleSpeakers 8 server-only, hysteresis), playback (jitter depths,
  default attenuation).

## Routing-policy matrix (review N7) — who receives a talker's bundles

The server routing pipeline is two processors: `FProcessor_VoiceChat_Route` (per talker:
authorization + staging) then `FProcessor_VoiceChat_FlushForwards` (per recipient: fairness cap →
byte budget → send). A bundle reaches a recipient only if EVERY row below passes:

| Gate (in order) | Positional3D | Global2D | HybridRadio (P4) |
|---|---|---|---|
| Wire `ChannelIdx` resolves in the registry | required (drop + count, never stash — N1) | same | same |
| Stamped sender owns the talker (spoof guard) | required where both resolve; player-less (NPC) talkers skip it — **known v1 gap**, an owning player can transmit on any NPC talker | same | same |
| Talker is a member with `CanTalk`, not server-muted | required | required | required (membership IS the entitlement) |
| Recipient set | `CanHear` members ∩ the sender's probe-maintained proximity set, with hysteresis: a recipient becomes audible inside the channel's range and stays audible until range + margin (`Get_ProximityHysteresisMarginCm`). A sender or recipient without a Transform has no probes and Positional3D **fails closed** for it — no position, no proximity, never a fallback to membership-wide sends | `CanHear` members regardless of range | `CanHear` members; one wire copy, 3D-vs-radio branch chosen client-side at render |
| Not the talker entity, not the talker's own connection | required (no echo back) | same | same |
| Recipient has not listener-muted the talker | required (the privacy exclusion — muted audio is never SENT) | same | same |
| Audible-speaker cap (`MaxAudibleSpeakers`, default 8) | envelope-bucket priority, least-recently-served rotation at saturation | same | same |
| Per-connection byte budget this tick | required | same | same |

Listen-server host: the host's capture injects straight into its own routing inbox (no
self-RPC); the host receives other talkers like any client (a Client RPC on a host-owned relay
executes locally).

## Consumer recipes

### Roger-beep / walkie-talkie bip (Ruling Q4: consumer recipe, NO CkCue dep in this module)

The module fires `OnVoiceTalker_TransmitStarted` / `OnVoiceTalker_TransmitStopped` on every
transmit edge; the beep is the consumer's cue, bound in game code (any of C++/BP/AS). AngelScript
shape:

```angelscript
// In the consumer's EntityScript BeginPlay - Talker is the FCk_Handle_VoiceTalker:
utils_voice_talker::BindTo_OnTransmitStarted(Talker, ECk_Signal_BindingPolicy::IgnorePayloadInFlight,
    ECk_Signal_PostFireBehavior::DoNothing, FCk_Delegate_VoiceTalker(this, n"OnTransmitStarted"));
utils_voice_talker::BindTo_OnTransmitStopped(Talker, ECk_Signal_BindingPolicy::IgnorePayloadInFlight,
    ECk_Signal_PostFireBehavior::DoNothing, FCk_Delegate_VoiceTalker(this, n"OnTransmitStopped"));

UFUNCTION()
private void OnTransmitStarted(FCk_Handle_VoiceTalker InTalker)
{
    // The consumer's own cue tag + CkCue executor - e.g. a squelch-open bip:
    // Request_ExecuteCue_Transient(n"Cue.Voice.RogerBeep.Open", ...) on the cue subsystem.
}
```

`OnTransmitStopped` is the classic roger-beep edge (squelch-close "over"). For a per-CHANNEL beep
(different radio nets sounding different), key the cue tag off the channel the talker transmits
on. Audition: the CkVoiceChat gym station (`[EDITOR-VERIFY]`).

## Anti-patterns

- Don't add a `CkRelationship` dep for team channels — team semantics are membership-flag
  configurations on channel entities (ADR-5; CTO review N4).
- Don't poll overlaps for the routing set — the routing set is a persistent probe's
  event-maintained overlap set (ADR-6); world scans in the packet path are the confirmed failure
  mode of a commercial reference. The routing processor READS `FFragment_Probe`'s
  maintained set (the CkCrowd Neighbors adopter pattern) only when bundles actually flow — it
  never issues a spatial query.
- Don't stash undecodable/unroutable voice packets — voice is disposable; drop + count (review N1).

## See also

- `CkRenderTarget/CLAUDE.md` — the paced relay-actor stream precedent (ADR-4 co-adopter).
- `CkActorRelay/CLAUDE.md` — relay doctrine incl. the ADR-4 stream clause.
