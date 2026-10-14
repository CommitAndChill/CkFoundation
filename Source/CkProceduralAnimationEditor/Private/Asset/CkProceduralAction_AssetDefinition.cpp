#include "Asset/CkProceduralAction_AssetDefinition.h"

#include "CkProceduralAnimation/Action/CkProceduralAction_Data.h"

#include "Toolkit/CkProceduralAction_EditorToolkit.h"

#define LOCTEXT_NAMESPACE "CkProceduralActionAssetDefinition"

auto
    UCk_ProceduralAction_AssetDefinition::
    GetAssetDisplayName() const
    -> FText
{
    return LOCTEXT("DisplayName", "Procedural Action Clip");
}

auto
    UCk_ProceduralAction_AssetDefinition::
    GetAssetColor() const
    -> FLinearColor
{
    return FLinearColor{0.12f, 0.67f, 0.78f};
}

auto
    UCk_ProceduralAction_AssetDefinition::
    GetAssetClass() const
    -> TSoftClassPtr<UObject>
{
    return UCk_ProceduralAction_Data::StaticClass();
}

auto
    UCk_ProceduralAction_AssetDefinition::
    GetAssetCategories() const
    -> TConstArrayView<FAssetCategoryPath>
{
    static const FAssetCategoryPath Categories[] = {EAssetCategoryPaths::Animation};
    return Categories;
}

auto
    UCk_ProceduralAction_AssetDefinition::
    OpenAssets(
        const FAssetOpenArgs& InOpenArgs) const
    -> EAssetCommandResult
{
    auto OpenedAny = false;
    for (auto* Clip : InOpenArgs.LoadObjects<UCk_ProceduralAction_Data>())
    {
        auto Toolkit = MakeShared<FCk_ProceduralAction_EditorToolkit>();
        Toolkit->Init(InOpenArgs.GetToolkitMode(), InOpenArgs.ToolkitHost, Clip);
        OpenedAny = true;
    }
    return OpenedAny ? EAssetCommandResult::Handled : EAssetCommandResult::Unhandled;
}

#undef LOCTEXT_NAMESPACE
