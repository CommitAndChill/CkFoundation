#include "CkProceduralAnimation/Action/CkProceduralAction_Data.h"

#include "CkProceduralAnimation/Core/CkProceduralActionValidation.h"

#include "Engine/AssetUserData.h"

#if WITH_EDITOR
#include <Misc/DataValidation.h>
#endif

#if WITH_EDITORONLY_DATA
TDelegate<void(UCk_ProceduralAction_Data&)> UCk_ProceduralAction_Data::OnMigrateEditorOnlyData;
#endif

// --------------------------------------------------------------------------------------------------------------------

#if WITH_EDITOR
auto
    UCk_ProceduralAction_Data::
    IsDataValid(FDataValidationContext& InContext) const
    -> EDataValidationResult
{
    auto Result = Super::IsDataValid(InContext);
    auto Compiled = ck::procedural_action::FCompiledClip{};
    auto Diagnostics = TArray<ck::procedural_action::FDiagnostic>{};
    if (NOT ck::procedural_action::Compile(*this, Compiled, Diagnostics))
    {
        for (const auto& Diagnostic : Diagnostics)
        {
            InContext.AddError(FText::FromString(FString::Printf(TEXT("%s [%s]: expected %s; got %s"),
                *Diagnostic.Get_FieldPath(), *Diagnostic.Get_Code().ToString(),
                *Diagnostic.Get_Expected(), *Diagnostic.Get_Actual())));
        }
        Result = EDataValidationResult::Invalid;
    }
    return Result == EDataValidationResult::Invalid ? Result : EDataValidationResult::Valid;
}
#endif

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_ProceduralAction_Data::
    PostLoad()
    -> void
{
    Super::PostLoad();

    for (auto& Slot : _Slots)
    {
        const auto* FirstTrack = static_cast<const FCk_ProceduralAction_LimbTrack*>(nullptr);
        for (const auto& Phase : _Phases)
        {
            FirstTrack = Phase.Get_LimbTracks().FindByPredicate([&Slot](const FCk_ProceduralAction_LimbTrack& InTrack)
                { return InTrack.Get_Slot() == Slot.Get_Id(); });
            if (FirstTrack != nullptr)
            { break; }
        }
        if (FirstTrack == nullptr)
        { continue; }

        if (FirstTrack->_Space_DEPRECATED != ECk_ProceduralAction_TrackSpace::BodyLocal)
        { Slot.Set_Space(FirstTrack->_Space_DEPRECATED); }
    }

    for (auto& Phase : _Phases)
    {
        for (auto& Track : Phase.Get_LimbTracks())
        { Track._Space_DEPRECATED = ECk_ProceduralAction_TrackSpace::BodyLocal; }
    }

    // An unbound migration (no editor module) leaves the legacy data and the version alone rather than drop data.
    auto LegacyEditorDataRemains = false;
#if WITH_EDITORONLY_DATA
    if (Get_HasLegacyEditorData())
    { OnMigrateEditorOnlyData.ExecuteIfBound(*this); }
    LegacyEditorDataRemains = Get_HasLegacyEditorData();
#endif
    if (NOT LegacyEditorDataRemains)
    { _CompatibilityVersion = CurrentCompatibilityVersion; }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_ProceduralAction_Data::
    AddAssetUserData(
        UAssetUserData* InUserData)
    -> void
{
#if WITH_EDITORONLY_DATA
    if (InUserData == nullptr)
    { return; }
    RemoveUserDataOfClass(InUserData->GetClass());
    _AssetUserDataEditorOnly.Add(InUserData);
#endif
}

auto
    UCk_ProceduralAction_Data::
    RemoveUserDataOfClass(
        TSubclassOf<UAssetUserData> InUserDataClass)
    -> void
{
#if WITH_EDITORONLY_DATA
    _AssetUserDataEditorOnly.RemoveAll([&InUserDataClass](const TObjectPtr<UAssetUserData>& InDatum)
        { return InDatum != nullptr && InDatum->IsA(InUserDataClass); });
#endif
}

auto
    UCk_ProceduralAction_Data::
    GetAssetUserDataOfClass(
        TSubclassOf<UAssetUserData> InUserDataClass)
    -> UAssetUserData*
{
#if WITH_EDITORONLY_DATA
    for (const auto& Datum : _AssetUserDataEditorOnly)
    {
        if (Datum != nullptr && Datum->IsA(InUserDataClass))
        { return Datum; }
    }
#endif
    return nullptr;
}

auto
    UCk_ProceduralAction_Data::
    GetAssetUserDataArray() const
    -> const TArray<UAssetUserData*>*
{
#if WITH_EDITORONLY_DATA
    return &ToRawPtrTArrayUnsafe(_AssetUserDataEditorOnly);
#else
    return nullptr;
#endif
}

// --------------------------------------------------------------------------------------------------------------------
