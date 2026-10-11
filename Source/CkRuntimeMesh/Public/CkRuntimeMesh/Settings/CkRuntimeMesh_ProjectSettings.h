#pragma once

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Time/CkTime.h"

#include "CkSettings/ProjectSettings/CkProjectSettings.h"

#include "Kismet/BlueprintFunctionLibrary.h"

#include "CkRuntimeMesh_ProjectSettings.generated.h"

// --------------------------------------------------------------------------------------------------------------------

UCLASS(meta = (DisplayName = "Runtime Mesh"))
class CKRUNTIMEMESH_API UCk_RuntimeMesh_ProjectSettings_UE : public UCk_Plugin_ProjectSettings_UE
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_RuntimeMesh_ProjectSettings_UE);

private:
    // Wall-clock time one drain of the world's slice queue may spend per tick, callbacks included. The oldest queued
    // slice always runs, so the queue makes progress under any budget; each further slice starts only while the time
    // spent so far is under the budget. The default keeps two ~768-triangle cuts (3.3-4.2 ms each) per tick.
    UPROPERTY(Config, EditDefaultsOnly, Category = "Slicing",
              meta = (AllowPrivateAccess = true))
    FCk_Time _DrainTimeBudget = FCk_Time{0.005};

    // Hard cap on slices one drain executes, whatever the time budget. The default 2 keeps the drain's original
    // two-per-tick pacing for small meshes (consumers and their tests rely on a queued slice waiting a few frames);
    // raise it to let the time budget alone decide how many cheap slices one tick runs.
    UPROPERTY(Config, EditDefaultsOnly, Category = "Slicing",
              meta = (AllowPrivateAccess = true, ClampMin = "1", UIMin = "1", ClampMax = "16", UIMax = "16"))
    int32 _MaximumSlicesPerDrain = 2;

public:
    CK_PROPERTY(_DrainTimeBudget);
    CK_PROPERTY(_MaximumSlicesPerDrain);
};

// --------------------------------------------------------------------------------------------------------------------

UCLASS()
class CKRUNTIMEMESH_API UCk_Utils_RuntimeMesh_Settings_UE : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_Utils_RuntimeMesh_Settings_UE);

public:
    static const UCk_RuntimeMesh_ProjectSettings_UE*
    Get();

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|RuntimeMesh|Settings")
    static FCk_Time
    Get_DrainTimeBudget();

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|RuntimeMesh|Settings")
    static int32
    Get_MaximumSlicesPerDrain();
};

// --------------------------------------------------------------------------------------------------------------------
