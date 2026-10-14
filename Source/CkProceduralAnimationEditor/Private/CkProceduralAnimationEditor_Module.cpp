#include "Modules/ModuleManager.h"

#include "Asset/CkProceduralAction_AssetDefinition.h"
#include "CkProceduralAnimationEditor/CkProceduralActionEditorData.h"
#include "CkProceduralAnimationEditor/CkProceduralAction_Factory.h"

class FCkProceduralAnimationEditorModule final : public IModuleInterface
{
public:
    auto StartupModule() -> void override
    {
        UCk_ProceduralAction_AssetDefinition::StaticClass();
        UCk_ProceduralAction_Factory::StaticClass();
        UCk_ProceduralActionPreview_Factory::StaticClass();
        UCk_ProceduralAction_Data::OnMigrateEditorOnlyData.BindStatic(&UCk_Utils_ProceduralActionEditor_UE::MigrateLegacyEditorData);
    }

    auto ShutdownModule() -> void override
    { UCk_ProceduralAction_Data::OnMigrateEditorOnlyData.Unbind(); }
};

IMPLEMENT_MODULE(FCkProceduralAnimationEditorModule, CkProceduralAnimationEditor)
