#pragma once

#include "CkPmg_Fragment_Data_Polyline.h"

#include "CkEcs/Tag/CkTag.h"

#include <ProceduralMeshComponent.h>

// --------------------------------------------------------------------------------------------------------------------

class UCk_Utils_Pmg_Polyline_UE;

// --------------------------------------------------------------------------------------------------------------------

namespace ck
{
    CK_DEFINE_ECS_TAG(FTag_Pmg_Polyline_NeedsSetup);

    // Present while a polyline's material preload batch is still loading (Setup / HandleRequests
    // re-poll, keeping their gating fragment). Observability only — nothing gates on it.
    CK_DEFINE_ECS_TAG(FTag_Pmg_Polyline_PendingAssetLoad);

    // --------------------------------------------------------------------------------------------------------------------

    using FFragment_Pmg_Polyline_Params = FCk_Pmg_Polyline_Spec;

    // --------------------------------------------------------------------------------------------------------------------

    struct CKPMG_API FFragment_Pmg_Polyline
    {
    public:
        CK_GENERATED_BODY(FFragment_Pmg_Polyline);

    public:
        friend class FProcessor_Pmg_Polyline_Setup;
        friend class FProcessor_Pmg_Polyline_HandleRequests;
        friend class FProcessor_Pmg_Polyline_EndPlay;
        friend class UCk_Utils_Pmg_Polyline_UE;

    private:
        // Lifetime owned by the CkCore ObjectPooling subsystem (DestroyOnRelease)
        TWeakObjectPtr<UProceduralMeshComponent> _MeshComponent;

        float _Radius = 1.0f;
        TObjectPtr<UMaterialInterface> _Material;

        // Roots the resolved material for the fragment's lifetime — GC does not trace fragments,
        // and _Material above is only otherwise rooted once the pooled mesh component applies it.
        FCk_ResourceLoader_RootedAssetBatch _MaterialPreloadBatch;

        // The last applied SetPoints, kept so a SetRadius can re-tube the same path
        TArray<FVector> _WorldPoints;
        int32 _NumDrawnPoints = 0;
        double _DrawnLength = 0.0;

    public:
        CK_PROPERTY_GET(_MeshComponent);
        CK_PROPERTY_GET(_Radius);
        CK_PROPERTY_GET(_Material);
        CK_PROPERTY_GET(_WorldPoints);
        CK_PROPERTY_GET(_NumDrawnPoints);
        CK_PROPERTY_GET(_DrawnLength);
    };

    // --------------------------------------------------------------------------------------------------------------------

    struct CKPMG_API FFragment_Pmg_Polyline_Requests
    {
    public:
        CK_GENERATED_BODY(FFragment_Pmg_Polyline_Requests);

    public:
        friend class FProcessor_Pmg_Polyline_HandleRequests;
        friend class FProcessor_Pmg_Polyline_CancelPendingRequests;
        friend class UCk_Utils_Pmg_Polyline_UE;

    private:
        TArray<FCk_Request_Pmg_Polyline_SetMaterial> _SetMaterialRequests;
        TArray<FCk_Request_Pmg_Polyline_SetRadius> _SetRadiusRequests;
        TArray<FCk_Request_Pmg_Polyline_SetPoints> _SetPointsRequests;

    public:
        CK_PROPERTY_GET(_SetMaterialRequests);
        CK_PROPERTY_GET(_SetRadiusRequests);
        CK_PROPERTY_GET(_SetPointsRequests);
    };
}

// --------------------------------------------------------------------------------------------------------------------
