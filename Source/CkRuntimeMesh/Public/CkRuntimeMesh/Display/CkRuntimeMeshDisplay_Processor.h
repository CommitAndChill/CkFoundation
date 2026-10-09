#pragma once

#include "CkRuntimeMesh/Display/CkRuntimeMeshDisplay_Fragment.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/Processor/CkProcessor.h"
#include "CkEcs/Scheduler/CkProcessorGroups.h"
#include "CkEcsExt/Transform/CkTransform_Fragment.h"

class UWorld;

// --------------------------------------------------------------------------------------------------------------------

namespace ck
{
    class CKRUNTIMEMESH_API FProcessor_RuntimeMeshDisplay_Setup : public ck_exp::TProcessor<
            FProcessor_RuntimeMeshDisplay_Setup,
            FCk_Handle_RuntimeMeshDisplay,
            TReadWrite<FFragment_RuntimeMeshDisplay>,
            TReadWrite<FFragment_RuntimeMeshDisplay_Setup>,
            FTag_RuntimeMeshDisplay_NeedsSetup,
            TExclude<FTag_DestroyEntity_Initiate>,
            CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_PostTransform;
        using MarkedDirtyBy = FTag_RuntimeMeshDisplay_NeedsSetup;

    public:
        using TProcessor::TProcessor;

    public:
        static auto
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            FFragment_RuntimeMeshDisplay& InDisplay,
            FFragment_RuntimeMeshDisplay_Setup& InSetup) -> void;
    };

    // --------------------------------------------------------------------------------------------------------------------

    class CKRUNTIMEMESH_API FProcessor_RuntimeMeshDisplay_UpdateTransform : public ck_exp::TProcessor<
            FProcessor_RuntimeMeshDisplay_UpdateTransform,
            FCk_Handle_RuntimeMeshDisplay,
            TReadOnly<FFragment_RuntimeMeshDisplay>,
            TReadOnly<FFragment_Transform>,
            FTag_Transform_Updated,
            TExclude<FTag_RuntimeMeshDisplay_NeedsSetup>,
            TExclude<FTag_DestroyEntity_Initiate>,
            CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_PostTransform;
        using RunAfter = TDepList<FProcessor_RuntimeMeshDisplay_Setup>;

    public:
        using TProcessor::TProcessor;

    public:
        static auto
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_RuntimeMeshDisplay& InDisplay,
            const FFragment_Transform& InTransform) -> void;
    };

    // --------------------------------------------------------------------------------------------------------------------

    class CKRUNTIMEMESH_API FProcessor_RuntimeMeshDisplay_EndPlay : public ck_exp::TProcessor<
            FProcessor_RuntimeMeshDisplay_EndPlay,
            FCk_Handle_RuntimeMeshDisplay,
            TReadWrite<FFragment_RuntimeMeshDisplay>,
            CK_IF_END_PLAY>
    {
    public:
        using Group = FGroup_EndPlay;

    public:
        using TProcessor::TProcessor;

    public:
        static auto
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            FFragment_RuntimeMeshDisplay& InDisplay) -> void;

        // Idempotent: also called for every display in a world that is being cleaned up.
        static auto
        Release(
            FCk_Handle InHandle,
            FFragment_RuntimeMeshDisplay& InDisplay) -> void;
    };

    // --------------------------------------------------------------------------------------------------------------------

    class CKRUNTIMEMESH_API FProcessor_RuntimeMeshDisplay_HandleRequests : public ck_exp::TProcessor<
            FProcessor_RuntimeMeshDisplay_HandleRequests,
            FCk_Handle_RuntimeMeshDisplay,
            TReadOnly<FFragment_RuntimeMeshDisplay>,
            TReadWrite<FFragment_RuntimeMeshDisplay_Requests>,
            TExclude<FTag_RuntimeMeshDisplay_NeedsSetup>,
            TExclude<FTag_DestroyEntity_Initiate>,
            CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_PostTransform;
        using RunAfter = TDepList<FProcessor_RuntimeMeshDisplay_Setup>;
        using MarkedDirtyBy = FFragment_RuntimeMeshDisplay_Requests;

    public:
        using TProcessor::TProcessor;

    public:
        static auto
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_RuntimeMeshDisplay& InDisplay,
            FFragment_RuntimeMeshDisplay_Requests& InRequestsComp) -> void;

    private:
        static auto
        DoHandleRequest(
            HandleType InHandle,
            const FFragment_RuntimeMeshDisplay& InDisplay,
            const FCk_Request_RuntimeMeshDisplay_SetCustomPrimitiveData& InRequest) -> ECk_Request_OperationResult;
    };

    // --------------------------------------------------------------------------------------------------------------------

    // HandleRequests excludes owners already tagged for destruction, so a destroyed display's still-queued requests
    // are never drained. This completes each of them with Failed_Cancelled so a caller awaiting completion terminates
    // instead of hanging. Request_SetCustomPrimitiveData refuses to enqueue once that tag is present, so nothing can be
    // queued behind this pass.
    class CKRUNTIMEMESH_API FProcessor_RuntimeMeshDisplay_CancelPendingRequests : public ck_exp::TProcessor<
            FProcessor_RuntimeMeshDisplay_CancelPendingRequests,
            FCk_Handle_RuntimeMeshDisplay,
            TReadOnly<FFragment_RuntimeMeshDisplay_Requests>,
            CK_IF_END_PLAY>
    {
    public:
        using Group = FGroup_EndPlay;
        using RunAfter = TDepList<FProcessor_RuntimeMeshDisplay_EndPlay>;

    public:
        using TProcessor::TProcessor;

    public:
        static auto
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_RuntimeMeshDisplay_Requests& InRequestsComp) -> void;
    };
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck::runtimemesh::display
{
    CKRUNTIMEMESH_API auto
    ReleaseWorld(
        UWorld* InWorld) -> void;

    // Builds a finite unit tangent frame for every triangle corner of the display's own copy. Missing or
    // degenerate UVs fall back to a normal-derived frame; no UV layer is invented.
    CKRUNTIMEMESH_API auto
    PrepareTangents(
        UE::Geometry::FDynamicMesh3& InDisplayCopy) -> bool;
}
