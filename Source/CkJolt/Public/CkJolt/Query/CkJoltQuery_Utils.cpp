#include "CkJoltQuery_Utils.h"

#include "CkCore/Ensure/CkEnsure.h"

#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"

#include "CkJolt/CkJoltShapeFactory.h"
#include "CkJolt/CkJolt_Log.h"
#include "CkJolt/CkJolt_Utils.h"
#include "CkJolt/Query/CkJoltBallistic_Math.h"
#include "CkJolt/Settings/CkJolt_ProjectSettings.h"
#include "CkJolt/Subsystem/CkJolt_Subsystem.h"

#include <Engine/World.h>
#include <ProfilingDebugging/CpuProfilerTrace.h>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/ShapeCast.h>

// --------------------------------------------------------------------------------------------------------------------

namespace ck_jolt_query_utils
{
    static auto Get_IsValidBodySelection(ECk_Jolt_QueryBodySelection InSelection) -> bool
    {
        return InSelection == ECk_Jolt_QueryBodySelection::Any ||
            InSelection == ECk_Jolt_QueryBodySelection::StaticNonSensor;
    }

    class FBodySelectionFilter final : public JPH::BodyFilter
    {
    public:
        explicit FBodySelectionFilter(ECk_Jolt_QueryBodySelection InSelection)
            : _Selection(InSelection)
        {}

        auto ShouldCollideLocked(const JPH::Body& InBody) const -> bool override
        {
            return _Selection == ECk_Jolt_QueryBodySelection::Any ||
                (InBody.GetMotionType() == JPH::EMotionType::Static && NOT InBody.IsSensor());
        }

    private:
        ECk_Jolt_QueryBodySelection _Selection;
    };

    struct FQueryContext
    {
        UWorld* _World = nullptr;
        UCk_Jolt_Subsystem* _JoltSubsystem = nullptr;
        TSharedPtr<JPH::PhysicsSystem> _PhysicsSystem;
    };

    static auto Get_QueryContext(const UObject* InWorldContextObject) -> FQueryContext
    {
        auto Context = FQueryContext{};

        if (ck::Is_NOT_Valid(InWorldContextObject))
        { return Context; }

        Context._World = InWorldContextObject->GetWorld();
        if (ck::Is_NOT_Valid(Context._World))
        { return Context; }

        Context._JoltSubsystem = Context._World->GetSubsystem<UCk_Jolt_Subsystem>();
        if (ck::Is_NOT_Valid(Context._JoltSubsystem))
        { return Context; }

        Context._PhysicsSystem = Context._JoltSubsystem->Get_PhysicsSystem().Pin();
        return Context;
    }

    // Registry-liveness check FIRST — no ensure on dead ids (mirrors the probe contact translation).
    static auto TryResolve_Entity(const FQueryContext& InContext, uint64 InUserData) -> FCk_Handle
    {
        const auto* EcsWorldSubsystem = InContext._World->GetSubsystem<UCk_EcsWorld_Subsystem_UE>();
        if (ck::Is_NOT_Valid(EcsWorldSubsystem))
        { return {}; }

        // UserData 0 = NO entity. Raw entity id 0 is always the registry's live transient root — resolving
        // it would return the transient root as a spurious result.
        if (InUserData == 0)
        { return {}; }

        const auto TransientEntity = EcsWorldSubsystem->Get_TransientEntity();
        const auto RegView = TransientEntity.Get_RegistryView();

        const auto Entity = FCk_Entity{static_cast<FCk_Entity::IdType>(InUserData)};
        if (NOT RegView.IsValid(Entity))
        { return {}; }

        return TransientEntity.Get_ValidHandle(Entity.Get_ID());
    }

    static auto Fill_HitAttribution(
        const FQueryContext& InContext,
        const JPH::BodyID& InBodyId,
        FCk_Jolt_HitResult& InOutResult)
        -> void
    {
        const auto& BodyInterface = InContext._PhysicsSystem->GetBodyInterface();

        InOutResult.Set_Entity(TryResolve_Entity(InContext, BodyInterface.GetUserData(InBodyId)));
    }

