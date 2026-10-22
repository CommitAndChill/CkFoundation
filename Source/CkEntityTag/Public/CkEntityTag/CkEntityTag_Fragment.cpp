#include "CkEntityTag_Fragment.h"

#include "CkEntityTag/CkEntityTag_Utils.h"

#include "CkCore/Ensure/CkEnsure.h"

#include "CkEcs/Persistence/CkPersistenceHandlerRegistry.h"
#include "CkEcs/Persistence/CkPersistenceHandlerRegistry.inl.h"
#include <NativeGameplayTags.h>

// --------------------------------------------------------------------------------------------------------------------

UE_DEFINE_GAMEPLAY_TAG(TAG_EntityTag_Root, TEXT("EntityTag"));

auto
    FCk_Request_EntityTag_RestoreSet::
    Get_IsValid() const
    -> bool
{
    if ((_Version != 0 && _Version != 1) || _TagNames.Num() != _Counts.Num() ||
        _GameplayTags.Num() != _GameplayCounts.Num())
    { return false; }
    if (_Version == 0 && NOT _GameplayTags.IsEmpty())
    { return false; }

    auto Names = TMap<FName, int32>{};
    for (auto Index = 0; Index < _TagNames.Num(); ++Index)
    {
        if (_TagNames[Index].IsNone() || _Counts[Index] <= 0 || Names.Contains(_TagNames[Index]))
        { return false; }
        Names.Add(_TagNames[Index], _Counts[Index]);
    }
    auto GameplayTags = TSet<FGameplayTag>{};
    auto RequiredNameCounts = TMap<FName, int64>{};
    for (auto Index = 0; Index < _GameplayTags.Num(); ++Index)
    {
        const auto Tag = _GameplayTags[Index];
        const auto Count = _GameplayCounts[Index];
        if (NOT Tag.IsValid() || Count <= 0 || GameplayTags.Contains(Tag))
        { return false; }
        GameplayTags.Add(Tag);
        for (const auto& Parent : Tag.GetGameplayTagParents())
        {
            auto& RequiredCount = RequiredNameCounts.FindOrAdd(Parent.GetTagName());
            RequiredCount += Count;
            const auto NameCount = Names.Find(Parent.GetTagName());
            if (NameCount == nullptr || RequiredCount > *NameCount)
            { return false; }
        }
    }
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

static struct FCkEntityTagSaveHandlerRegistrar
{
    FCkEntityTagSaveHandlerRegistrar()
    {
        FCk_PersistenceHandlerRegistry::Register_SaveOnly<FCk_SaveData_EntityTags>({
            .Posture = ECk_Snapshot_Posture::Durable,
            .Produce = [](FCk_Handle& Entity) -> TOptional<FInstancedStruct>
            {
                if (NOT Entity.Has<ck::FFragment_EntityTag>())
                { return {}; }

                const auto& Tags = Entity.Get<ck::FFragment_EntityTag>().Get_Tags();
                if (Tags.IsEmpty())
                { return {}; }

                auto Payload = FCk_SaveData_EntityTags{};
                Payload.Set_Version(1);
                for (const auto& TagCount : Entity.Get<ck::FFragment_EntityTag>().Get_GameplayTagCounts())
                {
                    Payload.Get_GameplayTags().Add(TagCount._Tag);
                    Payload.Get_GameplayCounts().Add(TagCount._Count);
                }
                Payload.Get_TagNames().Reserve(Tags.Num());
                Payload.Get_Counts().Reserve(Tags.Num());
                for (const auto& TagCount : Tags)
                {
                    Payload.Get_TagNames().Add(TagCount._Name);
                    Payload.Get_Counts().Add(TagCount._Count);
                }
                return FInstancedStruct::Make(Payload);
            },
            // Reading the live tag set here would miss the GatedDuringLoad construct-seeds still enqueued-but-undrained,
            // so this enqueues ONE composite request that diffs at DRAIN time instead — see CkEntityTag/CLAUDE.md
            // § "Save/load restore".
            .HydrationApply = [](FCk_Handle& Entity, const FInstancedStruct& New, const TOptional<FInstancedStruct>& /*Old*/) -> ECk_Persistence_ApplyResult
            {
                CK_ENSURE_IF_NOT(ck::IsValid(Entity),
                    TEXT("EntityTag hydration skipped — invalid entity [{}]"), Entity)
                { return ECk_Persistence_ApplyResult::Rejected; }

                CK_ENSURE_IF_NOT(New.GetPtr<FCk_SaveData_EntityTags>() != nullptr,
                    TEXT("EntityTag hydration payload has the wrong type on [{}]"), Entity)
                { return ECk_Persistence_ApplyResult::Rejected; }

                const auto& Payload     = New.Get<FCk_SaveData_EntityTags>();
                const auto& SavedNames  = Payload.Get_TagNames();
                const auto& SavedCounts = Payload.Get_Counts();

                auto Request = FCk_Request_EntityTag_RestoreSet{SavedNames, SavedCounts};
                Request.Set_Version(Payload.Get_Version())
                    .Set_GameplayTags(Payload.Get_GameplayTags())
                    .Set_GameplayCounts(Payload.Get_GameplayCounts());
                CK_ENSURE_IF_NOT(Request.Get_IsValid(),
                    TEXT("EntityTag hydration payload malformed on [{}] — expected unique valid names with positive parallel counts"), Entity)
                { return ECk_Persistence_ApplyResult::Rejected; }

                UCk_Utils_EntityTag_UE::Request_RestoreSet(Entity, Request, {});

                return ECk_Persistence_ApplyResult::Applied;
            }});
    }
} GCkEntityTagSaveHandlerRegistrar;

// --------------------------------------------------------------------------------------------------------------------
