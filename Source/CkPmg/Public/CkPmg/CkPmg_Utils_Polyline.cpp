#include "CkPmg_Utils_Polyline.h"

#include "CkPmg/CkPmg_Log.h"
#include "CkPmg_Fragment_Polyline.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Handle/CkHandle_Utils.h"

#include "CkResourceLoader/CkResourceLoader_Utils.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_pmg_utils_polyline
{
    // Consumer id — flip to Synchronous per-project in the ResourceLoader settings to debug.
    static const auto PreloadConsumerId = FName{TEXT("PmgPolyline.Material")};

    constexpr auto MinRadius = 0.01f;
    constexpr auto MinSides = 3;
    constexpr auto MaxSides = 16;
    constexpr auto MinPoints = 2;
    constexpr auto MaxPoints = 512;
}

// --------------------------------------------------------------------------------------------------------------------
// Pmg Polyline Utils
// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_Pmg_Polyline_UE::
    Add(
        FCk_Handle& InHandle,
        const FCk_Pmg_Polyline_Spec& InSpec)
    -> FCk_Handle_Pmg_Polyline
{
    ck::pmg::VeryVerbose(TEXT("Adding Pmg Polyline feature to Entity [{}]"), InHandle);

    const auto HandleIsValid = ck::IsValid(InHandle);
    CK_ENSURE_IF_NOT(HandleIsValid, TEXT("Cannot add a Pmg Polyline to an invalid handle"))
    { return {}; }

    const auto RadiusIsValid = InSpec.Get_Radius() >= ck_pmg_utils_polyline::MinRadius;
    CK_ENSURE_IF_NOT(RadiusIsValid,
        TEXT("Pmg Polyline radius [{}] must be at least [{}] cm for Entity [{}]"),
        InSpec.Get_Radius(), ck_pmg_utils_polyline::MinRadius, InHandle)
    { return {}; }

    const auto SidesAreValid = InSpec.Get_Sides() >= ck_pmg_utils_polyline::MinSides &&
                               InSpec.Get_Sides() <= ck_pmg_utils_polyline::MaxSides;
    CK_ENSURE_IF_NOT(SidesAreValid,
        TEXT("Pmg Polyline sides [{}] must be in [{}, {}] for Entity [{}]"),
        InSpec.Get_Sides(), ck_pmg_utils_polyline::MinSides, ck_pmg_utils_polyline::MaxSides, InHandle)
    { return {}; }

    const auto MaxPointsAreValid = InSpec.Get_MaxPoints() >= ck_pmg_utils_polyline::MinPoints &&
                                   InSpec.Get_MaxPoints() <= ck_pmg_utils_polyline::MaxPoints;
    CK_ENSURE_IF_NOT(MaxPointsAreValid,
        TEXT("Pmg Polyline max points [{}] must be in [{}, {}] for Entity [{}]"),
        InSpec.Get_MaxPoints(), ck_pmg_utils_polyline::MinPoints, ck_pmg_utils_polyline::MaxPoints, InHandle)
    { return {}; }

    const auto ArrowLengthIsValid = FMath::IsFinite(InSpec.Get_ArrowLength()) && InSpec.Get_ArrowLength() >= 0.0f;
    CK_ENSURE_IF_NOT(ArrowLengthIsValid,
        TEXT("Pmg Polyline arrow length [{}] must be 0 (no arrowhead) or positive for Entity [{}]"),
        InSpec.Get_ArrowLength(), InHandle)
    { return {}; }

    const auto ArrowRadiusIsValid = InSpec.Get_ArrowLength() <= 0.0f ||
                                    (FMath::IsFinite(InSpec.Get_ArrowRadius()) &&
                                     InSpec.Get_ArrowRadius() >= ck_pmg_utils_polyline::MinRadius);
    CK_ENSURE_IF_NOT(ArrowRadiusIsValid,
        TEXT("Pmg Polyline arrow radius [{}] must be at least [{}] cm when it draws an arrowhead, for Entity [{}]"),
        InSpec.Get_ArrowRadius(), ck_pmg_utils_polyline::MinRadius, InHandle)
    { return {}; }

    InHandle.Add<ck::FFragment_Pmg_Polyline_Params>(InSpec);

    auto& Polyline = InHandle.Add<ck::FFragment_Pmg_Polyline>();
    Polyline._Radius = InSpec.Get_Radius();

    InHandle.Add<ck::FTag_Pmg_Polyline_NeedsSetup>();

    // Kick the material preload at composition so the load overlaps the frames before Setup —
    // resident assets complete inline and Setup's gate falls through same-tick.
    if (ck::IsValid(InSpec.Get_Material()))
    {
        Polyline._MaterialPreloadBatch = UCk_Utils_ResourceLoader_UE::RequestLoad_RootedBatch(
            ck_pmg_utils_polyline::PreloadConsumerId, {InSpec.Get_Material().ToSoftObjectPath()});
    }

    UCk_Utils_Handle_UE::Set_DebugName(InHandle, TEXT("Pmg: Polyline"));

    return Cast(InHandle);
}

