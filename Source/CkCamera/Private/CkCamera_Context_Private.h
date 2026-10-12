#pragma once

#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/OwningActor/CkOwningActor_Fragment.h"
#include "CkEcs/OwningActor/CkOwningActor_Utils.h"
#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"

#include <Engine/World.h>
#include <GameFramework/Actor.h>

// Private to CkCamera's production paths. Never install an OwningActor fragment on a
// camera-only child: its ordinary entity destruction would destroy the pawn actor.
namespace ck::camera
{
    struct FCameraContext
    {
        UWorld* World = nullptr;
        AActor* OwningActor = nullptr;
    };

    inline auto TryResolveCameraContext(const FCk_Handle& InDirector,
        FCameraContext& OutContext) -> bool
    {
        OutContext = {};
        if (NOT ck::IsValid(InDirector)) { return false; }

        const auto Root = UCk_Utils_EntityLifetime_UE::Get_TransientEntity(InDirector);
        if (NOT ck::IsValid(Root) ||
            Root.Get_RegistryView().Get_RegistryHandle() !=
                InDirector.Get_RegistryView().Get_RegistryHandle() ||
            Root.Has_Any<ck::FTag_DestroyEntity_Initiate, ck::FTag_DestroyEntity_EndPlay,
                ck::FTag_DestroyEntity_Teardown, ck::FTag_DestroyEntity_Await,
                ck::FTag_DestroyEntity_Finalize>() ||
            NOT Root.Has<TWeakObjectPtr<UWorld>>())
        { return false; }

        auto* const World = Root.Get<TWeakObjectPtr<UWorld>>().Get();
        if (NOT ck::IsValid(World) ||
            World->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed) ||
            NOT World->IsGameWorld() || World->bIsTearingDown ||
            World->IsBeingCleanedUp())
        { return false; }
        const auto* const Subsystem = World->GetSubsystem<UCk_EcsWorld_Subsystem_UE>();
        if (NOT ck::IsValid(Subsystem) ||
            Subsystem->Get_LoadHold() == ECk_EcsWorld_LoadHold::Teardown ||
            UCk_Utils_EcsWorld_Subsystem_UE::TryGet_TransientEntityForWorld(*World) != Root)
        { return false; }

        const auto RegistryHandle = Root.Get_RegistryView().Get_RegistryHandle();
        TArray<FCk_Handle> Seen;
        auto Current = InDirector;
        AActor* NearestActor = nullptr;
        while (true)
        {
            if (NOT ck::IsValid(Current) ||
                Current.Get_RegistryView().Get_RegistryHandle() != RegistryHandle ||
                Current.Has_Any<ck::FTag_DestroyEntity_Initiate,
                    ck::FTag_DestroyEntity_EndPlay, ck::FTag_DestroyEntity_Teardown,
                    ck::FTag_DestroyEntity_Await, ck::FTag_DestroyEntity_Finalize>() ||
                Seen.Contains(Current))
            { return false; }
            Seen.Add(Current);
            if (Current.Has<TWeakObjectPtr<UWorld>>() &&
                Current.Get<TWeakObjectPtr<UWorld>>().Get() != World)
            { return false; }
            if (Current.Has<ck::FFragment_OwningActor>())
            {
                auto* const Actor = UCk_Utils_OwningActor_UE::TryGet_EntityOwningActor(Current);
                if (NOT ck::IsValid(Actor) ||
                    Actor->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed) ||
                    Actor->IsActorBeingDestroyed() || Actor->GetWorld() != World ||
                    UCk_Utils_OwningActor_UE::TryGet_ActorEntityHandle(Actor) != Current)
                { return false; }
                if (NearestActor == nullptr) { NearestActor = Actor; }
            }
            if (Current == Root)
            {
                OutContext.World = World;
                OutContext.OwningActor = NearestActor;
                return true;
            }
            if (NOT Current.Has<ck::FFragment_LifetimeOwner>())
            { return false; }
            const auto Next = UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(Current);
            if (Next == Current) { return false; }
            Current = Next;
        }
    }
}
