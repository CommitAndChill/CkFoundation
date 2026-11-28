#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "CkUsf_GeneratorSubsystem.generated.h"

class UCkUsf_LookDefinition;
class UMaterialInterface;

USTRUCT(BlueprintType)
struct CKUSFEDITOR_API FCk_Usf_CompiledSubstrateStats
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "CkUsf")
    bool Valid = false;

    UPROPERTY(BlueprintReadOnly, Category = "CkUsf")
    int32 ClosureCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "CkUsf")
    int32 UintPerPixel = 0;

    UPROPERTY(BlueprintReadOnly, Category = "CkUsf")
    int32 MaterialType = 0;

    UPROPERTY(BlueprintReadOnly, Category = "CkUsf")
    int64 BsdfFeatures = 0;

    UPROPERTY(BlueprintReadOnly, Category = "CkUsf")
    bool HasGlint = false;

    UPROPERTY(BlueprintReadOnly, Category = "CkUsf")
    bool ComplexSpecial = false;

    UPROPERTY(BlueprintReadOnly, Category = "CkUsf")
    TArray<FString> CompileErrors;
};

UCLASS()
class CKUSFEDITOR_API UCkUsf_GeneratorSubsystem : public UEditorSubsystem
{
    GENERATED_BODY()

public:
    // Regenerates every look master. Also reachable via the `Ck_Usf_GenerateLooks` console command.
    UFUNCTION(CallInEditor, Category = "CkUsf",
              DisplayName = "Generate Look Materials")
    void Generate_LookMaterials();

    // Regenerates one look's master (scriptable per-look entry point â€” editor utilities, AS, Python).
    // Console equivalent: `Ck_Usf_GenerateLooks <LookName>`.
    UFUNCTION(BlueprintCallable, Category = "CkUsf",
              DisplayName = "[Ck][Usf] Generate Look Material For")
    void Generate_LookMaterial_For(UCkUsf_LookDefinition* InLook);

    // Reads the existing compiled game-thread resource; never loads, compiles or saves an asset.
    UFUNCTION(BlueprintCallable, Category = "CkUsf",
              DisplayName = "[Ck][Usf] Get Compiled Substrate Stats")
    FCk_Usf_CompiledSubstrateStats Get_CompiledSubstrateStats(TSoftObjectPtr<UMaterialInterface> InMaterial) const;
};
