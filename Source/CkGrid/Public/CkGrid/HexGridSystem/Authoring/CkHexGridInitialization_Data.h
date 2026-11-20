#pragma once

#include "CkCore/Macros/CkMacros.h"
#include <GameplayTagContainer.h>

#include "CkHexGridInitialization_Data.generated.h"

struct FCk_Handle;
namespace ck::persistence { struct FInitializationIdentity; }

UENUM(BlueprintType)
enum class ECk_HexGridInitialization : uint8
{
    Automatic,
    Fresh,
    ExistingSaveKey,
    PendingOwnedLabel,
    Player
};

// Construction input for externally authored durable entities. The owner of a labeled
// child comes from its live lifetime relationship and is never serialized into spawn parameters.
USTRUCT(BlueprintType)
struct CKGRID_API FCk_HexGridInitialization
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_HexGridInitialization);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save Identity", meta = (AllowPrivateAccess = true))
    ECk_HexGridInitialization _Kind = ECk_HexGridInitialization::Automatic;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save Identity", meta = (AllowPrivateAccess = true,
        EditCondition = "_Kind == ECk_HexGridInitialization::ExistingSaveKey", EditConditionHides))
    FGuid _SaveKey;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save Identity", meta = (AllowPrivateAccess = true,
        EditCondition = "_Kind == ECk_HexGridInitialization::PendingOwnedLabel", EditConditionHides))
    FGameplayTag _Label;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save Identity", meta = (AllowPrivateAccess = true,
        EditCondition = "_Kind == ECk_HexGridInitialization::PendingOwnedLabel", EditConditionHides, ClampMin = "0"))
    int32 _Occurrence = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save Identity", meta = (AllowPrivateAccess = true,
        EditCondition = "_Kind == ECk_HexGridInitialization::Player", EditConditionHides))
    FString _PlayerId;

public:
    CK_PROPERTY(_Kind);
    CK_PROPERTY(_SaveKey);
    CK_PROPERTY(_Label);
    CK_PROPERTY(_Occurrence);
    CK_PROPERTY(_PlayerId);

    auto TryResolve(const FCk_Handle& InEntity, ck::persistence::FInitializationIdentity& OutIdentity) const -> bool;
};
