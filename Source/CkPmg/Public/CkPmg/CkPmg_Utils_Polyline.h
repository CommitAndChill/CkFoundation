#pragma once

#include "CkPmg_Fragment_Data_Polyline.h"

#include "CkEcs/Handle/CkHandle.h"

#include "CkPmg_Utils_Polyline.generated.h"

// --------------------------------------------------------------------------------------------------------------------

// A tube swept along world-space points, carrying a material. The topology is built once for the Spec's
// MaxPoints and Sides; every SetPoints rewrites the vertex buffer only. Not a debug shape: it has no
// Duration and lives until its entity is destroyed.
UCLASS(Meta = (ScriptMixin = "FCk_Handle_Pmg_Polyline"))
class CKPMG_API UCk_Utils_Pmg_Polyline_UE : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_Utils_Pmg_Polyline_UE);
    CK_DEFINE_CPP_CASTCHECKED_TYPESAFE(FCk_Handle_Pmg_Polyline);

public:
    // The shape on InHandle
    UFUNCTION(BlueprintCallable,
              Category = "Ck|Utils|Pmg|Polyline",
              DisplayName="[Ck][Pmg][Polyline] Add")
    static FCk_Handle_Pmg_Polyline
    Add(
        UPARAM(ref) FCk_Handle& InHandle,
        const FCk_Pmg_Polyline_Spec& InSpec);

    // The shape on a new child entity of InOwner
    UFUNCTION(BlueprintCallable,
              Category = "Ck|Utils|Pmg|Polyline",
              DisplayName="[Ck][Pmg][Polyline] Create")
    static FCk_Handle_Pmg_Polyline
    Create(
        UPARAM(ref) FCk_Handle& InOwner,
        const FCk_Pmg_Polyline_Spec& InSpec);

public:
    UFUNCTION(BlueprintPure,
              Category = "Ck|Utils|Pmg|Polyline",
              DisplayName="[Ck][Pmg][Polyline] Has Feature")
    static bool
    Has(
        const FCk_Handle& InHandle);

private:
    UFUNCTION(BlueprintCallable,
        Category = "Ck|Utils|Pmg|Polyline",
        DisplayName="[Ck][Pmg][Polyline] Cast",
        meta = (ExpandEnumAsExecs = "OutResult"))
    static FCk_Handle_Pmg_Polyline
    DoCast(
        UPARAM(ref) FCk_Handle& InHandle,
        ECk_SucceededFailed& OutResult);

    UFUNCTION(BlueprintPure,
        Category = "Ck|Utils|Pmg|Polyline",
        DisplayName="[Ck][Pmg][Polyline] Handle -> Pmg Polyline Handle",
        meta = (CompactNodeTitle = "<AsPmgPolyline>", BlueprintAutocast))
    static FCk_Handle_Pmg_Polyline
    DoCastChecked(
        FCk_Handle InHandle);

    UFUNCTION(BlueprintPure,
        DisplayName = "[Ck] Get Invalid Pmg Polyline Handle",
        Category = "Ck|Utils|Pmg|Polyline",
        meta = (CompactNodeTitle = "INVALID_PmgPolylineHandle", Keywords = "make"))
    static FCk_Handle_Pmg_Polyline
    Get_InvalidHandle() { return {}; };

public:
    // Replaces the drawn path. The last SetPoints of a frame wins; it is drawn in FGroup_PostTransform.
    UFUNCTION(BlueprintCallable,
              Category = "Ck|Utils|Pmg|Polyline",
              DisplayName="[Ck][Pmg][Polyline] Request Set Points")
    static FCk_Handle_Pmg_Polyline
    Request_SetPoints(
        UPARAM(ref) FCk_Handle_Pmg_Polyline& InPolyline,
        const FCk_Request_Pmg_Polyline_SetPoints& InRequest);

    UFUNCTION(BlueprintCallable,
              Category = "Ck|Utils|Pmg|Polyline",
              DisplayName="[Ck][Pmg][Polyline] Request Set Radius")
    static FCk_Handle_Pmg_Polyline
    Request_SetRadius(
        UPARAM(ref) FCk_Handle_Pmg_Polyline& InPolyline,
        const FCk_Request_Pmg_Polyline_SetRadius& InRequest);

    UFUNCTION(BlueprintCallable,
              Category = "Ck|Utils|Pmg|Polyline",
              DisplayName="[Ck][Pmg][Polyline] Request Set Material")
    static FCk_Handle_Pmg_Polyline
    Request_SetMaterial(
        UPARAM(ref) FCk_Handle_Pmg_Polyline& InPolyline,
        const FCk_Request_Pmg_Polyline_SetMaterial& InRequest);

public:
    // Points drawn after the last applied SetPoints; 0 while nothing is drawn
    UFUNCTION(BlueprintPure,
              Category = "Ck|Utils|Pmg|Polyline",
              DisplayName="[Ck][Pmg][Polyline] Get Num Drawn Points")
    static int32
    Get_NumDrawnPoints(
        const FCk_Handle_Pmg_Polyline& InPolyline);

    // Length of the drawn path in cm
    UFUNCTION(BlueprintPure,
              Category = "Ck|Utils|Pmg|Polyline",
              DisplayName="[Ck][Pmg][Polyline] Get Drawn Length")
    static float
    Get_DrawnLength(
        const FCk_Handle_Pmg_Polyline& InPolyline);

    UFUNCTION(BlueprintPure,
              Category = "Ck|Utils|Pmg|Polyline",
              DisplayName="[Ck][Pmg][Polyline] Get Radius")
    static float
    Get_Radius(
        const FCk_Handle_Pmg_Polyline& InPolyline);
};

// --------------------------------------------------------------------------------------------------------------------
