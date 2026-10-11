#pragma once

#include "CoreMinimal.h"
#include "EditorReimportHandler.h"
#include "Factories/Factory.h"

#include "CkPixelSpriteEditor_Factory.generated.h"

/** Imports one XPM or one version-1 animation manifest into the same native asset. */
UCLASS()
class CKPIXELSPRITEEDITOR_API UCk_PixelSpriteFactory_UE : public UFactory, public FReimportHandler
{
    GENERATED_BODY()

public:
    UCk_PixelSpriteFactory_UE();

    virtual auto
    FactoryCanImport(const FString& InFilename) -> bool override;
    virtual auto
    FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName,
        EObjectFlags InFlags, const FString& InFilename, const TCHAR* InParms,
        FFeedbackContext* InWarn, bool& OutOperationCanceled) -> UObject* override;

    virtual auto
    CanReimport(UObject* InObject, TArray<FString>& OutFilenames) -> bool override;
    virtual auto
    SetReimportPaths(UObject* InObject, const TArray<FString>& InPaths) -> void override;
    virtual auto
    SetReimportPaths(UObject* InObject, const FString& InPath,
        const int32 InSourceIndex) -> void override;
    virtual auto
    Reimport(UObject* InObject) -> EReimportResult::Type override;
    virtual auto
    Reimport(UObject* InObject, int32 InSourceIndex) -> EReimportResult::Type override;

private:
    TMap<TWeakObjectPtr<UObject>, FString> _PendingRootPaths;
};