    static auto Get_SurfaceNormal(
        const FQueryContext& InContext,
        const JPH::BodyID& InBodyId,
        const JPH::SubShapeID& InSubShapeId,
        const JPH::Vec3& InWorldPosition)
        -> FVector
    {
        const auto Lock = JPH::BodyLockRead{InContext._PhysicsSystem->GetBodyLockInterface(), InBodyId};

        if (NOT Lock.Succeeded())
        { return FVector::UpVector; }

        return ck::jolt::Conv(Lock.GetBody().GetWorldSpaceSurfaceNormal(InSubShapeId, InWorldPosition));
    }

    // A ballistic flight passes through sensors, and through every body owned by an ignored entity; ignored ids are
    // compared in the representation bodies are stamped with (the entity's raw id as user data).
    class FBallisticBodyFilter final : public JPH::BodyFilter
    {
    public:
        FBallisticBodyFilter(
            ECk_Jolt_QueryBodySelection InSelection,
            const TArray<FCk_Handle>& InIgnoredEntities)
            : _SelectionFilter(InSelection)
        {
            _IgnoredUserData.Reserve(InIgnoredEntities.Num());

            for (const auto& Entity : InIgnoredEntities)
            {
                if (ck::Is_NOT_Valid(Entity))
                { continue; }

                const auto UserData = static_cast<uint64>(Entity.Get_Entity().Get_ID());
                if (UserData == 0)
                { continue; }

                _IgnoredUserData.AddUnique(UserData);
            }
        }

        auto ShouldCollideLocked(const JPH::Body& InBody) const -> bool override
        {
            if (InBody.IsSensor())
            { return false; }

            if (NOT _SelectionFilter.ShouldCollideLocked(InBody))
            { return false; }

            const auto UserData = InBody.GetUserData();
            return UserData == 0 || NOT _IgnoredUserData.Contains(UserData);
        }

