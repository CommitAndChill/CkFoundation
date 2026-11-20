#include "CkEcsWorld.h"

#include "CkEcs/CkEcsLog.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck
{
    FEcsWorld::
    FEcsWorld()
    {
        if (DoInitialize() != ECreateResult::Success)
        {
            ck::ecs::Error(TEXT("FEcsWorld construction failed; use TryCreate to handle registry allocation failure."));
        }
    }

    FEcsWorld::
    FEcsWorld(EDeferredInitialize)
    {
    }

    auto
        FEcsWorld::
        TryCreate(ECreateResult& OutResult)
        -> TUniquePtr<FEcsWorld>
    {
        auto World = TUniquePtr<FEcsWorld>{new FEcsWorld{EDeferredInitialize::Defer}};
        OutResult = World->DoInitialize();
        if (OutResult != ECreateResult::Success)
        { return {}; }

        return World;
    }

    auto
        FEcsWorld::
        DoInitialize()
        -> ECreateResult
    {
        _OwnedRegistry = MakeUnique<ck::registry_table::EnttRegistryType>();

        auto Result = ECreateResult::InvalidState;
        const auto RegistryHandle = ck::registry_table::TryAllocate(_OwnedRegistry.Get(), Result);
        if (Result != ECreateResult::Success)
        {
            _OwnedRegistry.Reset();
            return Result;
        }

        const auto TransientEntityId = FCk_Entity{_OwnedRegistry->create()};

        // The transient entity lives in the registry's ctx so every view resolved from this slot,
        // including via *Handle, sees the same one.
        _Registry = FCk_Registry{RegistryHandle};
        _Registry.SetContext<ck::FCtx_TransientEntity>(ck::FCtx_TransientEntity{TransientEntityId});
        return ECreateResult::Success;
    }

    FEcsWorld::
    ~FEcsWorld()
    {
        // Free the slot BEFORE destroying the registry: in between, a ghost handle resolves to null
        // rather than reaching freed memory.
        if (_Registry.Get_RegistryHandle().IsSet())
        { ck::registry_table::Free(_Registry.Get_RegistryHandle()); }

        _Registry = FCk_Registry{};
        _OwnedRegistry.Reset();
    }
}

// --------------------------------------------------------------------------------------------------------------------
