# SteamJet — design sheet (behavior 47)

An ORIGINAL design, not a recreation: there is no source Niagara system and no corpus export, so every number
below is a **design constant** chosen for the read "a horizontal blast of steam from a nozzle that slows, thins and
lifts at its end". The recipe schema's archaeology sections (§1, §3, §5, §7, §10) have nothing to cite and are
omitted; the sections that remain are the ones that still carry information.

## 0. Completion state

Implemented (GPU `Behaviors/Behavior_SteamJet.ush` + CPU mirror `case 47`), unit-tested
(`CkTests.UnitTests.CkParticles.SteamJetBehavior`), own cadence row. Visual verdict is a human step at the
Particles gym station `Gym.Particles.SteamJet` — `[HUMAN-VERIFY]` open.

## 2. Visual intent

A continuous jet along the system's local **+X** (spawn rotation aims it). Puffs leave a small nozzle disc fast,
decelerate under drag, drift upward late in life, swell from a tight core to a soft cloud, and fade from warm white
to grey. It must read as a pressurized jet, not as a campfire plume — which is why it is not `SmokePlume` (12)
re-aimed: a plume's buoyant rise and multi-second puffs read as smoke from a fire.

## 4. Cadence

| Field | Value | Why |
|---|---|---|
| Row | `PS_CkParticles_Template_SteamJet` | own row; no existing cadence matches |
| Loop | 1.0 s | rate-only, so the loop only wraps `Emitter.Age` |
| Lifetime | 0.9 s | long enough for drag to carry a puff ~86% of its `v0/k` range |
| Burst | 0 | a continuous jet |
| Rate | 70 /s | ~63 live puffs; dense enough to read as one column |

## 6. Renderer

The shared VisTag-2 smoke sprite (`M_CkParticles_SoftSmoke`, translucent, `Rotation` applies). The row declares no
renderers of its own and the behavior binds no look (`Get_BehaviorLookName` = `NAME_None`).

## 8. CkParticles translation — constants

| Constant | Value | Role |
|---|---|---|
| Birth disc radius | 7 uu | `r0 = 7 sqrt(rand1)`, uniform over the disc in the YZ plane |
| Launch speed `v0` | 520–680 uu/s | `lerp(520, 680, rand3)` |
| Cone aperture | 7° | `a = radians(7) sqrt(rand4)`, `phi = 2π rand5` |
| Drag `k` | 2.2 /s | linear drag, integrated analytically |
| Buoyancy | `35 t²` uu | constant upward acceleration of 70 uu/s² |
| Size | 14 → 64 uu | `(14 + 50 n) * lerp(0.9, 1.1, rand6)`, both axes |
| Alpha peak | 0.55 | `0.55 smooth(0, 0.08, n) (1 - smooth(0.40, 1, n))` |
| Grey ramp | 0.95 → 0.80 | premultiplied: `Color = (g A, g A, g A, A)` |
| Rotation | ±180° + ±45°/s spin | `(rand7 - 0.5) 360 + (rand8 - 0.5) 90 t` |
| Dissolve | `saturate(n - 0.3) * 1.4` | `Dynamic.x`, off for the first 30% of life |

Rand salts are 1..8 in the order above. `smooth` is `CkParticles_SmoothStep` (Common.ush), mirrored by
`NDICkParticlesLocal::SmoothStep`.

### Closed-form drag

With linear drag, `dv/dt = -k v`, so `v(t) = v0 e^(-k t)` and the distance along the launch direction is

```
s(t) = ∫0..t v0 e^(-k τ) dτ = v0 (1 - e^(-k t)) / k
```

whose limit is `v0 / k` = 236–309 uu. At the row's 0.9 s lifetime `1 - e^(-1.98) = 0.862`, so a puff dies after
204–266 uu along its line — the visible jet length is that plus the last puffs' 64 uu size. Position and velocity:

```
Position = Birth + Dir * s(t) + (0, 0, 35 t²)
Velocity = Dir * v0 e^(-k t) + (0, 0, 70 t)
```

Both are functions of `(Age, Seed)` alone; `DeltaTime` is never read, so the jet is frame-rate independent (the unit
test evaluates the same particle at 1/60 and 1/15 s and requires bit-identical output). `Emitter.Age` is never read.

## 11. Runtime binding

`Spawn_BehaviorAtLocation(47, Location, Rotation, Scale, NAME_None)` — the rotation aims +X. A finished component
needs `Activate(true)` to restart. Whole-system tuning (`Request_ApplyTuningValues`): size `x`, colour `y`, alpha `z`,
playback `w`. Convention tuning asset: `DA_CkParticles_Tuning_SteamJet` (generated with identity values).

## 12. Verification

1. `CkTests.UnitTests.CkParticles.SteamJetBehavior` — roster, row, closed form, cone, range, envelope.
2. After a template regen: `grep -ac ExecuteStage|CkTuning|CkBakedIds PS_CkParticles_Template_SteamJet.uasset`
   all non-zero, then the stabilize lane.
3. `[HUMAN-VERIFY]` Particles gym, station `STEAM JET (47)`: a horizontal jet roughly 2.5–3 m long that thins and lifts
   at its end.

## 13. Known differences

- Original design, no reference system: there is nothing to A/B against; the verdict is the read in §2.
- Translucent smoke renderer only — no light layer, no heat-haze distortion, no condensation sparkle.
- The `v0/k` range (236–309 uu) is the asymptote; puffs retire at 0.9 s, ~86% of the way there.

## 14. Tunables

Jet length: `v0` range and `k`. Lift: the `35 t²` term (and its `70 t` velocity twin). Body: the size ramp. Density:
the alpha peak 0.55 and the row rate. All live in `Behavior_SteamJet.ush` and its CPU mirror `case 47`, which must move
together; per-instance retuning goes through `User.CkTuning` instead.
