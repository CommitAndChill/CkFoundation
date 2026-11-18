# CkActor

**Purpose:** Actor ↔ Entity bridge. Provides the fragment and utilities that associate an `AActor` with an ECS entity, plus actor-level gameplay tag constants for CkFoundation actors.

**Depends on:** `CkCore`, `CkEcs`, `CkLog`, `CkVariables`.
**Used by:** `CkEcsExt` (directly — EcsExt's EntityHolder builds on this), plus ~20 feature modules that need actor-owned entities.

---

## What's here

```
CkActor/Public/CkActor/
├── ActorModifier/     – fragment + utils for actor-level modifiers
└── CkActor_GameplayTags.cpp – gameplay tag definitions for CkActor
```

`CkActor` is deliberately thin. Its job is to declare the bridge data (fragments, tag constants) that higher-level modules consume. `CkEcsExt` owns the full actor-binding lifecycle logic.

---

## Actor entity pattern

An entity that belongs to an Actor carries a `ck::FFragment_EntityBridge` fragment (defined in `CkEcs/EntityBridge/`), which holds the Actor's `UCk_EntityBridge_ActorComponent_UE`. `CkActor` extends this with actor-specific modifiers and tag associations.

Creation pattern (the only one; rules in `CkEcs/Claude.md` § Actor entities):

```cpp
// The Actor has a UCk_EntityBridge_ActorComponent_UE; its script derives UCk_EntityScript_WithActor_UE.
// Called once, on every machine, from BeginPlay or later.
EntityBridge->Request_SpawnLinkedEntity(UMy_EntityScript::StaticClass());
```

Actor utilities (for getting the actor from an entity or vice versa) live in `CkCore/Actor/CkActor_Utils.h` and `CkEcs/EntityBridge/` (`UCk_Utils_EntityBridge_UE`).

---

## Actor-modifier fragment

`ActorModifier/` provides a fragment for temporary actor-state overrides (visibility, collision, movement locks) applied to an actor through its associated entity. Useful for ability/VFX systems that need to temporarily alter actor properties without tangling ECS state with actor state.

---

## Anti-patterns

1. Don't hold an `AActor*` raw pointer directly in a processor fragment — use `TStrongObjectPtr<AActor>` or route through the EntityBridge fragment. Raw actor pointers can become stale during streaming.
2. Don't assume every entity has an actor. ECS entities are independent of actors; only entities linked through a Ck Entity Bridge have one.

---

## See also
- `CkEcsExt/Claude.md` — EntityHolder lifecycle binding (actor destroyed → entity destroyed).
- `CkEcs/Claude.md` — `FFragment_EntityBridge` in `CkEcs/EntityBridge/`, § Actor entities.
- `CkCore/Actor/README.md` — `UCk_Utils_Actor_UE` spawn/find helpers.