    private:
        FBodySelectionFilter _SelectionFilter;
        TArray<uint64> _IgnoredUserData;
    };
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_JoltQuery_UE::
    Get_RayCast(
        const UObject* InWorldContextObject,
        FVector InStart,
        FVector InEnd,
        FCk_Jolt_QueryFilter InFilter)
    -> FCk_Jolt_HitResult
{
    using namespace ck_jolt_query_utils;

    auto Result = FCk_Jolt_HitResult{};

    const auto SelectionValid = Get_IsValidBodySelection(InFilter.Get_BodySelection());
    CK_ENSURE(SelectionValid, TEXT("Jolt ray cast body selection is invalid"));
    if (NOT SelectionValid)
    { return Result; }

    const auto Context = Get_QueryContext(InWorldContextObject);
    if (ck::Is_NOT_Valid(Context._PhysicsSystem))
    { return Result; }

    const auto Ray = JPH::RRayCast{ck::jolt::Conv(InStart), ck::jolt::Conv(InEnd - InStart)};
    auto RayResult = JPH::RayCastResult{};

    const auto ChannelFilter = ck::jolt::FCk_Jolt_ChannelQueryFilter{
        Context._JoltSubsystem->Get_LayerTable(), InFilter.Get_Channel(), InFilter.Get_MinResponse()};
    const auto BodyFilter = FBodySelectionFilter{InFilter.Get_BodySelection()};

    // One event per executed query (invalid worlds above do not count).
    TRACE_CPUPROFILER_EVENT_SCOPE(Ck_JoltQuery_RayCast);
    if (NOT Context._PhysicsSystem->GetNarrowPhaseQuery().CastRay(Ray, RayResult,
        JPH::BroadPhaseLayerFilter{}, ChannelFilter, BodyFilter))
    { return Result; }

    const auto HitPosition = Ray.GetPointOnRay(RayResult.mFraction);

    Result.Set_HasHit(true);
    Result.Set_Position(ck::jolt::Conv(HitPosition));
    Result.Set_Fraction(RayResult.mFraction);
    Result.Set_Normal(Get_SurfaceNormal(Context, RayResult.mBodyID, RayResult.mSubShapeID2, HitPosition));
    Fill_HitAttribution(Context, RayResult.mBodyID, Result);

    return Result;
}

auto
    UCk_Utils_JoltQuery_UE::
    Get_ShapeCast(
        const UObject* InWorldContextObject,
        FVector InStart,
        FVector InEnd,
        FRotator InRotation,
        FCk_Jolt_ShapeDimensions InShape,
        FCk_Jolt_QueryFilter InFilter)
    -> FCk_Jolt_HitResult
{
    using namespace ck_jolt_query_utils;

    auto Result = FCk_Jolt_HitResult{};

    const auto SelectionValid = Get_IsValidBodySelection(InFilter.Get_BodySelection());
    CK_ENSURE(SelectionValid, TEXT("Jolt shape cast body selection is invalid"));
    if (NOT SelectionValid)
    { return Result; }

    const auto Context = Get_QueryContext(InWorldContextObject);
    if (ck::Is_NOT_Valid(Context._PhysicsSystem))
    { return Result; }

    const auto Shape = ck::jolt::CreateShape_FromDimensions(InShape, TEXT("JoltQuery_ShapeCast"));
    if (Shape == nullptr)
    { return Result; }

    const auto StartTransform = JPH::RMat44::sRotationTranslation(
        ck::jolt::Conv(InRotation.Quaternion()), ck::jolt::Conv(InStart));

    const auto ShapeCast = JPH::RShapeCast{
        Shape.GetPtr(), JPH::Vec3::sReplicate(1.0f), StartTransform, ck::jolt::Conv(InEnd - InStart)};

    const auto ChannelFilter = ck::jolt::FCk_Jolt_ChannelQueryFilter{
        Context._JoltSubsystem->Get_LayerTable(), InFilter.Get_Channel(), InFilter.Get_MinResponse()};
    const auto BodyFilter = FBodySelectionFilter{InFilter.Get_BodySelection()};

    auto Settings = JPH::ShapeCastSettings{};
    if (InFilter.Get_CollideWithBackFacesConvex())
    { Settings.mBackFaceModeConvex = JPH::EBackFaceMode::CollideWithBackFaces; }

    TRACE_CPUPROFILER_EVENT_SCOPE(Ck_JoltQuery_ShapeCast);
    if (InStart.Equals(InEnd, 0.0))
    {
        auto Overlaps = JPH::ClosestHitCollisionCollector<JPH::CollideShapeCollector>{};
        Context._PhysicsSystem->GetNarrowPhaseQuery().CollideShape(
            Shape.GetPtr(), JPH::Vec3::sReplicate(1.0f), StartTransform,
            JPH::CollideShapeSettings{}, JPH::RVec3::sZero(), Overlaps,
            JPH::BroadPhaseLayerFilter{}, ChannelFilter, BodyFilter);
        if (NOT Overlaps.HadHit())
        { return Result; }

        const auto& Hit = Overlaps.mHit;
        Result.Set_HasHit(true);
        Result.Set_Position(ck::jolt::Conv(Hit.mContactPointOn2));
        Result.Set_Fraction(0.0f);
        Result.Set_Normal(ck::jolt::Conv(-Hit.mPenetrationAxis.Normalized()));
        Fill_HitAttribution(Context, Hit.mBodyID2, Result);
        return Result;
    }

    auto Collector = JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector>{};
    Context._PhysicsSystem->GetNarrowPhaseQuery().CastShape(
        ShapeCast, Settings, JPH::RVec3::sZero(), Collector,
        JPH::BroadPhaseLayerFilter{}, ChannelFilter, BodyFilter);

    if (NOT Collector.HadHit())
    { return Result; }

    const auto& Hit = Collector.mHit;

    Result.Set_HasHit(true);
    Result.Set_Position(ck::jolt::Conv(Hit.mContactPointOn2));
    Result.Set_Fraction(Hit.mFraction);
    Result.Set_Normal(ck::jolt::Conv(-Hit.mPenetrationAxis.Normalized()));
    Fill_HitAttribution(Context, Hit.mBodyID2, Result);

    return Result;
}

auto
    UCk_Utils_JoltQuery_UE::
    Get_Overlap(
        const UObject* InWorldContextObject,
        FVector InLocation,
        FRotator InRotation,
        FCk_Jolt_ShapeDimensions InShape,
        FCk_Jolt_QueryFilter InFilter)
    -> TArray<FCk_Jolt_HitResult>
{
    using namespace ck_jolt_query_utils;

    auto Results = TArray<FCk_Jolt_HitResult>{};

    const auto SelectionValid = Get_IsValidBodySelection(InFilter.Get_BodySelection());
    CK_ENSURE(SelectionValid, TEXT("Jolt overlap body selection is invalid"));
    if (NOT SelectionValid)
    { return Results; }

    const auto Context = Get_QueryContext(InWorldContextObject);
    if (ck::Is_NOT_Valid(Context._PhysicsSystem))
    { return Results; }

    const auto Shape = ck::jolt::CreateShape_FromDimensions(InShape, TEXT("JoltQuery_Overlap"));
    if (Shape == nullptr)
    { return Results; }

    const auto Transform = JPH::RMat44::sRotationTranslation(
        ck::jolt::Conv(InRotation.Quaternion()), ck::jolt::Conv(InLocation));

    const auto ChannelFilter = ck::jolt::FCk_Jolt_ChannelQueryFilter{
        Context._JoltSubsystem->Get_LayerTable(), InFilter.Get_Channel(), InFilter.Get_MinResponse()};
    const auto BodyFilter = FBodySelectionFilter{InFilter.Get_BodySelection()};

    auto Collector = JPH::AllHitCollisionCollector<JPH::CollideShapeCollector>{};

    Context._PhysicsSystem->GetNarrowPhaseQuery().CollideShape(
        Shape.GetPtr(), JPH::Vec3::sReplicate(1.0f), Transform, JPH::CollideShapeSettings{},
        JPH::RVec3::sZero(), Collector, JPH::BroadPhaseLayerFilter{}, ChannelFilter, BodyFilter);

    Results.Reserve(Collector.mHits.size());

    for (const auto& Hit : Collector.mHits)
    {
        auto Result = FCk_Jolt_HitResult{};
        Result.Set_HasHit(true);
        Result.Set_Position(ck::jolt::Conv(Hit.mContactPointOn2));
        Result.Set_Normal(ck::jolt::Conv(-Hit.mPenetrationAxis.Normalized()));
        Fill_HitAttribution(Context, Hit.mBodyID2, Result);
        Results.Emplace(MoveTemp(Result));
    }

    return Results;
}

auto
    UCk_Utils_JoltQuery_UE::
    Get_RayCastMulti(
        const UObject* InWorldContextObject,
        FVector InStart,
        FVector InEnd,
        FCk_Jolt_QueryFilter InFilter)
    -> TArray<FCk_Jolt_HitResult>
{
    using namespace ck_jolt_query_utils;

    auto Results = TArray<FCk_Jolt_HitResult>{};

    const auto SelectionValid = Get_IsValidBodySelection(InFilter.Get_BodySelection());
    CK_ENSURE(SelectionValid, TEXT("Jolt multi ray cast body selection is invalid"));
    if (NOT SelectionValid)
    { return Results; }

    const auto Context = Get_QueryContext(InWorldContextObject);
    if (ck::Is_NOT_Valid(Context._PhysicsSystem))
    { return Results; }

    const auto Ray = JPH::RRayCast{ck::jolt::Conv(InStart), ck::jolt::Conv(InEnd - InStart)};

    const auto ChannelFilter = ck::jolt::FCk_Jolt_ChannelQueryFilter{
        Context._JoltSubsystem->Get_LayerTable(), InFilter.Get_Channel(), InFilter.Get_MinResponse()};
    const auto BodyFilter = FBodySelectionFilter{InFilter.Get_BodySelection()};

    auto Collector = JPH::AllHitCollisionCollector<JPH::CastRayCollector>{};

    Context._PhysicsSystem->GetNarrowPhaseQuery().CastRay(
        Ray, JPH::RayCastSettings{}, Collector, JPH::BroadPhaseLayerFilter{}, ChannelFilter, BodyFilter);

    Collector.Sort();

    Results.Reserve(Collector.mHits.size());

    for (const auto& Hit : Collector.mHits)
    {
        const auto HitPosition = Ray.GetPointOnRay(Hit.mFraction);

        auto Result = FCk_Jolt_HitResult{};
        Result.Set_HasHit(true);
        Result.Set_Position(ck::jolt::Conv(HitPosition));
        Result.Set_Fraction(Hit.mFraction);
        Result.Set_Normal(Get_SurfaceNormal(Context, Hit.mBodyID, Hit.mSubShapeID2, HitPosition));
        Fill_HitAttribution(Context, Hit.mBodyID, Result);
        Results.Emplace(MoveTemp(Result));
    }

    return Results;
}

auto
    UCk_Utils_JoltQuery_UE::
    Get_ShapeCastMulti(
        const UObject* InWorldContextObject,
        FVector InStart,
        FVector InEnd,
        FRotator InRotation,
        FCk_Jolt_ShapeDimensions InShape,
        FCk_Jolt_QueryFilter InFilter)
    -> TArray<FCk_Jolt_HitResult>
{
    using namespace ck_jolt_query_utils;

    auto Results = TArray<FCk_Jolt_HitResult>{};

    const auto SelectionValid = Get_IsValidBodySelection(InFilter.Get_BodySelection());
    CK_ENSURE(SelectionValid, TEXT("Jolt multi shape cast body selection is invalid"));
    if (NOT SelectionValid)
    { return Results; }

    const auto Context = Get_QueryContext(InWorldContextObject);
    if (ck::Is_NOT_Valid(Context._PhysicsSystem))
    { return Results; }

    const auto Shape = ck::jolt::CreateShape_FromDimensions(InShape, TEXT("JoltQuery_ShapeCastMulti"));
    if (Shape == nullptr)
    { return Results; }

    const auto StartTransform = JPH::RMat44::sRotationTranslation(
        ck::jolt::Conv(InRotation.Quaternion()), ck::jolt::Conv(InStart));

    const auto ShapeCast = JPH::RShapeCast{
        Shape.GetPtr(), JPH::Vec3::sReplicate(1.0f), StartTransform, ck::jolt::Conv(InEnd - InStart)};

    const auto ChannelFilter = ck::jolt::FCk_Jolt_ChannelQueryFilter{
        Context._JoltSubsystem->Get_LayerTable(), InFilter.Get_Channel(), InFilter.Get_MinResponse()};
    const auto BodyFilter = FBodySelectionFilter{InFilter.Get_BodySelection()};

    auto Settings = JPH::ShapeCastSettings{};
    if (InFilter.Get_CollideWithBackFacesConvex())
    { Settings.mBackFaceModeConvex = JPH::EBackFaceMode::CollideWithBackFaces; }

    auto Collector = JPH::AllHitCollisionCollector<JPH::CastShapeCollector>{};

    Context._PhysicsSystem->GetNarrowPhaseQuery().CastShape(
        ShapeCast, Settings, JPH::RVec3::sZero(), Collector,
        JPH::BroadPhaseLayerFilter{}, ChannelFilter, BodyFilter);

    Collector.Sort();

    Results.Reserve(Collector.mHits.size());

    for (const auto& Hit : Collector.mHits)
    {
        auto Result = FCk_Jolt_HitResult{};
        Result.Set_HasHit(true);
        Result.Set_Position(ck::jolt::Conv(Hit.mContactPointOn2));
        Result.Set_Fraction(Hit.mFraction);
        Result.Set_Normal(ck::jolt::Conv(-Hit.mPenetrationAxis.Normalized()));
        Fill_HitAttribution(Context, Hit.mBodyID2, Result);
        Results.Emplace(MoveTemp(Result));
    }

    return Results;
}

auto
    UCk_Utils_JoltQuery_UE::
    Get_OverlapEntities(
        const UObject* InWorldContextObject,
        FVector InLocation,
        FRotator InRotation,
        FCk_Jolt_ShapeDimensions InShape,
        FCk_Jolt_QueryFilter InFilter)
    -> TArray<FCk_Handle>
{
    auto Results = TArray<FCk_Handle>{};

    const auto Hits = Get_Overlap(InWorldContextObject, InLocation, InRotation, InShape, InFilter);

    for (const auto& Hit : Hits)
    {
        const auto Entity = Hit.Get_Entity();
        if (ck::Is_NOT_Valid(Entity))
        { continue; }

        Results.AddUnique(Entity);
    }

    return Results;
}

auto
    UCk_Utils_JoltQuery_UE::
    Get_BallisticCast(
        const UObject* InWorldContextObject,
        const FCk_Jolt_BallisticCast_Params& InParams)
    -> FCk_Jolt_BallisticCast_Result
{
    using namespace ck_jolt_query_utils;

    // Long enough for any throw; bounds the per-call cost of a careless duration.
    constexpr auto MaxBallisticDurationSeconds = 30.0;

    const auto& Filter = InParams.Get_Filter();

    const auto IsStartFinite = NOT InParams.Get_StartLocation().ContainsNaN();
    CK_ENSURE_IF_NOT(IsStartFinite, TEXT("Jolt ballistic cast start location [{}] is not finite"),
        InParams.Get_StartLocation().ToString())
    { return {}; }

    const auto IsVelocityFinite = NOT InParams.Get_StartVelocity().ContainsNaN();
    CK_ENSURE_IF_NOT(IsVelocityFinite, TEXT("Jolt ballistic cast start velocity [{}] is not finite"),
        InParams.Get_StartVelocity().ToString())
    { return {}; }

    const auto StepsPerSegment = InParams.Get_StepsPerSegment();
    CK_ENSURE_IF_NOT(StepsPerSegment >= 1, TEXT("Jolt ballistic cast needs at least one step per segment (got {})"),
        StepsPerSegment)
    { return {}; }

    const auto RequestedDuration = InParams.Get_MaxDuration().Get_Seconds();
    CK_ENSURE_IF_NOT(RequestedDuration > 0.0, TEXT("Jolt ballistic cast max duration must be positive (got {})"),
        InParams.Get_MaxDuration())
    { return {}; }

    CK_ENSURE_IF_NOT(Get_IsValidBodySelection(Filter.Get_BodySelection()),
        TEXT("Jolt ballistic cast body selection is invalid"))
    { return {}; }

    const auto Context = Get_QueryContext(InWorldContextObject);
    if (ck::Is_NOT_Valid(Context._PhysicsSystem))
    { return {}; }

    TRACE_CPUPROFILER_EVENT_SCOPE(Ck_JoltQuery_BallisticCast);

    const auto Shape = ck::jolt::CreateShape_FromDimensions(InParams.Get_Shape(), TEXT("JoltQuery_BallisticCast"));
    if (Shape == nullptr)
    { return {}; }

    // Mirrors the step pump and PhysicsSystem::Update: the fixed dt in single precision, split by the collision steps.
    const auto FixedDt = 1.0f / static_cast<float>(FMath::Max(1, UCk_Utils_Jolt_ProjectSettings::Get_FixedTimestepHz()));
    const auto StepDt = FixedDt / static_cast<float>(FMath::Max(1, UCk_Utils_Jolt_ProjectSettings::Get_CollisionSteps()));

    const auto Gravity = FVector3f{ck::jolt::Conv(Context._PhysicsSystem->GetGravity())};
    const auto Duration = FMath::Min(RequestedDuration, MaxBallisticDurationSeconds);
    const auto NumSteps = FMath::Max(1, FMath::FloorToInt32(Duration / static_cast<double>(StepDt)));

    const auto ShapeRotation = ck::jolt::Conv(InParams.Get_ShapeRotation().Quaternion());
    const auto ChannelFilter = ck::jolt::FCk_Jolt_ChannelQueryFilter{
        Context._JoltSubsystem->Get_LayerTable(), Filter.Get_Channel(), Filter.Get_MinResponse()};
    const auto BodyFilter = FBallisticBodyFilter{Filter.Get_BodySelection(), InParams.Get_IgnoredEntities()};

    auto Settings = JPH::ShapeCastSettings{};
    if (Filter.Get_CollideWithBackFacesConvex())
    { Settings.mBackFaceModeConvex = JPH::EBackFaceMode::CollideWithBackFaces; }

    const auto& NarrowPhase = Context._PhysicsSystem->GetNarrowPhaseQuery();

    auto Result = FCk_Jolt_BallisticCast_Result{};
    auto Path = TArray<FCk_Jolt_BallisticCast_Point>{};
    Path.Reserve(NumSteps / StepsPerSegment + 2);
    Path.Emplace(InParams.Get_StartLocation(), InParams.Get_StartVelocity(), FCk_Time::ZeroSecond());

    auto Location = InParams.Get_StartLocation();
    auto Velocity = FVector3f{InParams.Get_StartVelocity()};

    auto SegmentStartLocation = Location;
    auto SegmentStartVelocity = Velocity;
    auto StepsBeforeSegment = 0;

    for (auto StepsDone = 1; StepsDone <= NumSteps; ++StepsDone)
    {
        ck::jolt::ballistic::Advance_OneStep(Velocity, Location, InParams.Get_Flight(), Gravity, StepDt);

        const auto StepsInSegment = StepsDone - StepsBeforeSegment;
        if (StepsInSegment < StepsPerSegment && StepsDone < NumSteps)
        { continue; }

        const auto Chord = Location - SegmentStartLocation;
        if (Chord.SizeSquared() >= FMath::Square(UE_KINDA_SMALL_NUMBER))
        {
            const auto StartTransform = JPH::RMat44::sRotationTranslation(
                ShapeRotation, ck::jolt::Conv(SegmentStartLocation));
            const auto ShapeCast = JPH::RShapeCast{
                Shape.GetPtr(), JPH::Vec3::sReplicate(1.0f), StartTransform, ck::jolt::Conv(Chord)};

            auto Collector = JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector>{};
            NarrowPhase.CastShape(ShapeCast, Settings, JPH::RVec3::sZero(), Collector,
                JPH::BroadPhaseLayerFilter{}, ChannelFilter, BodyFilter);

            if (Collector.HadHit())
            {
                const auto& Hit = Collector.mHit;
                const auto Fraction = Hit.mFraction;

                auto HitResult = FCk_Jolt_HitResult{};
                HitResult.Set_HasHit(true);
                HitResult.Set_Position(ck::jolt::Conv(Hit.mContactPointOn2));
                HitResult.Set_Fraction(Fraction);
                HitResult.Set_Normal(ck::jolt::Conv(-Hit.mPenetrationAxis.Normalized()));
                Fill_HitAttribution(Context, Hit.mBodyID2, HitResult);

                const auto HitSteps = static_cast<double>(StepsBeforeSegment) +
                    static_cast<double>(Fraction) * static_cast<double>(StepsInSegment);

                Path.Emplace(
                    SegmentStartLocation + static_cast<double>(Fraction) * Chord,
                    FMath::Lerp(FVector{SegmentStartVelocity}, FVector{Velocity}, static_cast<double>(Fraction)),
                    FCk_Time{HitSteps * static_cast<double>(StepDt)});

                Result.Set_Path(Path);
                Result.Set_Hit(HitResult);
                return Result;
            }
        }

        Path.Emplace(Location, FVector{Velocity}, FCk_Time{static_cast<double>(StepsDone) * static_cast<double>(StepDt)});

        SegmentStartLocation = Location;
        SegmentStartVelocity = Velocity;
        StepsBeforeSegment = StepsDone;
    }

    Result.Set_Path(Path);
    return Result;
}

// --------------------------------------------------------------------------------------------------------------------
