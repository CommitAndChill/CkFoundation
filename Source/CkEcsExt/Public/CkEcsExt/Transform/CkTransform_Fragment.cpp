#include "CkTransform_Fragment.h"

#include "CkTransform_Utils.h"

#include "CkEcs/Persistence/CkPersistenceHandlerRegistry.h"
#include "CkEcs/Persistence/CkPersistenceHandlerRegistry.inl.h" // Register_* entry-point bodies

// --------------------------------------------------------------------------------------------------------------------

namespace ck
{
    FFragment_Transform_MeshSocket::
        FFragment_Transform_MeshSocket(
            const UMeshComponent* InComponent,
            FName InSocket)
        : _Component(InComponent)
        , _Socket(InSocket)
    {
    }
}

// --------------------------------------------------------------------------------------------------------------------

static struct FTransformRepHandlerRegistrar
{
    FTransformRepHandlerRegistrar()
    {
        // Transform is composed pre-link, so the NotReady branches below are violation detectors, not an
        // expected wait. An unset Old means first application: snap directly (an interpolation offset
        // would glide the entity in from origin).

        FCk_PersistenceHandlerRegistry::Register_NetOnly<FCk_RepData_Location>({ .Posture = ECk_Snapshot_Posture::Session, .NetApply =
                [](FCk_Handle& Entity, const FInstancedStruct& New, const TOptional<FInstancedStruct>& Old) -> ECk_Persistence_ApplyResult
                {
                    auto HandleTransform = UCk_Utils_Transform_UE::Cast(Entity);
                    if (ck::Is_NOT_Valid(HandleTransform))
                    { return ECk_Persistence_ApplyResult::NotReady; }

                    const auto& Location = New.Get<FCk_RepData_Location>().Value;
                    const auto Snap = NOT Old.IsSet();

                    if (UCk_Utils_Transform_UE::Get_IsReplicatedPoseYieldingToParent(HandleTransform))
                    {
                        UCk_Utils_Transform_UE::Request_MarkReplicatedPosePending(HandleTransform, ECk_TransformComponents::Location,
                            Snap ? ECk_TransformComponents::Location : ECk_TransformComponents::None);
                        return ECk_Persistence_ApplyResult::Applied;
                    }

                    UCk_Utils_Transform_UE::Apply_ReplicatedLocation(HandleTransform, Location, Snap);
                    return ECk_Persistence_ApplyResult::Applied;
                } });

        FCk_PersistenceHandlerRegistry::Register_NetOnly<FCk_RepData_Rotation>({ .Posture = ECk_Snapshot_Posture::Session, .NetApply =
                [](FCk_Handle& Entity, const FInstancedStruct& New, const TOptional<FInstancedStruct>& Old) -> ECk_Persistence_ApplyResult
                {
                    auto HandleTransform = UCk_Utils_Transform_UE::Cast(Entity);
                    if (ck::Is_NOT_Valid(HandleTransform))
                    { return ECk_Persistence_ApplyResult::NotReady; }

                    const auto& Rotation = New.Get<FCk_RepData_Rotation>().Value;
                    const auto Snap = NOT Old.IsSet();

                    if (UCk_Utils_Transform_UE::Get_IsReplicatedPoseYieldingToParent(HandleTransform))
                    {
                        UCk_Utils_Transform_UE::Request_MarkReplicatedPosePending(HandleTransform, ECk_TransformComponents::Rotation,
                            Snap ? ECk_TransformComponents::Rotation : ECk_TransformComponents::None);
                        return ECk_Persistence_ApplyResult::Applied;
                    }

                    UCk_Utils_Transform_UE::Apply_ReplicatedRotation(HandleTransform, Rotation, Snap);
                    return ECk_Persistence_ApplyResult::Applied;
                } });

        FCk_PersistenceHandlerRegistry::Register_NetOnly<FCk_RepData_Scale>({ .Posture = ECk_Snapshot_Posture::Session, .NetApply =
                [](FCk_Handle& Entity, const FInstancedStruct& New, const TOptional<FInstancedStruct>& Old) -> ECk_Persistence_ApplyResult
                {
                    auto HandleTransform = UCk_Utils_Transform_UE::Cast(Entity);
                    if (ck::Is_NOT_Valid(HandleTransform))
                    { return ECk_Persistence_ApplyResult::NotReady; }

                    if (UCk_Utils_Transform_UE::Get_IsReplicatedPoseYieldingToParent(HandleTransform))
                    {
                        UCk_Utils_Transform_UE::Request_MarkReplicatedPosePending(HandleTransform, ECk_TransformComponents::Scale,
                            Old.IsSet() ? ECk_TransformComponents::None : ECk_TransformComponents::Scale);
                        return ECk_Persistence_ApplyResult::Applied;
                    }

                    UCk_Utils_Transform_UE::Apply_ReplicatedScale(HandleTransform, New.Get<FCk_RepData_Scale>().Value);
                    return ECk_Persistence_ApplyResult::Applied;
                } });
    }
} GTransformRepHandlerRegistrar;

// --------------------------------------------------------------------------------------------------------------------