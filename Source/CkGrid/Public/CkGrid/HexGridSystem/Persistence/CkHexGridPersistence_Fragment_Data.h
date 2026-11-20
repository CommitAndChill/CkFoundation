#pragma once

#include "CkGrid/HexGridSystem/Object/CkHexGridObject_Fragment_Data.h"

#include "CkHexGridPersistence_Fragment_Data.generated.h"

USTRUCT()
struct CKGRID_API FCk_HexGridRecipeIdentity
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_HexGridRecipeIdentity);

private:
    UPROPERTY()
    FString _AssetPath;

    UPROPERTY()
    int32 _RecipeRevision = 0;

    UPROPERTY()
    FString _Fingerprint;

public:
    CK_PROPERTY(_AssetPath);
    CK_PROPERTY(_RecipeRevision);
    CK_PROPERTY(_Fingerprint);
};

USTRUCT()
struct CKGRID_API FCk_HexGridPlacement_PersistedEntry
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_HexGridPlacement_PersistedEntry);

private:
    UPROPERTY()
    FGuid _PlacementId;

    UPROPERTY()
    bool _HasOccupantReference = false;

    UPROPERTY()
    FGuid _OccupantBindingId;

    UPROPERTY()
    FCk_Handle _Occupant;

    UPROPERTY()
    TArray<FCk_HexCoord> _FootprintOffsets;

    UPROPERTY()
    FGameplayTagContainer _RequiredCellTags;

    UPROPERTY()
    FGameplayTagContainer _ForbiddenCellTags;

    UPROPERTY()
    FCk_HexCoord _Anchor;

    UPROPERTY()
    ECk_HexRotation _Rotation = ECk_HexRotation::Turn0;

    UPROPERTY()
    ECk_HexGridConnectivity _Connectivity = ECk_HexGridConnectivity::Ignore;

    UPROPERTY()
    TArray<FCk_HexCoord> _Cells;

public:
    CK_PROPERTY(_PlacementId);
    CK_PROPERTY(_HasOccupantReference);
    CK_PROPERTY(_OccupantBindingId);
    CK_PROPERTY(_Occupant);
    CK_PROPERTY(_FootprintOffsets);
    CK_PROPERTY(_RequiredCellTags);
    CK_PROPERTY(_ForbiddenCellTags);
    CK_PROPERTY(_Anchor);
    CK_PROPERTY(_Rotation);
    CK_PROPERTY(_Connectivity);
    CK_PROPERTY(_Cells);
};

USTRUCT()
struct CKGRID_API FCk_HexGridBlocker_PersistedEntry
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_HexGridBlocker_PersistedEntry);

private:
    UPROPERTY()
    FGuid _BlockerId;

    UPROPERTY()
    FGameplayTag _Name;

    UPROPERTY()
    TArray<FCk_HexCoord> _Cells;

    UPROPERTY()
    bool _Active = false;

public:
    CK_PROPERTY(_BlockerId);
    CK_PROPERTY(_Name);
    CK_PROPERTY(_Cells);
    CK_PROPERTY(_Active);
};

USTRUCT()
struct CKGRID_API FCk_HexGridCellRuleDelta
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_HexGridCellRuleDelta);

private:
    UPROPERTY()
    FCk_HexCoord _Coordinate;

    UPROPERTY()
    FGameplayTagContainer _LocalTags;

    UPROPERTY()
    bool _ManualBlocked = false;

public:
    CK_PROPERTY(_Coordinate);
    CK_PROPERTY(_LocalTags);
    CK_PROPERTY(_ManualBlocked);
};

USTRUCT()
struct CKGRID_API FCk_RepData_HexGridState
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_RepData_HexGridState);

private:
    UPROPERTY()
    int32 _SchemaVersion = 1;

    UPROPERTY()
    FCk_HexGridRecipeIdentity _Recipe;

    UPROPERTY()
    TArray<FCk_HexGridPlacement_PersistedEntry> _Placements;

    UPROPERTY()
    TArray<FCk_HexGridBlocker_PersistedEntry> _Blockers;

    UPROPERTY()
    TArray<FCk_HexGridCellRuleDelta> _CellRules;

public:
    CK_PROPERTY(_SchemaVersion);
    CK_PROPERTY(_Recipe);
    CK_PROPERTY(_Placements);
    CK_PROPERTY(_Blockers);
    CK_PROPERTY(_CellRules);
};

USTRUCT()
struct CKGRID_API FCk_RepData_HexGridObjectIdentity
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_RepData_HexGridObjectIdentity);

private:
    UPROPERTY()
    int32 _SchemaVersion = 1;

    UPROPERTY()
    FGuid _BindingId;

public:
    CK_PROPERTY(_SchemaVersion);
    CK_PROPERTY(_BindingId);
};
