#pragma once

#include "CkRuntimeMesh/Display/CkRuntimeMeshDisplay_Fragment_Data.h"
#include "CkRuntimeMesh/Internal/CkRuntimeMesh_Geometry.h"

#include "CkEcs/Snapshot/CkSnapshot_Posture.h"
#include "CkResourceLoader/CkResourceLoader_Fragment_Data.h"

class UDynamicMeshComponent;
class UCk_Utils_RuntimeMeshDisplay_UE;

// --------------------------------------------------------------------------------------------------------------------

namespace ck
{
    CK_DEFINE_ECS_TAG(FTag_RuntimeMeshDisplay_NeedsSetup);

    // --------------------------------------------------------------------------------------------------------------------

    struct CKRUNTIMEMESH_API FFragment_RuntimeMeshDisplay : public FCk_Snapshot_Session
    {
    public:
        CK_GENERATED_BODY(FFragment_RuntimeMeshDisplay);

        friend class ::UCk_Utils_RuntimeMeshDisplay_UE;
        friend class FProcessor_RuntimeMeshDisplay_Setup;
        friend class FProcessor_RuntimeMeshDisplay_UpdateTransform;
        friend class FProcessor_RuntimeMeshDisplay_EndPlay;

    private:
        ECk_RuntimeMesh_SetupState _SetupState = ECk_RuntimeMesh_SetupState::Pending;
        ECk_RuntimeMeshDisplay_SetupFailure _SetupFailure = ECk_RuntimeMeshDisplay_SetupFailure::None;

        // WEAK - lifetime owned by the CkCore ObjectPooling subsystem (DestroyOnRelease)
        TWeakObjectPtr<UDynamicMeshComponent> _Component;

    public:
        CK_PROPERTY_GET(_SetupState);
        CK_PROPERTY_GET(_SetupFailure);
        CK_PROPERTY_GET(_Component);
    };

    // --------------------------------------------------------------------------------------------------------------------

    // Exists only while setup is pending; removing it releases the material batch root. Once Ready, the
    // component's material slots keep the materials alive.
    struct CKRUNTIMEMESH_API FFragment_RuntimeMeshDisplay_Setup : public FCk_Snapshot_Session
    {
    public:
        CK_GENERATED_BODY(FFragment_RuntimeMeshDisplay_Setup);

        friend class FProcessor_RuntimeMeshDisplay_Setup;

    private:
        runtimemesh::geometry::FValidatedGeometryPtr _Geometry;
        FCk_RuntimeMeshDisplay_Visuals _Visuals;
        FCk_ResourceLoader_RootedAssetBatch _Materials;

    public:
        CK_PROPERTY_GET(_Geometry);
        CK_PROPERTY_GET(_Visuals);
        CK_PROPERTY_GET(_Materials);

        CK_DEFINE_CONSTRUCTORS(FFragment_RuntimeMeshDisplay_Setup, _Geometry, _Visuals);
    };

    // --------------------------------------------------------------------------------------------------------------------

    struct CKRUNTIMEMESH_API FFragment_RuntimeMeshDisplay_Requests : public FCk_Snapshot_Session
    {
    public:
        CK_GENERATED_BODY(FFragment_RuntimeMeshDisplay_Requests);

    public:
        friend class FProcessor_RuntimeMeshDisplay_HandleRequests;
        friend class ::UCk_Utils_RuntimeMeshDisplay_UE;

    public:
        using RequestType = std::variant<FCk_Request_RuntimeMeshDisplay_SetCustomPrimitiveData>;
        using RequestList = TArray<RequestType>;

    private:
        RequestList _Requests;

    public:
        CK_PROPERTY_GET(_Requests);
    };
}