auto
    UCk_Utils_Pmg_Polyline_UE::
    Create(
        FCk_Handle& InOwner,
        const FCk_Pmg_Polyline_Spec& InSpec)
    -> FCk_Handle_Pmg_Polyline
{
    auto NewEntity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(InOwner);
    return Add(NewEntity, InSpec);
}

// --------------------------------------------------------------------------------------------------------------------

CK_DEFINE_HAS_CAST_CONV_HANDLE_TYPESAFE(UCk_Utils_Pmg_Polyline_UE, FCk_Handle_Pmg_Polyline,
    ck::FFragment_Pmg_Polyline)

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_Pmg_Polyline_UE::
    Request_SetPoints(
        FCk_Handle_Pmg_Polyline& InPolyline,
        const FCk_Request_Pmg_Polyline_SetPoints& InRequest)
    -> FCk_Handle_Pmg_Polyline
{
    InPolyline.AddOrGet<ck::FFragment_Pmg_Polyline_Requests>()._SetPointsRequests.Emplace(InRequest);
    return InPolyline;
}

auto
    UCk_Utils_Pmg_Polyline_UE::
    Request_SetRadius(
        FCk_Handle_Pmg_Polyline& InPolyline,
        const FCk_Request_Pmg_Polyline_SetRadius& InRequest)
    -> FCk_Handle_Pmg_Polyline
{
    const auto RadiusIsValid = InRequest.Get_Radius() >= ck_pmg_utils_polyline::MinRadius;
    CK_ENSURE_IF_NOT(RadiusIsValid,
        TEXT("Pmg Polyline [{}]: radius [{}] must be at least [{}] cm; the request is not enqueued"),
        InPolyline, InRequest.Get_Radius(), ck_pmg_utils_polyline::MinRadius)
    { return InPolyline; }

    InPolyline.AddOrGet<ck::FFragment_Pmg_Polyline_Requests>()._SetRadiusRequests.Emplace(InRequest);
    return InPolyline;
}

auto
    UCk_Utils_Pmg_Polyline_UE::
    Request_SetMaterial(
        FCk_Handle_Pmg_Polyline& InPolyline,
        const FCk_Request_Pmg_Polyline_SetMaterial& InRequest)
    -> FCk_Handle_Pmg_Polyline
{
    auto Request = InRequest;
    if (ck::IsValid(Request.Get_Material()))
    {
        Request.Set_PreloadBatch(UCk_Utils_ResourceLoader_UE::RequestLoad_RootedBatch(
            ck_pmg_utils_polyline::PreloadConsumerId, {Request.Get_Material().ToSoftObjectPath()}));
    }

    InPolyline.AddOrGet<ck::FFragment_Pmg_Polyline_Requests>()._SetMaterialRequests.Emplace(Request);
    return InPolyline;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_Pmg_Polyline_UE::
    Get_NumDrawnPoints(
        const FCk_Handle_Pmg_Polyline& InPolyline)
    -> int32
{
    CK_ENSURE_IF_NOT(ck::IsValid(InPolyline), TEXT("Invalid Pmg Polyline handle"))
    { return 0; }

    CK_ENSURE_IF_NOT(InPolyline.Has<ck::FFragment_Pmg_Polyline>(),
        TEXT("Pmg Polyline [{}] has no Polyline fragment"), InPolyline)
    { return 0; }

    return InPolyline.Get<ck::FFragment_Pmg_Polyline>().Get_NumDrawnPoints();
}

auto
    UCk_Utils_Pmg_Polyline_UE::
    Get_DrawnLength(
        const FCk_Handle_Pmg_Polyline& InPolyline)
    -> float
{
    CK_ENSURE_IF_NOT(ck::IsValid(InPolyline), TEXT("Invalid Pmg Polyline handle"))
    { return 0.0f; }

    CK_ENSURE_IF_NOT(InPolyline.Has<ck::FFragment_Pmg_Polyline>(),
        TEXT("Pmg Polyline [{}] has no Polyline fragment"), InPolyline)
    { return 0.0f; }

    return static_cast<float>(InPolyline.Get<ck::FFragment_Pmg_Polyline>().Get_DrawnLength());
}

auto
    UCk_Utils_Pmg_Polyline_UE::
    Get_Radius(
        const FCk_Handle_Pmg_Polyline& InPolyline)
    -> float
{
    CK_ENSURE_IF_NOT(ck::IsValid(InPolyline), TEXT("Invalid Pmg Polyline handle"))
    { return 0.0f; }

    CK_ENSURE_IF_NOT(InPolyline.Has<ck::FFragment_Pmg_Polyline>(),
        TEXT("Pmg Polyline [{}] has no Polyline fragment"), InPolyline)
    { return 0.0f; }

    return InPolyline.Get<ck::FFragment_Pmg_Polyline>().Get_Radius();
}

// --------------------------------------------------------------------------------------------------------------------
