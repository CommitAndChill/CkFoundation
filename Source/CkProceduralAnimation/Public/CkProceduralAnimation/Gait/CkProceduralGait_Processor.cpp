#include "CkProceduralAnimation/Gait/CkProceduralGait_Processor.h"
#include "CkProceduralAnimation/CkProceduralAnimation_Log.h"

#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Control.h"
#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Fragment.h"
#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Utils.h"
#include "CkProceduralAnimation/BodyPose/CkProceduralBodyPose_Fragment.h"
#include "CkProceduralAnimation/BodyPose/CkProceduralBodyPose_Utils.h"
#include "CkProceduralAnimation/Core/CkProceduralFeetPlane.h"
#include "CkProceduralAnimation/Gait/CkProceduralGait_Utils.h"
#include "CkProceduralAnimation/Gait/CkProceduralGaitParticipation_Control.h"
#include "CkProceduralAnimation/Leg/CkProceduralLeg_Fragment.h"
#include "CkProceduralAnimation/SurfaceMotion/CkSurfaceMotion_Fragment.h"
#include "CkProceduralAnimation/SurfaceMotion/CkSurfaceMotion_Utils.h"

#include "CkCore/Algorithms/CkAlgorithms.h"
#include "CkCore/Ensure/CkEnsure.h"
#include "CkCore/TypeTraits/CkTypeTraits.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "CkEcs/Handle/CkHandle_Utils.h"
#endif
#include "CkEcs/Request/CkRequest_Completion.h"
#include "CkEcs/Scheduler/CkProcessorRegistration.h"

#include "CkJolt/Query/CkJoltQuery_Utils.h"

#include <Engine/World.h>
#include <Misc/ScopeExit.h>
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include <CoreGlobals.h>
#include <HAL/CriticalSection.h>
#include <Misc/ScopeLock.h>
#endif
#include <cmath>
#include <limits>

CK_REGISTER_PROCESSOR(ck::FProcessor_ProceduralGait_Setup);
CK_REGISTER_PROCESSOR(ck::FProcessor_ProceduralGait_HandleRequests);
CK_REGISTER_PROCESSOR(ck::FProcessor_ProceduralGait_Update);
CK_REGISTER_PROCESSOR(ck::FProcessor_ProceduralGait_CancelPendingRequests);

// --------------------------------------------------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
namespace ck_procedural_gait_processor_hip_query_test
{
    struct FObservation
    {
        uint64 Words[40]{};
        int32 PointCount = 0;
        bool Enabled = false;
        bool OldSeen = false;
        bool Tilted = false;
        bool TiltedPoint = false;
    };

    struct FCounter
    {
        FCriticalSection Mutex;
        uint64 Key[4]{};
        uint32 Count = 0;
        bool Overflow = false;
    };

    static_assert(sizeof(FObservation) <= 352);
    static_assert(sizeof(FCounter) <= 256);

    auto
        Put(
            FObservation& InRow,
            int32 InSlot,
            uint64 InValue)
        -> void
    {
        InRow.Words[0] |= uint64{1} << (InSlot - 1);
        InRow.Words[InSlot] = InValue;
    }

    auto
        Vector(
            FObservation& InRow,
            int32 InSlot,
            const FVector& InValue)
        -> void
    {
        for (int32 Axis = 0; Axis < 3; ++Axis)
        {
            uint64 Bits = 0;
            const auto Value = InValue[Axis];
            FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
            Put(InRow, InSlot + Axis, Bits);
        }
    }

    auto
        Begin(
            const FCk_Handle& InRoot,
            const FCk_Handle& InLeg,
            int32 InLegIndex,
            UWorld* InWorld,
            uint64 InInputSolve,
            bool InStaging)
        -> FObservation
    {
        auto Row = FObservation{};
        if (NOT GIsAutomationTesting || NOT IsInGameThread())
        {
            return Row;
        }
        const auto Name = UCk_Utils_Handle_UE::Get_DebugName(InRoot);
        Row.Tilted = Name == FName{TEXT("CkAutomationTiltedSeed")};
        if (NOT Row.Tilted && Name != FName{TEXT("CkAutomationHipQuery")})
        {
            return Row;
        }
        Row.Enabled = true;
        const auto Registry = InRoot.Get_RegistryView().Get_RegistryHandle();
        Put(Row, 1, static_cast<uint64>(reinterpret_cast<UPTRINT>(InWorld)));
        Put(Row, 2, static_cast<uint32>(Registry.SlotIndex));
        Put(Row, 3, static_cast<uint32>(Registry.Generation));
        Put(Row, 4, static_cast<uint32>(InRoot.Get_Entity().Get_ID()));
        Put(Row, 5, static_cast<uint32>(InLeg.Get_Entity().Get_ID()));
        Put(Row, 6, static_cast<uint32>(InLegIndex));
        Put(Row, 7, GFrameCounter);
        Put(Row, 8, InInputSolve);
        Put(Row, 17, InStaging);
        Put(Row, 36, 1);
        Put(Row, 37, 0);
        return Row;
    }

    auto
        Counter()
        -> FCounter&
    {
        static auto Value = FCounter{};
        return Value;
    }

    auto
        Emit(
            FObservation& InRow)
        -> void
    {
        if (NOT InRow.Enabled || NOT InRow.OldSeen || (InRow.Tilted && NOT InRow.TiltedPoint))
        {
            return;
        }
        auto& State = Counter();
        const auto Lock = FScopeLock{&State.Mutex};
        if (State.Key[0] != InRow.Words[1] || State.Key[1] != InRow.Words[2]
            || State.Key[2] != InRow.Words[3] || State.Key[3] != InRow.Words[4])
        {
            for (int32 Index = 0; Index < 4; ++Index)
            {
                State.Key[Index] = InRow.Words[Index + 1];
            }
            State.Count = 0;
            State.Overflow = false;
        }
        if (State.Count == 512)
        {
            if (State.Overflow)
            {
                return;
            }
            State.Overflow = true;
            Put(InRow, 37, 1);
        }
        else
        {
            ++State.Count;
        }
        Put(InRow, 35, State.Overflow ? 513 : State.Count);
        TCHAR Hex[680]{};
        constexpr TCHAR Digits[] = TEXT("0123456789abcdef");
        for (int32 Word = 0; Word < 40; ++Word)
        {
            for (int32 Nibble = 0; Nibble < 16; ++Nibble)
            {
                Hex[17 * Word + Nibble] = Digits[(InRow.Words[Word] >> (4 * (15 - Nibble))) & 15];
            }
            Hex[17 * Word + 16] = Word == 39 ? TCHAR{0} : TCHAR{' '};
        }
        UE_LOG(LogTemp, Display, TEXT("%s %s"), InRow.Tilted ? TEXT("TQT1") : TEXT("HQT1"), Hex);
    }
}
#endif

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
namespace ck_procedural_height_seed_test
{
    struct FObservation
    {
        uint64 W[40]{};
        bool Enabled = false;
        bool Result = false;
    };
    struct FCounter
    {
        FCriticalSection Mutex;
        uint64 Key[4]{};
        uint32 Count = 0;
        bool Overflow = false;
    };
    static_assert(sizeof(FObservation) <= 336);
    static_assert(sizeof(FCounter) <= 256);
    auto
    Put(FObservation& R, int32 S, uint64 V) -> void
    {
        R.W[0] |= uint64{1} << (S - 1);
        R.W[S] = V;
    }
    auto
    Vector(FObservation& R, int32 S, const FVector& V) -> void
    {
        for (auto A = 0; A < 3; ++A)
        {
            uint64 Bits = 0;
            const auto Value = V[A];
            FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
            Put(R, S + A, Bits);
        }
    }
    auto
    Begin(const FCk_Handle& Root, const FCk_Handle& Leg, int32 Index, UWorld* World, uint64 Solve, uint64 Stage,
          int32 Rays, bool Requested) -> FObservation
    {
        auto R = FObservation{};
        if (NOT IsInGameThread() || NOT GIsAutomationTesting ||
            UCk_Utils_Handle_UE::Get_DebugName(Root) != FName{TEXT("CkAutomationHeightSeed")} ||
            Stage != static_cast<uint64>(ck::EProceduralHeightPlanStage::Landing))
        {
            return R;
        }
        R.Enabled = true;
        const auto Registry = Root.Get_RegistryView().Get_RegistryHandle();
        Put(R, 1, reinterpret_cast<UPTRINT>(World));
        Put(R, 2, static_cast<uint32>(Registry.SlotIndex));
        Put(R, 3, static_cast<uint32>(Registry.Generation));
        Put(R, 4, static_cast<uint32>(Root.Get_Entity().Get_ID()));
        Put(R, 5, static_cast<uint32>(Leg.Get_Entity().Get_ID()));
        Put(R, 6, Index);
        Put(R, 7, GFrameCounter);
        Put(R, 8, Solve);
        Put(R, 9, Stage);
        Put(R, 10, Rays);
        Put(R, 30, Requested);
        Put(R, 37, 1);
        Put(R, 38, 0);
        return R;
    }
    auto
    Finish(FObservation& R, int32 Rays, const ck::EProceduralHeightSeedFailure* Cause) -> void
    {
        if (NOT R.Enabled)
        {
            return;
        }
        Put(R, 11, Rays);
        Put(R, 29, R.Result);
        if (Cause != nullptr)
        {
            Put(R, 31, static_cast<uint64>(*Cause));
        }
        static auto Counter = FCounter{};
        const auto Lock = FScopeLock{&Counter.Mutex};
        if (Counter.Key[0] != R.W[1] || Counter.Key[1] != R.W[2] || Counter.Key[2] != R.W[3] ||
            Counter.Key[3] != R.W[4])
        {
            for (auto I = 0; I < 4; ++I)
            {
                Counter.Key[I] = R.W[I + 1];
            }
            Counter.Count = 0;
            Counter.Overflow = false;
        }
        if (Counter.Count == 64)
        {
            if (Counter.Overflow)
            {
                return;
            }
            Counter.Overflow = true;
            Put(R, 38, 1);
        }
        else
        {
            ++Counter.Count;
        }
        Put(R, 36, Counter.Overflow ? 65 : Counter.Count);
        TCHAR Hex[680]{};
        constexpr TCHAR Digits[] = TEXT("0123456789abcdef");
        for (auto I = 0; I < 40; ++I)
        {
            for (auto N = 0; N < 16; ++N)
            {
                Hex[17 * I + N] = Digits[(R.W[I] >> (4 * (15 - N))) & 15];
            }
            Hex[17 * I + 16] = I == 39 ? TCHAR{0} : TCHAR{' '};
        }
        UE_LOG(LogTemp, Display, TEXT("HST1 %s"), Hex);
    }
} // namespace ck_procedural_height_seed_test
#endif

namespace ck_procedural_gait
{
    auto
        Get_TrustedHit(
            const FCk_Jolt_HitResult& InHit,
            const FVector& InRayDirection)
        -> bool
    {
        // An inside-origin Jolt ray reports fraction zero at its origin, not a surface contact, and a normal facing
        // along the ray is the back of a surface the ray started behind. Reject both so the caller can retry from
        // outside the geometry.
        return InHit.Get_HasHit() && InHit.Get_Fraction() > 0.0f && InHit.Get_Fraction() <= 1.0f
            && NOT InHit.Get_Position().ContainsNaN() && NOT InHit.Get_Normal().ContainsNaN()
            && NOT InHit.Get_Normal().IsNearlyZero()
            && FVector::DotProduct(InHit.Get_Normal(), InRayDirection) < -KINDA_SMALL_NUMBER;
    }

    // How many of the three lengthening attempts a ray under a point may take. The ideal target and the floor in front of an
    // occluder take every one; the held foothold and the ring take the first, and the second only when the first began
    // inside a solid.
    enum class EGroundProbeAttempts : uint8
    {
        Every,
        RetryFromInsideSolid
    };

    // Returns the first trusted hit across the retry spans; the debug probe records the last attempt either way.
    auto
        Get_GroundHit(
            UWorld* InWorld,
            const FVector& InPoint,
            const FVector& InUp,
            const FVector& InRadial,
            const FCk_ProceduralGait_Probe& InProbe,
            EGroundProbeAttempts InAttempts,
            int32& InOutRayCount,
            FCk_ProceduralAnimation_DebugProbe* InDebugProbe)
        -> TOptional<FCk_Jolt_HitResult>
    {
        for (auto Attempt = 0; Attempt < ck::ProceduralGroundProbeAttempts; ++Attempt)
        {
            const auto Span = ck::MakeProceduralGroundProbeSpan(Attempt, InProbe.Get_Up(), InProbe.Get_Down(), InProbe.Get_OutwardLean());
            const auto Axis = ck::FProceduralGaitSolver::ComputeTraceAxis(InUp, InRadial, Span.Get_OutwardLean());
            const auto ProbeStart = InPoint + Axis * Span.Get_UpDistance();
            const auto ProbeEnd = InPoint - Axis * Span.Get_DownDistance();
            const auto Hit = UCk_Utils_JoltQuery_UE::Get_RayCast(InWorld, ProbeStart, ProbeEnd, InProbe.Get_QueryFilter());
            ++InOutRayCount;

            if (ck::IsValid(InDebugProbe, ck::IsValid_Policy_NullptrOnly{}))
            {
                InDebugProbe->Set_Start(ProbeStart)
                    .Set_End(ProbeEnd)
                    .Set_AttemptCount(Attempt + 1)
                    .Set_Hit(Hit.Get_HasHit())
                    .Set_HitFraction(Hit.Get_Fraction())
                    .Set_HitPosition(Hit.Get_Position())
                    .Set_HitNormal(Hit.Get_Normal());
            }

            if (Get_TrustedHit(Hit, (ProbeEnd - ProbeStart).GetSafeNormal()))
            { return Hit; }

            const auto StartedInsideSolid = Hit.Get_HasHit() && Hit.Get_Fraction() <= 0.0f;
            const auto MayRetry = InAttempts == EGroundProbeAttempts::Every || (Attempt == 0 && StartedInsideSolid);
            if (NOT MayRetry)
            { return {}; }
        }

        return {};
    }

    // --------------------------------------------------------------------------------------------------------------------

    struct FObservedBlockedFace
    {
        FVector Point = FVector::ZeroVector;
        FVector Normal = FVector::ZeroVector;
    };

    struct FHeightObservation
    {
        FVector Point;
        FVector Normal;
    };

    // A fresh already-paid native plant witness, separate from the solver's emitted plant pose.
    struct FPlantSeedPlane
    {
        FVector Point = FVector::ZeroVector;
        FVector Normal = FVector::UpVector;
    };

    struct FNativeLandingReceipt
    {
        FVector Point = FVector::ZeroVector;
        FVector Normal = FVector::UpVector;
        ck::EProceduralFootholdCast Cast = ck::EProceduralFootholdCast::Down;
    };

    struct FNativeTargetReceipt
    {
        FCk_Handle Owner;
        uint64 Frame = 0;
        uint64 InputSolve = 0;
        FTransform Body = FTransform::Identity;
        int32 LegIndex = INDEX_NONE;
        FVector Point = FVector::ZeroVector;
        FVector Normal = FVector::ZeroVector;
        FVector SimulationHip = FVector::ZeroVector;
        FCk_Jolt_QueryFilter Filter;
        ck::EProceduralFootholdCast Cast = ck::EProceduralFootholdCast::Down;
        bool SearchPredicates = false;
        bool SimulationVisible = false;
        TOptional<FVector> VisiblePosedHip;
        bool StagingGeometry = false;
    };

    struct FFootholdQuery
    {
        UWorld* World = nullptr;
        FQuat Basis = FQuat::Identity;
        FVector BodyLocation = FVector::ZeroVector;
        FVector Hip = FVector::ZeroVector;
        FVector Outboard = FVector::ZeroVector;
        float MaxHipLateral = 0.0f;
        FVector Ideal = FVector::ZeroVector;
        FVector Plant = FVector::ZeroVector;
        bool Planted = false;
        bool RefreshPlantedHold = false;
        // The body's velocity in the support plane; zero at rest and at setup.
        FVector Travel = FVector::ZeroVector;
        TOptional<FObservedBlockedFace> BlockedFace;
        TOptional<FObservedBlockedFace> AdaptiveTop;
        float Reach = 0.0f;
        float RestDrop = 0.0f;
        const FCk_ProceduralGait_Probe* Probe = nullptr;
        const FCk_ProceduralGait_Step* Step = nullptr;
        const FCk_ProceduralGait_Foothold* Foothold = nullptr;
        float FootContactRadius = 0.0f;
        int32 LegIndex = INDEX_NONE;
        TArrayView<const ck::FProceduralFootReservation> Reservations;
        bool* RejectedReservation = nullptr;
        TArray<FHeightObservation, TInlineAllocator<2>>* HeightObservations = nullptr;
        TOptional<FVector> CommittedPoint;
        TOptional<FNativeLandingReceipt>* MatchingLandingReceipt = nullptr;
        FCk_Handle ReceiptOwner;
        uint64 ReceiptInputSolve = 0;
        FTransform ReceiptBody = FTransform::Identity;
        bool CaptureTargetReceipt = false;
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
        ck_procedural_height_seed_test::FObservation* HeightSeedObservation = nullptr;
#endif
    };

    constexpr auto MaxInwardOccluderTopReceipts = 2;

    // A same-search native visibility contact proposes only a later query point, never an admitted foothold.
    struct FInwardOccluderTopReceipt
    {
        FVector Point = FVector::ZeroVector;
        FVector Normal = FVector::ZeroVector;
    };

    struct FInwardFailedFaceReceipt
    {
        FVector Point = FVector::ZeroVector;
        FVector Normal = FVector::ZeroVector;
    };

    // Source None means nothing usable was found; IdealVerdict then says why the ideal target was not.
    struct FFoothold
    {
        FVector Position = FVector::ZeroVector;
        FVector Normal = FVector::UpVector;
        ck::EProceduralFootholdSource Source = ck::EProceduralFootholdSource::None;
        ck::EProceduralFootholdVerdict IdealVerdict = ck::EProceduralFootholdVerdict::Miss;
        // A receipt from an unsuccessful budgeted full search only; early/held/backoff results grant no extra work.
        int32 UnusedBoundarySamples = 0;
        ck::EProceduralFootholdCast Cast = ck::EProceduralFootholdCast::Down;
        TArray<FInwardOccluderTopReceipt, TInlineAllocator<MaxInwardOccluderTopReceipts>> InwardOccluderTops;
        TOptional<FInwardFailedFaceReceipt> FailedInwardFace;
        TOptional<FNativeTargetReceipt> NativeReceipt;
    };

    // One validated candidate in world space; the occluder is the solid its hip trace met when it is Occluded. A down-ray
    // candidate may be clamped toward the hip and probed again, a face hit may not, because a down ray there would find a
    // different surface.
    struct FFootholdCandidate
    {
        FVector Position = FVector::ZeroVector;
        FVector Normal = FVector::UpVector;
        ck::EProceduralFootholdSource Source = ck::EProceduralFootholdSource::None;
        ck::EProceduralFootholdVerdict Verdict = ck::EProceduralFootholdVerdict::Miss;
        ck::EProceduralFootholdCast Cast = ck::EProceduralFootholdCast::Down;
        TOptional<FCk_Jolt_HitResult> Occluder;
        bool NativeHit = false;
        TOptional<FNativeTargetReceipt> NativeReceipt;
    };

    // Whether a candidate is one the search found, and so must also be level enough.
    enum class EFootholdStage : uint8
    {
        IdealOrHeld,
        Search
    };

    // What a ray under a point does with ground beyond the target reach from the hip. The ideal target and the search's
    // down rays clamp the point toward the hip and probe once more there; a hold is judged at its own point, because the
    // re-probe would stand in a different point for it and, on continuous ground, never let a trailing hold read
    // Unreachable.
    enum class EFootholdReprobe : uint8
    {
        ClampTowardHip,
        None
    };

    // What one solve may still spend on searches, and the solve a search that finds nothing is stamped with.
    struct FFootholdSearchBudget
    {
        int32 RemainingLegs = 0;
        uint64 Solve = 0;
    };

    // One search per this many enabled legs and solve, and never fewer than MinSearchLegs: a walker whose every ideal target
    // fails at once (a long body over a beam narrower than its hips) would otherwise run the full fan on every leg in the same frame. A hold serves for
    // as long as the ideal target is unusable, so a failing leg searches once and then holds, and the budget binds only while
    // many legs fail together. The legs left over keep the contact grace this solve and search on a later one, round-robin.
    constexpr auto EnabledLegsPerSearch = 2;
    constexpr auto MinSearchLegs = 4;
    // A search that found nothing usable is not repeated for this many solves: the ground around the leg barely changes in
    // that time, and every repeat is a full fan. The contact grace runs meanwhile.
    constexpr auto SearchBackoffSolves = uint64{3};
    // How far a search candidate may lie inboard of its hip, along the direction to its rest foot, before it would fold the
    // leg under the body: a little slack keeps a foothold straight under a hip that stands over the edge it grips.
    constexpr auto InboardToleranceShareOfReach = 0.1f;
    // How far inside the widest hip a foothold must lie to count as under the body: the hips sit on the body's surface, so
    // a foothold at a hip's own lateral offset is beside the body, not under it.
    constexpr auto UnderBodyMargin = 5.0f;
    // A face hold is checked with one ray this far to either side of it along its normal: the hold lies on the face, so a
    // short ray meets it again, and a short start stays out of solids next to the face.
    constexpr auto FaceHoldProbeHalfSpan = 10.0f;
    // A face-guided down ray samples near the edge before the search ring, at a small share of that ring's radius.
    // Prefer at least a centimetre inside the face; a selected search face may cap this to a narrower usable strip.
    constexpr auto FaceTopInsetShareOfSearchRadius = 0.15f;
    constexpr auto FaceTopPreferredMinInset = 1.0f;
    // A searched face may leave a narrower usable strip than the preferred inset. Skip only an inset too small to
    // distinguish its point from the collider edge; the ordinary candidate validation still judges the hit.
    constexpr auto FaceTopGeometryTolerance = 1.0e-3;
    // A search ring at this share of the reach reaches the next top across a gap as wide as a top while every ring point
    // stays inside the leg's target reach from an ideal at its rest distance.
    constexpr auto DerivedSearchRadiusShareOfReach = 0.3f;
    // Two spots closer than this many step thresholds agree: a foot moved between them would travel less than a step.
    constexpr auto DerivedKeepRadiusInStepThresholds = 1.5f;
    // The floor in front of an occluder is probed this share of the reach back from the face toward the hip: far enough
    // from the face for the foot to stand, close enough to be the spot the occluded target was aiming at.
    constexpr auto FrontFloorShareOfReach = 0.1f;
    // The inward casts aim from the ideal target under the hip at these depths in rest drops: the near one meets a pillar
    // side or a cylinder curving away at the leg's own height, the far one the floor under a hip raised on a top or a post.
    constexpr auto InwardCloseDepthInRestDrops = 1.5f;
    constexpr auto InwardFarDepthInRestDrops = 4.0f;
    // The outward cast starts this many rest drops above the hip so it looks down over a top or a face beyond the ideal.
    constexpr auto OutwardHeightInRestDrops = 2.0f;
    // Eight ring points, 45 degrees apart, put at least two on a top across any gap narrower than the ring's radius while
    // keeping a search under the fan the budget allows.
    constexpr auto RingPoints = 8;
    // While too few feet support the body the last fitted plane is held this many step durations: long enough to bridge the
    // swing of a phase group between two stances, short enough that a body that leaves its feet behind soon rides its rays.
    constexpr auto FeetPlaneHoldStepDurations = 1.5;

    auto
        Get_RestDrop(
            const FCk_ProceduralLeg_Placement& InPlacement)
        -> float
    {
        return static_cast<float>(FMath::Abs(InPlacement.Get_HipLocal().Z - InPlacement.Get_RestFootLocal().Z));
    }

    auto
        Get_PlantSeedDisplacement(
            const FVector& InPlant,
            const FVector& InLead,
            const FVector& InHip,
            float InForceReach,
            float InSearchRadius)
        -> FVector
    {
        // FVector arithmetic itself diagnoses non-finite results in diagnostic-enabled builds.
        if (InLead.ContainsNaN())
        { return FVector::ZeroVector; }
        const auto HalfLead = InLead * 0.5;
        if (InPlant.ContainsNaN() || InHip.ContainsNaN()
            || NOT FMath::IsFinite(InForceReach) || InForceReach <= 0.0f
            || NOT FMath::IsFinite(InSearchRadius) || InSearchRadius <= 0.0f)
        { return HalfLead; }

        const auto LeadSquared = InLead.SizeSquared();
        const auto SearchSquared = FMath::Square(static_cast<double>(InSearchRadius));
        const auto ForceSquared = FMath::Square(static_cast<double>(InForceReach));
        if (NOT FMath::IsFinite(LeadSquared) || LeadSquared <= 0.0
            || HalfLead.SizeSquared() > SearchSquared)
        { return HalfLead; }
        const auto OffsetX = InPlant.X - InHip.X;
        const auto OffsetY = InPlant.Y - InHip.Y;
        const auto OffsetZ = InPlant.Z - InHip.Z;
        if (NOT FMath::IsFinite(OffsetX) || NOT FMath::IsFinite(OffsetY) || NOT FMath::IsFinite(OffsetZ))
        { return HalfLead; }
        const auto Offset = FVector{OffsetX, OffsetY, OffsetZ};
        const auto HalfOffsetX = OffsetX + HalfLead.X;
        const auto HalfOffsetY = OffsetY + HalfLead.Y;
        const auto HalfOffsetZ = OffsetZ + HalfLead.Z;
        if (NOT FMath::IsFinite(HalfOffsetX) || NOT FMath::IsFinite(HalfOffsetY) || NOT FMath::IsFinite(HalfOffsetZ))
        { return HalfLead; }
        const auto HalfDistanceSquared = HalfOffsetX * HalfOffsetX + HalfOffsetY * HalfOffsetY + HalfOffsetZ * HalfOffsetZ;
        if (NOT FMath::IsFinite(HalfDistanceSquared) || HalfDistanceSquared <= ForceSquared)
        { return HalfLead; }

        // Only propose a point inside the remaining bounded lead interval. A force-sphere intersection is geometry,
        // not a contact: the single ordinary native probe below still decides the hit's complete admission.
        const auto Along = FVector::DotProduct(Offset, InLead);
        const auto Constant = Offset.SizeSquared() - ForceSquared;
        if (NOT FMath::IsFinite(Along) || NOT FMath::IsFinite(Constant))
        { return HalfLead; }
        const auto Discriminant = Along * Along - LeadSquared * Constant;
        if (NOT FMath::IsFinite(Discriminant) || Discriminant <= 0.0)
        { return HalfLead; }
        const auto Root = FMath::Sqrt(Discriminant);
        const auto Entry = (-Along - Root) / LeadSquared;
        const auto Exit = (-Along + Root) / LeadSquared;
        const auto SearchCap = static_cast<double>(InSearchRadius) / FMath::Sqrt(LeadSquared);
        if (NOT FMath::IsFinite(Entry) || NOT FMath::IsFinite(Exit) || NOT FMath::IsFinite(SearchCap))
        { return HalfLead; }
        const auto Lower = FMath::Max(0.5, Entry);
        const auto Upper = FMath::Min(FMath::Min(1.0, SearchCap), Exit);
        if (Upper <= Lower)
        { return HalfLead; }
        const auto Fraction = (Lower + Upper) * 0.5;
        if (NOT FMath::IsFinite(Fraction) || Fraction < 0.5 || Fraction > 1.0)
        { return HalfLead; }
        const auto Displacement = InLead * Fraction;
        const auto PointX = InPlant.X + Displacement.X;
        const auto PointY = InPlant.Y + Displacement.Y;
        const auto PointZ = InPlant.Z + Displacement.Z;
        if (NOT FMath::IsFinite(PointX) || NOT FMath::IsFinite(PointY) || NOT FMath::IsFinite(PointZ))
        { return HalfLead; }
        const auto Point = FVector{PointX, PointY, PointZ};
        if (Displacement.ContainsNaN()
            || Displacement.SizeSquared() > SearchSquared || FVector::DistSquared(Point, InHip) > ForceSquared)
        { return HalfLead; }
        return Displacement;
    }

    // Query geometry only: no contact, target or take-off authority is produced here.
    auto
        Get_PlantSeedBoundaryPoint(
            const FVector& InPlant,
            const FVector& InLead,
            const FVector& InHip,
            const FVector& InOutboard,
            const FPlantSeedPlane& InPlane,
            float InReach,
            float InForceReach,
            float InSearchRadius)
        -> TOptional<FVector>
    {
        if (InPlant.ContainsNaN() || InLead.ContainsNaN() || InHip.ContainsNaN() || InOutboard.ContainsNaN()
            || InPlane.Point.ContainsNaN() || InPlane.Normal.ContainsNaN() || NOT InPlane.Normal.IsNormalized()
            || NOT FMath::IsFinite(InReach) || InReach <= 0.0f
            || NOT FMath::IsFinite(InForceReach) || InForceReach <= 0.0f
            || NOT FMath::IsFinite(InSearchRadius) || InSearchRadius <= 0.0f)
        {
            return {};
        }

        const auto FloatMax = static_cast<double>(std::numeric_limits<float>::max());
        const auto FitsFloat = [FloatMax](const FVector& InPoint) -> bool
        {
            return FMath::IsFinite(InPoint.X) && FMath::Abs(InPoint.X) <= FloatMax
                && FMath::IsFinite(InPoint.Y) && FMath::Abs(InPoint.Y) <= FloatMax
                && FMath::IsFinite(InPoint.Z) && FMath::Abs(InPoint.Z) <= FloatMax;
        };
        if (NOT FitsFloat(InPlant) || NOT FitsFloat(InHip) || NOT FitsFloat(InLead))
        {
            return {};
        }

        const auto HalfLead = InLead * 0.5;
        const auto LeadSquared = InLead.SizeSquared();
        const auto SearchSquared = FMath::Square(static_cast<double>(InSearchRadius));
        const auto ForceSquared = FMath::Square(static_cast<double>(InForceReach));
        if (NOT FMath::IsFinite(LeadSquared) || LeadSquared <= 0.0
            || HalfLead.SizeSquared() > SearchSquared)
        {
            return {};
        }
        const auto Offset = InPlant - InHip;
        const auto HalfOffset = Offset + HalfLead;
        if (HalfOffset.SizeSquared() <= ForceSquared)
        {
            return {};
        }
        const auto Along = FVector::DotProduct(Offset, InLead);
        const auto Constant = Offset.SizeSquared() - ForceSquared;
        const auto Discriminant = Along * Along - LeadSquared * Constant;
        if (NOT FMath::IsFinite(Along) || NOT FMath::IsFinite(Constant)
            || NOT FMath::IsFinite(Discriminant) || Discriminant <= 0.0)
        {
            return {};
        }
        const auto Root = FMath::Sqrt(Discriminant);
        const auto Entry = (-Along - Root) / LeadSquared;
        const auto Exit = (-Along + Root) / LeadSquared;
        const auto SearchCap = static_cast<double>(InSearchRadius) / FMath::Sqrt(LeadSquared);
        if (NOT FMath::IsFinite(Entry) || NOT FMath::IsFinite(Exit) || NOT FMath::IsFinite(SearchCap))
        {
            return {};
        }
        const auto Lower = FMath::Max(0.5, Entry);
        const auto Upper = FMath::Min(FMath::Min(1.0, SearchCap), Exit);
        if (Upper <= Lower)
        {
            return {};
        }
        const auto ExactEntry = InPlant + InLead * Lower;
        if (NOT FitsFloat(ExactEntry))
        {
            return {};
        }

        // Check range before every narrowing conversion. Enumerate at most eight native-float corners.
        const float Rounded[3] = {static_cast<float>(ExactEntry.X), static_cast<float>(ExactEntry.Y),
            static_cast<float>(ExactEntry.Z)};
        const double Hip[3] = {InHip.X, InHip.Y, InHip.Z};
        float Inward[3];
        for (auto Axis = 0; Axis < 3; ++Axis)
        {
            if (static_cast<double>(Rounded[Axis]) == Hip[Axis])
            {
                Inward[Axis] = Rounded[Axis];
            }
            else
            {
                const auto Direction = static_cast<double>(Rounded[Axis]) < Hip[Axis]
                    ? std::numeric_limits<float>::max() : -std::numeric_limits<float>::max();
                Inward[Axis] = std::nextafter(Rounded[Axis], Direction);
            }
        }
        TOptional<FVector> Nearest;
        auto NearestSquared = 0.0;
        for (auto Corner = 0; Corner < 8; ++Corner)
        {
            const auto Point = FVector{Corner & 1 ? Inward[0] : Rounded[0],
                Corner & 2 ? Inward[1] : Rounded[1], Corner & 4 ? Inward[2] : Rounded[2]};
            if (NOT FitsFloat(Point))
            {
                continue;
            }
            const auto Displacement = Point - InPlant;
            const auto Fraction = FVector::DotProduct(Displacement, InLead) / LeadSquared;
            const auto PlaneResidual = FVector::DotProduct(Point - InPlane.Point, InPlane.Normal);
            const auto DistanceSquared = FVector::DistSquared(Point, ExactEntry);
            if (NOT FMath::IsFinite(Fraction) || Fraction < Lower || Fraction > Upper
                || FVector::CrossProduct(Displacement, InLead) != FVector::ZeroVector
                || NOT FMath::IsFinite(PlaneResidual) || PlaneResidual != 0.0
                || Displacement.ContainsNaN() || Displacement.SizeSquared() > SearchSquared
                || FVector::DistSquared(Point, InHip) > ForceSquared || FVector::Dist(Point, InHip) > InForceReach
                || NOT ck::Get_IsFootholdOnOwnSide(Point - InHip, InOutboard, InboardToleranceShareOfReach * InReach)
                || NOT FMath::IsFinite(DistanceSquared))
            {
                continue;
            }
            if (NOT Nearest.IsSet() || DistanceSquared < NearestSquared)
            {
                Nearest = Point;
                NearestSquared = DistanceSquared;
            }
        }
        return Nearest;
    }

    // Ordinary estimates propose one native-grid cell; the original point predicates retain authority.
    struct FPlantQueryInterval
    {
        double Lower = 0.0;
        double Upper = 0.0;
    };

    enum class EPlantQueryInterval : uint8
    {
        Invalid,
        Empty,
        Valid
    };

    auto
        Get_PlantQueryDot(
            const FVector& InA,
            const FVector& InB,
            double& OutDot)
        -> bool
    {
        const auto X = InA.X * InB.X;
        const auto Y = InA.Y * InB.Y;
        const auto Z = InA.Z * InB.Z;
        if (NOT FMath::IsFinite(X) || NOT FMath::IsFinite(Y) || NOT FMath::IsFinite(Z))
        { return false; }
        const auto XY = X + Y;
        if (NOT FMath::IsFinite(XY))
        { return false; }
        OutDot = XY + Z;
        return FMath::IsFinite(OutDot);
    }

    auto
        Get_PlantQueryDifference(
            const FVector& InA,
            const FVector& InB,
            FVector& OutDifference)
        -> bool
    {
        const auto X = InA.X - InB.X;
        const auto Y = InA.Y - InB.Y;
        const auto Z = InA.Z - InB.Z;
        if (NOT FMath::IsFinite(X) || NOT FMath::IsFinite(Y) || NOT FMath::IsFinite(Z))
        { return false; }
        OutDifference = FVector{X, Y, Z};
        return true;
    }

    auto
        Get_PlantQueryImage(
            const FVector& InOrigin,
            const FVector& InVelocity,
            double InParameter,
            FVector& OutPoint)
        -> bool
    {
        const auto OffsetX = InVelocity.X * InParameter;
        const auto OffsetY = InVelocity.Y * InParameter;
        const auto OffsetZ = InVelocity.Z * InParameter;
        if (NOT FMath::IsFinite(OffsetX) || NOT FMath::IsFinite(OffsetY) || NOT FMath::IsFinite(OffsetZ))
        { return false; }
        const auto X = InOrigin.X + OffsetX;
        const auto Y = InOrigin.Y + OffsetY;
        const auto Z = InOrigin.Z + OffsetZ;
        if (NOT FMath::IsFinite(X) || NOT FMath::IsFinite(Y) || NOT FMath::IsFinite(Z))
        { return false; }
        OutPoint = FVector{X, Y, Z};
        return true;
    }

    auto
        Get_PlantQuerySphereInterval(
            const FVector& InOrigin,
            const FVector& InVelocity,
            const FVector& InCenter,
            double InRadius,
            FPlantQueryInterval& OutInterval)
        -> EPlantQueryInterval
    {
        FVector Offset;
        if (NOT Get_PlantQueryDifference(InOrigin, InCenter, Offset)
            || NOT FMath::IsFinite(InRadius) || InRadius <= 0.0)
        { return EPlantQueryInterval::Invalid; }
        double A = 0.0;
        double HalfB = 0.0;
        double OffsetSquared = 0.0;
        if (NOT Get_PlantQueryDot(InVelocity, InVelocity, A) || A <= 0.0
            || NOT Get_PlantQueryDot(Offset, InVelocity, HalfB)
            || NOT Get_PlantQueryDot(Offset, Offset, OffsetSquared))
        { return EPlantQueryInterval::Invalid; }
        const auto B = 2.0 * HalfB;
        const auto RadiusSquared = InRadius * InRadius;
        if (NOT FMath::IsFinite(B) || NOT FMath::IsFinite(RadiusSquared))
        { return EPlantQueryInterval::Invalid; }
        const auto C = OffsetSquared - RadiusSquared;
        const auto BSquared = B * B;
        const auto FourA = 4.0 * A;
        if (NOT FMath::IsFinite(C) || NOT FMath::IsFinite(BSquared) || NOT FMath::IsFinite(FourA))
        { return EPlantQueryInterval::Invalid; }
        const auto FourAC = FourA * C;
        if (NOT FMath::IsFinite(FourAC))
        { return EPlantQueryInterval::Invalid; }
        const auto Discriminant = BSquared - FourAC;
        if (NOT FMath::IsFinite(Discriminant) || Discriminant == 0.0)
        { return EPlantQueryInterval::Invalid; }
        if (Discriminant < 0.0)
        { return EPlantQueryInterval::Empty; }
        const auto Root = std::sqrt(Discriminant);
        const auto Sum = B + std::copysign(Root, B);
        if (NOT FMath::IsFinite(Root) || NOT FMath::IsFinite(Sum))
        { return EPlantQueryInterval::Invalid; }
        const auto Q = -0.5 * Sum;
        if (NOT FMath::IsFinite(Q) || Q == 0.0)
        { return EPlantQueryInterval::Invalid; }
        const auto Root0 = Q / A;
        const auto Root1 = C / Q;
        if (NOT FMath::IsFinite(Root0) || NOT FMath::IsFinite(Root1) || Root0 == Root1)
        { return EPlantQueryInterval::Invalid; }
        OutInterval = {FMath::Min(Root0, Root1), FMath::Max(Root0, Root1)};
        return EPlantQueryInterval::Valid;
    }

    auto
        Get_PlantQueryAxisCell(
            double InValue,
            double (&OutValues)[2])
        -> bool
    {
        constexpr auto FloatMax = static_cast<double>(std::numeric_limits<float>::max());
        if (NOT FMath::IsFinite(InValue) || FMath::Abs(InValue) > FloatMax)
        { return false; }
        const auto Rounded = static_cast<float>(InValue);
        OutValues[0] = Rounded == 0.0f ? 0.0 : static_cast<double>(Rounded);
        OutValues[1] = OutValues[0];
        if (OutValues[0] == InValue)
        { return true; }
        const auto Direction = OutValues[0] < InValue
            ? std::numeric_limits<float>::max() : -std::numeric_limits<float>::max();
        const auto Adjacent = std::nextafter(Rounded, Direction);
        if (NOT FMath::IsFinite(Adjacent))
        { return false; }
        OutValues[1] = Adjacent == 0.0f ? 0.0 : static_cast<double>(Adjacent);
        return OutValues[0] != OutValues[1]
            && FMath::Min(OutValues[0], OutValues[1]) <= InValue
            && InValue <= FMath::Max(OutValues[0], OutValues[1]);
    }

    auto
        Get_FinalPlantQueryPoint(
            const FVector& InPlant,
            const FVector& InLead,
            const FVector& InHip,
            const FVector& InOutboard,
            const FPlantSeedPlane& InPlane,
            const FVector& InUp,
            float InForceReach,
            float InSearchRadius,
            float InOwnSideTolerance,
            float InContactRadius,
            int32 InLegIndex,
            TArrayView<const ck::FProceduralFootReservation> InReservations,
            const TOptional<FVector>& InIdenticalPaidPoint = {})
        -> TOptional<FVector>
    {
        constexpr auto MaxReservations = 64;
        constexpr auto FloatMax = static_cast<double>(std::numeric_limits<float>::max());
        if (InPlant.ContainsNaN() || InLead.ContainsNaN() || InHip.ContainsNaN() || InOutboard.ContainsNaN()
            || InPlane.Point.ContainsNaN() || InPlane.Normal.ContainsNaN() || InUp.ContainsNaN()
            || NOT FMath::IsFinite(InForceReach) || InForceReach <= 0.0f
            || NOT FMath::IsFinite(InSearchRadius) || InSearchRadius <= 0.0f
            || NOT FMath::IsFinite(InOwnSideTolerance) || InOwnSideTolerance < 0.0f
            || NOT FMath::IsFinite(InContactRadius) || InContactRadius < 0.0f || InLegIndex < 0
            || InReservations.Num() > MaxReservations || InReservations.Num() < 0
            || InLead == FVector::ZeroVector || InLead.Z != 0.0 || InOutboard.Z != 0.0
            || InUp != FVector::UpVector || InPlane.Normal != FVector::UpVector
            || FMath::Abs(InPlane.Point.Z) > FloatMax)
        { return {}; }
        if (static_cast<double>(static_cast<float>(InPlane.Point.Z)) != InPlane.Point.Z
            || (InIdenticalPaidPoint.IsSet() && InIdenticalPaidPoint.GetValue().ContainsNaN()))
        { return {}; }

        // Preserve the contact predicate's radius-before-self validation and zero-query-radius bypass.
        if (InContactRadius > 0.0f)
        {
            for (const auto& Reservation : InReservations)
            {
                const auto Radius = Reservation.Get_Radius();
                if (NOT FMath::IsFinite(Radius) || Radius < 0.0f)
                { return {}; }
                if (Radius == 0.0f || Reservation.Get_LegIndex() == InLegIndex)
                { continue; }
                if (Reservation.Get_Position().ContainsNaN() || Reservation.Get_LegIndex() < 0)
                { return {}; }
                const auto Sum = static_cast<double>(InContactRadius) + Radius;
                if (NOT FMath::IsFinite(Sum))
                { return {}; }
            }
        }
        double OutboardSquared = 0.0;
        if (NOT Get_PlantQueryDot(InOutboard, InOutboard, OutboardSquared) || OutboardSquared <= SMALL_NUMBER)
        { return {}; }
        const auto Direction = InOutboard.GetSafeNormal();
        FVector Offset;
        double Denominator = 0.0;
        double A0 = 0.0;
        double A1 = 0.0;
        if (Direction.ContainsNaN() || NOT Direction.IsNormalized()
            || NOT Get_PlantQueryDifference(InPlane.Point, InHip, Offset)
            || NOT Get_PlantQueryDot(Direction, Direction, Denominator) || Denominator <= SMALL_NUMBER
            || NOT Get_PlantQueryDot(Offset, Direction, A0) || NOT Get_PlantQueryDot(InLead, Direction, A1))
        { return {}; }
        const auto Intercept = A0 + static_cast<double>(InOwnSideTolerance);
        if (NOT FMath::IsFinite(Intercept))
        { return {}; }

        struct FStart
        {
            FVector Origin = FVector::ZeroVector;
            FVector Velocity = FVector::ZeroVector;
            double Parameter = 0.0;
        };
        FStart Starts[2];
        int32 StartCount = 0;
        FPlantQueryInterval Reservations[MaxReservations];
        for (int32 PieceIndex = 0; PieceIndex < 2; ++PieceIndex)
        {
            const auto Corrected = PieceIndex == 1;
            auto Lower = 0.5;
            auto Upper = 1.0;
            auto LowerClosed = true;
            auto UpperClosed = true;
            if (A1 == 0.0)
            {
                if ((Intercept < 0.0) != Corrected)
                { continue; }
            }
            else
            {
                const auto Branch = -Intercept / A1;
                if (NOT FMath::IsFinite(Branch))
                { return {}; }
                if ((A1 > 0.0) == Corrected)
                {
                    if (Branch < Lower)
                    { continue; }
                    if (Branch <= Upper)
                    { Upper = Branch; UpperClosed = NOT Corrected; }
                }
                else
                {
                    if (Branch > Upper)
                    { continue; }
                    if (Branch >= Lower)
                    { Lower = Branch; LowerClosed = NOT Corrected; }
                }
            }
            if (Lower >= Upper)
            { continue; }
            auto Origin = InPlane.Point;
            auto Velocity = InLead;
            if (Corrected)
            {
                const auto Margin = -static_cast<double>(InOwnSideTolerance) + 0.1;
                if (NOT FMath::IsFinite(Margin))
                { return {}; }
                const auto Numerator = Margin - A0;
                if (NOT FMath::IsFinite(Numerator))
                { return {}; }
                const auto Scale0 = Numerator / Denominator;
                const auto Scale1 = -A1 / Denominator;
                if (NOT FMath::IsFinite(Scale0) || NOT FMath::IsFinite(Scale1)
                    || NOT Get_PlantQueryImage(InPlane.Point, Direction, Scale0, Origin)
                    || NOT Get_PlantQueryImage(InLead, Direction, Scale1, Velocity))
                { return {}; }
            }
            auto HasDomain = true;
            for (int32 SphereIndex = 0; SphereIndex < 2; ++SphereIndex)
            {
                FPlantQueryInterval Interval;
                const auto Status = Get_PlantQuerySphereInterval(Origin, Velocity,
                    SphereIndex == 0 ? InHip : InPlant,
                    static_cast<double>(SphereIndex == 0 ? InForceReach : InSearchRadius), Interval);
                if (Status == EPlantQueryInterval::Invalid)
                { return {}; }
                if (Status == EPlantQueryInterval::Empty)
                { HasDomain = false; break; }
                if (Interval.Lower > Lower)
                { Lower = Interval.Lower; LowerClosed = true; }
                if (Interval.Upper < Upper)
                { Upper = Interval.Upper; UpperClosed = true; }
                if (Lower >= Upper)
                { HasDomain = false; break; }
            }
            if (NOT HasDomain)
            { continue; }
            int32 ReservationCount = 0;
            if (InContactRadius > 0.0f)
            {
                for (const auto& Reservation : InReservations)
                {
                    if (Reservation.Get_Radius() == 0.0f || Reservation.Get_LegIndex() == InLegIndex)
                    { continue; }
                    FPlantQueryInterval Interval;
                    const auto Status = Get_PlantQuerySphereInterval(Origin, Velocity, Reservation.Get_Position(),
                        static_cast<double>(InContactRadius) + Reservation.Get_Radius(), Interval);
                    if (Status == EPlantQueryInterval::Invalid)
                    { return {}; }
                    if (Status == EPlantQueryInterval::Empty)
                    { continue; }
                    int32 Index = ReservationCount;
                    while (Index > 0)
                    {
                        const auto& Previous = Reservations[Index - 1];
                        if (Previous.Lower < Interval.Lower
                            || (Previous.Lower == Interval.Lower && Previous.Upper <= Interval.Upper))
                        { break; }
                        Reservations[Index] = Previous;
                        --Index;
                    }
                    Reservations[Index] = Interval;
                    ++ReservationCount;
                }
            }
            auto Parameter = LowerClosed ? Lower : std::nextafter(Lower, std::numeric_limits<double>::infinity());
            for (int32 Index = 0; Index < ReservationCount; ++Index)
            {
                const auto& Interval = Reservations[Index];
                if (Interval.Lower >= Parameter)
                { break; }
                if (Parameter < Interval.Upper)
                { Parameter = Interval.Upper; }
            }
            if (NOT FMath::IsFinite(Parameter) || Parameter > Upper || (Parameter == Upper && NOT UpperClosed))
            { continue; }
            Starts[StartCount++] = {Origin, Velocity, Parameter};
        }
        if (StartCount == 0)
        { return {}; }
        const auto& Start = Starts[StartCount == 2 && Starts[1].Parameter < Starts[0].Parameter ? 1 : 0];
        FVector Anchor;
        if (NOT Get_PlantQueryImage(Start.Origin, Start.Velocity, Start.Parameter, Anchor))
        { return {}; }
        Anchor.Z = InPlane.Point.Z;
        double X[2];
        double Y[2];
        if (NOT Get_PlantQueryAxisCell(Anchor.X, X) || NOT Get_PlantQueryAxisCell(Anchor.Y, Y))
        { return {}; }
        FVector Points[4];
        int32 PointCount = 0;
        const auto ForceSquared = FMath::Square(static_cast<double>(InForceReach));
        const auto SearchSquared = FMath::Square(static_cast<double>(InSearchRadius));
        for (int32 Corner = 0; Corner < 4; ++Corner)
        {
            const FVector Point{X[Corner & 1], Y[(Corner >> 1) & 1], Anchor.Z};
            auto Duplicate = false;
            for (int32 Index = 0; Index < PointCount; ++Index)
            {
                if (Points[Index] == Point)
                { Duplicate = true; break; }
            }
            if (Duplicate)
            { continue; }
            Points[PointCount++] = Point;
            FVector HipToPoint;
            FVector FromPlant;
            if (NOT Get_PlantQueryDifference(Point, InHip, HipToPoint)
                || NOT Get_PlantQueryDifference(Point, InPlant, FromPlant))
            { continue; }
            const auto HipSquared = FVector::DistSquared(Point, InHip);
            const auto PlantSquared = FromPlant.SizeSquared();
            if (Point.ContainsNaN() || HipToPoint.ContainsNaN() || FromPlant.ContainsNaN()
                || NOT FMath::IsFinite(HipSquared) || NOT FMath::IsFinite(PlantSquared)
                || HipSquared > ForceSquared || FVector::Dist(Point, InHip) > InForceReach
                || PlantSquared > SearchSquared
                || NOT ck::Get_IsFootholdOnOwnSide(HipToPoint, InOutboard, InOwnSideTolerance)
                || NOT ck::Get_IsProceduralFootContactAvailable(Point, InContactRadius, InLegIndex, InReservations))
            { continue; }
            // The caller supplies this only after proving the complete paid probe signature identical.
            if (InIdenticalPaidPoint.IsSet() && Point == InIdenticalPaidPoint.GetValue())
            { continue; }
            return Point;
        }
        return {};
    }

    // Ordinary acquisition after older failures: one body-limited anchor and one float XY cell.
    // Query geometry grants no landing authority; the caller retains native/trial admission.
    auto
        Get_HipBodyBoundaryQueryPoint(
            const FFootholdQuery& InQuery,
            const FPlantSeedPlane& InPlane,
            const FVector& InUp,
            float InForceReach,
            float InOwnSideTolerance,
            const TOptional<FVector>& InIdenticalFinalPoint
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
            , int32* OutTestPointCount = nullptr
#endif
            )
        -> TOptional<FVector>
    {
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
        if (OutTestPointCount != nullptr)
        {
            *OutTestPointCount = 0;
        }
#endif
        constexpr auto Margin = 0.1;
        constexpr auto MaxReservations = 64;
        constexpr auto FloatMax = static_cast<double>(std::numeric_limits<float>::max());
        if (InQuery.Hip.ContainsNaN() || InQuery.BodyLocation.ContainsNaN()
            || InQuery.Outboard.ContainsNaN() || InPlane.Point.ContainsNaN()
            || InPlane.Normal.ContainsNaN() || InUp.ContainsNaN()
            || InQuery.Basis.ContainsNaN() || NOT InQuery.Basis.IsNormalized()
            || NOT FMath::IsFinite(InForceReach) || InForceReach <= 0.0f
            || NOT FMath::IsFinite(InOwnSideTolerance) || InOwnSideTolerance <= Margin
            || NOT FMath::IsFinite(InQuery.MaxHipLateral)
            || NOT FMath::IsFinite(InQuery.FootContactRadius) || InQuery.FootContactRadius < 0.0f
            || InQuery.LegIndex < 0 || InQuery.Reservations.Num() < 0
            || InQuery.Reservations.Num() > MaxReservations
            || (InQuery.Reservations.Num() > 0 && InQuery.Reservations.GetData() == nullptr)
            || InUp != FVector::UpVector || InPlane.Normal != FVector::UpVector
            || FMath::Abs(InPlane.Point.Z) > FloatMax
            || static_cast<double>(static_cast<float>(InPlane.Point.Z)) != InPlane.Point.Z
            || (InIdenticalFinalPoint.IsSet() && InIdenticalFinalPoint.GetValue().ContainsNaN()))
        {
            return {};
        }
        const auto BodyY = InQuery.Basis.GetAxisY();
        if (BodyY.ContainsNaN() || NOT BodyY.IsNormalized() || BodyY.Z != 0.0
            || InQuery.Basis.GetAxisZ() != InUp)
        {
            return {};
        }
        double OutboardSquared = 0.0;
        if (NOT Get_PlantQueryDot(InQuery.Outboard, InQuery.Outboard, OutboardSquared)
            || OutboardSquared <= SMALL_NUMBER)
        {
            return {};
        }
        const auto Direction = InQuery.Outboard.GetSafeNormal();
        const auto Origin = FVector{InQuery.Hip.X, InQuery.Hip.Y, InPlane.Point.Z};
        FVector BodyOffset;
        double Lateral = 0.0;
        double Denominator = 0.0;
        double AlongBody = 0.0;
        if (Direction.ContainsNaN() || NOT Direction.IsNormalized() || Direction.Z != 0.0
            || NOT Get_PlantQueryDifference(Origin, InQuery.BodyLocation, BodyOffset)
            || NOT Get_PlantQueryDot(BodyOffset, BodyY, Lateral) || Lateral == 0.0
            || NOT Get_PlantQueryDot(Direction, Direction, Denominator) || Denominator <= 0.0
            || NOT Get_PlantQueryDot(Direction, BodyY, AlongBody))
        {
            return {};
        }
        const auto Sigma = Lateral > 0.0 ? 1.0 : -1.0;
        const auto BodyBoundary = static_cast<double>(InQuery.MaxHipLateral)
            - static_cast<double>(UnderBodyMargin) + Margin;
        const auto BodyDenominator = Sigma * AlongBody;
        if (NOT FMath::IsFinite(BodyBoundary) || BodyBoundary <= 0.0
            || Sigma * Lateral < BodyBoundary
            || NOT FMath::IsFinite(BodyDenominator) || BodyDenominator <= 0.0)
        {
            return {};
        }
        const auto OwnParameter = (static_cast<double>(InOwnSideTolerance) - Margin) / Denominator;
        const auto BodyParameter = (Sigma * Lateral - BodyBoundary) / BodyDenominator;
        if (NOT FMath::IsFinite(OwnParameter) || OwnParameter < 0.0
            || NOT FMath::IsFinite(BodyParameter) || BodyParameter < 0.0)
        {
            return {};
        }
        const auto Parameter = FMath::Min(OwnParameter, BodyParameter);
        FVector Anchor;
        if (NOT Get_PlantQueryImage(Origin, -Direction, Parameter, Anchor))
        {
            return {};
        }
        Anchor.Z = InPlane.Point.Z;
        double X[2];
        double Y[2];
        if (NOT Get_PlantQueryAxisCell(Anchor.X, X) || NOT Get_PlantQueryAxisCell(Anchor.Y, Y))
        {
            return {};
        }
        const auto InverseBasis = InQuery.Basis.Inverse();
        const auto ForceSquared = FMath::Square(static_cast<double>(InForceReach));
        const auto NativeBodyBand = InQuery.MaxHipLateral - UnderBodyMargin;
        if (NOT FMath::IsFinite(ForceSquared) || NOT FMath::IsFinite(NativeBodyBand))
        {
            return {};
        }
        // Preserve radius-before-self validation before any reservation may be ignored.
        if (InQuery.FootContactRadius > 0.0f)
        {
            for (const auto& Reservation : InQuery.Reservations)
            {
                const auto Radius = Reservation.Get_Radius();
                if (NOT FMath::IsFinite(Radius) || Radius < 0.0f)
                {
                    return {};
                }
                if (Radius == 0.0f || Reservation.Get_LegIndex() == InQuery.LegIndex)
                {
                    continue;
                }
                if (Reservation.Get_Position().ContainsNaN() || Reservation.Get_LegIndex() < 0)
                {
                    return {};
                }
            }
        }
        FVector Points[4];
        int32 PointCount = 0;
        for (int32 Corner = 0; Corner < 4; ++Corner)
        {
            const FVector Point{X[Corner & 1], Y[(Corner >> 1) & 1], Anchor.Z};
            auto Duplicate = false;
            for (int32 Index = 0; Index < PointCount; ++Index)
            {
                if (Points[Index] == Point)
                {
                    Duplicate = true;
                    break;
                }
            }
            if (Duplicate)
            {
                continue;
            }
            Points[PointCount++] = Point;
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
            if (OutTestPointCount != nullptr)
            {
                *OutTestPointCount = PointCount;
            }
#endif
            FVector HipOffset;
            FVector RoundedBodyOffset;
            double RoundedLateral = 0.0;
            if (Point.ContainsNaN()
                || NOT Get_PlantQueryDifference(Point, InQuery.Hip, HipOffset)
                || NOT Get_PlantQueryDifference(Point, InQuery.BodyLocation, RoundedBodyOffset)
                || NOT Get_PlantQueryDot(RoundedBodyOffset, BodyY, RoundedLateral))
            {
                continue;
            }
            const auto NativeBodyOffset = InverseBasis.RotateVector(RoundedBodyOffset);
            if (NativeBodyOffset.ContainsNaN() || FMath::Abs(NativeBodyOffset.Y) > FloatMax)
            {
                continue;
            }
            const auto NativeLateral = static_cast<float>(NativeBodyOffset.Y);
            const auto HipSquared = HipOffset.SizeSquared();
            if (NOT FMath::IsFinite(NativeLateral)
                || NOT FMath::IsFinite(HipSquared) || HipSquared > ForceSquared
                || FVector::Dist(Point, InQuery.Hip) > InForceReach
                || Sigma * RoundedLateral < BodyBoundary
                || Sigma * NativeBodyOffset.Y < BodyBoundary
                || Sigma * static_cast<double>(NativeLateral) < BodyBoundary
                || FMath::Abs(NativeLateral) < NativeBodyBand
                || NOT ck::Get_IsFootholdOnOwnSide(HipOffset, InQuery.Outboard, InOwnSideTolerance)
                || NOT ck::Get_IsProceduralFootContactAvailable(Point, InQuery.FootContactRadius,
                    InQuery.LegIndex, InQuery.Reservations))
            {
                continue;
            }
            // This point is deduplicated ONLY against the source-proven identical old final straight call.
            if (InIdenticalFinalPoint.IsSet() && Point == InIdenticalFinalPoint.GetValue())
            {
                continue;
            }
            return Point;
        }
        return {};
    }

    struct FTiltedPlantSeedContext
    {
        bool PaidNativeDownUnreachable = false;
        bool SearchGranted = false;
        int32 RemainingSlots = 0;
        bool StagingAtEntry = false;
        bool StagingNow = false;
    };

    // One query placement from a fresh plant plane. Native admission remains in the existing final block.
    auto
        Get_TiltedPlantSeedQueryPoint(
            const FFootholdQuery& InQuery,
            const FVector& InPaidSeed,
            const FVector& InPlant,
            const FVector& InLead,
            const FPlantSeedPlane& InPlane,
            float InForceReach,
            float InSearchRadius,
            const ck::FProceduralGaitReachPaceTrial& InTrial,
            const FTiltedPlantSeedContext& InContext,
            const TOptional<FVector>& InIdenticalPaidPoint)
        -> TOptional<FVector>
    {
        if (NOT InContext.PaidNativeDownUnreachable || NOT InContext.SearchGranted
            || InContext.RemainingSlots <= 0 || InContext.StagingAtEntry || InContext.StagingNow
            || InQuery.Basis.ContainsNaN() || NOT InQuery.Basis.IsNormalized()
            || NOT FMath::IsFinite(InQuery.Reach) || InQuery.Reach <= 0.0f
            || NOT FMath::IsFinite(InForceReach) || InForceReach <= 0.0f
            || NOT FMath::IsFinite(InSearchRadius) || InSearchRadius <= 0.0f
            || NOT FMath::IsFinite(InQuery.RestDrop) || NOT FMath::IsFinite(InQuery.MaxHipLateral)
            || NOT FMath::IsFinite(InQuery.FootContactRadius) || InQuery.FootContactRadius < 0.0f
            || InQuery.LegIndex < 0 || InQuery.Reservations.Num() < 0 || InQuery.Reservations.Num() > 64
            || (InQuery.Reservations.Num() > 0 && InQuery.Reservations.GetData() == nullptr))
        {
            return {};
        }
        constexpr auto FloatMax = static_cast<double>(std::numeric_limits<float>::max());
        const auto FitsFloat = [](const FVector& InPoint) -> bool
        {
            constexpr auto Limit = static_cast<double>(std::numeric_limits<float>::max());
            return FMath::IsFinite(InPoint.X) && FMath::Abs(InPoint.X) <= Limit
                && FMath::IsFinite(InPoint.Y) && FMath::Abs(InPoint.Y) <= Limit
                && FMath::IsFinite(InPoint.Z) && FMath::Abs(InPoint.Z) <= Limit;
        };
        if (NOT FitsFloat(InPaidSeed) || NOT FitsFloat(InPlant) || NOT FitsFloat(InLead)
            || NOT FitsFloat(InPlane.Point) || InPlane.Normal != FVector::UpVector
            || NOT FitsFloat(InQuery.Hip) || NOT FitsFloat(InQuery.BodyLocation)
            || NOT FitsFloat(InTrial.Get_Hip())
            || (InTrial.Get_PosedHip().IsSet() && NOT FitsFloat(InTrial.Get_PosedHip().GetValue()))
            || (InIdenticalPaidPoint.IsSet() && NOT FitsFloat(InIdenticalPaidPoint.GetValue())))
        {
            return {};
        }
        const auto Up = InQuery.Basis.GetAxisZ();
        double OutboardSquared = 0.0;
        if (Up.ContainsNaN() || NOT Up.IsNormalized() || Up == FVector::UpVector
            || InQuery.Outboard.ContainsNaN()
            || NOT Get_PlantQueryDot(InQuery.Outboard, InQuery.Outboard, OutboardSquared)
            || OutboardSquared <= SMALL_NUMBER
            || static_cast<double>(static_cast<float>(InPlane.Point.Z)) != InPlane.Point.Z)
        {
            return {};
        }
        const auto Point = FVector{static_cast<double>(static_cast<float>(InPaidSeed.X)),
            static_cast<double>(static_cast<float>(InPaidSeed.Y)), InPlane.Point.Z};
        FVector Displacement;
        FVector HipOffset;
        FVector BodyOffset;
        double LeadSquared = 0.0;
        double Progress = 0.0;
        double SearchSquared = 0.0;
        double HipSquared = 0.0;
        if (Point == InPlant || (InIdenticalPaidPoint.IsSet() && Point == InIdenticalPaidPoint.GetValue())
            || NOT Get_PlantQueryDifference(Point, InPlant, Displacement)
            || NOT Get_PlantQueryDifference(Point, InQuery.Hip, HipOffset)
            || NOT Get_PlantQueryDifference(Point, InQuery.BodyLocation, BodyOffset)
            || NOT Get_PlantQueryDot(InLead, InLead, LeadSquared) || LeadSquared <= 0.0
            || NOT Get_PlantQueryDot(Displacement, InLead, Progress)
            || Progress < 0.5 * LeadSquared || Progress > LeadSquared
            || NOT Get_PlantQueryDot(Displacement, Displacement, SearchSquared)
            || SearchSquared > FMath::Square(static_cast<double>(InSearchRadius))
            || NOT Get_PlantQueryDot(HipOffset, HipOffset, HipSquared)
            || HipSquared > FMath::Square(static_cast<double>(InForceReach))
            || FVector::Dist(Point, InQuery.Hip) > InForceReach)
        {
            return {};
        }
        const auto InverseBasis = InQuery.Basis.Inverse();
        const auto FromBody = InverseBasis.RotateVector(BodyOffset);
        const auto FromHip = InverseBasis.RotateVector(HipOffset);
        const auto TrialPoint = InverseBasis.RotateVector(Point);
        if (FromBody.ContainsNaN() || FromHip.ContainsNaN() || NOT FitsFloat(TrialPoint)
            || FMath::Abs(FromBody.Y) > FloatMax || FMath::Abs(FromHip.Z) > FloatMax)
        {
            return {};
        }
        FVector TrialOffset;
        double TrialSquared = 0.0;
        const auto PhysicalSquared = FMath::Square(static_cast<double>(InQuery.Reach) + 1.0e-3);
        if (NOT Get_PlantQueryDifference(TrialPoint, InTrial.Get_Hip(), TrialOffset)
            || NOT Get_PlantQueryDot(TrialOffset, TrialOffset, TrialSquared) || TrialSquared > PhysicalSquared
            || FVector::Dist(TrialPoint, InTrial.Get_Hip()) > static_cast<double>(InQuery.Reach) + 1.0e-3)
        {
            return {};
        }
        if (InTrial.Get_PosedHip().IsSet())
        {
            if (NOT Get_PlantQueryDifference(TrialPoint, InTrial.Get_PosedHip().GetValue(), TrialOffset)
                || NOT Get_PlantQueryDot(TrialOffset, TrialOffset, TrialSquared) || TrialSquared > PhysicalSquared
                || FVector::Dist(TrialPoint, InTrial.Get_PosedHip().GetValue()) > static_cast<double>(InQuery.Reach) + 1.0e-3)
            {
                return {};
            }
        }
        // Preserve the contact predicate's radius-before-self and zero-radius rules.
        if (InQuery.FootContactRadius > 0.0f)
        {
            for (const auto& Reservation : InQuery.Reservations)
            {
                const auto Radius = Reservation.Get_Radius();
                if (NOT FMath::IsFinite(Radius) || Radius < 0.0f)
                {
                    return {};
                }
                if (Radius == 0.0f || Reservation.Get_LegIndex() == InQuery.LegIndex)
                {
                    continue;
                }
                FVector Offset;
                double DistanceSquared = 0.0;
                if (Reservation.Get_LegIndex() < 0 || Reservation.Get_Position().ContainsNaN()
                    || NOT Get_PlantQueryDifference(Point, Reservation.Get_Position(), Offset)
                    || NOT Get_PlantQueryDot(Offset, Offset, DistanceSquared))
                {
                    return {};
                }
            }
        }
        if (NOT ck::Get_IsFootholdOnOwnSide(HipOffset, InQuery.Outboard,
                InboardToleranceShareOfReach * InQuery.Reach)
            || ck::Get_IsFootholdUnderBody(static_cast<float>(FromBody.Y), static_cast<float>(-FromHip.Z),
                InQuery.MaxHipLateral, InQuery.RestDrop, UnderBodyMargin)
            || NOT ck::Get_IsProceduralFootContactAvailable(Point, InQuery.FootContactRadius,
                InQuery.LegIndex, InQuery.Reservations))
        {
            return {};
        }
        return Point;
    }

    auto
        Get_IsPlantSeedProbeIdentical(
            const FCk_ProceduralGait_Probe& InPaid,
            const FCk_ProceduralGait_Probe& InProposed)
        -> bool
    {
        const auto& PaidFilter = InPaid.Get_QueryFilter();
        const auto& ProposedFilter = InProposed.Get_QueryFilter();
        return InPaid.Get_Up() == InProposed.Get_Up()
            && InPaid.Get_Down() == InProposed.Get_Down()
            && InPaid.Get_OutwardLean() == InProposed.Get_OutwardLean()
            && InPaid.Get_ContactGrace().Get_Seconds() == InProposed.Get_ContactGrace().Get_Seconds()
            && PaidFilter.Get_Channel() == ProposedFilter.Get_Channel()
            && PaidFilter.Get_MinResponse() == ProposedFilter.Get_MinResponse()
            && PaidFilter.Get_BodySelection() == ProposedFilter.Get_BodySelection()
            && PaidFilter.Get_CollideWithBackFacesConvex() == ProposedFilter.Get_CollideWithBackFacesConvex();
    }

    // Preserve an already-valid proposal. Only an inboard seed may move along its fresh native contact plane;
    // this changes query geometry, never contact authority, admission thresholds or the single allocated probe.
    auto
        Get_OwnSidePlantSeedPoint(
            const FVector& InSeed,
            const FVector& InPlant,
            const FVector& InHip,
            const FVector& InOutboard,
            const FPlantSeedPlane& InPlane,
            float InReach,
            float InForceReach,
            float InSearchRadius)
        -> FVector
    {
        const auto IsFinite3 = [](double InX, double InY, double InZ)
        { return FMath::IsFinite(InX) && FMath::IsFinite(InY) && FMath::IsFinite(InZ); };
        if (InSeed.ContainsNaN() || InPlant.ContainsNaN() || InHip.ContainsNaN() || InOutboard.ContainsNaN()
            || NOT FMath::IsFinite(InReach) || InReach <= 0.0f
            || NOT FMath::IsFinite(InForceReach) || InForceReach <= 0.0f
            || NOT FMath::IsFinite(InSearchRadius) || InSearchRadius <= 0.0f)
        { return InSeed; }

        const auto OutboardSquared = InOutboard.SizeSquared();
        const auto HipX = InSeed.X - InHip.X;
        const auto HipY = InSeed.Y - InHip.Y;
        const auto HipZ = InSeed.Z - InHip.Z;
        if (NOT FMath::IsFinite(OutboardSquared) || OutboardSquared <= SMALL_NUMBER
            || NOT IsFinite3(HipX, HipY, HipZ))
        { return InSeed; }
        const auto Tolerance = InboardToleranceShareOfReach * InReach;
        if (ck::Get_IsFootholdOnOwnSide(FVector{HipX, HipY, HipZ}, InOutboard, Tolerance))
        { return InSeed; }

        if (InPlane.Point.ContainsNaN() || InPlane.Normal.ContainsNaN() || NOT InPlane.Normal.IsNormalized())
        { return InSeed; }
        const auto Outboard = InOutboard.GetSafeNormal();
        const auto Normal = InPlane.Normal.GetSafeNormal();
        const auto PlaneX = InSeed.X - InPlane.Point.X;
        const auto PlaneY = InSeed.Y - InPlane.Point.Y;
        const auto PlaneZ = InSeed.Z - InPlane.Point.Z;
        if (NOT IsFinite3(PlaneX, PlaneY, PlaneZ))
        { return InSeed; }
        const auto PlaneDistance = PlaneX * Normal.X + PlaneY * Normal.Y + PlaneZ * Normal.Z;
        const auto OutboardNormal = FVector::DotProduct(Outboard, Normal);
        const auto TangentX = Outboard.X - OutboardNormal * Normal.X;
        const auto TangentY = Outboard.Y - OutboardNormal * Normal.Y;
        const auto TangentZ = Outboard.Z - OutboardNormal * Normal.Z;
        const auto Denominator = TangentX * Outboard.X + TangentY * Outboard.Y + TangentZ * Outboard.Z;
        if (NOT FMath::IsFinite(PlaneDistance) || NOT IsFinite3(TangentX, TangentY, TangentZ)
            || NOT FMath::IsFinite(Denominator) || Denominator <= SMALL_NUMBER)
        { return InSeed; }

        const auto ProjectedX = InSeed.X - PlaneDistance * Normal.X;
        const auto ProjectedY = InSeed.Y - PlaneDistance * Normal.Y;
        const auto ProjectedZ = InSeed.Z - PlaneDistance * Normal.Z;
        const auto ProjectedHipX = ProjectedX - InHip.X;
        const auto ProjectedHipY = ProjectedY - InHip.Y;
        const auto ProjectedHipZ = ProjectedZ - InHip.Z;
        if (NOT IsFinite3(ProjectedX, ProjectedY, ProjectedZ) || NOT IsFinite3(ProjectedHipX, ProjectedHipY, ProjectedHipZ))
        { return InSeed; }
        const auto ProjectedOwnSide = ProjectedHipX * Outboard.X + ProjectedHipY * Outboard.Y + ProjectedHipZ * Outboard.Z;
        // Match the existing 0.1 cm boundary-query inset; the admission tolerance itself remains unchanged.
        constexpr auto QueryMargin = 0.1;
        const auto Scale = (-static_cast<double>(Tolerance) + QueryMargin - ProjectedOwnSide) / Denominator;
        if (NOT FMath::IsFinite(ProjectedOwnSide) || NOT FMath::IsFinite(Scale))
        { return InSeed; }
        const auto ResultX = ProjectedX + Scale * TangentX;
        const auto ResultY = ProjectedY + Scale * TangentY;
        const auto ResultZ = ProjectedZ + Scale * TangentZ;
        const auto FromHipX = ResultX - InHip.X;
        const auto FromHipY = ResultY - InHip.Y;
        const auto FromHipZ = ResultZ - InHip.Z;
        const auto FromPlantX = ResultX - InPlant.X;
        const auto FromPlantY = ResultY - InPlant.Y;
        const auto FromPlantZ = ResultZ - InPlant.Z;
        const auto FromPlaneX = ResultX - InPlane.Point.X;
        const auto FromPlaneY = ResultY - InPlane.Point.Y;
        const auto FromPlaneZ = ResultZ - InPlane.Point.Z;
        if (NOT IsFinite3(ResultX, ResultY, ResultZ) || NOT IsFinite3(FromHipX, FromHipY, FromHipZ)
            || NOT IsFinite3(FromPlantX, FromPlantY, FromPlantZ) || NOT IsFinite3(FromPlaneX, FromPlaneY, FromPlaneZ))
        { return InSeed; }
        const auto HipSquared = FromHipX * FromHipX + FromHipY * FromHipY + FromHipZ * FromHipZ;
        const auto PlantSquared = FromPlantX * FromPlantX + FromPlantY * FromPlantY + FromPlantZ * FromPlantZ;
        const auto PlaneResidual = FromPlaneX * Normal.X + FromPlaneY * Normal.Y + FromPlaneZ * Normal.Z;
        if (NOT FMath::IsFinite(HipSquared) || NOT FMath::IsFinite(PlantSquared) || NOT FMath::IsFinite(PlaneResidual)
            || HipSquared > FMath::Square(static_cast<double>(InForceReach))
            || PlantSquared > FMath::Square(static_cast<double>(InSearchRadius))
            || FMath::Abs(PlaneResidual) > FaceTopGeometryTolerance
            || NOT ck::Get_IsFootholdOnOwnSide(FVector{FromHipX, FromHipY, FromHipZ}, InOutboard, Tolerance))
        { return InSeed; }

        return FVector{ResultX, ResultY, ResultZ};
    }

    auto
        Get_LegFoothold(
            ck::EProceduralFootholdSource InSource)
        -> ECk_ProceduralLeg_Foothold
    {
        switch (InSource)
        {
            case ck::EProceduralFootholdSource::None: return ECk_ProceduralLeg_Foothold::None;
            case ck::EProceduralFootholdSource::Ideal: return ECk_ProceduralLeg_Foothold::Ideal;
            case ck::EProceduralFootholdSource::Held: return ECk_ProceduralLeg_Foothold::Held;
            case ck::EProceduralFootholdSource::Front: return ECk_ProceduralLeg_Foothold::Front;
            case ck::EProceduralFootholdSource::Inward: return ECk_ProceduralLeg_Foothold::Inward;
            case ck::EProceduralFootholdSource::Outward: return ECk_ProceduralLeg_Foothold::Outward;
            case ck::EProceduralFootholdSource::Ring: return ECk_ProceduralLeg_Foothold::Ring;
        }
        return ECk_ProceduralLeg_Foothold::None;
    }

    auto
        Get_LegFootholdVerdict(
            ck::EProceduralFootholdVerdict InVerdict)
        -> ECk_ProceduralLeg_FootholdVerdict
    {
        switch (InVerdict)
        {
            case ck::EProceduralFootholdVerdict::Usable: return ECk_ProceduralLeg_FootholdVerdict::Usable;
            case ck::EProceduralFootholdVerdict::Miss: return ECk_ProceduralLeg_FootholdVerdict::Miss;
            case ck::EProceduralFootholdVerdict::Unreachable: return ECk_ProceduralLeg_FootholdVerdict::Unreachable;
            case ck::EProceduralFootholdVerdict::TooSteep: return ECk_ProceduralLeg_FootholdVerdict::TooSteep;
            case ck::EProceduralFootholdVerdict::Occluded: return ECk_ProceduralLeg_FootholdVerdict::Occluded;
            case ck::EProceduralFootholdVerdict::Inboard: return ECk_ProceduralLeg_FootholdVerdict::Inboard;
            case ck::EProceduralFootholdVerdict::UnderBody: return ECk_ProceduralLeg_FootholdVerdict::UnderBody;
            case ck::EProceduralFootholdVerdict::Reserved: return ECk_ProceduralLeg_FootholdVerdict::Reserved;
        }
        return ECk_ProceduralLeg_FootholdVerdict::Miss;
    }

    // The trace from the hip to InPoint: the solid it meets farther than the occlusion tolerance from InPoint. A trace that
    // starts inside a solid (fraction 0, a hip inside the body's own collider) tells nothing, so it finds no occluder.
    auto
        Get_Occluder(
            const FFootholdQuery& InQuery,
            const FVector& InPoint,
            int32& InOutRayCount)
        -> TOptional<FCk_Jolt_HitResult>
    {
        const auto Hit = UCk_Utils_JoltQuery_UE::Get_RayCast(InQuery.World, InQuery.Hip, InPoint, InQuery.Probe->Get_QueryFilter());
        ++InOutRayCount;

        const auto Blocks = Hit.Get_HasHit() && Hit.Get_Fraction() > 0.0f && NOT Hit.Get_Position().ContainsNaN()
            && FVector::Dist(Hit.Get_Position(), InPoint) > InQuery.Foothold->Get_OcclusionTolerance();
        if (NOT Blocks)
        { return {}; }

        return Hit;
    }

    // A face cast (inward, outward): the first surface on the segment that faces it.
    auto
        DoCast_Segment(
            const FFootholdQuery& InQuery,
            const FVector& InStart,
            const FVector& InEnd,
            int32& InOutRayCount)
        -> TOptional<FCk_Jolt_HitResult>
    {
        const auto Direction = (InEnd - InStart).GetSafeNormal();
        if (Direction.IsNearlyZero())
        { return {}; }

        const auto Hit = UCk_Utils_JoltQuery_UE::Get_RayCast(InQuery.World, InStart, InEnd, InQuery.Probe->Get_QueryFilter());
        ++InOutRayCount;

        if (NOT Get_TrustedHit(Hit, Direction))
        { return {}; }

        return Hit;
    }

    // Validation of a hit, in order: Unreachable beyond the force-step reach (beyond the target reach at once for a face
    // hit), TooSteep for a search candidate beyond the max angle, Inboard for a search candidate off the leg's own side of
    // its hip, UnderBody below the body, Occluded when the hip cannot see it, Reserved by another foot, else Usable.
    auto
        DoValidate(
            const FFootholdQuery& InQuery,
            FFootholdCandidate& InOutCandidate,
            EFootholdStage InStage,
            int32& InOutRayCount,
            ck::EProceduralHeightSeedFailure* OutSeedFailure = nullptr)
        -> void
    {
        if (OutSeedFailure != nullptr)
        { *OutSeedFailure = ck::EProceduralHeightSeedFailure::None; }
        InOutCandidate.NativeReceipt.Reset();
        const auto HipDistance = FVector::Dist(InOutCandidate.Position, InQuery.Hip);
        const auto LimitFraction = InOutCandidate.Cast == ck::EProceduralFootholdCast::Face
            ? InQuery.Step->Get_TargetReachFraction()
            : InQuery.Step->Get_ForceStepReachFraction();
        if (HipDistance > LimitFraction * InQuery.Reach)
        {
            InOutCandidate.Verdict = ck::EProceduralFootholdVerdict::Unreachable;
            if (OutSeedFailure != nullptr && InOutCandidate.NativeHit
                && InOutCandidate.Cast == ck::EProceduralFootholdCast::Down)
            { *OutSeedFailure = ck::EProceduralHeightSeedFailure::TrustedDownFinalForceReject; }
            return;
        }

        const auto SupportNormal = InQuery.Basis.Inverse().RotateVector(InOutCandidate.Normal);
        if (InStage == EFootholdStage::Search && NOT ck::Get_IsFootholdLevelEnough(SupportNormal, InQuery.Foothold->Get_MaxAngle()))
        {
            InOutCandidate.Verdict = ck::EProceduralFootholdVerdict::TooSteep;
            return;
        }

        if (InStage == EFootholdStage::Search && NOT ck::Get_IsFootholdOnOwnSide(InOutCandidate.Position - InQuery.Hip,
                InQuery.Outboard, InboardToleranceShareOfReach * InQuery.Reach))
        {
            InOutCandidate.Verdict = ck::EProceduralFootholdVerdict::Inboard;
            return;
        }

        const auto FromBody = InQuery.Basis.Inverse().RotateVector(InOutCandidate.Position - InQuery.BodyLocation);
        const auto FromHip = InQuery.Basis.Inverse().RotateVector(InOutCandidate.Position - InQuery.Hip);
        if (InStage == EFootholdStage::Search && ck::Get_IsFootholdUnderBody(static_cast<float>(FromBody.Y),
                static_cast<float>(-FromHip.Z), InQuery.MaxHipLateral, InQuery.RestDrop, UnderBodyMargin))
        {
            InOutCandidate.Verdict = ck::EProceduralFootholdVerdict::UnderBody;
            return;
        }

        InOutCandidate.Occluder = Get_Occluder(InQuery, InOutCandidate.Position, InOutRayCount);
        if (InOutCandidate.Occluder.IsSet())
        {
            InOutCandidate.Verdict = ck::EProceduralFootholdVerdict::Occluded;
            return;
        }

        if (NOT ck::Get_IsProceduralFootContactAvailable(InOutCandidate.Position, InQuery.FootContactRadius,
                InQuery.LegIndex, InQuery.Reservations))
        {
            InOutCandidate.Verdict = ck::EProceduralFootholdVerdict::Reserved;
            if (InQuery.RejectedReservation != nullptr)
            { *InQuery.RejectedReservation = true; }
            return;
        }

        InOutCandidate.Verdict = ck::EProceduralFootholdVerdict::Usable;
        if (InQuery.CaptureTargetReceipt && InOutCandidate.NativeHit)
        {
            InOutCandidate.NativeReceipt = FNativeTargetReceipt{
                InQuery.ReceiptOwner, GFrameCounter, InQuery.ReceiptInputSolve, InQuery.ReceiptBody, InQuery.LegIndex,
                InOutCandidate.Position, InOutCandidate.Normal, InQuery.Hip, InQuery.Probe->Get_QueryFilter(),
                InOutCandidate.Cast, InStage == EFootholdStage::Search, true, {}};
        }
        if (InQuery.MatchingLandingReceipt != nullptr && InQuery.CommittedPoint.IsSet()
            && InOutCandidate.Position.Equals(InQuery.CommittedPoint.GetValue(), 1.0e-3))
        {
            *InQuery.MatchingLandingReceipt = FNativeLandingReceipt{
                InOutCandidate.Position, InOutCandidate.Normal, InOutCandidate.Cast};
        }
    }

    auto
        DoObserve_HeightCandidate(
            const FFootholdQuery& InQuery,
            const FCk_Jolt_HitResult& InHit)
        -> void
    {
        if (InQuery.HeightObservations == nullptr)
        { return; }
        auto& Observations = *InQuery.HeightObservations;
        const auto& Point = InHit.Get_Position();
        if (Observations.ContainsByPredicate([&Point](const auto& InExisting)
            { return InExisting.Point.Equals(Point, 1.0e-3); }))
        { return; }
        const auto Distance = FVector::VectorPlaneProject(Point - InQuery.Hip, InQuery.Basis.GetAxisZ()).SizeSquared();
        const auto Insert = Observations.IndexOfByPredicate([&](const auto& InExisting)
        {
            return Distance < FVector::VectorPlaneProject(InExisting.Point - InQuery.Hip, InQuery.Basis.GetAxisZ()).SizeSquared();
        });
        if (Insert != INDEX_NONE)
        { Observations.Insert(FHeightObservation{Point, InHit.Get_Normal().GetSafeNormal()}, Insert); }
        else if (Observations.Num() < 2)
        { Observations.Add(FHeightObservation{Point, InHit.Get_Normal().GetSafeNormal()}); }
        if (Observations.Num() > 2)
        { Observations.SetNum(2); }
    }

    // Reuse an actual trusted native hit; this never repeats its initial ground probe or height observation.
    auto
        DoReprobe_DownTowardHip(
            const FFootholdQuery& InQuery,
            FFootholdCandidate InCandidate,
            EGroundProbeAttempts InAttempts,
            EFootholdStage InStage,
            int32& InOutRayCount,
            FCk_ProceduralAnimation_DebugProbe* InDebugProbe,
            ck::EProceduralHeightSeedFailure* OutSeedFailure = nullptr)
        -> FFootholdCandidate
    {
        if (OutSeedFailure != nullptr)
        { *OutSeedFailure = ck::EProceduralHeightSeedFailure::None; }
        const auto TargetLimit = InQuery.Step->Get_TargetReachFraction() * InQuery.Reach;
        if (FVector::Dist(InCandidate.Position, InQuery.Hip) > TargetLimit)
        {
            const auto Up = InQuery.Basis.GetAxisZ();
            const auto InverseBasis = InQuery.Basis.Inverse();
            auto StraightDown = *InQuery.Probe;
            StraightDown.Set_OutwardLean(0.0f);
            const auto Point = InQuery.Basis.RotateVector(ck::FProceduralGaitSolver::ClampToReach(
                InverseBasis.RotateVector(InQuery.Hip), InverseBasis.RotateVector(InCandidate.Position), TargetLimit));
            const auto Radial = FVector::VectorPlaneProject(Point - InQuery.BodyLocation, Up).GetSafeNormal();
            const auto Reprobed = Get_GroundHit(InQuery.World, Point, Up, Radial, StraightDown, InAttempts,
                InOutRayCount, InDebugProbe);
            if (NOT Reprobed.IsSet())
            {
                InCandidate.Verdict = ck::EProceduralFootholdVerdict::Unreachable;
                if (OutSeedFailure != nullptr && InCandidate.NativeHit
                    && InCandidate.Cast == ck::EProceduralFootholdCast::Down)
                { *OutSeedFailure = ck::EProceduralHeightSeedFailure::TrustedInitialDownClampReprobeMiss; }
                return InCandidate;
            }

            DoObserve_HeightCandidate(InQuery, Reprobed.GetValue());
            InCandidate.Position = Reprobed->Get_Position();
            InCandidate.Normal = Reprobed->Get_Normal().GetSafeNormal();
            InCandidate.NativeHit = true;
        }

        DoValidate(InQuery, InCandidate, InStage, InOutRayCount, OutSeedFailure);
        return InCandidate;
    }

    // A down ray under InPoint. When its first, leaned attempt meets a face, one straight ray is cast under the same point,
    // and level ground it meets within the force-step reach of the hip is the candidate instead: the lean reaches out past the
    // point and can meet the face of a pillar or a riser beside the floor or the tread under it. Over a gap the straight ray
    // finds nothing in reach and the face stands. With ClampTowardHip, ground farther than the target reach from the hip is
    // clamped toward the hip like the query point and probed once more there, straight down: the solver's own clamp keeps a
    // target's support-frame height, which across a convex bend lies inside the ground, and a second probe leaning out like
    // the first could clip the same edge again. Ground the second probe misses stays Unreachable, as out of reach.
    auto
        DoProbe_Down(
            const FFootholdQuery& InQuery,
            const FVector& InPoint,
            ck::EProceduralFootholdSource InSource,
            EGroundProbeAttempts InAttempts,
            EFootholdReprobe InReprobe,
            EFootholdStage InStage,
            int32& InOutRayCount,
            FCk_ProceduralAnimation_DebugProbe* InDebugProbe,
            ck::EProceduralHeightSeedFailure* OutSeedFailure = nullptr)
        -> FFootholdCandidate
    {
        if (OutSeedFailure != nullptr)
        { *OutSeedFailure = ck::EProceduralHeightSeedFailure::None; }
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
        auto* HeightTrace = InQuery.HeightSeedObservation;
        if (HeightTrace != nullptr) { ck_procedural_height_seed_test::Put(*HeightTrace, 13, 0); ck_procedural_height_seed_test::Put(*HeightTrace, 16, 0); }
#endif
        const auto Up = InQuery.Basis.GetAxisZ();
        const auto ProbeAt = [&](const FVector& InProbePoint, const FCk_ProceduralGait_Probe& InProbe) -> TOptional<FCk_Jolt_HitResult>
        {
            const auto Radial = FVector::VectorPlaneProject(InProbePoint - InQuery.BodyLocation, Up).GetSafeNormal();
            return Get_GroundHit(InQuery.World, InProbePoint, Up, Radial, InProbe, InAttempts, InOutRayCount, InDebugProbe);
        };

        auto Candidate = FFootholdCandidate{InPoint, Up, InSource, ck::EProceduralFootholdVerdict::Miss, ck::EProceduralFootholdCast::Down};
        auto Hit = ProbeAt(InPoint, *InQuery.Probe);
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
        if (HeightTrace != nullptr)
        { ck_procedural_height_seed_test::Put(*HeightTrace, 12, Hit.IsSet()); if (Hit.IsSet()) { ck_procedural_height_seed_test::Vector(*HeightTrace, 17, Hit->Get_Position()); } }
#endif
        if (NOT Hit.IsSet())
        { return Candidate; }
        DoObserve_HeightCandidate(InQuery, Hit.GetValue());

        if (InQuery.Probe->Get_OutwardLean() > 0.0f && ck::FProceduralGaitSolver::Get_IsFaceNormal(Hit->Get_Normal(), Up))
        {
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
            if (HeightTrace != nullptr) { ck_procedural_height_seed_test::Put(*HeightTrace, 13, 1); }
#endif
            const auto StraightHit = UCk_Utils_JoltQuery_UE::Get_RayCast(InQuery.World, InPoint + Up * InQuery.Probe->Get_Up(),
                InPoint - Up * InQuery.Probe->Get_Down(), InQuery.Probe->Get_QueryFilter());
            ++InOutRayCount;
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
            if (HeightTrace != nullptr)
            { ck_procedural_height_seed_test::Put(*HeightTrace, 14, StraightHit.Get_HasHit()); if (StraightHit.Get_HasHit()) { ck_procedural_height_seed_test::Vector(*HeightTrace, 20, StraightHit.Get_Position()); } }
#endif
            if (Get_TrustedHit(StraightHit, -Up))
            { DoObserve_HeightCandidate(InQuery, StraightHit); }
            const auto LevelInReach = Get_TrustedHit(StraightHit, -Up)
                && NOT ck::FProceduralGaitSolver::Get_IsFaceNormal(StraightHit.Get_Normal(), Up)
                && FVector::Dist(StraightHit.Get_Position(), InQuery.Hip) <= InQuery.Step->Get_ForceStepReachFraction() * InQuery.Reach;
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
            if (HeightTrace != nullptr) { ck_procedural_height_seed_test::Put(*HeightTrace, 15, LevelInReach); }
#endif
            if (LevelInReach)
            {
                Hit = StraightHit;
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
                if (HeightTrace != nullptr) { ck_procedural_height_seed_test::Put(*HeightTrace, 16, 1); }
#endif
            }
        }

        Candidate.Position = Hit->Get_Position();
        Candidate.Normal = Hit->Get_Normal().GetSafeNormal();
        Candidate.NativeHit = true;
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
        if (HeightTrace != nullptr) { ck_procedural_height_seed_test::Vector(*HeightTrace, 23, Hit->Get_Position()); }
#endif

        if (InReprobe == EFootholdReprobe::ClampTowardHip)
        { return DoReprobe_DownTowardHip(InQuery, Candidate, InAttempts, InStage, InOutRayCount, InDebugProbe, OutSeedFailure); }

        DoValidate(InQuery, Candidate, InStage, InOutRayCount, OutSeedFailure);
        return Candidate;
    }

    // A straight ray just inside a face may find the top its leaned or diagonal ray did not sample. The caller supplies
    // the inset: the ideal keeps its existing query, while a selected search face can cap the inset to its usable strip.
    auto
        DoProbe_FaceTop(
            const FFootholdQuery& InQuery,
            const FFootholdCandidate& InFace,
            double InInset,
            int32& InOutRayCount)
        -> TOptional<FFootholdCandidate>
    {
        const auto FaceNormalInPlane = FVector::VectorPlaneProject(InFace.Normal, InQuery.Basis.GetAxisZ()).GetSafeNormal();
        if (FaceNormalInPlane.IsNearlyZero())
        { return {}; }

        auto StraightProbe = *InQuery.Probe;
        StraightProbe.Set_OutwardLean(0.0f);
        auto TopQuery = InQuery;
        TopQuery.Probe = &StraightProbe;
        const auto Source = InFace.Source == ck::EProceduralFootholdSource::Ideal
            || InFace.Source == ck::EProceduralFootholdSource::Held
            ? ck::EProceduralFootholdSource::Inward : InFace.Source;
        return DoProbe_Down(TopQuery, InFace.Position - FaceNormalInPlane * InInset, Source,
            EGroundProbeAttempts::RetryFromInsideSolid, EFootholdReprobe::ClampTowardHip, EFootholdStage::Search, InOutRayCount,
            nullptr);
    }

    auto
        DoProbe_Face(
            const FFootholdQuery& InQuery,
            const TOptional<FCk_Jolt_HitResult>& InHit,
            const FVector& InMissPosition,
            ck::EProceduralFootholdSource InSource,
            EFootholdStage InStage,
            int32& InOutRayCount)
        -> FFootholdCandidate
    {
        auto Candidate = FFootholdCandidate{InMissPosition, InQuery.Basis.GetAxisZ(), InSource, ck::EProceduralFootholdVerdict::Miss,
            ck::EProceduralFootholdCast::Face};
        if (NOT InHit.IsSet())
        { return Candidate; }

        Candidate.Position = InHit->Get_Position();
        Candidate.Normal = InHit->Get_Normal().GetSafeNormal();
        Candidate.NativeHit = true;
        DoValidate(InQuery, Candidate, InStage, InOutRayCount);
        return Candidate;
    }

    auto
        DoProbe_AlongNormal(
            const FFootholdQuery& InQuery,
            const ck::FProceduralFootholdState& InHold,
            int32& InOutRayCount,
            FCk_ProceduralAnimation_DebugProbe* InDebugProbe)
        -> FFootholdCandidate
    {
        const auto Normal = InHold.Get_Normal().GetSafeNormal();
        const auto Start = InHold.Get_Position() + Normal * FaceHoldProbeHalfSpan;
        const auto End = InHold.Get_Position() - Normal * FaceHoldProbeHalfSpan;
        const auto Hit = DoCast_Segment(InQuery, Start, End, InOutRayCount);

        if (ck::IsValid(InDebugProbe, ck::IsValid_Policy_NullptrOnly{}))
        {
            constexpr auto SingleAttempt = 1;
            InDebugProbe->Set_Start(Start)
                .Set_End(End)
                .Set_AttemptCount(SingleAttempt)
                .Set_Hit(Hit.IsSet())
                .Set_HitFraction(Hit.IsSet() ? Hit->Get_Fraction() : 1.0f)
                .Set_HitPosition(Hit.IsSet() ? Hit->Get_Position() : End)
                .Set_HitNormal(Hit.IsSet() ? Hit->Get_Normal() : FVector::ZeroVector);
        }

        return DoProbe_Face(InQuery, Hit, InHold.Get_Position(), ck::EProceduralFootholdSource::Held, EFootholdStage::IdealOrHeld,
            InOutRayCount);
    }

    auto
        DoRecord_Candidate(
            const FFootholdCandidate& InCandidate,
            TArray<FFootholdCandidate, TInlineAllocator<24>>& OutCandidates,
            FCk_ProceduralAnimation_DebugLeg* InOutDebugLeg)
        -> void
    {
        OutCandidates.Add(InCandidate);
        if (ck::Is_NOT_Valid(InOutDebugLeg, ck::IsValid_Policy_NullptrOnly{}))
        { return; }

        InOutDebugLeg->Get_Footholds().Add(FCk_ProceduralAnimation_DebugFoothold{}
            .Set_Position(InCandidate.Position)
            .Set_Normal(InCandidate.Normal)
            .Set_Source(InCandidate.Source)
            .Set_Verdict(InCandidate.Verdict));
    }

    auto
        DoFinish(
            const FFootholdCandidate& InCandidate,
            int32 InIndex,
            ck::EProceduralFootholdVerdict InIdealVerdict,
            FCk_ProceduralAnimation_DebugLeg* InOutDebugLeg)
        -> FFoothold
    {
        if (ck::IsValid(InOutDebugLeg, ck::IsValid_Policy_NullptrOnly{}))
        {
            InOutDebugLeg->Set_ChosenFoothold(InIndex)
                .Set_FootholdSource(InCandidate.Source);
        }
        auto Result = FFoothold{InCandidate.Position, InCandidate.Normal, InCandidate.Source, InIdealVerdict, 0, InCandidate.Cast};
        Result.NativeReceipt = InCandidate.NativeReceipt;
        return Result;
    }

    // The held foothold is re-validated at its own point and the ideal target probed as the gait always probed it. A
    // swinging leg keeps a usable hold. Otherwise a usable hold within the keep radius of the ideal target stays the
    // target unless the ideal exposes an admitted new face or an obstruction, so agreeing spots do not cause hopping. A
    // usable non-face ideal beyond the keep radius clears the hold. An unusable ideal, or one on a face (its normal farther than
    // ProceduralGaitFaceAngleDegrees from the support up), runs the search when the budget has one left and the leg is not
    // backing off a search that found nothing. A usable hold within the leg's force-step reach of the ideal serves while it
    // lies ahead of the hip along the travel, or the body is at rest: an ideal hanging beside a beam or over a gap would
    // otherwise drop the hold its own search found, and a pick beside a moving ideal would be searched again every frame.
    // A hold beyond the keep radius and behind the hip along the travel, one the leg has walked on past, competes in the
    // search as one more candidate, at its own distance from the ideal: a spot beside the ideal beats it, and a hold near
    // the ideal keeps winning. Farther than the force-step reach, the body has walked away from the hold (up a log, over a
    // crest) and a leg kept on it re-plants behind its rest point, so the hold is cleared. A face ideal competes too, with
    // the costs measured from it: a top within half the reach beats a vertical face. The least-cost candidate is the target
    // and, when the search found it, the new hold; when the search finds nothing better, or the leg is past its budget or
    // backing off, the hold stays the target, so the planted foot keeps standing on it, else the face ideal. Every ray
    // increments InOutRayCount; the debug leg, when given, receives every candidate.
    auto
        Get_Foothold(
            const FFootholdQuery& InQuery,
            ck::FProceduralFootholdState& InOutState,
            FFootholdSearchBudget& InOutBudget,
            int32& InOutRayCount,
            FCk_ProceduralAnimation_DebugLeg* InOutDebugLeg)
        -> FFoothold
    {
        auto Candidates = TArray<FFootholdCandidate, TInlineAllocator<24>>{};
        if (ck::IsValid(InOutDebugLeg, ck::IsValid_Policy_NullptrOnly{}))
        {
            InOutDebugLeg->Get_Footholds().Reset();
            InOutDebugLeg->Set_ChosenFoothold(INDEX_NONE)
                .Set_FootholdSource(ck::EProceduralFootholdSource::None);
        }

        const auto& FootholdTuning = *InQuery.Foothold;
        auto* DebugProbe = ck::IsValid(InOutDebugLeg, ck::IsValid_Policy_NullptrOnly{}) ? &InOutDebugLeg->Get_Probe() : nullptr;

        auto Held = FFootholdCandidate{};
        auto HeldIndex = int32{INDEX_NONE};
        if (InOutState.Get_Hold() == ck::EProceduralFootholdHold::Held)
        {
            Held = InOutState.Get_Cast() == ck::EProceduralFootholdCast::Face
                ? DoProbe_AlongNormal(InQuery, InOutState, InOutRayCount, DebugProbe)
                : DoProbe_Down(InQuery, InOutState.Get_Position(), ck::EProceduralFootholdSource::Held,
                    EGroundProbeAttempts::RetryFromInsideSolid, EFootholdReprobe::None, EFootholdStage::IdealOrHeld, InOutRayCount,
                    DebugProbe);
            DoRecord_Candidate(Held, Candidates, InOutDebugLeg);
            HeldIndex = Candidates.Num() - 1;
        }
        const auto HoldIsUsable = HeldIndex != INDEX_NONE && Held.Verdict == ck::EProceduralFootholdVerdict::Usable;
        if (HeldIndex != INDEX_NONE && NOT HoldIsUsable)
        { InOutState = ck::FProceduralFootholdState{}; }

        const auto Ideal = DoProbe_Down(InQuery, InQuery.Ideal, ck::EProceduralFootholdSource::Ideal,
            EGroundProbeAttempts::Every, EFootholdReprobe::ClampTowardHip, EFootholdStage::IdealOrHeld, InOutRayCount, DebugProbe);
        DoRecord_Candidate(Ideal, Candidates, InOutDebugLeg);
        const auto IdealIndex = Candidates.Num() - 1;

        const auto MatchesBlockedFace = [&InQuery](const FFootholdCandidate& InCandidate) -> bool
        {
            if (NOT InQuery.BlockedFace.IsSet() || InCandidate.Verdict != ck::EProceduralFootholdVerdict::Usable)
            { return false; }
            const auto& Face = InQuery.BlockedFace.GetValue();
            const auto Normal = Face.Normal.GetSafeNormal();
            constexpr auto PlaneTolerance = 1.0f;
            constexpr auto NormalAngleDegrees = 15.0f;
            return FMath::Abs(FVector::DotProduct(InCandidate.Position - Face.Point, Normal)) <= PlaneTolerance
                && FVector::DotProduct(InCandidate.Normal.GetSafeNormal(), Normal)
                    >= FMath::Cos(FMath::DegreesToRadians(NormalAngleDegrees));
        };

        const auto AcquireAdaptiveTop = InQuery.Planted && NOT InQuery.BlockedFace.IsSet() && InQuery.AdaptiveTop.IsSet();
        const auto MatchesAdaptiveTop = [&](const FFootholdCandidate& InCandidate) -> bool
        {
            if (NOT AcquireAdaptiveTop || InCandidate.Verdict != ck::EProceduralFootholdVerdict::Usable)
            { return false; }
            const auto& Top = InQuery.AdaptiveTop.GetValue();
            constexpr auto PlaneTolerance = 1.0f;
            constexpr auto NormalAngleDegrees = 15.0f;
            return FMath::Abs(FVector::DotProduct(InCandidate.Position - Top.Point, Top.Normal)) <= PlaneTolerance
                && FVector::DotProduct(InCandidate.Normal, Top.Normal)
                    >= FMath::Cos(FMath::DegreesToRadians(NormalAngleDegrees));
        };
        const auto PreviousState = InOutState;
        const auto PreviousSearch = PreviousState.Get_SearchedSolve();

        // A swinging leg keeps the hold it swings to while the hold stays usable: switching to the ideal in flight would move
        // a foot already on its way, and at the freeze the swing would take the new target for the ground it validated.
        if (HoldIsUsable && NOT InQuery.Planted)
        { return DoFinish(Held, HeldIndex, Ideal.Verdict, InOutDebugLeg); }
        if (HoldIsUsable && InQuery.Planted && Held.Cast == ck::EProceduralFootholdCast::Face
            && MatchesBlockedFace(Held))
        { return DoFinish(Held, HeldIndex, Ideal.Verdict, InOutDebugLeg); }

        const auto KeepRadius = FootholdTuning.Get_KeepRadius() > 0.0f
            ? FootholdTuning.Get_KeepRadius()
            : DerivedKeepRadiusInStepThresholds * InQuery.Step->Get_Threshold();
        const auto HoldToIdeal = HoldIsUsable ? FVector::Dist(InOutState.Get_Position(), InQuery.Ideal) : 0.0;
        const auto HoldAgrees = HoldIsUsable && HoldToIdeal <= KeepRadius;
        const auto TrailLimit = InQuery.Step->Get_ForceStepReachFraction() * InQuery.Reach;
        const auto HoldWithinReach = HoldIsUsable && HoldToIdeal <= TrailLimit;
        // A valid plant beside a blocked ideal is support, but not evidence that the next step may stay there. Search the
        // obstruction before keeping an ahead-of-hip hold; retain that support as fallback when no alternative is found.
        const auto IdealOccluded = Ideal.Verdict == ck::EProceduralFootholdVerdict::Occluded && Ideal.Occluder.IsSet();

        const auto IdealIsUsable = Ideal.Verdict == ck::EProceduralFootholdVerdict::Usable;
        const auto Up = InQuery.Basis.GetAxisZ();
        const auto IdealOnAFace = IdealIsUsable && ck::FProceduralGaitSolver::Get_IsFaceNormal(Ideal.Normal, Up);
        const auto AdmittedIdealFace = IdealOnAFace && ck::Get_IsFootholdLevelEnough(
            InQuery.Basis.Inverse().RotateVector(Ideal.Normal), FootholdTuning.Get_MaxAngle());
        const auto FaceChangesSurface = AdmittedIdealFace
            && (NOT HoldIsUsable || ck::FProceduralGaitSolver::Get_IsFaceNormal(Ideal.Normal, Held.Normal));
        const auto AcquireBlockedFace = InQuery.Planted && InQuery.BlockedFace.IsSet();
        const auto HoldNeedsSearch = IdealOccluded || FaceChangesSurface || AcquireBlockedFace;
        const auto RefreshPlantedHold = InQuery.RefreshPlantedHold && InQuery.Planted && HoldWithinReach && NOT HoldNeedsSearch;
        if (AcquireAdaptiveTop && HoldWithinReach && MatchesAdaptiveTop(Held))
        { return DoFinish(Held, HeldIndex, Ideal.Verdict, InOutDebugLeg); }
        if (IdealIsUsable && NOT IdealOnAFace && NOT AcquireBlockedFace && NOT RefreshPlantedHold && NOT AcquireAdaptiveTop)
        {
            if (HoldAgrees)
            { return DoFinish(Held, HeldIndex, Ideal.Verdict, InOutDebugLeg); }

            InOutState = ck::FProceduralFootholdState{};
            return DoFinish(Ideal, IdealIndex, Ideal.Verdict, InOutDebugLeg);
        }

        if (HoldAgrees && NOT HoldNeedsSearch && NOT RefreshPlantedHold && NOT AcquireAdaptiveTop)
        { return DoFinish(Held, HeldIndex, Ideal.Verdict, InOutDebugLeg); }

        const auto HoldCompetes = HoldWithinReach && (RefreshPlantedHold || (NOT HoldNeedsSearch
            && FVector::DotProduct(InOutState.Get_Position() - InQuery.Hip, InQuery.Travel.GetSafeNormal()) < 0.0));
        if (HoldWithinReach && NOT HoldCompetes && NOT HoldNeedsSearch && NOT AcquireAdaptiveTop)
        { return DoFinish(Held, HeldIndex, Ideal.Verdict, InOutDebugLeg); }

        // Keep the ordinary result independent of the top objective, including the retry stamp when it falls back.
        const auto EarlyFallbackIndex = IdealIsUsable && NOT IdealOnAFace && NOT AcquireBlockedFace && NOT RefreshPlantedHold
            ? (HoldAgrees ? HeldIndex : IdealIndex)
            : ((HoldAgrees && NOT HoldNeedsSearch && NOT RefreshPlantedHold)
                || (HoldWithinReach && NOT HoldCompetes && NOT HoldNeedsSearch) ? HeldIndex : INDEX_NONE);
        const auto FinishAdaptiveFallback = [&](int32 InOrdinaryIndex, bool InBackoff) -> FFoothold
        {
            const auto Search = InOutState.Get_SearchedSolve().IsSet() ? InOutState.Get_SearchedSolve() : PreviousSearch;
            const auto FallbackIndex = EarlyFallbackIndex != INDEX_NONE ? EarlyFallbackIndex : InOrdinaryIndex;
            if ((FallbackIndex == HeldIndex && HeldIndex != INDEX_NONE)
                || (FallbackIndex == INDEX_NONE && (HoldCompetes || (HoldNeedsSearch && HoldWithinReach))))
            {
                InOutState = PreviousState;
                InOutState.Set_SearchedSolve(Search);
                return DoFinish(Held, HeldIndex, Ideal.Verdict, InOutDebugLeg);
            }
            if (FallbackIndex == IdealIndex || (FallbackIndex == INDEX_NONE && InBackoff && IdealOnAFace))
            {
                InOutState = ck::FProceduralFootholdState{}.Set_SearchedSolve(Search);
                return DoFinish(Ideal, IdealIndex, Ideal.Verdict, InOutDebugLeg);
            }
            if (FallbackIndex != INDEX_NONE)
            {
                const auto& Pick = Candidates[FallbackIndex];
                InOutState = ck::FProceduralFootholdState{}
                    .Set_Position(Pick.Position)
                    .Set_Normal(Pick.Normal)
                    .Set_Source(Pick.Source)
                    .Set_Hold(ck::EProceduralFootholdHold::Held)
                    .Set_Cast(Pick.Cast)
                    .Set_SearchedSolve(Search);
                return DoFinish(Pick, FallbackIndex, Ideal.Verdict, InOutDebugLeg);
            }
            InOutState = ck::FProceduralFootholdState{}.Set_SearchedSolve(Search);
            return FFoothold{InQuery.Ideal, Up, ck::EProceduralFootholdSource::None, Ideal.Verdict};
        };
        if (HoldIsUsable && NOT HoldWithinReach)
        {
            InOutState = ck::FProceduralFootholdState{};
            if (AcquireAdaptiveTop)
            { InOutState.Set_SearchedSolve(PreviousSearch); }
        }

        const auto NothingUsable = FFoothold{InQuery.Ideal, Up, ck::EProceduralFootholdSource::None, Ideal.Verdict};
        const auto& SearchedSolve = InOutState.Get_SearchedSolve();
        const auto BackingOff = SearchedSolve.IsSet() && InOutBudget.Solve - SearchedSolve.GetValue() <= SearchBackoffSolves;
        if (BackingOff || InOutBudget.RemainingLegs <= 0)
        {
            if (AcquireAdaptiveTop)
            {
                constexpr auto DuringBackoff = true;
                return FinishAdaptiveFallback(INDEX_NONE, DuringBackoff);
            }
            if (HoldCompetes || (HoldNeedsSearch && HoldWithinReach))
            { return DoFinish(Held, HeldIndex, Ideal.Verdict, InOutDebugLeg); }
            return IdealOnAFace ? DoFinish(Ideal, IdealIndex, Ideal.Verdict, InOutDebugLeg) : NothingUsable;
        }

        --InOutBudget.RemainingLegs;
        const auto SearchRadius = FootholdTuning.Get_SearchRadius() > 0.0f
            ? FootholdTuning.Get_SearchRadius()
            : DerivedSearchRadiusShareOfReach * InQuery.Reach;

        if (AcquireBlockedFace)
        {
            const auto& Face = InQuery.BlockedFace.GetValue();
            const auto Normal = Face.Normal.GetSafeNormal();
            // Project this leg's ideal to the observed plane, then re-cast at its own height.
            const auto Projected = InQuery.Ideal - Normal * FVector::DotProduct(InQuery.Ideal - Face.Point, Normal);
            const auto SeedStart = Projected + Normal * FaceHoldProbeHalfSpan;
            const auto SeedEnd = Projected - Normal * FaceHoldProbeHalfSpan;
            const auto Hit = DoCast_Segment(InQuery, SeedStart, SeedEnd, InOutRayCount);
            const auto NormalDot = Hit.IsSet() ? FVector::DotProduct(Hit->Get_Normal().GetSafeNormal(), Normal) : -1.0;
            auto ProjectedUsableFace = false;
            if (Hit.IsSet() && NormalDot > 0.95f)
            {
                const auto Candidate = DoProbe_Face(InQuery, Hit, Projected, ck::EProceduralFootholdSource::Front,
                    EFootholdStage::Search, InOutRayCount);
                DoRecord_Candidate(Candidate, Candidates, InOutDebugLeg);
                ProjectedUsableFace = MatchesBlockedFace(Candidate);
            }
            if (NOT ProjectedUsableFace)
            {
                // The leg-height ray can meet the floor edge; retry once at this frame's observed face point.
                const auto FaceStart = Face.Point + Normal * FaceHoldProbeHalfSpan;
                const auto FaceEnd = Face.Point - Normal * FaceHoldProbeHalfSpan;
                const auto FaceHit = DoCast_Segment(InQuery, FaceStart, FaceEnd, InOutRayCount);
                const auto FaceNormalDot = FaceHit.IsSet()
                    ? FVector::DotProduct(FaceHit->Get_Normal().GetSafeNormal(), Normal) : -1.0;
                if (FaceHit.IsSet() && FaceNormalDot > 0.95f)
                {
                    const auto Candidate = DoProbe_Face(InQuery, FaceHit, Face.Point, ck::EProceduralFootholdSource::Front,
                        EFootholdStage::Search, InOutRayCount);
                    if (MatchesBlockedFace(Candidate))
                    { DoRecord_Candidate(Candidate, Candidates, InOutDebugLeg); }
                }
            }
        }

        else if (AcquireAdaptiveTop)
        {
            const auto& Top = InQuery.AdaptiveTop.GetValue();
            const auto Projected = InQuery.Ideal - Top.Normal * FVector::DotProduct(InQuery.Ideal - Top.Point, Top.Normal);
            for (const auto& Point : {Projected, Top.Point})
            {
                const auto Hit = DoCast_Segment(InQuery, Point + Top.Normal * FaceHoldProbeHalfSpan,
                    Point - Top.Normal * FaceHoldProbeHalfSpan, InOutRayCount);
                if (NOT Hit.IsSet() || NOT FMath::IsFinite(Hit->Get_Fraction()) || NOT Hit->Get_Normal().IsNormalized())
                { continue; }
                auto Candidate = FFootholdCandidate{Hit->Get_Position(), Hit->Get_Normal(),
                    ck::EProceduralFootholdSource::Front, ck::EProceduralFootholdVerdict::Miss, ck::EProceduralFootholdCast::Down};
                Candidate.NativeHit = true;
                DoObserve_HeightCandidate(InQuery, Hit.GetValue());
                DoValidate(InQuery, Candidate, EFootholdStage::Search, InOutRayCount);
                DoRecord_Candidate(Candidate, Candidates, InOutDebugLeg);
                if (MatchesAdaptiveTop(Candidate))
                { break; }
            }
        }

        if (Ideal.Verdict == ck::EProceduralFootholdVerdict::Occluded && Ideal.Occluder.IsSet())
        {
            const auto& Occluder = Ideal.Occluder.GetValue();
            DoRecord_Candidate(DoProbe_Face(InQuery, TOptional<FCk_Jolt_HitResult>{Occluder}, Occluder.Get_Position(),
                ck::EProceduralFootholdSource::Front, EFootholdStage::Search, InOutRayCount), Candidates, InOutDebugLeg);

            const auto InFront = Occluder.Get_Position()
                + (InQuery.Hip - Occluder.Get_Position()).GetSafeNormal() * (FrontFloorShareOfReach * InQuery.Reach);
            DoRecord_Candidate(DoProbe_Down(InQuery, InFront, ck::EProceduralFootholdSource::Front, EGroundProbeAttempts::Every,
                EFootholdReprobe::ClampTowardHip, EFootholdStage::Search, InOutRayCount, nullptr), Candidates, InOutDebugLeg);
        }

        auto InwardOccluderTops = TArray<FInwardOccluderTopReceipt, TInlineAllocator<MaxInwardOccluderTopReceipts>>{};
        auto FailedInwardFace = TOptional<FInwardFailedFaceReceipt>{};
        for (const auto Depth : {InwardCloseDepthInRestDrops, InwardFarDepthInRestDrops})
        {
            const auto Focal = InQuery.Hip - Up * (Depth * InQuery.RestDrop);
            const auto End = Focal + (Focal - InQuery.Ideal).GetSafeNormal() * InQuery.Probe->Get_Down();
            const auto InwardHit = DoCast_Segment(InQuery, InQuery.Ideal, End, InOutRayCount);
            const auto Inward = DoProbe_Face(InQuery, InwardHit, InQuery.Ideal,
                ck::EProceduralFootholdSource::Inward, EFootholdStage::Search, InOutRayCount);
            DoRecord_Candidate(Inward, Candidates, InOutDebugLeg);

            if (NOT FailedInwardFace.IsSet() && InwardHit.IsSet()
                && Inward.Cast == ck::EProceduralFootholdCast::Face && Inward.Verdict == ck::EProceduralFootholdVerdict::Unreachable
                && InwardHit->Get_HasHit() && FMath::IsFinite(InwardHit->Get_Fraction())
                && InwardHit->Get_Fraction() > 0.0f && InwardHit->Get_Fraction() <= 1.0f
                && NOT InwardHit->Get_Position().ContainsNaN() && NOT InwardHit->Get_Normal().ContainsNaN()
                && InwardHit->Get_Normal().IsNormalized()
                && ck::FProceduralGaitSolver::Get_IsFaceNormal(InwardHit->Get_Normal(), Up))
            {
                FailedInwardFace = FInwardFailedFaceReceipt{InwardHit->Get_Position(), InwardHit->Get_Normal()};
            }

            if (Inward.Verdict == ck::EProceduralFootholdVerdict::Occluded && Inward.Occluder.IsSet()
                && InwardOccluderTops.Num() < MaxInwardOccluderTopReceipts)
            {
                const auto& Occluder = Inward.Occluder.GetValue();
                const auto& Point = Occluder.Get_Position();
                const auto& Normal = Occluder.Get_Normal();
                // Keep only the original native top contact. A malformed normal is rejected, never repaired.
                if (Occluder.Get_HasHit() && FMath::IsFinite(Occluder.Get_Fraction())
                    && Occluder.Get_Fraction() > 0.0f && Occluder.Get_Fraction() <= 1.0f
                    && NOT Point.ContainsNaN() && NOT Normal.ContainsNaN() && Normal.IsNormalized()
                    && FVector::DotProduct(Normal, Up) > KINDA_SMALL_NUMBER
                    && ck::Get_IsFootholdLevelEnough(InQuery.Basis.Inverse().RotateVector(Normal), FootholdTuning.Get_MaxAngle())
                    && NOT InwardOccluderTops.ContainsByPredicate([&](const FInwardOccluderTopReceipt& InReceipt)
                        { return InReceipt.Point.Equals(Point, FaceTopGeometryTolerance); }))
                { InwardOccluderTops.Add(FInwardOccluderTopReceipt{Point, Normal}); }
            }

            if (Inward.Verdict == ck::EProceduralFootholdVerdict::UnderBody
                || Inward.Verdict == ck::EProceduralFootholdVerdict::Inboard)
            {
                // Converging on the hip can erase foot spacing on convex support and cross the leg's own-side or body
                // boundary. Retain lateral spacing in a second cast; every ordinary admission gate still applies.
                const auto LateralAxis = InQuery.Basis.GetAxisY();
                const auto SpreadFocal = Focal + LateralAxis * FVector::DotProduct(InQuery.Ideal - InQuery.Hip, LateralAxis);
                const auto SpreadEnd = SpreadFocal + (SpreadFocal - InQuery.Ideal).GetSafeNormal() * InQuery.Probe->Get_Down();
                DoRecord_Candidate(DoProbe_Face(InQuery, DoCast_Segment(InQuery, InQuery.Ideal, SpreadEnd, InOutRayCount),
                    InQuery.Ideal, ck::EProceduralFootholdSource::Inward, EFootholdStage::Search, InOutRayCount),
                    Candidates, InOutDebugLeg);
            }
        }

        {
            const auto Origin = InQuery.Hip + Up * (OutwardHeightInRestDrops * InQuery.RestDrop);
            const auto End = InQuery.Ideal + (InQuery.Ideal - Origin).GetSafeNormal() * InQuery.Probe->Get_Down();
            DoRecord_Candidate(DoProbe_Face(InQuery, DoCast_Segment(InQuery, Origin, End, InOutRayCount), InQuery.Ideal,
                ck::EProceduralFootholdSource::Outward, EFootholdStage::Search, InOutRayCount), Candidates, InOutDebugLeg);
        }

        // A face at the ideal can have a shallow top just behind its edge. The search ring may jump over that top; a
        // straight down ray a short way inside the face samples it without leaning back into the same face. Scale the
        // inset with the search radius, and run the ordinary search validation on whatever surface the ray finds.
        auto FaceTopProbed = false;
        if (IdealOnAFace)
        {
            const auto Inset = FMath::Max(FaceTopPreferredMinInset, SearchRadius * FaceTopInsetShareOfSearchRadius);
            const auto Top = DoProbe_FaceTop(InQuery, Ideal, Inset, InOutRayCount);
            if (Top.IsSet())
            {
                DoRecord_Candidate(Top.GetValue(), Candidates, InOutDebugLeg);
                FaceTopProbed = true;
            }
        }

        const auto BasisX = InQuery.Basis.GetAxisX();
        const auto BasisY = InQuery.Basis.GetAxisY();
        constexpr auto MaxInboardRingRetries = 2;
        auto InboardRingPoints = TArray<FVector, TInlineAllocator<MaxInboardRingRetries>>{};
        for (auto Point = 0; Point < RingPoints; ++Point)
        {
            const auto Angle = UE_DOUBLE_TWO_PI * Point / RingPoints;
            const auto RingPoint = InQuery.Ideal + (BasisX * FMath::Cos(Angle) + BasisY * FMath::Sin(Angle)) * SearchRadius;
            const auto Ring = DoProbe_Down(InQuery, RingPoint, ck::EProceduralFootholdSource::Ring,
                EGroundProbeAttempts::RetryFromInsideSolid, EFootholdReprobe::ClampTowardHip, EFootholdStage::Search, InOutRayCount,
                nullptr);
            DoRecord_Candidate(Ring, Candidates, InOutDebugLeg);
            if (InQuery.Probe->Get_OutwardLean() > 0.0f && Ring.Verdict == ck::EProceduralFootholdVerdict::Inboard
                && InboardRingPoints.Num() < MaxInboardRingRetries)
            { InboardRingPoints.Add(RingPoint); }
        }

        // A hold the force-step bound dropped is not among the picks, and picking it again would hold it forever. A hold
        // behind the hip and beyond the keep radius competes first, so it wins a tie, then a face ideal, and the costs are
        // measured from the ideal, so a face ideal stands at distance 0 and the hold at its own distance from it.
        const auto OrdinaryFirstPickIndex = HoldCompetes ? HeldIndex : (IdealOnAFace ? IdealIndex : IdealIndex + 1);
        const auto FirstPickIndex = AcquireAdaptiveTop ? 0 : OrdinaryFirstPickIndex;
        auto AdmittedHeldTop = TOptional<FFootholdCandidate>{};
        auto AdmittedIdealTop = TOptional<FFootholdCandidate>{};
        if (AcquireAdaptiveTop)
        {
            for (const auto Index : {HeldIndex, IdealIndex})
            {
                if (Index == INDEX_NONE || NOT MatchesAdaptiveTop(Candidates[Index])
                    || (Index == HeldIndex && NOT HoldWithinReach && EarlyFallbackIndex != HeldIndex))
                { continue; }
                auto Admitted = Candidates[Index];
                DoValidate(InQuery, Admitted, EFootholdStage::Search, InOutRayCount);
                if (Index == HeldIndex)
                { AdmittedHeldTop = Admitted; }
                else
                { AdmittedIdealTop = Admitted; }
            }
        }
        const auto InverseBasis = InQuery.Basis.Inverse();
        auto SlopeUp = Up;
        const auto TransitionNormal = IdealOccluded ? TOptional<FVector>{Ideal.Occluder->Get_Normal()} : TOptional<FVector>{};
        if (TransitionNormal.IsSet() && ck::FProceduralGaitSolver::Get_IsFaceNormal(TransitionNormal.GetValue(), Up)
            && ck::Get_IsFootholdLevelEnough(InverseBasis.RotateVector(TransitionNormal.GetValue()), FootholdTuning.Get_MaxAngle()))
        {
            // A blocked ideal establishes an approach obstruction: rank its face and current support evenly. A usable
            // face ideal retains the ordinary slope reference so a nearby top still beats the face. Admission continues
            // to use the current support frame; this neither admits an over-steep hit nor rotates the body or its probes.
            const auto Midpoint = (Up + TransitionNormal->GetSafeNormal()).GetSafeNormal();
            if (NOT Midpoint.IsNearlyZero())
            { SlopeUp = Midpoint; }
        }
        const auto Settings = ck::FProceduralFootholdSettings{}
            .Set_SlopeWeight(FootholdTuning.Get_SlopeWeight())
            .Set_ContinuityWeight(FootholdTuning.Get_ContinuityWeight())
            .Set_MaxAngleDegrees(FootholdTuning.Get_MaxAngle())
            .Set_SlopeUp(InverseBasis.RotateVector(SlopeUp));
        const auto CostOrigin = IdealOnAFace || IdealOccluded ? Ideal.Position : InQuery.Ideal;
        const auto Get_OrdinarySearchPick = [&]() -> int32
        {
            const auto PickCandidates = TArrayView<const FFootholdCandidate>{Candidates}.RightChop(OrdinaryFirstPickIndex);
            const auto SupportCandidates = ck::algo::Transform<TArray<ck::FProceduralFootholdCandidate, TInlineAllocator<24>>>(PickCandidates,
            [&](const FFootholdCandidate& InCandidate)
            {
                return ck::FProceduralFootholdCandidate{InverseBasis.RotateVector(InCandidate.Position),
                    InverseBasis.RotateVector(InCandidate.Normal), InCandidate.Source, InCandidate.Verdict};
            });
            return ck::SelectProceduralFoothold(SupportCandidates, InverseBasis.RotateVector(CostOrigin),
                InverseBasis.RotateVector(InQuery.Plant), InQuery.Planted, InQuery.Reach, Settings);
        };
        const auto Get_SearchPick = [&]() -> int32
        {
            if (NOT AcquireAdaptiveTop)
            { return Get_OrdinarySearchPick(); }
            auto SupportCandidates = TArray<ck::FProceduralFootholdCandidate, TInlineAllocator<24>>{};
            for (auto Index = FirstPickIndex; Index < Candidates.Num(); ++Index)
            {
                const auto* Candidate = &Candidates[Index];
                if (Index == HeldIndex)
                { Candidate = AdmittedHeldTop.IsSet() ? &AdmittedHeldTop.GetValue() : nullptr; }
                else if (Index == IdealIndex)
                { Candidate = AdmittedIdealTop.IsSet() ? &AdmittedIdealTop.GetValue() : nullptr; }
                const auto& Geometry = Candidate != nullptr ? *Candidate : Candidates[Index];
                SupportCandidates.Add(ck::FProceduralFootholdCandidate{InverseBasis.RotateVector(Geometry.Position),
                    InverseBasis.RotateVector(Geometry.Normal), Geometry.Source,
                    Candidate != nullptr && MatchesAdaptiveTop(*Candidate)
                        ? Candidate->Verdict : ck::EProceduralFootholdVerdict::Miss});
            }
            return ck::SelectProceduralFoothold(SupportCandidates, InverseBasis.RotateVector(CostOrigin),
                InverseBasis.RotateVector(InQuery.Plant), InQuery.Planted, InQuery.Reach, Settings);
        };
        auto SearchPick = Get_SearchPick();
        const auto FaceTopPick = AcquireAdaptiveTop ? Get_OrdinarySearchPick() : SearchPick;
        const auto FaceTopIndex = FaceTopPick == INDEX_NONE ? INDEX_NONE
            : (AcquireAdaptiveTop ? OrdinaryFirstPickIndex : FirstPickIndex) + FaceTopPick;

        // The ideal probe owns the first opportunity to refine a face, in its original candidate order. If it did not
        // run, refine only the usable face the completed search actually chose. Append the top so a strict-cost tie keeps
        // the old winner, and copy the face before the candidate array can grow.
        if (NOT FaceTopProbed && FaceTopIndex != INDEX_NONE)
        {
            const auto Face = Candidates[FaceTopIndex];
            if (Face.Verdict == ck::EProceduralFootholdVerdict::Usable
                && ck::FProceduralGaitSolver::Get_IsFaceNormal(Face.Normal, Up))
            {
                const auto FaceNormalInPlane = FVector::VectorPlaneProject(Face.Normal, Up).GetSafeNormal();
                auto Inset = static_cast<double>(FMath::Max(FaceTopPreferredMinInset, SearchRadius * FaceTopInsetShareOfSearchRadius));
                const auto Lateral = InverseBasis.RotateVector(Face.Position - InQuery.BodyLocation).Y;
                const auto TowardBody = FMath::Sign(Lateral) * InverseBasis.RotateVector(FaceNormalInPlane).Y;
                const auto LateralGap = FMath::Abs(Lateral) - (InQuery.MaxHipLateral - UnderBodyMargin);
                if (InQuery.MaxHipLateral > UnderBodyMargin && LateralGap > 0.0 && TowardBody > KINDA_SMALL_NUMBER)
                {
                    // Keep half the lateral strip outside the existing UnderBody boundary. A nominal inset would
                    // jump over a narrow valid top into the body band; this changes the sample, never its validation.
                    Inset = FMath::Min(Inset, 0.5 * LateralGap / TowardBody);
                }
                if (Inset > FaceTopGeometryTolerance)
                {
                    const auto Top = DoProbe_FaceTop(InQuery, Face, Inset, InOutRayCount);
                    if (Top.IsSet())
                    {
                        DoRecord_Candidate(Top.GetValue(), Candidates, InOutDebugLeg);
                        SearchPick = Get_SearchPick();
                    }
                }
            }
        }

        constexpr auto MaxBoundarySamples = 4;
        constexpr auto ReservationBoundaryMargin = 0.1;
        const auto OrdinaryCandidateCount = Candidates.Num();
        auto BoundarySamples = 0;
        // A narrow usable strip can fall between the ring points while their nearby hits are reserved. Only after the
        // ordinary search fails, sample the edge of the first blocking reservation around at most two down-ray hits.
        // These are new queries, not adjusted plants: every actual hit still passes the ordinary admission checks.
        if (SearchPick == INDEX_NONE && FMath::IsFinite(InQuery.FootContactRadius) && InQuery.FootContactRadius > 0.0f)
        {
            constexpr auto MaxReservedCandidates = 2;
            constexpr auto ReservationBoundaryAngleDegrees = 22.5;
            auto ReservedCandidatesProbed = 0;
            for (auto CandidateIndex = FirstPickIndex; CandidateIndex < OrdinaryCandidateCount
                && ReservedCandidatesProbed < MaxReservedCandidates && BoundarySamples < MaxBoundarySamples; ++CandidateIndex)
            {
                const auto Candidate = Candidates[CandidateIndex];
                if (Candidate.Verdict != ck::EProceduralFootholdVerdict::Reserved
                    || Candidate.Cast != ck::EProceduralFootholdCast::Down
                    || Candidate.Position.ContainsNaN() || Candidate.Normal.ContainsNaN() || Candidate.Normal.IsNearlyZero())
                { continue; }

                for (const auto& Reservation : InQuery.Reservations)
                {
                    if (NOT FMath::IsFinite(Reservation.Get_Radius()) || Reservation.Get_Radius() <= 0.0f
                        || Reservation.Get_LegIndex() < 0 || Reservation.Get_LegIndex() == InQuery.LegIndex
                        || Reservation.Get_Position().ContainsNaN())
                    { continue; }

                    const auto Radius = static_cast<double>(InQuery.FootContactRadius) + Reservation.Get_Radius();
                    if (FVector::DistSquared(Candidate.Position, Reservation.Get_Position()) >= Radius * Radius)
                    { continue; }

                    const auto Normal = Candidate.Normal.GetSafeNormal();
                    const auto PlaneDistance = FVector::DotProduct(Reservation.Get_Position() - Candidate.Position, Normal);
                    const auto Center = Reservation.Get_Position() - Normal * PlaneDistance;
                    auto Direction = (Candidate.Position - Center).GetSafeNormal();
                    if (Direction.IsNearlyZero())
                    { Direction = FVector::VectorPlaneProject(InQuery.Outboard, Normal).GetSafeNormal(); }
                    if (Normal.IsNearlyZero() || Direction.IsNearlyZero())
                    { break; }

                    const auto BoundaryRadius = FMath::Sqrt(FMath::Max(0.0, Radius * Radius - PlaneDistance * PlaneDistance))
                        + ReservationBoundaryMargin;
                    ++ReservedCandidatesProbed;
                    for (const auto Angle : {-ReservationBoundaryAngleDegrees, ReservationBoundaryAngleDegrees})
                    {
                        const auto Point = Center + Direction.RotateAngleAxis(Angle, Normal) * BoundaryRadius;
                        ++BoundarySamples;
                        DoRecord_Candidate(DoProbe_Down(InQuery, Point, ck::EProceduralFootholdSource::Ring,
                            EGroundProbeAttempts::RetryFromInsideSolid, EFootholdReprobe::None, EFootholdStage::Search,
                            InOutRayCount, nullptr), Candidates, InOutDebugLeg);
                    }
                    break;
                }
            }
            SearchPick = Get_SearchPick();
        }

        // Rank all validated candidates on the freshly observed plane, including an ordinary face ideal or search hit.
        auto PromotedFace = TOptional<FFootholdCandidate>{};
        if (AcquireBlockedFace)
        {
            auto Matching = TArray<ck::FProceduralFootholdCandidate, TInlineAllocator<24>>{};
            auto MatchingIndexes = TArray<int32, TInlineAllocator<24>>{};
            auto MatchingCandidates = TArray<FFootholdCandidate, TInlineAllocator<24>>{};
            for (auto Index = FirstPickIndex; Index < Candidates.Num(); ++Index)
            {
                const auto& Candidate = Candidates[Index];
                if (NOT MatchesBlockedFace(Candidate)) { continue; }
                auto Admitted = Candidate;
                // A wall hold must re-probe along its normal, and every promoted hit must pass face reach and search gates.
                Admitted.Cast = ck::EProceduralFootholdCast::Face;
                if (Index == IdealIndex) { Admitted.Source = ck::EProceduralFootholdSource::Front; }
                if (Index == HeldIndex || Index == IdealIndex)
                { DoValidate(InQuery, Admitted, EFootholdStage::Search, InOutRayCount); }
                else if (FVector::Dist(Admitted.Position, InQuery.Hip)
                    > InQuery.Step->Get_TargetReachFraction() * InQuery.Reach)
                { continue; }
                if (Admitted.Verdict != ck::EProceduralFootholdVerdict::Usable) { continue; }
                Matching.Add(ck::FProceduralFootholdCandidate{InverseBasis.RotateVector(Admitted.Position),
                    InverseBasis.RotateVector(Admitted.Normal), Admitted.Source, Admitted.Verdict});
                MatchingIndexes.Add(Index);
                MatchingCandidates.Add(Admitted);
            }
            if (NOT Matching.IsEmpty())
            {
                const auto FacePick = ck::SelectProceduralFoothold(TArrayView<const ck::FProceduralFootholdCandidate>{Matching},
                    InverseBasis.RotateVector(CostOrigin), InverseBasis.RotateVector(InQuery.Plant), InQuery.Planted,
                    InQuery.Reach, Settings);
                if (FacePick != INDEX_NONE)
                {
                    SearchPick = MatchingIndexes[FacePick] - FirstPickIndex;
                    PromotedFace = MatchingCandidates[FacePick];
                }
            }
        }

        auto ReservedStraightRingReceipts = TArray<FFootholdCandidate, TInlineAllocator<MaxInboardRingRetries>>{};
        if (SearchPick == INDEX_NONE)
        {
            // Refine only failed leaned ring queries, at their original points. Each retry spends one native ray
            // and at most DoValidate's one visibility ray; no inside-solid or reach-reprojection retries run here.
            for (const auto& RingPoint : InboardRingPoints)
            {
                auto Straight = FFootholdCandidate{RingPoint, Up, ck::EProceduralFootholdSource::Ring,
                    ck::EProceduralFootholdVerdict::Miss, ck::EProceduralFootholdCast::Down};
                const auto Hit = UCk_Utils_JoltQuery_UE::Get_RayCast(InQuery.World,
                    RingPoint + Up * InQuery.Probe->Get_Up(), RingPoint - Up * InQuery.Probe->Get_Down(),
                    InQuery.Probe->Get_QueryFilter());
                ++InOutRayCount;
                if (Get_TrustedHit(Hit, -Up))
                {
                    Straight.Position = Hit.Get_Position();
                    Straight.Normal = Hit.Get_Normal().GetSafeNormal();
                    Straight.NativeHit = true;
                    if (FVector::Dist(Straight.Position, InQuery.Hip) > InQuery.Step->Get_TargetReachFraction() * InQuery.Reach)
                    { Straight.Verdict = ck::EProceduralFootholdVerdict::Unreachable; }
                    else
                    { DoValidate(InQuery, Straight, EFootholdStage::Search, InOutRayCount); }
                }
                DoRecord_Candidate(Straight, Candidates, InOutDebugLeg);
                if (Straight.Verdict == ck::EProceduralFootholdVerdict::Reserved
                    && Straight.Cast == ck::EProceduralFootholdCast::Down
                    && ReservedStraightRingReceipts.Num() < MaxInboardRingRetries)
                { ReservedStraightRingReceipts.Add(Straight); }
            }
            if (NOT InboardRingPoints.IsEmpty())
            { SearchPick = Get_SearchPick(); }
        }

        // Spend only unused boundary samples after every older refinement failed. The original candidate set is frozen
        // before boundary appends: fresh hits are admitted and ranked normally, never used to recurse into more queries.
        if (SearchPick == INDEX_NONE && BoundarySamples < MaxBoundarySamples
            && FMath::IsFinite(InQuery.FootContactRadius) && InQuery.FootContactRadius > 0.0f
            && FMath::IsFinite(SearchRadius) && SearchRadius > 0.0f)
        {
            const auto Lateral = InQuery.Basis.GetAxisY();
            const auto SideDot = FVector::DotProduct(InQuery.Outboard, Lateral);
            const auto Side = FMath::Sign(SideDot);
            const auto HasSide = FMath::IsFinite(SideDot) && FMath::Abs(SideDot) > UE_DOUBLE_SMALL_NUMBER;
            const auto ProbeBoundary = [&](const FVector& InPoint, const FVector& InSeed) -> void
            {
                if (InPoint.ContainsNaN() || FVector::DistSquared(InPoint, InSeed) > FMath::Square(static_cast<double>(SearchRadius)))
                { return; }
                ++BoundarySamples;
                DoRecord_Candidate(DoProbe_Down(InQuery, InPoint, ck::EProceduralFootholdSource::Ring,
                    EGroundProbeAttempts::RetryFromInsideSolid, EFootholdReprobe::None, EFootholdStage::Search,
                    InOutRayCount, nullptr), Candidates, InOutDebugLeg);
            };

            if (HasSide)
            {
                const auto BodyBoundary = static_cast<double>(InQuery.MaxHipLateral) - UnderBodyMargin + ReservationBoundaryMargin;
                auto JointBoundaryAttempted = false;
                const auto BoundaryQuery = ck::FProceduralFootholdUnderBodyBoundaryQuery{
                    .BodyLocation = InQuery.BodyLocation,
                    .Hip = InQuery.Hip,
                    .Lateral = Lateral,
                    .Outboard = InQuery.Outboard,
                    .BodyBoundary = BodyBoundary,
                    .SearchRadius = SearchRadius,
                    .ForceReach = InQuery.Step->Get_ForceStepReachFraction() * InQuery.Reach,
                    .OwnSideTolerance = InboardToleranceShareOfReach * InQuery.Reach,
                    .FootContactRadius = InQuery.FootContactRadius,
                    .LegIndex = InQuery.LegIndex,
                    .Reservations = InQuery.Reservations};
                for (auto Index = FirstPickIndex; Index < OrdinaryCandidateCount && BoundarySamples < MaxBoundarySamples; ++Index)
                {
                    const auto Candidate = Candidates[Index];
                    if (Candidate.Verdict != ck::EProceduralFootholdVerdict::UnderBody
                        || Candidate.Cast != ck::EProceduralFootholdCast::Down
                        || Candidate.Position.ContainsNaN() || Candidate.Normal.ContainsNaN() || Candidate.Normal.IsNearlyZero()
                        || NOT FMath::IsFinite(BodyBoundary) || BodyBoundary <= 0.0)
                    { continue; }
                    const auto Point = ck::Get_ProceduralFootholdUnderBodyBoundary(BoundaryQuery,
                        Candidate.Position, Candidate.Normal, JointBoundaryAttempted);
                    if (Point.IsSet())
                    { ProbeBoundary(Point.GetValue(), Candidate.Position); }
                }
                SearchPick = Get_SearchPick();
            }

            if (HasSide && SearchPick == INDEX_NONE)
            {
                for (auto Index = FirstPickIndex; Index < OrdinaryCandidateCount && BoundarySamples < MaxBoundarySamples; ++Index)
                {
                    const auto Candidate = Candidates[Index];
                    if (Candidate.Verdict != ck::EProceduralFootholdVerdict::Reserved
                        || Candidate.Cast != ck::EProceduralFootholdCast::Down
                        || Candidate.Position.ContainsNaN() || Candidate.Normal.ContainsNaN() || Candidate.Normal.IsNearlyZero())
                    { continue; }
                    for (const auto& Reservation : InQuery.Reservations)
                    {
                        if (NOT FMath::IsFinite(Reservation.Get_Radius()) || Reservation.Get_Radius() <= 0.0f
                            || Reservation.Get_LegIndex() < 0 || Reservation.Get_LegIndex() == InQuery.LegIndex
                            || Reservation.Get_Position().ContainsNaN())
                        { continue; }
                        const auto Radius = static_cast<double>(InQuery.FootContactRadius) + Reservation.Get_Radius();
                        if (FVector::DistSquared(Candidate.Position, Reservation.Get_Position()) >= Radius * Radius)
                        { continue; }

                        const auto Normal = Candidate.Normal.GetSafeNormal();
                        if (NOT Normal.IsNormalized())
                        { break; }
                        const auto PlaneDistance = FVector::DotProduct(Reservation.Get_Position() - Candidate.Position, Normal);
                        const auto Center = Reservation.Get_Position() - Normal * PlaneDistance;
                        const auto BoundaryRadius = FMath::Sqrt(FMath::Max(0.0, Radius * Radius - PlaneDistance * PlaneDistance))
                            + ReservationBoundaryMargin;
                        const auto ForceRadius = static_cast<double>(InQuery.Step->Get_ForceStepReachFraction()) * InQuery.Reach
                            - ReservationBoundaryMargin;
                        const auto HipDistance = FVector::DotProduct(InQuery.Hip - Candidate.Position, Normal);
                        const auto HipCenter = InQuery.Hip - Normal * HipDistance;
                        const auto AllowedSquared = ForceRadius * ForceRadius - HipDistance * HipDistance;
                        auto Direction = FVector::VectorPlaneProject(Lateral * Side, Normal).GetSafeNormal();
                        const auto CenterDelta = HipCenter - Center;
                        const auto Distance = CenterDelta.Size();
                        if (NOT FMath::IsFinite(BoundaryRadius) || BoundaryRadius <= 0.0
                            || NOT FMath::IsFinite(ForceRadius) || ForceRadius <= 0.0
                            || NOT FMath::IsFinite(AllowedSquared) || AllowedSquared < 0.0
                            || NOT FMath::IsFinite(Distance) || Direction.IsNearlyZero())
                        { break; }

                        // A reservation circle intersects the force sphere in this observed plane. Choose its feasible
                        // direction nearest the leg's own side; geometry proposes a query, never an admitted contact.
                        if (Distance <= UE_DOUBLE_SMALL_NUMBER)
                        {
                            if (BoundaryRadius * BoundaryRadius > AllowedSquared)
                            { break; }
                        }
                        else
                        {
                            const auto Axis = CenterDelta / Distance;
                            const auto MinimumDot = (Distance * Distance + BoundaryRadius * BoundaryRadius - AllowedSquared)
                                / (2.0 * Distance * BoundaryRadius);
                            if (NOT FMath::IsFinite(MinimumDot) || MinimumDot > 1.0)
                            { break; }
                            const auto DesiredDot = FVector::DotProduct(Direction, Axis);
                            if (MinimumDot > -1.0 && DesiredDot < MinimumDot)
                            {
                                auto Tangent = (Direction - Axis * DesiredDot).GetSafeNormal();
                                if (Tangent.IsNearlyZero())
                                {
                                    const auto SeedDirection = (Candidate.Position - Center).GetSafeNormal();
                                    Tangent = FVector::VectorPlaneProject(SeedDirection, Axis).GetSafeNormal();
                                    if (Tangent.IsNearlyZero())
                                    { Tangent = FVector::CrossProduct(Normal, Axis).GetSafeNormal(); }
                                }
                                if (Tangent.IsNearlyZero())
                                { break; }
                                Direction = Axis * MinimumDot + Tangent * FMath::Sqrt(FMath::Max(0.0, 1.0 - MinimumDot * MinimumDot));
                            }
                        }
                        ProbeBoundary(Center + Direction * BoundaryRadius, Candidate.Position);
                        break;
                    }
                }
                SearchPick = Get_SearchPick();
            }

            // A freshly cast straight retry can be reserved after the ordinary prefix was frozen. Only after every
            // older fallback fails, spend an unused shared slot at its closest observed reservation boundary. The
            // native down query remains the only admission authority; appended results never become new seeds.
            for (const auto& Receipt : ReservedStraightRingReceipts)
            {
                if (SearchPick != INDEX_NONE || BoundarySamples >= MaxBoundarySamples)
                { break; }
                if (Receipt.Position.ContainsNaN() || Receipt.Normal.ContainsNaN())
                { continue; }
                const auto Normal = Receipt.Normal.GetSafeNormal();
                if (NOT Normal.IsNormalized())
                { continue; }
                for (const auto& Reservation : InQuery.Reservations)
                {
                    if (NOT FMath::IsFinite(Reservation.Get_Radius()) || Reservation.Get_Radius() <= 0.0f
                        || Reservation.Get_LegIndex() < 0 || Reservation.Get_LegIndex() == InQuery.LegIndex
                        || Reservation.Get_Position().ContainsNaN())
                    { continue; }
                    const auto Radius = static_cast<double>(InQuery.FootContactRadius) + Reservation.Get_Radius();
                    const auto RadiusSquared = Radius * Radius;
                    const auto Delta = Receipt.Position - Reservation.Get_Position();
                    const auto DistanceSquared = Delta.SizeSquared();
                    if (NOT FMath::IsFinite(Radius) || Radius <= 0.0 || NOT FMath::IsFinite(RadiusSquared)
                        || Delta.ContainsNaN() || NOT FMath::IsFinite(DistanceSquared)
                        || DistanceSquared >= RadiusSquared)
                    { continue; }

                    // Keep the first blocking reservation's native plane and move only as far as its boundary.
                    const auto PlaneDistance = FVector::DotProduct(-Delta, Normal);
                    const auto PlaneRadiusSquared = RadiusSquared - PlaneDistance * PlaneDistance;
                    if (NOT FMath::IsFinite(PlaneDistance) || NOT FMath::IsFinite(PlaneRadiusSquared) || PlaneRadiusSquared < 0.0)
                    { break; }
                    const auto Center = Reservation.Get_Position() - Normal * PlaneDistance;
                    if (Center.ContainsNaN())
                    { break; }
                    const auto DirectionDelta = Receipt.Position - Center;
                    if (DirectionDelta.ContainsNaN())
                    { break; }
                    const auto Direction = DirectionDelta.GetSafeNormal();
                    const auto BoundaryRadius = FMath::Sqrt(PlaneRadiusSquared) + ReservationBoundaryMargin;
                    if (NOT Direction.IsNormalized() || NOT FMath::IsFinite(BoundaryRadius) || BoundaryRadius <= 0.0)
                    { break; }
                    ProbeBoundary(Center + Direction * BoundaryRadius, Receipt.Position);
                    break;
                }
                SearchPick = Get_SearchPick();
            }
        }

        if (SearchPick == INDEX_NONE)
        {
            InOutState.Set_SearchedSolve(InOutBudget.Solve);
            const auto OrdinaryPick = AcquireAdaptiveTop ? Get_OrdinarySearchPick() : INDEX_NONE;
            constexpr auto DuringBackoff = false;
            auto Result = AcquireAdaptiveTop
                ? FinishAdaptiveFallback(OrdinaryPick != INDEX_NONE ? OrdinaryFirstPickIndex + OrdinaryPick : INDEX_NONE, DuringBackoff)
                : (HoldCompetes || (HoldNeedsSearch && HoldWithinReach)
                    ? DoFinish(Held, HeldIndex, Ideal.Verdict, InOutDebugLeg) : NothingUsable);
            Result.UnusedBoundarySamples = MaxBoundarySamples - BoundarySamples;
            if (Result.Source == ck::EProceduralFootholdSource::None)
            {
                Result.InwardOccluderTops = MoveTemp(InwardOccluderTops);
                Result.FailedInwardFace = FailedInwardFace;
            }
            return Result;
        }

        const auto Chosen = FirstPickIndex + SearchPick;
        if (AcquireAdaptiveTop && (Chosen == HeldIndex || Chosen == IdealIndex))
        {
            const auto& Top = Chosen == HeldIndex ? AdmittedHeldTop.GetValue() : AdmittedIdealTop.GetValue();
            InOutState = ck::FProceduralFootholdState{}
                .Set_Position(Top.Position)
                .Set_Normal(Top.Normal)
                .Set_Source(Top.Source)
                .Set_Hold(ck::EProceduralFootholdHold::Held)
                .Set_Cast(Top.Cast)
                .Set_SearchedSolve(InOutBudget.Solve);
            return DoFinish(Top, Chosen, Ideal.Verdict, InOutDebugLeg);
        }
        if (PromotedFace.IsSet())
        {
            const auto& Face = PromotedFace.GetValue();
            InOutState = ck::FProceduralFootholdState{}
                .Set_Position(Face.Position)
                .Set_Normal(Face.Normal)
                .Set_Source(Face.Source)
                .Set_Hold(ck::EProceduralFootholdHold::Held)
                .Set_Cast(Face.Cast);
            if (ck::IsValid(InOutDebugLeg, ck::IsValid_Policy_NullptrOnly{}))
            { InOutDebugLeg->Get_Footholds()[Chosen].Set_Source(Face.Source); }
            return DoFinish(Face, Chosen, Ideal.Verdict, InOutDebugLeg);
        }
        // The hold stays the target; a search that found nothing better backs off like one that found nothing.
        if (Chosen == HeldIndex)
        {
            InOutState.Set_SearchedSolve(InOutBudget.Solve);
            return DoFinish(Held, HeldIndex, Ideal.Verdict, InOutDebugLeg);
        }

        // The face ideal stays the target; a search that found nothing better backs off like one that found nothing.
        if (Chosen == IdealIndex)
        {
            InOutState = ck::FProceduralFootholdState{}.Set_SearchedSolve(InOutBudget.Solve);
            return DoFinish(Ideal, IdealIndex, Ideal.Verdict, InOutDebugLeg);
        }

        const auto& Pick = Candidates[Chosen];
        InOutState = ck::FProceduralFootholdState{}
            .Set_Position(Pick.Position)
            .Set_Normal(Pick.Normal)
            .Set_Source(Pick.Source)
            .Set_Hold(ck::EProceduralFootholdHold::Held)
            .Set_Cast(Pick.Cast);
        return DoFinish(Pick, Chosen, Ideal.Verdict, InOutDebugLeg);
    }

    auto
        Get_IsPlantOccluded(
            const FFootholdQuery& InQuery,
            int32& InOutRayCount)
        -> bool
    {
        return Get_Occluder(InQuery, InQuery.Plant, InOutRayCount).IsSet();
    }

    struct FLandingGround
    {
        ck::EProceduralGaitLandingGround Report = ck::EProceduralGaitLandingGround::Unknown;
        FVector Position = FVector::ZeroVector;
    };

    // The ground under the point a swing will land on, as of the last solve. The stroke overshoot and the freeze push can
    // carry a target probed on a lower tread past the next riser, where the swing lifts onto the ground found, or past a
    // top's edge into the air, where nothing is found and the swing lands on the target the gait validated instead. A
    // swing whose target was on a face when it accepted it has no tread to lift onto, so it is not probed. The debug leg
    // records the point and the ray.
    auto
        Get_LandingGround(
            UWorld* InWorld,
            const FQuat& InBasis,
            const FVector& InHip,
            float InReach,
            const ck::FProceduralGaitLegSwing& InSwing,
            const FCk_ProceduralGait_Probe& InProbe,
            const FCk_ProceduralGait_Step& InStep,
            int32& InOutRayCount,
            FCk_ProceduralAnimation_DebugLeg& InOutDebugLeg)
        -> FLandingGround
    {
        if (NOT InSwing.Get_Active() || InSwing.Get_CatchStep() || InSwing.Get_TargetOnAFace())
        { return {}; }

        const auto Up = InBasis.GetAxisZ();

        const auto Landing = InBasis.RotateVector(InSwing.Get_LandingPoint());
        const auto ProbeStart = Landing + Up * InProbe.Get_Up();
        const auto ProbeEnd = Landing - Up * InProbe.Get_Down();
        const auto Hit = UCk_Utils_JoltQuery_UE::Get_RayCast(InWorld, ProbeStart, ProbeEnd, InProbe.Get_QueryFilter());
        ++InOutRayCount;

        constexpr auto SingleAttempt = 1;
        InOutDebugLeg.Set_LandingPointWorld(Landing);
        InOutDebugLeg.Get_LandingProbe().Set_Start(ProbeStart)
            .Set_End(ProbeEnd)
            .Set_AttemptCount(SingleAttempt)
            .Set_Hit(Hit.Get_HasHit())
            .Set_HitFraction(Hit.Get_Fraction())
            .Set_HitPosition(Hit.Get_Position())
            .Set_HitNormal(Hit.Get_Normal());

        if (NOT Get_TrustedHit(Hit, -Up) || FVector::Dist(Hit.Get_Position(), InHip) > InStep.Get_ForceStepReachFraction() * InReach)
        { return FLandingGround{ck::EProceduralGaitLandingGround::None}; }

        return FLandingGround{ck::EProceduralGaitLandingGround::Found, Hit.Get_Position()};
    }

    auto
        Get_LegBit(
            int32 InLegIndex)
        -> uint64
    {
        return uint64{1} << InLegIndex;
    }

    // The direction from the hip to its rest foot in the support plane: the leg's own side. Zero for a rest foot straight
    // under the hip.
    auto
        Get_Outboard(
            const FVector& InHip,
            const FVector& InRestFoot,
            const FVector& InUp)
        -> FVector
    {
        return FVector::VectorPlaneProject(InRestFoot - InHip, InUp).GetSafeNormal();
    }

    // The largest lateral offset of the given legs' hips from the body's centreline, in the body's frame: the body's
    // half-width when the hips sit on its surface.
    template <typename T_Predicate>
    auto
        Get_MaxHipLateral(
            const TArray<FCk_Handle_ProceduralLeg>& InLegs,
            T_Predicate InCounts)
        -> float
    {
        auto MaxHipLateral = 0.0f;
        for (const auto& Leg : InLegs)
        {
            if (NOT InCounts(Leg))
            { continue; }

            const auto& HipLocal = Leg.Get<ck::FFragment_ProceduralLeg_Params>().Get_Placement().Get_HipLocal();
            MaxHipLateral = FMath::Max(MaxHipLateral, static_cast<float>(FMath::Abs(HipLocal.Y)));
        }
        return MaxHipLateral;
    }

    auto
        Get_IsLegEnabled(
            const FCk_Handle_ProceduralLeg& InLeg)
        -> bool
    {
        return ck::procedural_leg::Get_IsStructurallyEnabled(InLeg);
    }

    // Rebuilt from committed solver state; no second reservation lifetime or stale release path.
    auto
        Get_FootReservations(
            TArrayView<const FCk_Handle_ProceduralLeg> InLegs,
            const ck::FProceduralGaitSolver& InSolver,
            const FQuat& InBasis)
        -> TArray<ck::FProceduralFootReservation, TInlineAllocator<8>>
    {
        auto Reservations = TArray<ck::FProceduralFootReservation, TInlineAllocator<8>>{};
        for (auto Index = 0; Index < InLegs.Num(); ++Index)
        {
            const auto& Leg = InLegs[Index];
            if (NOT Get_IsLegEnabled(Leg) || NOT InSolver.IsLegParticipating(Index))
            { continue; }
            const auto Radius = Leg.Get<ck::FFragment_ProceduralLeg_Params>().Get_FootContactRadius();
            if (Radius <= 0.0f)
            { continue; }
            const auto& State = InSolver.GetLegState(Index);
            const auto& Swing = State.Get_Swing();
            if (Swing.Get_Active())
            {
                if (Swing.Get_TargetTrusted())
                {
                    const auto Point = Swing.Get_CommittedLandingPoint();
                    Reservations.Emplace(InBasis.RotateVector(Point), Radius, Index);
                }
            }
            else if (State.Get_Plant().Get_Trusted())
            { Reservations.Emplace(InBasis.RotateVector(State.Get_Plant().Get_Position()), Radius, Index); }
        }
        return Reservations;
    }

    auto
        Get_SearchBudgetLegs(
            int32 InEnabledLegs)
        -> int32
    {
        return FMath::Max(MinSearchLegs, InEnabledLegs / EnabledLegsPerSearch);
    }

    struct FFeetPlaneFit
    {
        ck::EProceduralFeetPlaneResult Result = ck::EProceduralFeetPlaneResult::Underdetermined;
        ck::FProceduralSurfaceFeetSupport Support;
    };

    // How much a point of ground supports the body from below: nothing unless trusted, and in proportion to how level it
    // lies, so a foot gripping a face (it holds the body sideways) weighs nothing and one on a 45-degree slope 0.71.
    auto
        Get_GroundWeight(
            const FVector& InNormal,
            bool InTrusted)
        -> float
    {
        if (NOT InTrusted)
        { return 0.0f; }

        return static_cast<float>(FMath::Max(0.0, InNormal.GetSafeNormal().Z));
    }

    // One point of ground under the feet, in the solver's support frame: Weight is how much it holds the body up (its trust
    // and how level it lies) times the swing's crossfade. The footprint spans every trusted point whatever its weight, so the
    // plant a swing left and the ground it will land on both bound it whatever the swing alpha, and a leg going to a face
    // foothold ahead keeps the body inside it though the face holds nothing up.
    struct FFeetPlaneGround
    {
        FVector Position = FVector::ZeroVector;
        float Weight = 0.0f;
        bool Trusted = false;
    };

    // The ground the enabled legs stand on or are about to, as the solve left it: a planted leg's plant; a swinging leg's
    // plant it left, weighing 1 - alpha, and the accepted landing point, weighing alpha, including only a lift the solver
    // admitted, with the trust and normal the swing recorded for its target. A foot in flight is never ground.
    // Nothing supports an airborne body.
    auto
        Get_FeetPlaneGround(
            const ck::FProceduralGaitSolver& InSolver,
            TArrayView<const ck::FProceduralGaitLegInput> InInputs)
        -> TArray<FFeetPlaneGround, TInlineAllocator<32>>
    {
        auto Ground = TArray<FFeetPlaneGround, TInlineAllocator<32>>{};
        if (InSolver.IsAirborne())
        { return Ground; }

        for (auto Index = 0; Index < InInputs.Num(); ++Index)
        {
            const auto& Input = InInputs[Index];
            if (NOT Input.Get_Enabled() || NOT Input.Get_Participating())
            { continue; }

            const auto& State = InSolver.GetLegState(Index);
            const auto& Plant = State.Get_Plant();
            const auto& Swing = State.Get_Swing();
            const auto PlantSupport = Get_GroundWeight(Plant.Get_Normal(), Plant.Get_Trusted());
            if (NOT Swing.Get_Active())
            {
                Ground.Add(FFeetPlaneGround{Plant.Get_Position(), PlantSupport, Plant.Get_Trusted()});
                continue;
            }

            const auto Alpha = FMath::Clamp(Swing.Get_Phase(), 0.0f, 1.0f);
            const auto Landing = Swing.Get_CommittedLandingPoint();
            const auto LandingSupport = Get_GroundWeight(Swing.Get_TargetNormal(), Swing.Get_TargetTrusted());
            Ground.Add(FFeetPlaneGround{Swing.Get_StartPosition(), (1.0f - Alpha) * PlantSupport, Plant.Get_Trusted()});
            Ground.Add(FFeetPlaneGround{Landing, Alpha * LandingSupport, Swing.Get_TargetTrusted()});
        }
        return Ground;
    }

    // The plane through the ground points (support frame), fitted about the body and published in world space with that frame
    // and the footprint of the trusted points.
    auto
        Get_FeetPlane(
            TArrayView<const FFeetPlaneGround> InGround,
            const FTransform& InBody)
        -> FFeetPlaneFit
    {
        const auto Basis = InBody.GetRotation();
        const auto BodySupport = Basis.UnrotateVector(InBody.GetLocation());
        auto Feet = TArray<ck::FProceduralFeetPlaneFoot, TInlineAllocator<32>>{};
        auto FootprintMin = FVector2D{TNumericLimits<double>::Max()};
        auto FootprintMax = FVector2D{TNumericLimits<double>::Lowest()};
        for (const auto& Point : InGround)
        {
            const auto Local = Point.Position - BodySupport;
            Feet.Emplace(Local, Point.Weight);
            if (NOT Point.Trusted)
            { continue; }

            FootprintMin = FVector2D{FMath::Min(FootprintMin.X, Local.X), FMath::Min(FootprintMin.Y, Local.Y)};
            FootprintMax = FVector2D{FMath::Max(FootprintMax.X, Local.X), FMath::Max(FootprintMax.Y, Local.Y)};
        }

        auto Plane = ck::FProceduralFeetPlane{};
        const auto Result = ck::FitProceduralFeetPlane(Feet, ck::ProceduralFeetPlaneMaxAngleDegrees, Plane);
        if (Result != ck::EProceduralFeetPlaneResult::Fitted)
        { return FFeetPlaneFit{Result}; }

        return FFeetPlaneFit{Result, ck::FProceduralSurfaceFeetSupport{}
            .Set_Point(InBody.GetLocation() + Basis.RotateVector(Plane.Get_Point()))
            .Set_Normal(Basis.RotateVector(Plane.Get_Normal()))
            .Set_Basis(Basis)
            .Set_Origin(InBody.GetLocation())
            .Set_FootprintMin(FootprintMin)
            .Set_FootprintMax(FootprintMax)};
    }

    // The ground under a touchdown (ck::ResolveProceduralTouchdown) in world space, confirmed with the ray a face hold is
    // re-validated with: a top along its up, a face along its own normal.
    auto
        Get_Touchdown(
            UWorld* InWorld,
            const FVector& InPlant,
            const FVector& InValidatedTarget,
            const FVector& InTargetNormal,
            const FVector& InUp,
            const FCk_ProceduralGait_Probe& InProbe,
            const FVector& InSimulationHip,
            const TOptional<FVector>& InPresentationHip,
            float InReach,
            float InFootContactRadius,
            int32 InLegIndex,
            TArrayView<const ck::FProceduralFootReservation> InReservations)
        -> ck::FProceduralTouchdown
    {
        const auto RayCast = [&](const FVector& InStart, const FVector& InEnd) -> ck::FProceduralSurfaceHit
        {
            const auto Hit = UCk_Utils_JoltQuery_UE::Get_RayCast(InWorld, InStart, InEnd, InProbe.Get_QueryFilter());
            return ck::FProceduralSurfaceHit{}
                .Set_Hit(Hit.Get_HasHit())
                .Set_Position(Hit.Get_Position())
                .Set_Normal(Hit.Get_Normal())
                .Set_Fraction(Hit.Get_Fraction());
        };
        const auto Available = [&](const FVector& InPosition) -> bool
        {
            return ck::Get_IsProceduralFootContactAvailable(InPosition, InFootContactRadius, InLegIndex, InReservations);
        };
        return ck::ResolveProceduralTouchdown(InPlant, InValidatedTarget, InTargetNormal, InUp, FaceHoldProbeHalfSpan,
            RayCast, InSimulationHip, InPresentationHip, InReach, Available);
    }

    auto
        DoPublish_FootPhaseChange(
            FCk_Handle_ProceduralLeg& InLeg,
            const ck::FFragment_ProceduralLeg& InLegComp,
            ECk_ProceduralLeg_FootPhase InPreviousPhase,
            const FVector& InPreviousPosition,
            double InDeltaSeconds)
        -> void
    {
        const auto& Foot = InLegComp.Get_Foot();
        if (Foot.Get_Phase() == InPreviousPhase)
        { return; }

        if (Foot.Get_Phase() == ECk_ProceduralLeg_FootPhase::Planted)
        {
            const auto LandingSpeed = InDeltaSeconds > 0.0 ? (Foot.Get_Position() - InPreviousPosition).Size() / InDeltaSeconds : 0.0;
            const auto Footfall = FCk_ProceduralLeg_Footfall{Foot.Get_Position(), Foot.Get_Normal(), Foot.Get_Contact(),
                static_cast<float>(LandingSpeed)};
            ck::UUtils_Signal_OnProceduralLeg_Planted::Broadcast(InLeg, ck::MakePayload(InLeg, Footfall));
            return;
        }

        constexpr auto LiftSpeed = 0.0f;
        const auto Footfall = FCk_ProceduralLeg_Footfall{Foot.Get_Position(), Foot.Get_Normal(), Foot.Get_Contact(), LiftSpeed};
        ck::UUtils_Signal_OnProceduralLeg_Lifted::Broadcast(InLeg, ck::MakePayload(InLeg, Footfall));
    }
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck_procedural_gait
{
    auto
        Get_LandingClaimsRevision(
            const FCk_Handle_ProceduralGait& InGait,
            TOptional<uint64>& OutRevision)
        -> bool
    {
        OutRevision.Reset();
        if (NOT InGait.Has<ck::FFragment_ProceduralArticulation_Params>()
            && NOT InGait.Has<ck::FFragment_ProceduralArticulation_Claims>())
        { return true; }
        if (NOT UCk_Utils_ProceduralArticulation_UE::Has(InGait)
            || NOT InGait.Has<ck::FFragment_ProceduralArticulation_Claims>())
        { return false; }
        const auto Subject = UCk_Utils_ProceduralArticulation_UE::Cast(InGait);
        constexpr auto BodyChannel = true;
        if (UCk_Utils_ProceduralArticulation_UE::Get_Status(Subject) != ECk_ProceduralAnimation_Status::Ready
            || FCk_ProceduralArticulation_Control::GetClaimant(Subject, NAME_None, BodyChannel) != FCk_ArticulationControlToken{})
        { return false; }
        OutRevision = InGait.Get<ck::FFragment_ProceduralArticulation_Claims>().Get_Revision();
        return true;
    }

    auto
        Get_IsLandingLegEligible(
            const FCk_Handle_ProceduralGait& InGait,
            const FCk_Handle_ProceduralLeg& InLeg,
            const ck::FProceduralGaitSolver& InSolver,
            int32 InIndex)
        -> bool
    {
        if (InIndex < 0 || InIndex >= InSolver.NumLegs() || ck::Is_NOT_Valid(InLeg)
            || NOT InLeg.Has<ck::FFragment_ProceduralLeg_Params>() || NOT InLeg.Has<ck::FFragment_ProceduralLeg>()
            || NOT InLeg.Has<ck::FFragment_LifetimeOwner>()
            || UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(InLeg) != InGait.ConvertToHandle()
            || NOT ck::procedural_leg::Get_IsSupporting(InLeg)
            || NOT InSolver.IsLegParticipating(InIndex))
        { return false; }
        const auto& Params = InLeg.Get<ck::FFragment_ProceduralLeg_Params>();
        if (NOT FMath::IsFinite(Params.Get_FootContactRadius()) || Params.Get_FootContactRadius() <= 0.0f)
        { return false; }
        if (InGait.Has<ck::FFragment_ProceduralArticulation_Claims>())
        {
            constexpr auto LimbChannel = false;
            if (FCk_ProceduralArticulation_Control::GetClaimant(UCk_Utils_ProceduralArticulation_UE::Cast(InGait),
                    Params.Get_Id(), LimbChannel) != FCk_ArticulationControlToken{})
            { return false; }
        }
        const auto& State = InSolver.GetLegState(InIndex);
        const auto& Swing = State.Get_Swing();
        return State.Get_AutomaticRecovery() == ck::EProceduralGaitAutomaticRecovery::None
            && Swing.Get_Active() && NOT Swing.Get_CatchStep() && Swing.Get_TargetTrusted()
            && NOT Swing.Get_CommittedLandingPoint().ContainsNaN() && NOT Swing.Get_TargetNormal().ContainsNaN()
            && Swing.Get_TargetNormal().IsNormalized();
    }
}

namespace ck
{
    auto
        Get_ProceduralGaitLandingConstraints(
            FCk_Handle_ProceduralGait InGait,
            const FTransform& InBody,
            EProceduralGaitLandingConsumer InConsumer)
        -> TArray<FProceduralGaitCommittedLanding>
    {
        auto Result = TArray<FProceduralGaitCommittedLanding>{};
        if ((InConsumer != EProceduralGaitLandingConsumer::SurfaceMotion && InConsumer != EProceduralGaitLandingConsumer::BodyPose)
            || ck::Is_NOT_Valid(InGait) || NOT InGait.Has<FFragment_ProceduralGait>()
            || NOT InGait.Has<FFragment_ProceduralGait_Tunables>() || InBody.ContainsNaN()
            || UCk_Utils_ProceduralGait_UE::Get_Status(InGait) != ECk_ProceduralAnimation_Status::Ready)
        { return Result; }
        const auto& Gait = InGait.Get<FFragment_ProceduralGait>();
        const auto& Planning = Gait._LandingPlanning;
        const auto PreviousFrame = InConsumer == EProceduralGaitLandingConsumer::SurfaceMotion;
        if ((PreviousFrame && GFrameCounter == 0) || NOT Planning.Get_HasSample()
            || Planning.Get_Frame() != GFrameCounter - (PreviousFrame ? 1 : 0)
            || Planning.Get_SolveSequence() != Gait._SolveSequence
            || NOT Planning.Get_BodyAtSolve().Equals(InBody, 1.0e-3) || Gait._Solver.IsAirborne())
        { return Result; }
        if (NOT UCk_Utils_SurfaceMotion_UE::Has(InGait) || NOT InGait.Has<FFragment_SurfaceMotion_Support>())
        { return Result; }
        const auto Motion = UCk_Utils_SurfaceMotion_UE::Cast(InGait);
        const auto& MotionSupport = InGait.Get<FFragment_SurfaceMotion_Support>();
        if (UCk_Utils_SurfaceMotion_UE::Get_Status(Motion) != ECk_ProceduralAnimation_Status::Ready
            || UCk_Utils_SurfaceMotion_UE::Get_Support(Motion) != ECk_SurfaceMotion_Support::Grounded
            || MotionSupport.Get_EvaluatedFrame() != Planning.Get_Frame()
            || NOT MotionSupport.Get_EvaluatedBody().Equals(InBody, 1.0e-3))
        { return Result; }
        auto ClaimsRevision = TOptional<uint64>{};
        if (NOT ck_procedural_gait::Get_LandingClaimsRevision(InGait, ClaimsRevision)
            || ClaimsRevision != Planning.Get_ClaimsRevision())
        { return Result; }
        const auto HasPoseFeature = InGait.Has<FFragment_ProceduralBodyPose>();
        if (HasPoseFeature && UCk_Utils_ProceduralBodyPose_UE::Get_Status(UCk_Utils_ProceduralBodyPose_UE::Cast(InGait))
            != ECk_ProceduralAnimation_Status::Ready)
        { return Result; }
        const auto PoseOffset = HasPoseFeature ? TOptional<FTransform>{
            UCk_Utils_ProceduralBodyPose_UE::Get_Offset(UCk_Utils_ProceduralBodyPose_UE::Cast(InGait))} : TOptional<FTransform>{};
        const auto& Step = InGait.Get<FFragment_ProceduralGait_Tunables>().Get_Step();
        for (const auto& Landing : Planning.Get_Landings())
        {
            const auto& Leg = Landing.Get_Leg();
            const auto Index = Gait._Legs.IndexOfByKey(Leg);
            if (NOT ck_procedural_gait::Get_IsLandingLegEligible(InGait, Leg, Gait._Solver, Index))
            { continue; }
            const auto& Params = Leg.Get<FFragment_ProceduralLeg_Params>();
            const auto& Constraint = Landing.Get_Constraint();
            const auto& Swing = Gait._Solver.GetLegState(Index).Get_Swing();
            const auto Reach = ck_procedural_gait_utils::Get_Reach(Params.Get_Chain());
            const auto Fraction = Landing.Get_Cast() == EProceduralFootholdCast::Face
                ? Step.Get_TargetReachFraction() : Step.Get_ForceStepReachFraction();
            if (Constraint.Get_Reach() != Reach || Constraint.Get_AdmissionReach() != Fraction * Reach
                || NOT Constraint.Get_HipLocal().Equals(Params.Get_Placement().Get_HipLocal(), 1.0e-6)
                || NOT Gait._Basis.RotateVector(Swing.Get_CommittedLandingPoint()).Equals(Constraint.Get_PointWorld(), 1.0e-3)
                || NOT Gait._Basis.RotateVector(Swing.Get_TargetNormal()).Equals(Landing.Get_NormalWorld(), 1.0e-3)
                || NOT Get_IsProceduralSurfaceLandingReachValid(InBody, PoseOffset, Constraint))
            { continue; }
            Result.Add(Landing);
        }
        return Result;
    }

    auto
        Get_IsProceduralHeightPlanAdmitted(
            FCk_Handle_ProceduralGait InGait,
            const FTransform& InBody,
            const TOptional<FTransform>& InPoseOffset,
            const FProceduralGaitHeightCandidate& InCandidate,
            int32& InOutRays,
            int32 InRayLimit,
            EProceduralHeightPlanStage InStage,
            TOptional<FProceduralGaitHeightRetryWitness>* OutVisibilityRetry,
            TArray<FProceduralContactStagingBoundary>* OutStagingBoundaries,
            EProceduralHeightSeedFailure* OutSeedFailure,
            FProceduralHeightRecoveryContinuation* InRecoveryContinuation)
        -> bool
    {
        if (OutSeedFailure != nullptr)
        { *OutSeedFailure = ck::EProceduralHeightSeedFailure::None; }
        if (OutVisibilityRetry != nullptr)
        { OutVisibilityRetry->Reset(); }
        if (ck::Is_NOT_Valid(InGait) || NOT InGait.Has<FFragment_ProceduralGait>()
            || NOT InGait.Has<FFragment_ProceduralGait_Tunables>() || NOT InGait.Has<FFragment_Transform>()
            || InGait.Has<FFragment_ProceduralGait_Failure>() || InGait.Has<FTag_ProceduralGait_NeedsSetup>()
            || InGait.Has<FTag_DestroyEntity_Initiate>() || InBody.ContainsNaN()
            || NOT InBody.GetRotation().IsNormalized() || InOutRays < 0 || InOutRays > InRayLimit
            || (InPoseOffset.IsSet() && (InPoseOffset->ContainsNaN() || NOT InPoseOffset->GetRotation().IsNormalized()))
            || (InStage != EProceduralHeightPlanStage::Landing && InStage != EProceduralHeightPlanStage::Transit
                && InStage != EProceduralHeightPlanStage::Recovery && InStage != EProceduralHeightPlanStage::PreparatoryBody))
        { return false; }

        const auto& Gait = InGait.Get<FFragment_ProceduralGait>();
        const auto& Planning = Gait._HeightPlanning;
        const auto& Stance = Gait._ReachStance;
        const auto& CurrentBody = InGait.Get<FFragment_Transform>().Get_Transform();
        const auto Motion = UCk_Utils_SurfaceMotion_UE::Cast(InGait);
        if (ck::Is_NOT_Valid(Motion)
            || UCk_Utils_SurfaceMotion_UE::Get_Status(Motion) != ECk_ProceduralAnimation_Status::Ready)
        { return false; }
        const auto PreparatoryBody = InStage == EProceduralHeightPlanStage::PreparatoryBody;
        if (PreparatoryBody && (NOT InGait.Has<FFragment_SurfaceMotion>()
                || NOT InGait.Has<FFragment_SurfaceMotion_Support>() || NOT InGait.Has<FFragment_SurfaceMotion_Params>()))
        { return false; }
        const auto Up = UCk_Utils_SurfaceMotion_UE::Get_SupportNormal(Motion);
        if (PreparatoryBody)
        {
            const auto& MotionState = InGait.Get<FFragment_SurfaceMotion>();
            const auto& Support = InGait.Get<FFragment_SurfaceMotion_Support>();
            auto ClaimsRevision = TOptional<uint64>{};
            if (NOT InGait.Get<FFragment_SurfaceMotion_Params>().Get_AdaptiveHeight().Get_Enabled()
                || GFrameCounter == 0 || Planning.Get_Frame() != GFrameCounter - 1
                || Support.Get_EvaluatedFrame() != Planning.Get_Frame()
                || NOT Support.Get_EvaluatedBody().Equals(CurrentBody, 1.0e-3)
                || UCk_Utils_SurfaceMotion_UE::Get_Support(Motion) != ECk_SurfaceMotion_Support::Grounded
                || Gait._Solver.IsAirborne() || Stance.Get_Anchors().IsEmpty()
                || NOT InCandidate.Get_RequiresRelief() || Up.ContainsNaN() || NOT Up.IsNormalized()
                || NOT (FVector::DotProduct(InBody.GetLocation() - CurrentBody.GetLocation(), Up) < 0.0)
                || MotionState.Get_Direction().ContainsNaN() || NOT FMath::IsFinite(MotionState.Get_Speed())
                || NOT Planning.Get_Direction().Equals(MotionState.Get_Direction(), 1.0e-3)
                || NOT FMath::IsNearlyEqual(Planning.Get_Speed(), MotionState.Get_Speed())
                || NOT ck_procedural_gait::Get_LandingClaimsRevision(InGait, ClaimsRevision))
            { return false; }
            const auto HasPose = InGait.Has<FFragment_ProceduralBodyPose>();
            if (HasPose != InPoseOffset.IsSet())
            { return false; }
            if (HasPose)
            {
                const auto Pose = UCk_Utils_ProceduralBodyPose_UE::Cast(InGait);
                if (UCk_Utils_ProceduralBodyPose_UE::Get_Status(Pose) != ECk_ProceduralAnimation_Status::Ready
                    || NOT UCk_Utils_ProceduralBodyPose_UE::Get_Offset(Pose).Equals(InPoseOffset.GetValue(), 1.0e-6))
                { return false; }
            }
            const auto IsUnclaimedMember = [&](const FCk_Handle_ProceduralLeg& InLeg) -> bool
            {
                if (NOT Gait._Legs.Contains(InLeg) || ck::Is_NOT_Valid(InLeg)
                    || NOT InLeg.Has<FFragment_ProceduralLeg>() || NOT InLeg.Has<FFragment_ProceduralLeg_Params>()
                    || NOT InLeg.Has<FFragment_LifetimeOwner>()
                    || UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(InLeg) != InGait.ConvertToHandle())
                { return false; }
                if (InGait.Has<FFragment_ProceduralArticulation_Claims>())
                {
                    constexpr auto LimbChannel = false;
                    if (FCk_ProceduralArticulation_Control::GetClaimant(UCk_Utils_ProceduralArticulation_UE::Cast(InGait),
                            InLeg.Get<FFragment_ProceduralLeg_Params>().Get_Id(), LimbChannel) != FCk_ArticulationControlToken{})
                    { return false; }
                }
                return true;
            };
            if (NOT IsUnclaimedMember(InCandidate.Get_Leg()))
            { return false; }
            for (const auto& Anchor : Stance.Get_Anchors())
            {
                if (NOT IsUnclaimedMember(Anchor.Get_Leg()))
                { return false; }
            }
        }
        if (Gait._Legs.Num() < 2 || Gait._Legs.Num() > 64 || Planning.Get_Candidates().Num() > 2 * Gait._Legs.Num()
            || NOT Up.IsNormalized()
            || NOT Planning.Get_HasSample() || Planning.Get_SolveSequence() != Gait._SolveSequence
            || NOT Planning.Get_BodyAtSolve().Equals(CurrentBody, 1.0e-3)
            || NOT Stance.Get_HasSample() || Stance.Get_SolveSequence() != Gait._SolveSequence
            || NOT Stance.Get_BodyAtSolve().Equals(CurrentBody, 1.0e-3)
            || NOT CurrentBody.GetRotation().Equals(InBody.GetRotation(), 1.0e-3)
            || NOT CurrentBody.GetScale3D().Equals(InBody.GetScale3D(), 1.0e-3)
            || NOT FVector::VectorPlaneProject(InBody.GetLocation() - CurrentBody.GetLocation(),
                Up).IsNearlyZero(1.0e-3))
        { return false; }
        const auto BelongsToSample = Planning.Get_Candidates().ContainsByPredicate([&](const auto& InStored)
        {
            return InStored.Get_Leg() == InCandidate.Get_Leg()
                && InStored.Get_PointWorld().Equals(InCandidate.Get_PointWorld(), 1.0e-3)
                && InStored.Get_NormalWorld().Equals(InCandidate.Get_NormalWorld(), 1.0e-3)
                && InStored.Get_HipLocal().Equals(InCandidate.Get_HipLocal(), 1.0e-3)
                && InStored.Get_FootWorld().Equals(InCandidate.Get_FootWorld(), 1.0e-3)
                && InStored.Get_Reach() == InCandidate.Get_Reach()
                && InStored.Get_AdmissionReach() == InCandidate.Get_AdmissionReach()
                && InStored.Get_RequiresRelief() == InCandidate.Get_RequiresRelief();
        });
        if (NOT BelongsToSample || InCandidate.Get_PointWorld().ContainsNaN()
            || NOT InCandidate.Get_NormalWorld().IsNormalized())
        { return false; }

        auto EnabledMask = uint64{0};
        auto SupportingMask = uint64{0};
        auto CurrentPlants = 0;
        for (auto Index = 0; Index < Gait._Legs.Num(); ++Index)
        {
            const auto& Leg = Gait._Legs[Index];
            if (procedural_leg::Get_IsLive(Leg)
                && (NOT Leg.Has<FFragment_ProceduralLeg>() || NOT Leg.Has<FFragment_ProceduralLeg_Params>()
                    || NOT Leg.Has<FFragment_LifetimeOwner>()
                    || UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(Leg) != InGait.ConvertToHandle()))
            { return false; }
            if (ck_procedural_gait::Get_IsLegEnabled(Leg))
            { EnabledMask |= ck_procedural_gait::Get_LegBit(Index); }
            if (NOT procedural_leg::Get_IsSupporting(Leg))
            { continue; }
            SupportingMask |= ck_procedural_gait::Get_LegBit(Index);
            if (NOT Leg.Has<FFragment_ProceduralLeg>() || NOT Leg.Has<FFragment_ProceduralLeg_Params>())
            { return false; }
            const auto& Foot = Leg.Get<FFragment_ProceduralLeg>().Get_Foot();
            if (Foot.Get_Phase() == ECk_ProceduralLeg_FootPhase::Planted
                && Foot.Get_Contact() == ECk_ProceduralLeg_FootContact::Trusted)
            { ++CurrentPlants; }
        }
        if (EnabledMask != Planning.Get_EnabledMask() || SupportingMask != Planning.Get_SupportingMask()
            || CurrentPlants != Stance.Get_Anchors().Num())
        { return false; }

        const auto& Leg = InCandidate.Get_Leg();
        const auto LegIndex = Gait._Legs.IndexOfByKey(Leg);
        if (LegIndex == INDEX_NONE || NOT procedural_leg::Get_IsSupporting(Leg)
            || NOT Leg.Get<FFragment_ProceduralLeg>().Get_Foot().Get_Position().Equals(InCandidate.Get_FootWorld(), 1.0e-3))
        { return false; }
        const auto& Tunables = InGait.Get<FFragment_ProceduralGait_Tunables>();
        const auto& Params = Leg.Get<FFragment_ProceduralLeg_Params>();
        const auto& Placement = Params.Get_Placement();
        const auto Reach = ck_procedural_gait_utils::Get_Reach(Params.Get_Chain());
        if (Reach != InCandidate.Get_Reach() || NOT Placement.Get_HipLocal().Equals(InCandidate.Get_HipLocal(), 1.0e-3)
            || Reach * Tunables.Get_Step().Get_ForceStepReachFraction() != InCandidate.Get_AdmissionReach())
        { return false; }
        const auto PresentationBody = InPoseOffset.IsSet() ? InPoseOffset.GetValue() * InBody : InBody;

        // Transit approaches a separately admitted goal. PreparatoryBody preserves the same native retained contacts
        // without a landing promise; SurfaceMotion separately requires this call's blocked full simulation sweep.
        // Each landing seed needs its freshness ray, up to three initial rays, one face fallback, three reprobe rays
        // and the final native candidate's visibility ray. Reprojection never publishes a foothold or search history.
        constexpr auto MaxCandidateRays = 9;
        const auto CandidateRays = InStage == EProceduralHeightPlanStage::Recovery
            ? MaxCandidateRays * Planning.Get_Candidates().Num()
            : InStage == EProceduralHeightPlanStage::Landing ? MaxCandidateRays : 1;
        const auto RequiredRays = CandidateRays + 3 * Stance.Get_Anchors().Num();
        if (RequiredRays > InRayLimit - InOutRays)
        { return false; }
        auto* World = UCk_Utils_EntityLifetime_UE::Get_WorldForEntity(InGait);
        if (ck::Is_NOT_Valid(World))
        { return false; }
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
        auto HeightObservation = ck_procedural_height_seed_test::Begin(InGait, Leg, LegIndex, World,
            Gait._SolveSequence, static_cast<uint64>(InStage), InOutRays, OutSeedFailure != nullptr);
        ON_SCOPE_EXIT { ck_procedural_height_seed_test::Finish(HeightObservation, InOutRays, OutSeedFailure); };
#endif
        const auto& Probe = Tunables.Get_Probe();
        const auto& Foothold = Tunables.Get_Foothold();
        const auto Rehit = [&](const FVector& InPoint, const FVector& InNormal) -> TOptional<FCk_Jolt_HitResult>
        {
            const auto Hit = UCk_Utils_JoltQuery_UE::Get_RayCast(World,
                InPoint + InNormal * ck_procedural_gait::FaceHoldProbeHalfSpan,
                InPoint - InNormal * ck_procedural_gait::FaceHoldProbeHalfSpan, Probe.Get_QueryFilter());
            ++InOutRays;
            if (NOT ck_procedural_gait::Get_TrustedHit(Hit, -InNormal)
                || FVector::Dist(Hit.Get_Position(), InPoint) > Foothold.Get_OcclusionTolerance()
                || FVector::DotProduct(Hit.Get_Normal(), InNormal) <= 0.0f)
            { return {}; }
            return Hit;
        };
        const auto Reservations = ck_procedural_gait::Get_FootReservations(Gait._Legs, Gait._Solver, Gait._Basis);
        const auto MaxHipLateral = ck_procedural_gait::Get_MaxHipLateral(Gait._Legs, &ck_procedural_gait::Get_IsLegEnabled);
        const auto MakeQuery = [&](int32 InIndex) -> ck_procedural_gait::FFootholdQuery
        {
            const auto& QueryParams = Gait._Legs[InIndex].Get<FFragment_ProceduralLeg_Params>();
            const auto& QueryPlacement = QueryParams.Get_Placement();
            const auto QueryHip = InBody.TransformPosition(QueryPlacement.Get_HipLocal());
            return ck_procedural_gait::FFootholdQuery{
                .World = World,
                .Basis = InBody.GetRotation(),
                .BodyLocation = InBody.GetLocation(),
                .Hip = QueryHip,
                .Outboard = ck_procedural_gait::Get_Outboard(QueryHip,
                    InBody.TransformPosition(QueryPlacement.Get_RestFootLocal()), InBody.GetRotation().GetAxisZ()),
                .MaxHipLateral = MaxHipLateral,
                .Reach = ck_procedural_gait_utils::Get_Reach(QueryParams.Get_Chain()),
                .RestDrop = ck_procedural_gait::Get_RestDrop(QueryPlacement),
                .Probe = &Probe,
                .Step = &Tunables.Get_Step(),
                .Foothold = &Foothold,
                .FootContactRadius = QueryParams.Get_FootContactRadius(),
                .LegIndex = InIndex,
                .Reservations = Reservations
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
                , .HeightSeedObservation = HeightObservation.Enabled && InIndex == LegIndex ? &HeightObservation : nullptr
#endif
            };
        };
        auto ContinuationUsed = false;
        auto CaptureRecoveryNative = InRecoveryContinuation != nullptr;
#if WITH_DEV_AUTOMATION_TESTS
        auto* RecoveryReceipt = InRecoveryContinuation != nullptr && InStage == EProceduralHeightPlanStage::Recovery
            ? InRecoveryContinuation->_TestReceipt : nullptr;
        if (RecoveryReceipt == nullptr && InStage == EProceduralHeightPlanStage::Recovery && IsInGameThread()
            && InGait.Has<FFragment_ProceduralHeightPlanning_Test>())
        {
            const auto& ReceiptOwner = InGait.Get<FFragment_ProceduralHeightPlanning_Test>();
            if (ReceiptOwner.Frame == GFrameCounter && ReceiptOwner.Solve == Gait._SolveSequence)
            { RecoveryReceipt = ReceiptOwner.ActiveRecoveryReceipt; }
        }
        CaptureRecoveryNative = CaptureRecoveryNative || RecoveryReceipt != nullptr;
        if (RecoveryReceipt != nullptr)
        { *RecoveryReceipt = FProceduralHeightRecoveryTestReceipt{}; }
        const auto RecordRecoverySeed = [](const ck_procedural_gait::FFootholdCandidate& InNative,
            bool InMetadataFresh, int32 InRays) -> FProceduralHeightRecoveryTestSeed
        {
            return {InMetadataFresh, InNative.NativeHit, InNative.Cast, InNative.Verdict,
                InNative.Position, InNative.Normal, InRays};
        };
#endif
        auto AdmittedSeed = TOptional<ck_procedural_gait::FFootholdCandidate>{};
        const auto AdmitLanding = [&](const FProceduralGaitHeightCandidate& InObserved, int32 InIndex,
            EProceduralHeightSeedFailure* OutLandingFailure = nullptr,
            TOptional<ck_procedural_gait::FFootholdCandidate>* OutNative = nullptr) -> bool
        {
            if (OutLandingFailure != nullptr)
            { *OutLandingFailure = EProceduralHeightSeedFailure::None; }
            if (OutNative != nullptr)
            { OutNative->Reset(); }
            const auto& ObservedLeg = Gait._Legs[InIndex];
            const auto& ObservedParams = ObservedLeg.Get<FFragment_ProceduralLeg_Params>();
            const auto& ObservedPlacement = ObservedParams.Get_Placement();
            const auto ObservedReach = ck_procedural_gait_utils::Get_Reach(ObservedParams.Get_Chain());
            if (ObservedLeg != InObserved.Get_Leg()
                || NOT ObservedLeg.Get<FFragment_ProceduralLeg>().Get_Foot().Get_Position().Equals(InObserved.Get_FootWorld(), 1.0e-3)
                || NOT ObservedPlacement.Get_HipLocal().Equals(InObserved.Get_HipLocal(), 1.0e-3)
                || ObservedReach != InObserved.Get_Reach()
                || ObservedReach * Tunables.Get_Step().Get_ForceStepReachFraction() != InObserved.Get_AdmissionReach()
                || InObserved.Get_PointWorld().ContainsNaN() || NOT InObserved.Get_NormalWorld().IsNormalized())
            { return false; }
            const auto Hit = Rehit(InObserved.Get_PointWorld(), InObserved.Get_NormalWorld());
            if (NOT Hit.IsSet())
            { return false; }
            const auto LandingQuery = MakeQuery(InIndex);
            auto SeedFailure = EProceduralHeightSeedFailure::None;
            const auto Candidate = ck_procedural_gait::DoProbe_Down(LandingQuery, InObserved.Get_PointWorld(),
                EProceduralFootholdSource::Ring, ck_procedural_gait::EGroundProbeAttempts::Every,
                ck_procedural_gait::EFootholdReprobe::ClampTowardHip, ck_procedural_gait::EFootholdStage::Search,
                InOutRays, nullptr, OutLandingFailure != nullptr ? &SeedFailure : nullptr);
            if (OutNative != nullptr)
            { *OutNative = Candidate; }
            const auto Admitted = Candidate.Verdict == EProceduralFootholdVerdict::Usable
                && FVector::Dist(LandingQuery.Hip, Candidate.Position) <= InObserved.Get_AdmissionReach()
                && FVector::Dist(PresentationBody.TransformPosition(ObservedPlacement.Get_HipLocal()), Candidate.Position)
                    <= InObserved.Get_AdmissionReach();
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
            if (HeightObservation.Enabled)
            {
                using namespace ck_procedural_height_seed_test;
                Put(HeightObservation, 26, Candidate.NativeHit); Put(HeightObservation, 27, static_cast<uint64>(Candidate.Verdict));
                if (OutLandingFailure != nullptr) { Put(HeightObservation, 28, static_cast<uint64>(SeedFailure)); }
                if (Candidate.NativeHit) { Vector(HeightObservation, 32, Candidate.Position); }
                Put(HeightObservation, 35, Admitted);
            }
#endif
            if (NOT Admitted && OutLandingFailure != nullptr)
            { *OutLandingFailure = SeedFailure; }
            if (Admitted && Candidate.Cast == EProceduralFootholdCast::Down
                && InStage == EProceduralHeightPlanStage::Landing && OutVisibilityRetry != nullptr)
            { AdmittedSeed = Candidate; }
            return Admitted;
        };
        if (InStage == EProceduralHeightPlanStage::Recovery)
        {
            for (auto Index = 0; Index < Gait._Legs.Num(); ++Index)
            {
                if ((SupportingMask & ck_procedural_gait::Get_LegBit(Index)) == 0)
                { continue; }
                auto HasLanding = false;
                auto AllFailedSeedsFinalForce = true;
                auto AllFailedSeedsUnderBody = true;
                auto FailedSeeds = TArray<ck_procedural_gait::FFootholdCandidate, TInlineAllocator<2>>{};
                auto TriedCandidates = 0;
#if WITH_DEV_AUTOMATION_TESTS
                auto OriginalSeeds = TArray<FProceduralHeightRecoveryTestSeed, TInlineAllocator<2>>{};
#endif
                for (const auto& Observed : Planning.Get_Candidates())
                {
                    if (Observed.Get_Leg() != Gait._Legs[Index])
                    { continue; }
                    if (++TriedCandidates > 2)
                    { return false; }
                    auto SeedFailure = EProceduralHeightSeedFailure::None;
                    auto Native = TOptional<ck_procedural_gait::FFootholdCandidate>{};
#if WITH_DEV_AUTOMATION_TESTS
                    const auto SeedRays = InOutRays;
                    if (RecoveryReceipt != nullptr)
                    { ++RecoveryReceipt->OriginalCallsPerLeg[Index]; }
#endif
                    const auto Admitted = AdmitLanding(Observed, Index, &SeedFailure,
                        CaptureRecoveryNative ? &Native : nullptr);
#if WITH_DEV_AUTOMATION_TESTS
                    if (RecoveryReceipt != nullptr)
                    {
                        auto SeedReceipt = Native.IsSet() ? RecordRecoverySeed(Native.GetValue(), true, InOutRays - SeedRays)
                            : FProceduralHeightRecoveryTestSeed{};
                        SeedReceipt.Rays = InOutRays - SeedRays;
                        OriginalSeeds.Add(SeedReceipt);
                    }
#endif
                    if (Admitted)
                    {
                        HasLanding = true;
                        break;
                    }
                    AllFailedSeedsFinalForce = AllFailedSeedsFinalForce
                        && SeedFailure == EProceduralHeightSeedFailure::TrustedDownFinalForceReject;
                    const auto TrustedUnderBody = Native.IsSet() && Native->NativeHit
                        && Native->Cast == EProceduralFootholdCast::Down
                        && Native->Verdict == EProceduralFootholdVerdict::UnderBody
                        && NOT Native->Position.ContainsNaN() && Native->Normal.IsNormalized();
                    AllFailedSeedsUnderBody = AllFailedSeedsUnderBody && TrustedUnderBody;
                    if (TrustedUnderBody)
                    { FailedSeeds.Add(Native.GetValue()); }
                }
                if (NOT HasLanding)
                {
#if WITH_DEV_AUTOMATION_TESTS
                    if (RecoveryReceipt != nullptr && RecoveryReceipt->FirstFailedLeg == INDEX_NONE)
                    {
                        RecoveryReceipt->FirstFailedLeg = Index;
                        RecoveryReceipt->OriginalSeedCount = OriginalSeeds.Num();
                        for (auto SeedIndex = 0; SeedIndex < OriginalSeeds.Num(); ++SeedIndex)
                        { RecoveryReceipt->OriginalSeeds[SeedIndex] = OriginalSeeds[SeedIndex]; }
                        RecoveryReceipt->OriginalRaysBeforeContinuation = InOutRays;
                    }
                    else if (RecoveryReceipt != nullptr && ContinuationUsed)
                    { RecoveryReceipt->LaterLegFailed = true; }
#endif
                    const auto Qualifies = NOT ContinuationUsed && TriedCandidates > 0 && TriedCandidates <= 2
                        && AllFailedSeedsUnderBody && FailedSeeds.Num() == TriedCandidates
                        && InRecoveryContinuation != nullptr && InRecoveryContinuation->_TryConsume
                        && InRecoveryContinuation->_IsCurrent;
#if WITH_DEV_AUTOMATION_TESTS
                    if (RecoveryReceipt != nullptr && NOT ContinuationUsed)
                    { RecoveryReceipt->Qualified = Qualifies; }
#endif
                    if (Qualifies && InRecoveryContinuation->_TryConsume(InOutRays))
                    {
                        ContinuationUsed = true;
#if WITH_DEV_AUTOMATION_TESTS
                        if (RecoveryReceipt != nullptr)
                        { RecoveryReceipt->Consumed = true; }
#endif
                        const auto Query = MakeQuery(Index);
                        const auto SearchRadius = Foothold.Get_SearchRadius() > 0.0f
                            ? Foothold.Get_SearchRadius() : ck_procedural_gait::DerivedSearchRadiusShareOfReach * Query.Reach;
                        const auto BoundaryQuery = FProceduralFootholdUnderBodyBoundaryQuery{
                            .BodyLocation = Query.BodyLocation,
                            .Hip = Query.Hip,
                            .Lateral = Query.Basis.GetAxisY(),
                            .Outboard = Query.Outboard,
                            .BodyBoundary = static_cast<double>(Query.MaxHipLateral) - ck_procedural_gait::UnderBodyMargin + 0.1,
                            .SearchRadius = SearchRadius,
                            .ForceReach = Query.Step->Get_ForceStepReachFraction() * Query.Reach,
                            .OwnSideTolerance = ck_procedural_gait::InboardToleranceShareOfReach * Query.Reach,
                            .FootContactRadius = Query.FootContactRadius,
                            .LegIndex = Query.LegIndex,
                            .Reservations = Query.Reservations};
                        auto JointBoundaryAttempted = false;
#if WITH_DEV_AUTOMATION_TESTS
                        const auto BoundaryStartRays = InOutRays;
#endif
                        for (const auto& Failed : FailedSeeds)
                        {
#if WITH_DEV_AUTOMATION_TESTS
                            auto ConstructedPoints = 0;
#endif
                            const auto Point = Get_ProceduralFootholdUnderBodyBoundary(BoundaryQuery,
                                Failed.Position, Failed.Normal, JointBoundaryAttempted
#if WITH_DEV_AUTOMATION_TESTS
                                , &ConstructedPoints
#endif
                                );
#if WITH_DEV_AUTOMATION_TESTS
                            if (RecoveryReceipt != nullptr)
                            { RecoveryReceipt->GeometryPointCount += ConstructedPoints; }
#endif
                            if (NOT Point.IsSet() || NOT Get_IsProceduralFootholdNativePointRepresentable(Point.GetValue()))
                            { continue; }
                            auto NativeSpansFit = true;
                            for (auto Attempt = 0; Attempt < 2; ++Attempt)
                            {
                                const auto Span = MakeProceduralGroundProbeSpan(Attempt, Probe.Get_Up(),
                                    Probe.Get_Down(), Probe.Get_OutwardLean());
                                if (NOT FMath::IsFinite(Span.Get_UpDistance()) || NOT FMath::IsFinite(Span.Get_DownDistance())
                                    || NOT FMath::IsFinite(Span.Get_OutwardLean()))
                                { NativeSpansFit = false; break; }
                                const auto ProbeUp = Query.Basis.GetAxisZ();
                                const auto Radial = FVector::VectorPlaneProject(Point.GetValue() - Query.BodyLocation, ProbeUp).GetSafeNormal();
                                const auto Axis = FProceduralGaitSolver::ComputeTraceAxis(ProbeUp, Radial, Span.Get_OutwardLean());
                                if (NOT Get_IsProceduralFootholdNativePointRepresentable(Point.GetValue() + Axis * Span.Get_UpDistance())
                                    || NOT Get_IsProceduralFootholdNativePointRepresentable(Point.GetValue() - Axis * Span.Get_DownDistance()))
                                { NativeSpansFit = false; break; }
                            }
                            if (NOT NativeSpansFit)
                            { continue; }
                            if (NOT InRecoveryContinuation->_IsCurrent())
                            { return false; }
#if WITH_DEV_AUTOMATION_TESTS
                            const auto BoundaryRays = InOutRays;
#endif
                            const auto Boundary = ck_procedural_gait::DoProbe_Down(Query, Point.GetValue(),
                                EProceduralFootholdSource::Ring, ck_procedural_gait::EGroundProbeAttempts::RetryFromInsideSolid,
                                ck_procedural_gait::EFootholdReprobe::None, ck_procedural_gait::EFootholdStage::Search,
                                InOutRays, nullptr);
                            HasLanding = Boundary.Verdict == EProceduralFootholdVerdict::Usable
                                && FVector::Dist(Query.Hip, Boundary.Position) <= BoundaryQuery.ForceReach
                                && FVector::Dist(PresentationBody.TransformPosition(
                                    Gait._Legs[Index].Get<FFragment_ProceduralLeg_Params>().Get_Placement().Get_HipLocal()),
                                    Boundary.Position) <= BoundaryQuery.ForceReach;
#if WITH_DEV_AUTOMATION_TESTS
                            if (RecoveryReceipt != nullptr)
                            {
                                const auto SeedIndex = RecoveryReceipt->BoundarySeedCount++;
                                RecoveryReceipt->BoundaryPoints[SeedIndex] = Point.GetValue();
                                RecoveryReceipt->BoundarySeeds[SeedIndex] = RecordRecoverySeed(Boundary, false, InOutRays - BoundaryRays);
                                RecoveryReceipt->ContinuationRays = InOutRays - BoundaryStartRays;
                                RecoveryReceipt->BoundaryAccepted = HasLanding;
                            }
#endif
                            if (HasLanding)
                            { break; }
                            if (NOT InRecoveryContinuation->_IsCurrent())
                            { return false; }
                        }
                        if (NOT InRecoveryContinuation->_IsCurrent())
                        { return false; }
                    }
                    if (HasLanding)
                    { continue; }
                    if (OutSeedFailure != nullptr && TriedCandidates > 0 && TriedCandidates <= 2 && AllFailedSeedsFinalForce)
                    { *OutSeedFailure = EProceduralHeightSeedFailure::TrustedDownFinalForceReject; }
                    return false;
                }
            }
        }
        else if (InStage == EProceduralHeightPlanStage::Landing)
        {
            auto SeedFailure = EProceduralHeightSeedFailure::None;
            if (NOT AdmitLanding(InCandidate, LegIndex, &SeedFailure))
            {
                if (OutSeedFailure != nullptr)
                { *OutSeedFailure = SeedFailure; }
                return false;
            }
        }
        else if (NOT Rehit(InCandidate.Get_PointWorld(), InCandidate.Get_NormalWorld()).IsSet())
        { return false; }

        const auto PublishVisibilityRetry = [&]() -> void
        {
            if (OutVisibilityRetry == nullptr || InStage != EProceduralHeightPlanStage::Landing || NOT AdmittedSeed.IsSet())
            { return; }
            *OutVisibilityRetry = FProceduralGaitHeightRetryWitness{InCandidate, CurrentBody, InBody, InPoseOffset,
                Up, AdmittedSeed->Position, AdmittedSeed->Normal, AdmittedSeed->Cast, Gait._SolveSequence, GFrameCounter};
        };
        const auto RecordBoundary = [&](const FProceduralGaitReachAnchor& InAnchor,
            const FCk_Jolt_HitResult& InTop, const TOptional<FCk_Jolt_HitResult>& InFace) -> void
        {
            if (OutStagingBoundaries == nullptr || ck::Is_NOT_Valid(InTop.Get_Entity())
                || InTop.Get_Position().ContainsNaN() || NOT InTop.Get_Normal().IsNormalized())
            { return; }
            auto* Boundary = OutStagingBoundaries->FindByPredicate([&](const auto& InExisting)
                { return InExisting._Leg == InAnchor.Get_Leg(); });
            if (Boundary == nullptr)
            {
                if (OutStagingBoundaries->Num() >= 64)
                { return; }
                Boundary = &OutStagingBoundaries->AddDefaulted_GetRef();
                Boundary->_Leg = InAnchor.Get_Leg();
                Boundary->_Plant = InAnchor.Get_FootWorld();
                Boundary->_Attribution = InTop.Get_Entity();
            }
            if (Boundary->_Attribution != InTop.Get_Entity()
                || Boundary->_Plant != InAnchor.Get_FootWorld())
            { return; }
            Boundary->_TopPoint = InTop.Get_Position();
            Boundary->_TopNormal = InTop.Get_Normal();
            if (Boundary->_FaceNormal.IsZero() && InFace.IsSet()
                && ck::IsValid(InFace->Get_Entity()) && InFace->Get_Entity() == InTop.Get_Entity()
                && NOT InFace->Get_Position().ContainsNaN() && InFace->Get_Normal().IsNormalized())
            {
                Boundary->_FacePoint = InFace->Get_Position();
                Boundary->_FaceNormal = InFace->Get_Normal();
            }
        };
        auto Query = MakeQuery(LegIndex);
        for (const auto& Anchor : Stance.Get_Anchors())
        {
            const auto& AnchorLeg = Anchor.Get_Leg();
            const auto AnchorIndex = Gait._Legs.IndexOfByKey(AnchorLeg);
            if (AnchorIndex == INDEX_NONE || NOT procedural_leg::Get_IsLive(AnchorLeg)
                || NOT AnchorLeg.Has<FFragment_ProceduralLeg>() || NOT AnchorLeg.Has<FFragment_ProceduralLeg_Params>()
                || NOT AnchorLeg.Has<FFragment_LifetimeOwner>()
                || UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(AnchorLeg) != InGait.ConvertToHandle()
                || NOT procedural_leg::Get_IsSupporting(AnchorLeg))
            { return false; }
            const auto& Foot = AnchorLeg.Get<FFragment_ProceduralLeg>().Get_Foot();
            const auto& AnchorParams = AnchorLeg.Get<FFragment_ProceduralLeg_Params>();
            if (Foot.Get_Phase() != ECk_ProceduralLeg_FootPhase::Planted
                || Foot.Get_Contact() != ECk_ProceduralLeg_FootContact::Trusted
                || NOT Foot.Get_Position().Equals(Anchor.Get_FootWorld(), 1.0e-3)
                || NOT AnchorParams.Get_Placement().Get_HipLocal().Equals(Anchor.Get_HipLocal(), 1.0e-3)
                || ck_procedural_gait_utils::Get_Reach(AnchorParams.Get_Chain()) != Anchor.Get_Reach()
                || Foot.Get_Normal().ContainsNaN() || NOT Foot.Get_Normal().IsNormalized())
            { return false; }
            const auto AnchorHip = InBody.TransformPosition(Anchor.Get_HipLocal());
            const auto AnchorPosedHip = PresentationBody.TransformPosition(Anchor.Get_HipLocal());
            const auto PhysicalReach = Anchor.Get_Reach() + 1.0e-3;
            auto NativeTop = TOptional<FCk_Jolt_HitResult>{};
            if (FVector::Dist(AnchorHip, Anchor.Get_FootWorld()) > PhysicalReach
                || FVector::Dist(AnchorPosedHip, Anchor.Get_FootWorld()) > PhysicalReach
                || NOT Get_IsProceduralFootContactAvailable(Anchor.Get_FootWorld(), AnchorParams.Get_FootContactRadius(),
                    AnchorIndex, Reservations)
                || NOT (NativeTop = Rehit(Anchor.Get_FootWorld(), Foot.Get_Normal())).IsSet())
            { return false; }
            RecordBoundary(Anchor, NativeTop.GetValue(), {});
            Query.Hip = AnchorHip;
            const auto SimulationBlocker = ck_procedural_gait::Get_Occluder(Query, Anchor.Get_FootWorld(), InOutRays);
            if (SimulationBlocker.IsSet())
            { RecordBoundary(Anchor, NativeTop.GetValue(), SimulationBlocker); PublishVisibilityRetry(); return false; }
            Query.Hip = AnchorPosedHip;
            const auto PresentationBlocker = ck_procedural_gait::Get_Occluder(Query, Anchor.Get_FootWorld(), InOutRays);
            if (PresentationBlocker.IsSet())
            { RecordBoundary(Anchor, NativeTop.GetValue(), PresentationBlocker); PublishVisibilityRetry(); return false; }
        }
        if (ContinuationUsed && NOT InRecoveryContinuation->_IsCurrent())
        { return false; }
#if WITH_DEV_AUTOMATION_TESTS
        if (RecoveryReceipt != nullptr)
        { RecoveryReceipt->AdmissionAccepted = true; }
#endif
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
        HeightObservation.Result = true;
#endif
        return true;
    }

    auto
        Get_ProceduralContactStagingLegs(
            FCk_Handle_ProceduralGait InGait,
            const TArray<FProceduralContactStagingBoundary>& InBoundaries)
        -> TArray<FProceduralContactStagingLeg>
    {
        auto Result = TArray<FProceduralContactStagingLeg>{};
        if (ck::Is_NOT_Valid(InGait) || NOT InGait.Has<FFragment_ProceduralGait>()
            || NOT InGait.Has<FFragment_ProceduralGait_Tunables>() || NOT InGait.Has<FFragment_Transform>()
            || NOT InGait.Has<FFragment_SurfaceMotion_Params>() || InBoundaries.Num() > 64
            || UCk_Utils_ProceduralGait_UE::Get_Status(InGait) != ECk_ProceduralAnimation_Status::Ready
            || NOT InGait.Get<FFragment_SurfaceMotion_Params>().Get_AdaptiveHeight().Get_Enabled()
            || NOT UCk_Utils_SurfaceMotion_UE::Has(InGait))
        { return Result; }
        const auto Motion = UCk_Utils_SurfaceMotion_UE::Cast(InGait);
        const auto& Body = InGait.Get<FFragment_Transform>().Get_Transform();
        const auto& Gait = InGait.Get<FFragment_ProceduralGait>();
        const auto& Planning = Gait._HeightPlanning;
        const auto& Stance = Gait._ReachStance;
        auto ClaimsRevision = TOptional<uint64>{};
        if (UCk_Utils_SurfaceMotion_UE::Get_Status(Motion) != ECk_ProceduralAnimation_Status::Ready
            || UCk_Utils_SurfaceMotion_UE::Get_Support(Motion) != ECk_SurfaceMotion_Support::Grounded
            || Body.ContainsNaN() || NOT Body.GetRotation().IsNormalized() || GFrameCounter == 0 || GFrameCounter == MAX_uint64
            || Gait._SolveSequence == MAX_uint64
            || Gait._Legs.Num() < 2 || Gait._Legs.Num() > 64 || Gait._Solver.NumLegs() != Gait._Legs.Num()
            || Gait._Solver.IsAirborne() || NOT Planning.Get_HasSample() || NOT Stance.Get_HasSample()
            || Planning.Get_Frame() != GFrameCounter - 1 || Planning.Get_SolveSequence() != Gait._SolveSequence
            || Stance.Get_SolveSequence() != Gait._SolveSequence
            || NOT Planning.Get_BodyAtSolve().Equals(Body, 1.0e-3) || NOT Stance.Get_BodyAtSolve().Equals(Body, 1.0e-3)
            || NOT Planning.Get_Direction().Equals(InGait.Get<FFragment_SurfaceMotion>().Get_Direction(), 1.0e-3)
            || Planning.Get_Speed() != InGait.Get<FFragment_SurfaceMotion>().Get_Speed()
            || NOT ck_procedural_gait::Get_LandingClaimsRevision(InGait, ClaimsRevision))
        { return Result; }
        const auto& Tunables = InGait.Get<FFragment_ProceduralGait_Tunables>();
        auto EnabledMask = uint64{0};
        auto SupportingMask = uint64{0};
        auto PlantCount = 0;
        for (auto Index = 0; Index < Gait._Legs.Num(); ++Index)
        {
            const auto& Leg = Gait._Legs[Index];
            if (ck_procedural_gait::Get_IsLegEnabled(Leg))
            { EnabledMask |= ck_procedural_gait::Get_LegBit(Index); }
            if (procedural_leg::Get_IsStructurallyEnabled(Leg)
                && (NOT Leg.Has<FFragment_ProceduralLeg_Params>() || NOT Leg.Has<FFragment_LifetimeOwner>()
                    || UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(Leg) != InGait.ConvertToHandle()))
            { return {}; }
            if (NOT procedural_leg::Get_IsSupporting(Leg))
            { continue; }
            SupportingMask |= ck_procedural_gait::Get_LegBit(Index);
            if (ck::Is_NOT_Valid(Leg) || NOT Leg.Has<FFragment_ProceduralLeg>()
                || NOT Leg.Has<FFragment_ProceduralLeg_Params>() || Leg.Has<FTag_DestroyEntity_Initiate>()
                || UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(Leg) != InGait.ConvertToHandle()
                || NOT Gait._Solver.IsLegParticipating(Index)
                || (Gait._UnplantedMask & ck_procedural_gait::Get_LegBit(Index)) != 0
                || Gait._ParticipationLeases.ContainsByPredicate([&](const auto& InLease)
                    { return InLease.Get_LegIndices().Contains(Index); }))
            { return {}; }
            const auto& Params = Leg.Get<FFragment_ProceduralLeg_Params>();
            if (InGait.Has<FFragment_ProceduralArticulation_Claims>())
            {
                constexpr auto LimbChannel = false;
                if (FCk_ProceduralArticulation_Control::GetClaimant(UCk_Utils_ProceduralArticulation_UE::Cast(InGait),
                        Params.Get_Id(), LimbChannel) != FCk_ArticulationControlToken{})
                { return {}; }
            }
            const auto& Foot = Leg.Get<FFragment_ProceduralLeg>().Get_Foot();
            if (Foot.Get_Phase() != ECk_ProceduralLeg_FootPhase::Planted
                || Foot.Get_Contact() != ECk_ProceduralLeg_FootContact::Trusted)
            { continue; }
            ++PlantCount;
            const auto* Anchor = Stance.Get_Anchors().FindByPredicate([&](const auto& InAnchor)
                { return InAnchor.Get_Leg() == Leg; });
            const auto Reach = ck_procedural_gait_utils::Get_Reach(Params.Get_Chain());
            const auto& Placement = Params.Get_Placement();
            const auto& LegState = Gait._Solver.GetLegState(Index);
            if (Anchor == nullptr || LegState.Get_Swing().Get_Active() || Gait._Solver.HasPendingStep(Index)
                || NOT LegState.Get_Plant().Get_Trusted() || Foot.Get_Position().ContainsNaN()
                || NOT Foot.Get_Normal().IsNormalized() || NOT FMath::IsFinite(Reach) || Reach <= 0.0f
                || NOT FMath::IsFinite(Params.Get_FootContactRadius()) || Params.Get_FootContactRadius() <= 0.0f
                || NOT Anchor->Get_FootWorld().Equals(Foot.Get_Position(), 1.0e-3) || Anchor->Get_Reach() != Reach
                || NOT Anchor->Get_HipLocal().Equals(Placement.Get_HipLocal(), 1.0e-3)
                || NOT Gait._Basis.RotateVector(LegState.Get_Plant().Get_Position()).Equals(Foot.Get_Position(), 1.0e-3))
            { return {}; }
            const auto Hip = Body.TransformPosition(Placement.Get_HipLocal());
            auto Geometry = FProceduralContactStagingLeg{};
            Geometry._RosterIndex = Index;
            Geometry._Anchor = FProceduralSurfaceReachPaceAnchor{Anchor->Get_FootWorld(), Anchor->Get_HipLocal(), Reach};
            Geometry._OutboardWorld = ck_procedural_gait::Get_Outboard(Hip,
                Body.TransformPosition(Placement.Get_RestFootLocal()), Body.GetRotation().GetAxisZ());
            Geometry._ForceReach = Tunables.Get_Step().Get_ForceStepReachFraction() * Reach;
            Geometry._SearchRadius = Tunables.Get_Foothold().Get_SearchRadius() > 0.0f
                ? Tunables.Get_Foothold().Get_SearchRadius() : ck_procedural_gait::DerivedSearchRadiusShareOfReach * Reach;
            Geometry._OwnSideTolerance = ck_procedural_gait::InboardToleranceShareOfReach * Reach;
            const auto* Boundary = InBoundaries.FindByPredicate([&](const auto& InBoundary)
                { return InBoundary._Leg == Leg && InBoundary._Plant == Foot.Get_Position(); });
            if (Boundary != nullptr && ck::IsValid(Boundary->_Attribution)
                && NOT Boundary->_TopPoint.ContainsNaN() && Boundary->_TopNormal.IsNormalized())
            { Geometry._NativeTop = FProceduralContactStagingPlane{Boundary->_TopPoint, Boundary->_TopNormal}; }
            Result.Add(Geometry);
        }
        if (EnabledMask != Planning.Get_EnabledMask() || SupportingMask != Planning.Get_SupportingMask()
            || PlantCount != Stance.Get_Anchors().Num() || PlantCount == 0)
        { return {}; }
        return Result;
    }

    auto
        Get_IsProceduralContactStagingBodyAdmitted(
            FCk_Handle_ProceduralGait InGait,
            const FTransform& InBody,
            const TOptional<FTransform>& InPoseOffset,
            int32& InOutRays,
            int32 InRayLimit)
        -> bool
    {
        if (ck::Is_NOT_Valid(InGait) || NOT InGait.Has<FFragment_SurfaceMotion_Support>()
            || NOT InGait.Has<FFragment_SurfaceMotion>() || InBody.ContainsNaN()
            || NOT InBody.GetRotation().IsNormalized() || InOutRays < 0 || InOutRays > InRayLimit
            || (InPoseOffset.IsSet() && (InPoseOffset->ContainsNaN() || NOT InPoseOffset->GetRotation().IsNormalized())))
        { return false; }
        const auto Legs = Get_ProceduralContactStagingLegs(InGait, {});
        if (Legs.IsEmpty())
        { return false; }
        const auto& Gait = InGait.Get<FFragment_ProceduralGait>();
        const auto& Body = InGait.Get<FFragment_Transform>().Get_Transform();
        const auto& Support = InGait.Get<FFragment_SurfaceMotion_Support>();
        const auto& Staging = Support.Get_ContactStaging();
        const auto HasPose = InGait.Has<FFragment_ProceduralBodyPose>();
        if (Staging._Owner != InGait.ConvertToHandle() || Staging._Episode == 0
            || (Staging._Phase != EProceduralContactStagingPhase::Retreat
                && Staging._Phase != EProceduralContactStagingPhase::Replant
                && Staging._Phase != EProceduralContactStagingPhase::AdvanceExit)
            || Staging._IntentFrame != GFrameCounter || Staging._ConsumerSolve != Gait._SolveSequence
            || NOT Staging._IntentBody.Equals(Body, 1.0e-3) || Staging._Direction.ContainsNaN()
            || NOT Staging._Direction.IsNormalized() || Staging._Up.ContainsNaN() || NOT Staging._Up.IsNormalized()
            || Staging._Required.IsEmpty() || Staging._Required.Num() > 2 || Staging._PendingBody.IsSet()
            || Staging._ReservedRays < 0 || Staging._ReservedRays > 2
            || Staging._RetreatBody.ContainsNaN() || NOT Staging._RetreatBody.GetRotation().IsNormalized()
            || Staging._ExitBody.ContainsNaN() || NOT Staging._ExitBody.GetRotation().IsNormalized()
            || NOT Body.GetScale3D().Equals(FVector::OneVector, 0.0)
            || NOT Staging._ExitBody.GetScale3D().Equals(Body.GetScale3D(), 0.0)
            || NOT Staging._RetreatBody.GetScale3D().Equals(Body.GetScale3D(), 0.0)
            || NOT Staging._ExitBody.GetRotation().Equals(Body.GetRotation(), 0.0)
            || NOT Staging._RetreatBody.GetRotation().Equals(Body.GetRotation(), 0.0)
            || NOT Staging._Direction.Equals(InGait.Get<FFragment_SurfaceMotion>().Get_Direction(), 1.0e-3)
            || Staging._Speed != InGait.Get<FFragment_SurfaceMotion>().Get_Speed()
            || NOT FMath::IsFinite(Staging._Speed) || Staging._Speed <= 0.0f
            || HasPose != InPoseOffset.IsSet()
            || NOT Body.GetRotation().Equals(InBody.GetRotation(), 1.0e-3)
            || NOT Body.GetScale3D().Equals(InBody.GetScale3D(), 1.0e-3)
            || NOT FVector::VectorPlaneProject(InBody.GetLocation() - Body.GetLocation(), Staging._Direction).IsNearlyZero(1.0e-3)
            || (Staging._Phase == EProceduralContactStagingPhase::Replant && NOT InBody.Equals(Body, 1.0e-3)))
        { return false; }
        const auto SignedDelta = FVector::DotProduct(InBody.GetLocation() - Body.GetLocation(), Staging._Direction);
        const auto RetreatRemaining = FVector::DotProduct(Staging._RetreatBody.GetLocation() - Body.GetLocation(), Staging._Direction);
        const auto ExitRemaining = FVector::DotProduct(Staging._ExitBody.GetLocation() - Body.GetLocation(), Staging._Direction);
        if (NOT FMath::IsFinite(SignedDelta) || NOT FMath::IsFinite(RetreatRemaining) || NOT FMath::IsFinite(ExitRemaining)
            || (Staging._Phase == EProceduralContactStagingPhase::Retreat && (SignedDelta > 0.0 || SignedDelta < RetreatRemaining))
            || (Staging._Phase == EProceduralContactStagingPhase::AdvanceExit && (SignedDelta < 0.0 || SignedDelta > ExitRemaining)))
        { return false; }
        for (const auto& Required : Staging._Required)
        {
            if (Required._Index < 0 || Required._Index >= Gait._Legs.Num() || Gait._Legs[Required._Index] != Required._Leg
                || ck::Is_NOT_Valid(Required._Leg) || NOT Required._Leg.Has<FFragment_ProceduralLeg_Params>()
                || NOT Required._Leg.Has<FFragment_LifetimeOwner>() || NOT procedural_leg::Get_IsSupporting(Required._Leg)
                || UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(Required._Leg) != InGait.ConvertToHandle())
            { return false; }
        }
        if (HasPose)
        {
            const auto Pose = UCk_Utils_ProceduralBodyPose_UE::Cast(InGait);
            if (UCk_Utils_ProceduralBodyPose_UE::Get_Status(Pose) != ECk_ProceduralAnimation_Status::Ready
                || NOT UCk_Utils_ProceduralBodyPose_UE::Get_Offset(Pose).Equals(InPoseOffset.GetValue(), 1.0e-6))
            { return false; }
        }
        const auto Landings = Get_ProceduralGaitLandingConstraints(InGait, Body, EProceduralGaitLandingConsumer::SurfaceMotion);
        auto ActiveLandings = 0;
        for (auto Index = 0; Index < Gait._Legs.Num(); ++Index)
        {
            if (NOT ck_procedural_gait::Get_IsLegEnabled(Gait._Legs[Index]) || NOT Gait._Solver.IsLegParticipating(Index)
                || NOT Gait._Solver.GetLegState(Index).Get_Swing().Get_Active())
            { continue; }
            ++ActiveLandings;
            if (NOT ck_procedural_gait::Get_IsLandingLegEligible(InGait, Gait._Legs[Index], Gait._Solver, Index))
            { return false; }
        }
        if (ActiveLandings != Landings.Num() || 3 * Legs.Num() > InRayLimit - InOutRays - Staging._ReservedRays)
        { return false; }
        for (const auto& Landing : Landings)
        {
            auto FutureFits = Get_IsProceduralContactStagingFuturePlantReachValid(Staging._ExitBody, InPoseOffset, Landing.Get_Constraint());
            if (NOT FutureFits && Staging._TwoPlacement && InGait.Has<FFragment_ProceduralContactStagingFence>())
            {
                const auto& Fence = InGait.Get<FFragment_ProceduralContactStagingFence>();
                const auto RequiredIndex = Staging._Required.IndexOfByPredicate([&](const auto& InRequired)
                    { return InRequired._Leg == Landing.Get_Leg(); });
                if (Fence._Spent && Fence._Owner == InGait.ConvertToHandle() && Fence._Episode == Staging._Episode
                    && RequiredIndex != INDEX_NONE && Fence._RequiredCount == Staging._Required.Num()
                    && Fence._Legs[RequiredIndex]._StartedCount > 0 && Fence._Legs[RequiredIndex]._StartedCount <= 2)
                {
                    const auto& Milestones = Fence._Legs[RequiredIndex];
                    const auto& Placement = Milestones._Placements[Milestones._StartedCount - 1];
                    const auto& Required = Staging._Required[RequiredIndex];
                    FutureFits = Placement._Started && NOT Placement._Completed && NOT Placement._Final
                        && Placement._Target == Landing.Get_Constraint().Get_PointWorld()
                        && Get_IsProceduralContactStagingIntermediateTargetValid(Placement._Target, Milestones._ActualPlant,
                            Required._Query, Staging._Direction, Required._Geometry._SearchRadius);
                }
            }
            if (NOT Get_IsProceduralSurfaceLandingReachValid(InBody, InPoseOffset, Landing.Get_Constraint())
                || NOT FutureFits)
            { return false; }
        }
        auto* World = UCk_Utils_EntityLifetime_UE::Get_WorldForEntity(InGait);
        if (ck::Is_NOT_Valid(World))
        { return false; }
        const auto& Tunables = InGait.Get<FFragment_ProceduralGait_Tunables>();
        const auto& Probe = Tunables.Get_Probe();
        const auto& Foothold = Tunables.Get_Foothold();
        const auto PresentationBody = InPoseOffset.IsSet() ? InPoseOffset.GetValue() * InBody : InBody;
        const auto ExitPresentationBody = InPoseOffset.IsSet() ? InPoseOffset.GetValue() * Staging._ExitBody : Staging._ExitBody;
        if (PresentationBody.ContainsNaN() || ExitPresentationBody.ContainsNaN())
        { return false; }
        const auto Reservations = ck_procedural_gait::Get_FootReservations(Gait._Legs, Gait._Solver, Gait._Basis);
        for (const auto& Geometry : Legs)
        {
            const auto& Leg = Gait._Legs[Geometry._RosterIndex];
            const auto& Foot = Leg.Get<FFragment_ProceduralLeg>().Get_Foot();
            const auto& Anchor = Geometry._Anchor;
            const auto Hip = InBody.TransformPosition(Anchor.Get_HipLocal());
            const auto PosedHip = PresentationBody.TransformPosition(Anchor.Get_HipLocal());
            const auto PhysicalReach = Anchor.Get_Reach() + 1.0e-3;
            const auto Required = Staging._Required.IndexOfByPredicate([&](const auto& InRequired)
                { return InRequired._Index == Geometry._RosterIndex; });
            const auto MustFitExit = Required == INDEX_NONE
                || (Staging._LandedMask & static_cast<uint8>(uint8{1} << Required)) != 0;
            if (Hip.ContainsNaN() || PosedHip.ContainsNaN()
                || FVector::Dist(Hip, Anchor.Get_FootWorld()) > PhysicalReach
                || FVector::Dist(PosedHip, Anchor.Get_FootWorld()) > PhysicalReach
                || (MustFitExit && (FVector::Dist(Staging._ExitBody.TransformPosition(Anchor.Get_HipLocal()), Anchor.Get_FootWorld()) > PhysicalReach
                    || FVector::Dist(ExitPresentationBody.TransformPosition(Anchor.Get_HipLocal()), Anchor.Get_FootWorld()) > PhysicalReach))
                || NOT Get_IsProceduralFootContactAvailable(Anchor.Get_FootWorld(),
                    Leg.Get<FFragment_ProceduralLeg_Params>().Get_FootContactRadius(), Geometry._RosterIndex, Reservations))
            { return false; }
            const auto Hit = UCk_Utils_JoltQuery_UE::Get_RayCast(World,
                Anchor.Get_FootWorld() + Foot.Get_Normal() * ck_procedural_gait::FaceHoldProbeHalfSpan,
                Anchor.Get_FootWorld() - Foot.Get_Normal() * ck_procedural_gait::FaceHoldProbeHalfSpan, Probe.Get_QueryFilter());
            ++InOutRays;
            if (NOT ck_procedural_gait::Get_TrustedHit(Hit, -Foot.Get_Normal())
                || FVector::Dist(Hit.Get_Position(), Anchor.Get_FootWorld()) > Foothold.Get_OcclusionTolerance()
                || FVector::DotProduct(Hit.Get_Normal(), Foot.Get_Normal()) <= 0.0f)
            { return false; }
            auto Query = ck_procedural_gait::FFootholdQuery{.World = World, .Hip = Hip, .Probe = &Probe, .Foothold = &Foothold};
            if (ck_procedural_gait::Get_Occluder(Query, Anchor.Get_FootWorld(), InOutRays).IsSet())
            { return false; }
            Query.Hip = PosedHip;
            if (ck_procedural_gait::Get_Occluder(Query, Anchor.Get_FootWorld(), InOutRays).IsSet())
            { return false; }
        }
        return true;
    }

    // Actual return settlement is qualified by the same touchdown resolver and current reservations as an ordinary gait
    // touchdown. The caller's accepted pose is the only endpoint eligible for trust.
    // --------------------------------------------------------------------------------------------------------------------

}

auto
    FCk_ProceduralGaitParticipation_Control::
    AcceptRecoverySettlement(
        const FCk_Handle_ProceduralGait& InGait,
        const FCk_ProceduralGaitParticipation_RecoveryCandidate& InCandidate,
        const FCk_ProceduralArticulation_Pose& InActualSettled)
    -> ECk_ProceduralGaitParticipation_Result
{
    auto Snapshot = FCk_ProceduralGaitParticipation_Snapshot{};
    const auto SnapshotResult = GetSnapshot(InGait, Snapshot);
    if (SnapshotResult != ECk_ProceduralGaitParticipation_Result::Accepted)
    { return SnapshotResult; }
    auto Gait = InGait;
    auto& Fragment = Gait.Get<ck::FFragment_ProceduralGait>();
    auto* Lease = Fragment._ParticipationLeases.FindByPredicate([&InCandidate](const auto& InLease)
        { return InLease._Token == InCandidate.Get_Token(); });
    if (Lease == nullptr || NOT Lease->_Recovering)
    { return ECk_ProceduralGaitParticipation_Result::InvalidToken; }
    if (Lease->_RecoveryGeneration != InCandidate.Get_Generation()
        || Fragment._SolveSequence != InCandidate.Get_SolveSequence()
        || InCandidate.Get_ProposedElapsed() < Lease->_AcceptedElapsed
        || InCandidate.Get_Targets().Num() != Lease->_LimbIds.Num()
        || Lease->_LandingWorld.Num() != Lease->_LimbIds.Num()
        || Lease->_LandingNormalWorld.Num() != Lease->_LimbIds.Num())
    { return ECk_ProceduralGaitParticipation_Result::Stale; }

    auto Actual = TArray<FTransform>{};
    Actual.Reserve(Lease->_LimbIds.Num());
    for (auto Entry = 0; Entry < Lease->_LimbIds.Num(); ++Entry)
    {
        const auto Id = Lease->_LimbIds[Entry];
        const auto* Limb = InActualSettled.Get_Limbs().FindByPredicate([Id](const auto& InLimb)
            { return InLimb.Get_Id() == Id; });
        if (Limb == nullptr || InCandidate.Get_Targets()[Entry].Get_Id() != Id
            || Limb->Get_AcceptedEndpoint().ContainsNaN()
            || NOT Limb->Get_AcceptedEndpoint().GetRotation().IsNormalized())
        { return ECk_ProceduralGaitParticipation_Result::NotReady; }
        Actual.Add(Limb->Get_AcceptedEndpoint());
    }

    auto Trusted = TArray<bool>{};
    Trusted.Init(false, Lease->_LimbIds.Num());
    if (InCandidate.Get_ProposedElapsed() - Lease->_RecoveryOrigin >= Lease->_ReturnPolicy.Get_Duration())
    {
        auto* World = UCk_Utils_EntityLifetime_UE::Get_WorldForEntity(Gait);
        if (ck::Is_NOT_Valid(World) || NOT Gait.Has<ck::FFragment_ProceduralGait_Tunables>())
        { return ECk_ProceduralGaitParticipation_Result::NotReady; }
        const auto& Tunables = Gait.Get<ck::FFragment_ProceduralGait_Tunables>();
        auto Reservations = ck_procedural_gait::Get_FootReservations(Fragment._Legs, Fragment._Solver, Fragment._Basis);
        for (auto Entry = 0; Entry < Lease->_LegIndices.Num(); ++Entry)
        {
            const auto Index = Lease->_LegIndices[Entry];
            const auto& Leg = Fragment._Legs[Index];
            if (NOT ck::procedural_leg::Get_IsStructurallyEnabled(Leg))
            { continue; }
            const auto& Params = Leg.Get<ck::FFragment_ProceduralLeg_Params>();
            const auto Hip = Snapshot.Get_RootFrame().TransformPosition(Params.Get_Placement().Get_HipLocal());
            const auto Reach = ck_procedural_gait_utils::Get_Reach(Params.Get_Chain());
            const auto Touchdown = ck_procedural_gait::Get_Touchdown(World, Actual[Entry].GetLocation(),
                Lease->_LandingWorld[Entry], Lease->_LandingNormalWorld[Entry], Snapshot.Get_SupportUp(),
                Tunables.Get_Probe(), Hip, TOptional<FVector>{}, Reach,
                Params.Get_FootContactRadius(), Index, Reservations);
            const auto ContactAccepted = Touchdown.Get_Trusted()
                && Touchdown.Get_Position().Equals(Actual[Entry].GetLocation(),
                    Lease->_ReturnPolicy.Get_ContactToleranceCm());
            Trusted[Entry] = ContactAccepted;
            if (ContactAccepted && Params.Get_FootContactRadius() > 0.0f)
            { Reservations.Emplace(Touchdown.Get_Position(), Params.Get_FootContactRadius(), Index); }
        }
    }
    Lease->_AcceptedWorld = MoveTemp(Actual);
    Lease->_ContactTrusted = MoveTemp(Trusted);
    Lease->_AcceptedElapsed = InCandidate.Get_ProposedElapsed();
    Lease->_ReadyForHandoff = Lease->_ContactTrusted.Num() == Lease->_LegIndices.Num()
        && Lease->_ContactTrusted.Contains(false) == false;
    if (InCandidate.Get_ProposedElapsed() - Lease->_RecoveryOrigin >= Lease->_ReturnPolicy.Get_Duration()
        && NOT Lease->_ReadyForHandoff)
    { return ECk_ProceduralGaitParticipation_Result::NotReady; }
    return ECk_ProceduralGaitParticipation_Result::Accepted;
}

namespace ck
{
    auto
        FProcessor_ProceduralGait_Setup::
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_ProceduralGait_Tunables& InTunables,
            FFragment_ProceduralGait& InGaitComp,
            FFragment_ProceduralGait_Debug& InDebugComp,
            const FFragment_Transform& InTransform)
        -> void
    {
        auto* World = UCk_Utils_EntityLifetime_UE::Get_WorldForEntity(InHandle);
        if (ck::Is_NOT_Valid(World))
        { return; }

        const auto& Body = InTransform.Get_Transform();
        const auto BodyFinite = NOT Body.ContainsNaN();
        CK_ENSURE_IF_NOT(BodyFinite,
            TEXT("Procedural gait [{}] body transform contains NaN; feature is failed."), InHandle)
        {
            InHandle.Add<FFragment_ProceduralGait_Failure>(ECk_ProceduralGait_Failure::NanBody);
            InHandle.Remove<MarkedDirtyBy>();
            return;
        }

        const auto Basis = Body.GetRotation();
        const auto InverseBasis = Basis.Inverse();
        const auto Up = Basis.GetAxisZ();

        auto RayCount = int32{0};
        // Setup runs once, so every leg searches: a pick made here is held from the first update on.
        auto SearchBudget = ck_procedural_gait::FFootholdSearchBudget{
            .RemainingLegs = TNumericLimits<int32>::Max(),
            .Solve = InGaitComp._SolveSequence};
        const auto MaxHipLateral = ck_procedural_gait::Get_MaxHipLateral(InGaitComp._Legs,
            [](const FCk_Handle_ProceduralLeg& InLeg) { return procedural_leg::Get_IsLive(InLeg); });
        auto InitialFeet = TArray<FVector, TInlineAllocator<8>>{};
        auto InitialTrust = TArray<bool, TInlineAllocator<8>>{};
        auto InitialReservations = TArray<FProceduralFootReservation, TInlineAllocator<8>>{};
        InitialFeet.Reserve(InGaitComp._Legs.Num());
        InitialTrust.Reserve(InGaitComp._Legs.Num());
        for (auto Index = 0; Index < InGaitComp._Legs.Num(); ++Index)
        {
            auto& Leg = InGaitComp._Legs[Index];
            if (NOT procedural_leg::Get_IsLive(Leg))
            {
                constexpr auto Untrusted = false;
                InitialFeet.Add(InverseBasis.RotateVector(Body.GetLocation()));
                InitialTrust.Add(Untrusted);
                continue;
            }

            const auto& Params = Leg.Get<FFragment_ProceduralLeg_Params>();
            const auto& Placement = Params.Get_Placement();
            const auto Neutral = Body.TransformPosition(Placement.Get_RestFootLocal());
            const auto Hip = Body.TransformPosition(Placement.Get_HipLocal());
            constexpr auto NotYetPlanted = false;
            const auto Query = ck_procedural_gait::FFootholdQuery{
                .World = World,
                .Basis = Basis,
                .BodyLocation = Body.GetLocation(),
                .Hip = Hip,
                .Outboard = ck_procedural_gait::Get_Outboard(Hip, Neutral, Up),
                .MaxHipLateral = MaxHipLateral,
                .Ideal = Neutral,
                .Plant = Neutral,
                .Planted = NotYetPlanted,
                .Reach = ck_procedural_gait_utils::Get_Reach(Params.Get_Chain()),
                .RestDrop = ck_procedural_gait::Get_RestDrop(Placement),
                .Probe = &InTunables.Get_Probe(),
                .Step = &InTunables.Get_Step(),
                .Foothold = &InTunables.Get_Foothold(),
                .FootContactRadius = Params.Get_FootContactRadius(),
                .LegIndex = Index,
                .Reservations = InitialReservations};
            const auto Foothold = ck_procedural_gait::Get_Foothold(Query, InGaitComp._Footholds[Index], SearchBudget, RayCount,
                &InDebugComp._ScratchLegs[Index]);
            const auto Trusted = Foothold.Source != EProceduralFootholdSource::None;
            const auto Position = Trusted ? Foothold.Position : Neutral;
            const auto Normal = Trusted ? Foothold.Normal : Up;

            auto& LegComp = Leg.Get<FFragment_ProceduralLeg>();
            // Update freezes a leg disabled before Add from this published foot, so it must already hold the probed pose.
            LegComp._Foot.Set_Position(Position)
                .Set_Normal(Normal)
                .Set_Rotation(Basis)
                .Set_SwingAlpha(0.0f)
                .Set_Phase(ECk_ProceduralLeg_FootPhase::Planted)
                .Set_Contact(Trusted ? ECk_ProceduralLeg_FootContact::Trusted : ECk_ProceduralLeg_FootContact::Guessed)
                .Set_Foothold(ck_procedural_gait::Get_LegFoothold(Foothold.Source));
            LegComp._IdealVerdict = ck_procedural_gait::Get_LegFootholdVerdict(Foothold.IdealVerdict);

            InitialFeet.Add(InverseBasis.RotateVector(Position));
            InitialTrust.Add(Trusted);
            if (Trusted && Params.Get_FootContactRadius() > 0.0f && ck_procedural_gait::Get_IsLegEnabled(Leg))
            { InitialReservations.Emplace(Position, Params.Get_FootContactRadius(), Index); }
        }
        InDebugComp._RaysLastSolve = RayCount;

        const auto SolverReset = InGaitComp._Solver.Reset(InitialFeet, InitialTrust);
        CK_ENSURE_IF_NOT(SolverReset,
            TEXT("Procedural gait [{}] solver rejected the initial foot positions; feature is failed."), InHandle)
        {
            InHandle.Add<FFragment_ProceduralGait_Failure>(ECk_ProceduralGait_Failure::SolverReset);
            InHandle.Remove<MarkedDirtyBy>();
            return;
        }

        InGaitComp._Basis = Basis;
        InGaitComp._ReachStance = FProceduralGaitReachStance{};
        InGaitComp._LandingPlanning = FProceduralGaitLandingPlanning{};
        InGaitComp._HeightPlanning = FProceduralGaitHeightPlanning{};
        InHandle.Remove<MarkedDirtyBy>();
    }

    // --------------------------------------------------------------------------------------------------------------------

    auto
        FProcessor_ProceduralGait_HandleRequests::
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            FFragment_ProceduralGait_Tunables& InTunables,
            FFragment_ProceduralGait& InGaitComp,
            FFragment_ProceduralGait_Debug& InDebugComp,
            FFragment_ProceduralGait_Requests& InRequestsComp)
        -> void
    {
        auto Requests = MoveTemp(InRequestsComp._Requests);
        InRequestsComp._Requests.Reset();

        algo::ForEachRequest(Requests, ck::Visitor(
        [&](const auto& InRequest) -> void
        {
            auto Result = ECk_Request_OperationResult::Failed_Cancelled;
            const auto Guard = MakeCompletionGuard(InRequest, InHandle, Result);

            if (InHandle.Has<FTag_DestroyEntity_Initiate>())
            { return; }

            DoHandleRequest(InHandle, InTunables, InGaitComp, InDebugComp, InRequest);
            Result = ECk_Request_OperationResult::Succeeded;
        }), policy::DontResetContainer{});

        if (InRequestsComp._Requests.IsEmpty())
        { InHandle.Remove<MarkedDirtyBy>(); }
    }

    auto
        FProcessor_ProceduralGait_HandleRequests::
        DoHandleRequest(
            HandleType InHandle,
            FFragment_ProceduralGait_Tunables& InTunables,
            FFragment_ProceduralGait& InGaitComp,
            FFragment_ProceduralGait_Debug& InDebugComp,
            const FCk_Request_ProceduralGait_ApplyPreset& InRequest)
        -> void
    {
        InTunables = FFragment_ProceduralGait_Tunables{InRequest.Get_Timing(), InRequest.Get_Step(), InRequest.Get_Probe(),
            InRequest.Get_Foothold()};
        InGaitComp._HeightPlanning = FProceduralGaitHeightPlanning{};
        InGaitComp._LandingPlanning = FProceduralGaitLandingPlanning{};

        const auto Built = UCk_Utils_ProceduralGait_UE::DoBuild_SolverSettings(InTunables, InGaitComp._Legs, InGaitComp._EnabledMask);
        InGaitComp._Solver.Set_Settings(Built.Get_Settings());
        InDebugComp._ReachCadenceFloor = Built.Get_ReachCadenceFloor();
        InDebugComp._ReachSkippedLegs = Built.Get_ReachSkippedLegs();
    }

    // --------------------------------------------------------------------------------------------------------------------

    auto
        FProcessor_ProceduralGait_Update::
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_ProceduralGait_Tunables& InTunables,
            FFragment_ProceduralGait& InGaitComp,
            FFragment_ProceduralGait_Debug& InDebugComp,
            FFragment_ProceduralGait_FeetPlane& InFeetPlaneComp,
            const FFragment_Transform& InTransform)
        -> void
    {
        InGaitComp._LandingPlanning = FProceduralGaitLandingPlanning{};
        const auto StagingAtEntry = ck::IsValid(InHandle) && InHandle.Has<FFragment_SurfaceMotion_Support>()
            ? TOptional<FProceduralContactStagingState>{InHandle.Get<FFragment_SurfaceMotion_Support>()._ContactStaging}
            : TOptional<FProceduralContactStagingState>{};
        const auto GetCurrentStaging = [&]() -> FProceduralContactStagingState*
        {
            if (NOT StagingAtEntry.IsSet() || ck::Is_NOT_Valid(InHandle) || InHandle.Has<FTag_DestroyEntity_Initiate>()
                || NOT InHandle.Has<FFragment_SurfaceMotion_Support>() || NOT InHandle.Has<FFragment_SurfaceMotion>()
                || NOT InHandle.Has<FFragment_ProceduralGait>() || NOT InHandle.Has<FFragment_Transform>()
                || UCk_Utils_ProceduralGait_UE::Get_Status(InHandle) != ECk_ProceduralAnimation_Status::Ready
                || UCk_Utils_SurfaceMotion_UE::Get_Status(UCk_Utils_SurfaceMotion_UE::Cast(InHandle))
                    != ECk_ProceduralAnimation_Status::Ready)
            { return nullptr; }
            auto& Current = InHandle.Get<FFragment_SurfaceMotion_Support>()._ContactStaging;
            const auto& Entry = StagingAtEntry.GetValue();
            const auto ActualSolve = InHandle.Get<FFragment_ProceduralGait>()._SolveSequence;
            if (Current._Owner != InHandle.ConvertToHandle() || Current._Owner != Entry._Owner
                || Current._Episode == 0 || Current._Episode != Entry._Episode || Current._Phase != Entry._Phase
                || Current._IntentFrame != Entry._IntentFrame || Current._IntentFrame != GFrameCounter
                || Current._ConsumerSolve != Entry._ConsumerSolve
                || (ActualSolve != Current._ConsumerSolve
                    && (Current._ConsumerSolve == MAX_uint64 || ActualSolve != Current._ConsumerSolve + 1))
                || Current._Required.IsEmpty() || Current._Required.Num() > 2 || Current._Required.Num() != Entry._Required.Num()
                || NOT Current._IntentBody.Equals(Entry._IntentBody, 0.0)
                || NOT InHandle.Get<FFragment_Transform>().Get_Transform().Equals(Current._IntentBody, 1.0e-3)
                || NOT Current._Direction.Equals(InHandle.Get<FFragment_SurfaceMotion>().Get_Direction(), 1.0e-3)
                || Current._Speed != InHandle.Get<FFragment_SurfaceMotion>().Get_Speed())
            { return nullptr; }
            for (auto Index = 0; Index < Current._Required.Num(); ++Index)
            {
                if (Current._Required[Index]._Leg != Entry._Required[Index]._Leg
                    || Current._Required[Index]._Index != Entry._Required[Index]._Index)
                { return nullptr; }
            }
            return &Current;
        };
        ON_SCOPE_EXIT
        {
            if (auto* Current = GetCurrentStaging())
            {
                Current->_PoseRayTickets.Reset();
                Current->_ReservedRays = 0;
            }
        };
        const auto Dt = InDeltaT.Get_Seconds();
        if (NOT FMath::IsFinite(Dt) || Dt < 0.0)
        { return; }

        auto* World = UCk_Utils_EntityLifetime_UE::Get_WorldForEntity(InHandle);
        if (ck::Is_NOT_Valid(World))
        { return; }

        const auto& Body = InTransform.Get_Transform();
        const auto BodyFinite = NOT Body.ContainsNaN();
        CK_ENSURE_IF_NOT(BodyFinite,
            TEXT("Procedural gait [{}] body transform contains NaN; feature is failed."), InHandle)
        {
            InHandle.Add<FFragment_ProceduralGait_Failure>(ECk_ProceduralGait_Failure::NanBody);
            return;
        }

        const auto HasPose = InHandle.Has<FFragment_ProceduralBodyPose>()
            && UCk_Utils_ProceduralBodyPose_UE::Get_Status(UCk_Utils_ProceduralBodyPose_UE::Cast(InHandle))
                == ECk_ProceduralAnimation_Status::Ready;
        const auto PresentationBody = HasPose
            ? UCk_Utils_ProceduralBodyPose_UE::Get_Offset(UCk_Utils_ProceduralBodyPose_UE::CastChecked(InHandle)) * Body
            : FTransform::Identity;

        const auto& Timing = InTunables.Get_Timing();
        const auto& Step = InTunables.Get_Step();
        const auto& Probe = InTunables.Get_Probe();
        const auto Basis = Body.GetRotation();
        const auto InverseBasis = Basis.Inverse();
        const auto Up = Basis.GetAxisZ();
        const auto Velocity = InGaitComp._VelocityTracker.Update(Body.GetLocation(), InDeltaT);
        const auto PlanarVelocity = InverseBasis.RotateVector(FVector::VectorPlaneProject(Velocity, Up));
        const auto YawRate = InGaitComp._YawRateTracker.Update(InGaitComp._Basis, Basis, InDeltaT);
        // Only placement lead may use requested motion during pacing; measured velocity still owns motion and swing inputs.
        auto Lead = FVector::VectorPlaneProject(Velocity, Up) * Timing.Get_StepDuration().Get_Seconds();
        auto HasFreshRequestedLead = false;

        InGaitComp._Solver.TransformState(InverseBasis * InGaitComp._Basis);

        // A continuing plant already has a published world anchor. Capture its transformed solver state before
        // any per-leg work so publication can avoid another basis round trip only when that plant is unchanged.
        struct FContinuingWorldPlant
        {
            FCk_Handle_ProceduralLeg Leg;
            FVector WorldPosition;
            FVector WorldNormal;
            FVector SupportPosition;
            FVector SupportNormal;
            bool Trusted = false;
        };
        auto ContinuingWorldPlants = TArray<TOptional<FContinuingWorldPlant>, TInlineAllocator<8>>{};
        ContinuingWorldPlants.SetNum(InGaitComp._Legs.Num());
        const auto ContinuingPlantSolve = InGaitComp._SolveSequence;
        const auto HasCurrentWorldPlant = [&](const FCk_Handle_ProceduralLeg& InLeg, int32 InIndex) -> bool
        {
            if (InIndex < 0 || InIndex >= 64 || InIndex >= InGaitComp._Solver.NumLegs()
                || ck::Is_NOT_Valid(InLeg) || NOT InLeg.Has<FFragment_ProceduralLeg>()
                || NOT InLeg.Has<FFragment_ProceduralLeg_Params>()
                || NOT InLeg.Has<FFragment_LifetimeOwner>()
                || UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(InLeg) != InHandle.ConvertToHandle()
                || NOT procedural_leg::Get_IsSupporting(InLeg)
                || NOT InGaitComp._Solver.IsLegEnabled(InIndex)
                || NOT InGaitComp._Solver.IsLegParticipating(InIndex)
                || InGaitComp._Solver.HasPendingStep(InIndex)
                || (InGaitComp._UnplantedMask & ck_procedural_gait::Get_LegBit(InIndex)) != 0
                || InGaitComp._ParticipationLeases.ContainsByPredicate([InIndex](const auto& InLease)
                    { return InLease._LegIndices.Contains(InIndex); }))
            { return false; }

            const auto& Stance = InGaitComp._ReachStance;
            const auto& Foot = InLeg.Get<FFragment_ProceduralLeg>().Get_Foot();
            const auto& State = InGaitComp._Solver.GetLegState(InIndex);
            return Stance.Get_HasSample() && Stance.Get_SolveSequence() == ContinuingPlantSolve
                && InGaitComp._SolveSequence == ContinuingPlantSolve
                && Foot.Get_Phase() == ECk_ProceduralLeg_FootPhase::Planted
                && Foot.Get_Contact() == ECk_ProceduralLeg_FootContact::Trusted
                && NOT Foot.Get_Position().ContainsNaN() && Foot.Get_Normal().IsNormalized()
                && NOT State.Get_Swing().Get_Active() && State.Get_Plant().Get_Trusted()
                && State.Get_AutomaticRecovery() == EProceduralGaitAutomaticRecovery::None
                && NOT State.Get_Plant().Get_Position().ContainsNaN() && State.Get_Plant().Get_Normal().IsNormalized()
                && Stance.Get_Anchors().ContainsByPredicate([&](const auto& InAnchor)
                    { return InAnchor.Get_Leg() == InLeg && InAnchor.Get_FootWorld() == Foot.Get_Position(); });
        };
        if (Basis.IsNormalized() && (InverseBasis * InGaitComp._Basis).IsNormalized())
        {
            for (auto Index = 0; Index < InGaitComp._Legs.Num(); ++Index)
            {
                const auto& Leg = InGaitComp._Legs[Index];
                if (NOT HasCurrentWorldPlant(Leg, Index))
                { continue; }
                const auto& Foot = Leg.Get<FFragment_ProceduralLeg>().Get_Foot();
                const auto& Plant = InGaitComp._Solver.GetLegState(Index).Get_Plant();
                ContinuingWorldPlants[Index] = FContinuingWorldPlant{Leg, Foot.Get_Position(), Foot.Get_Normal(),
                    Plant.Get_Position(), Plant.Get_Normal(), Plant.Get_Trusted()};
            }
        }

        const FFragment_SurfaceMotion_Support* PaceSupport = nullptr;
        TOptional<ck_procedural_gait::FObservedBlockedFace> BlockedFace;
        TOptional<ck_procedural_gait::FObservedBlockedFace> AdaptiveTop;
        if (Dt > 0.0 && InHandle.Has<FFragment_SurfaceMotion>() && InHandle.Has<FFragment_SurfaceMotion_Support>()
            && UCk_Utils_SurfaceMotion_UE::Get_Status(UCk_Utils_SurfaceMotion_UE::Cast(InHandle)) == ECk_ProceduralAnimation_Status::Ready)
        {
            const auto& Support = InHandle.Get<FFragment_SurfaceMotion_Support>();
            const auto& Motion = InHandle.Get<FFragment_SurfaceMotion>();
            if (Support._EvaluatedFrame == GFrameCounter && Support._EvaluatedBody.Equals(Body, 1.0e-3)
                && Support._State.Get_Grounded() && Support._AttemptedStanceSpeed > 0.0f
                && (Support._ReachPaceState == ECk_SurfaceMotion_ReachPaceState::Pacing
                    || Support._ReachPaceState == ECk_SurfaceMotion_ReachPaceState::Blocked))
            { PaceSupport = &Support; }
            if (PaceSupport && FMath::IsFinite(Motion._Speed) && Motion._Speed > 0.0f && NOT Motion._Direction.ContainsNaN())
            {
                const auto Intent = FVector::VectorPlaneProject(Motion._Direction, Up).GetSafeNormal();
                if (NOT Intent.IsNearlyZero())
                {
                    // ComputeLeadQuery caps travel and turn together, as for measured placement lead.
                    Lead = Intent * Motion._Speed * Timing.Get_StepDuration().Get_Seconds();
                    HasFreshRequestedLead = true;
                }
            }
            if (InHandle.Has<FFragment_SurfaceMotion_Params>()
                && InHandle.Get<FFragment_SurfaceMotion_Params>().Get_Contact().Get_WallPolicy() == ECk_SurfaceMotion_WallPolicy::Climb
                && Support._EvaluatedFrame == GFrameCounter && Support._EvaluatedBody.Equals(Body, 1.0e-3)
                && Motion._Speed > 0.0f && Support._ReachPaceState != ECk_SurfaceMotion_ReachPaceState::PhysicalOverride)
            {
                const auto& StepForward = Support._State.Get_StepForward();
                if (Support._State.Get_Grounded() && Support._State.Get_Obstruction() == ck::EProceduralSurfaceObstruction::Wall
                    && NOT Support._State.Get_ObstructionPoint().ContainsNaN()
                    && NOT Support._State.Get_ObstructionNormal().ContainsNaN()
                    && NOT Support._State.Get_ObstructionNormal().IsNearlyZero()
                    && NOT StepForward.ContainsNaN() && NOT StepForward.IsNearlyZero()
                    && FVector::DotProduct(StepForward, Support._State.Get_ObstructionNormal()) < 0.0f)
                {
                    BlockedFace = ck_procedural_gait::FObservedBlockedFace{
                        Support._State.Get_ObstructionPoint(), Support._State.Get_ObstructionNormal()};
                }
                else if (Support._State.Get_ClimbAcquisition().IsSet())
                {
                    // A convex crest can be visible before a foot reaches its top, without opposing steering or
                    // supporting the body. Treat that current observation only as a budgeted search objective.
                    const auto& Contact = Support._State.Get_ClimbAcquisition().GetValue();
                    if (Contact.Get_Hit() && FMath::IsFinite(Contact.Get_Fraction()) && Contact.Get_Fraction() > 0.0f
                        && NOT Contact.Get_Position().ContainsNaN() && NOT Contact.Get_Normal().ContainsNaN()
                        && NOT Contact.Get_Normal().IsNearlyZero())
                    {
                        BlockedFace = ck_procedural_gait::FObservedBlockedFace{Contact.Get_Position(), Contact.Get_Normal()};
                    }
                }
            }
        }

        const auto& CurrentStance = InGaitComp._ReachStance;
        const auto IsCurrentNativePlant = [&](const FCk_Handle_ProceduralLeg& InLeg, int32 InIndex) -> bool
        {
            if (InIndex < 0 || InIndex >= InGaitComp._Solver.NumLegs()
                || ck::Is_NOT_Valid(InLeg) || NOT InLeg.Has<FFragment_ProceduralLeg>()
                || NOT InLeg.Has<FFragment_ProceduralLeg_Params>()
                || NOT procedural_leg::Get_IsSupporting(InLeg)
                || UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(InLeg) != InHandle.ConvertToHandle()
                || NOT InGaitComp._Solver.IsLegParticipating(InIndex) || InGaitComp._Solver.HasPendingStep(InIndex))
            { return false; }

            const auto ContactRadius = InLeg.Get<FFragment_ProceduralLeg_Params>().Get_FootContactRadius();
            if (NOT FMath::IsFinite(ContactRadius) || ContactRadius <= 0.0f)
            { return false; }

            const auto& Foot = InLeg.Get<FFragment_ProceduralLeg>().Get_Foot();
            const auto& State = InGaitComp._Solver.GetLegState(InIndex);
            if (Foot.Get_Phase() != ECk_ProceduralLeg_FootPhase::Planted
                || Foot.Get_Contact() != ECk_ProceduralLeg_FootContact::Trusted
                || State.Get_Swing().Get_Active() || NOT State.Get_Plant().Get_Trusted()
                || Foot.Get_Position().ContainsNaN() || State.Get_Plant().Get_Position().ContainsNaN()
                || NOT Basis.RotateVector(State.Get_Plant().Get_Position()).Equals(Foot.Get_Position(), 1.0e-3))
            { return false; }

            return CurrentStance.Get_Anchors().ContainsByPredicate([&](const auto& InAnchor)
                { return InAnchor.Get_Leg() == InLeg && InAnchor.Get_FootWorld().Equals(Foot.Get_Position(), 1.0e-3); });
        };
        if (PaceSupport && NOT BlockedFace.IsSet() && HasFreshRequestedLead
            && PaceSupport->_ReachPaceState == ECk_SurfaceMotion_ReachPaceState::Blocked
            && PaceSupport->_AdaptiveTopAcquisition.IsSet()
            && InHandle.Has<FFragment_SurfaceMotion_Params>()
            && InHandle.Get<FFragment_SurfaceMotion_Params>().Get_AdaptiveHeight().Get_Enabled()
            && CurrentStance.Get_HasSample() && CurrentStance.Get_SolveSequence() == InGaitComp._SolveSequence
            && NOT CurrentStance.Get_BodyAtSolve().ContainsNaN() && CurrentStance.Get_BodyAtSolve().Equals(Body, 1.0e-3))
        {
            const auto& TopReceipt = PaceSupport->_AdaptiveTopAcquisition.GetValue();
            const auto& Motion = InHandle.Get<FFragment_SurfaceMotion>();
            if (TopReceipt.Get_Frame() == GFrameCounter && NOT TopReceipt.Get_Body().ContainsNaN() && TopReceipt.Get_Body().Equals(Body, 1.0e-3)
                && NOT TopReceipt.Get_Point().ContainsNaN() && NOT TopReceipt.Get_Normal().ContainsNaN() && TopReceipt.Get_Normal().IsNormalized()
                && NOT TopReceipt.Get_Direction().ContainsNaN() && TopReceipt.Get_Direction().IsNormalized()
                && NOT TopReceipt.Get_Up().ContainsNaN() && TopReceipt.Get_Up().IsNormalized()
                && TopReceipt.Get_Direction().Equals(Motion._Direction, 1.0e-3)
                && TopReceipt.Get_Up().Equals(PaceSupport->_State.Get_SupportNormal(), 1.0e-3)
                && FMath::IsFinite(TopReceipt.Get_Speed()) && TopReceipt.Get_Speed() > 0.0f && TopReceipt.Get_Speed() == Motion._Speed)
            { AdaptiveTop = ck_procedural_gait::FObservedBlockedFace{TopReceipt.Get_Point(), TopReceipt.Get_Normal()}; }
        }

        auto RefreshPlantedHolds = false;
        if (PaceSupport && PaceSupport->_ReachPaceState == ECk_SurfaceMotion_ReachPaceState::Blocked
            && FMath::IsFinite(PaceSupport->_AttemptedStanceSpeed)
            && CurrentStance.Get_HasSample() && CurrentStance.Get_SolveSequence() == InGaitComp._SolveSequence
            && NOT CurrentStance.Get_BodyAtSolve().ContainsNaN()
            && CurrentStance.Get_BodyAtSolve().Equals(Body, 1.0e-3) && NOT PaceSupport->_ReachPaceFeedback.IsEmpty())
        {
            // Pressure may refresh every supporting planted hold, but only while every rejected anchor still matches
            // the accepted stance. This grants search work, never trial feedback or take-off permission to another leg.
            RefreshPlantedHolds = algo::AllOf(PaceSupport->_ReachPaceFeedback, [&](const auto& InFeedback)
            {
                const auto& Leg = InFeedback.Get_Leg();
                const auto Index = InGaitComp._Legs.IndexOfByKey(Leg);
                return IsCurrentNativePlant(Leg, Index) && NOT InFeedback.Get_FootWorld().ContainsNaN()
                    && InFeedback.Get_FootWorld().Equals(Leg.Get<FFragment_ProceduralLeg>().Get_Foot().Get_Position(), 1.0e-3)
                    && NOT InFeedback.Get_TrialHipWorld().ContainsNaN()
                    && (NOT InFeedback.Get_TrialPosedHipWorld().IsSet()
                        || NOT InFeedback.Get_TrialPosedHipWorld().GetValue().ContainsNaN());
            });
        }

        const auto CaptureHeight = InHandle.Has<FFragment_SurfaceMotion_Params>()
            && InHandle.Get<FFragment_SurfaceMotion_Params>().Get_AdaptiveHeight().Get_Enabled();
        auto HeightObservations = TArray<TArray<ck_procedural_gait::FHeightObservation, TInlineAllocator<2>>, TInlineAllocator<8>>{};
        auto HeightRequiresRelief = TArray<bool, TInlineAllocator<8>>{};
        if (CaptureHeight)
        {
            HeightObservations.SetNum(InGaitComp._Legs.Num());
            HeightRequiresRelief.SetNumZeroed(InGaitComp._Legs.Num());
        }
        InGaitComp._HeightPlanning = FProceduralGaitHeightPlanning{};
        const auto LegCount = InGaitComp._Legs.Num();
        auto MatchingLandingReceipts = TArray<TOptional<ck_procedural_gait::FNativeLandingReceipt>>{};
        auto SelectedLandingReceipts = TArray<TOptional<ck_procedural_gait::FNativeLandingReceipt>>{};
        MatchingLandingReceipts.SetNum(LegCount);
        SelectedLandingReceipts.SetNum(LegCount);
        const auto Reservations = ck_procedural_gait::Get_FootReservations(InGaitComp._Legs, InGaitComp._Solver, Basis);

        auto Inputs = TArray<FProceduralGaitLegInput, TInlineAllocator<8>>{};
        Inputs.SetNum(LegCount);
        auto Outputs = TArray<FProceduralGaitLegOutput, TInlineAllocator<8>>{};
        Outputs.SetNum(LegCount);

        auto AllLost = true;
        auto MeanFootRadius = 0.0;
        auto EnabledCount = 0;
        auto EnabledMask = ~uint64{0};
        auto RayCount = int32{0};
        auto SearchBudget = ck_procedural_gait::FFootholdSearchBudget{
            .RemainingLegs = ck_procedural_gait::Get_SearchBudgetLegs(algo::CountIf(InGaitComp._Legs, &ck_procedural_gait::Get_IsLegEnabled)),
            .Solve = InGaitComp._SolveSequence};
        auto LastSearchedLeg = int32{INDEX_NONE};
        auto RevalidatedPlants = uint64{0};
        auto MotionAirborne = TOptional<bool>{};
        if (UCk_Utils_SurfaceMotion_UE::Has(InHandle))
        {
            MotionAirborne = UCk_Utils_SurfaceMotion_UE::Get_Support(UCk_Utils_SurfaceMotion_UE::CastChecked(InHandle))
                == ECk_SurfaceMotion_Support::Airborne;
        }
        auto NativeMotionObservation = false;
        if (MotionAirborne.IsSet() && InHandle.Has<FFragment_SurfaceMotion_Support>()
            && UCk_Utils_SurfaceMotion_UE::Get_Status(UCk_Utils_SurfaceMotion_UE::Cast(InHandle))
                == ECk_ProceduralAnimation_Status::Ready)
        {
            const auto& MotionSupport = InHandle.Get<FFragment_SurfaceMotion_Support>();
            NativeMotionObservation = MotionSupport._EvaluatedFrame == GFrameCounter
                && MotionSupport._EvaluatedBody.Equals(Body, 1.0e-3);
        }
        auto* Staging = InHandle.Has<FFragment_SurfaceMotion_Support>()
            ? &InHandle.Get<FFragment_SurfaceMotion_Support>()._ContactStaging : nullptr;
        auto* StagingFence = InHandle.Has<FFragment_ProceduralContactStagingFence>()
            ? &InHandle.Get<FFragment_ProceduralContactStagingFence>() : nullptr;
        const auto ConsumerInputSolve = InGaitComp._SolveSequence;
        const auto FreshStagingReplant = Staging != nullptr && Dt > 0.0 && NativeMotionObservation
            && GFrameCounter != 0 && GFrameCounter != MAX_uint64 && ConsumerInputSolve != MAX_uint64
            && MotionAirborne.IsSet() && NOT MotionAirborne.GetValue()
            && Staging->_Phase == EProceduralContactStagingPhase::Replant
            && Staging->_Owner == InHandle.ConvertToHandle() && Staging->_Episode != 0
            && Staging->_IntentFrame == GFrameCounter && Staging->_ConsumerSolve == ConsumerInputSolve
            && Staging->_IntentBody.Equals(Body, 1.0e-3) && NOT Staging->_PendingBody.IsSet()
            && Staging->_Required.Num() > 0 && Staging->_Required.Num() <= 2
            && FMath::IsFinite(Staging->_Speed) && Staging->_Speed > 0.0f
            && NOT Staging->_Direction.ContainsNaN() && Staging->_Direction.IsNormalized()
            && InHandle.Has<FFragment_SurfaceMotion>()
            && Staging->_Direction.Equals(InHandle.Get<FFragment_SurfaceMotion>()._Direction, 1.0e-3)
            && Staging->_Speed == InHandle.Get<FFragment_SurfaceMotion>()._Speed
            && (NOT InHandle.Has<FFragment_ProceduralBodyPose>() || HasPose)
            && NOT Staging->_ExitBody.ContainsNaN() && Staging->_ExitBody.GetRotation().IsNormalized()
            && (NOT Staging->_TwoPlacement || (StagingFence != nullptr && StagingFence->_Spent
                && StagingFence->_Owner == InHandle.ConvertToHandle() && StagingFence->_World.Get() == World
                && StagingFence->_Episode == Staging->_Episode
                && StagingFence->_RequiredCount == Staging->_Required.Num()));
        auto StagingCompletions = uint8{0};
        auto StagingStarted = uint8{0};
        auto StagingCompletionReceipts = std::array<TOptional<ck_procedural_gait::FPlantSeedPlane>, 2>{};
        const auto MaxHipLateral = ck_procedural_gait::Get_MaxHipLateral(InGaitComp._Legs, &ck_procedural_gait::Get_IsLegEnabled);
        // The legs are visited from the one after the last leg that searched, so the search budget goes round the legs.
        for (auto Visit = 0; Visit < LegCount; ++Visit)
        {
            const auto Index = (InGaitComp._NextSearchLeg + Visit) % LegCount;
            auto& Leg = InGaitComp._Legs[Index];
            auto& Input = Inputs[Index];
            auto& DebugLeg = InDebugComp._ScratchLegs[Index];
            const auto WasEnabled = (InGaitComp._EnabledMask & ck_procedural_gait::Get_LegBit(Index)) != 0;
            const auto LegValid = ck::IsValid(Leg);
            const auto Live = procedural_leg::Get_IsLive(Leg);
            const auto Enabled = ck_procedural_gait::Get_IsLegEnabled(Leg);

            const auto Withdrawn = InGaitComp._ParticipationLeases.ContainsByPredicate([Index](const auto& InLease)
                { return InLease._LegIndices.Contains(Index); });
            const auto Unplanted = (InGaitComp._UnplantedMask & (uint64{1} << Index)) != 0;
            Input.Set_Enabled(Enabled).Set_Participating(NOT Withdrawn && NOT Unplanted);
            DebugLeg.Set_Enabled(Enabled)
                .Set_Status(NOT Live ? ECk_ProceduralLeg_Status::Detached
                    : Enabled ? ECk_ProceduralLeg_Status::Enabled
                    : ECk_ProceduralLeg_Status::Disabled)
                .Set_LandingPointWorld(FVector::ZeroVector)
                .Set_LandingProbe(FCk_ProceduralAnimation_DebugProbe{})
                .Set_LandingGround(EProceduralGaitLandingGround::Unknown)
                .Set_ChosenFoothold(INDEX_NONE)
                .Set_FootholdSource(EProceduralFootholdSource::None)
                .Set_PlantOccluded(false)
                .Set_ActualPlantSeedAttempted(false)
                .Set_ActualPlantSeedForceAdjusted(false)
                .Set_ActualPlantSeedAdmitted(false);
            DebugLeg.Get_Footholds().Reset();
            DebugLeg.Get_Probe() = FCk_ProceduralAnimation_DebugProbe{};
            DebugLeg.Get_Foot().Set_ContactTrusted(false);

            if (NOT Live)
            {
                EnabledMask &= ~ck_procedural_gait::Get_LegBit(Index);
                if (LegValid)
                {
                    Leg.Try_Remove<FFragment_ProceduralLeg_FrozenPose>();
                    DebugLeg.Set_Id(Leg.Get<FFragment_ProceduralLeg_Params>().Get_Id());
                }
                continue;
            }

            const auto& Params = Leg.Get<FFragment_ProceduralLeg_Params>();
            const auto& Placement = Params.Get_Placement();
            auto& LegComp = Leg.Get<FFragment_ProceduralLeg>();
            const auto Neutral = Body.TransformPosition(Placement.Get_RestFootLocal());
            const auto Hip = Body.TransformPosition(Placement.Get_HipLocal());
            Input.Set_PhaseOffset(Placement.Get_PhaseOffset())
                .Set_StepThresholdScale(Placement.Get_StepThresholdScale())
                .Set_FootContactRadius(Params.Get_FootContactRadius());
            DebugLeg.Set_Id(Params.Get_Id());
            DebugLeg.Get_Targeting().Set_HipWorld(Hip)
                .Set_NeutralWorld(Neutral);

            if (NOT Enabled)
            {
                EnabledMask &= ~ck_procedural_gait::Get_LegBit(Index);

                if (WasEnabled)
                {
                    Leg.AddOrGet<FFragment_ProceduralLeg_FrozenPose>()
                        .Set_FootLocal(Body.InverseTransformPosition(LegComp._Foot.Get_Position()))
                        .Set_RotationLocal((InverseBasis * LegComp._Foot.Get_Rotation()).GetNormalized());
                }

                const auto& Frozen = Leg.Get<FFragment_ProceduralLeg_FrozenPose>();
                const auto PreviousPhase = LegComp._Foot.Get_Phase();
                const auto PreviousPosition = LegComp._Foot.Get_Position();
                LegComp._Foot.Set_Position(Body.TransformPosition(Frozen.Get_FootLocal()))
                    .Set_Rotation((Basis * Frozen.Get_RotationLocal()).GetNormalized())
                    .Set_Phase(ECk_ProceduralLeg_FootPhase::Planted)
                    .Set_SwingAlpha(0.0f)
                    .Set_Contact(ECk_ProceduralLeg_FootContact::Guessed)
                    .Set_Foothold(ECk_ProceduralLeg_Foothold::None);

                // Freezing a swinging foot as it goes is not a plant.
                ck_procedural_gait::DoPublish_FootPhaseChange(Leg, LegComp, PreviousPhase, PreviousPosition, Dt);

                // The frozen pose is the source of truth so a re-enabled leg swings from where it is drawn.
                // On the transition frame the solver's own reconcile freezes this same pose.
                const auto SolverHoldsDisabledLeg = NOT InGaitComp._Solver.IsLegEnabled(Index);
                if (SolverHoldsDisabledLeg)
                {
                    const auto PoseSynced = InGaitComp._Solver.SetDisabledPose(Index,
                        InverseBasis.RotateVector(LegComp._Foot.Get_Position()),
                        (InverseBasis * LegComp._Foot.Get_Rotation()).GetNormalized(),
                        InverseBasis.RotateVector(Up));

                    CK_ENSURE_IF_NOT(PoseSynced,
                        TEXT("Procedural gait [{}] could not sync the frozen pose of disabled leg [{}]; feature is failed."),
                        InHandle, Index)
                    {
                        InHandle.Add<FFragment_ProceduralGait_Failure>(ECk_ProceduralGait_Failure::DisabledPoseSync);
                        return;
                    }
                }
                continue;
            }

            if (NOT WasEnabled)
            { Leg.Try_Remove<FFragment_ProceduralLeg_FrozenPose>(); }

            ++EnabledCount;
            const auto Reach = ck_procedural_gait_utils::Get_Reach(Params.Get_Chain());
            const auto HipSupport = InverseBasis.RotateVector(Hip);
            const auto Query = FProceduralGaitSolver::ComputeLeadQuery(Neutral, Lead, Body.GetLocation(), Up, YawRate,
                Timing.Get_StepDuration(), Step.Get_MaxVelocityLead());
            const auto Ideal = Basis.RotateVector(FProceduralGaitSolver::ClampToReach(HipSupport,
                InverseBasis.RotateVector(Query), Step.Get_TargetReachFraction() * Reach));
            DebugLeg.Get_Targeting().Set_QueryTarget(Ideal);
            MeanFootRadius += FVector::VectorPlaneProject(Placement.Get_RestFootLocal(), FVector::UpVector).Size();

            const auto Planted = NOT Withdrawn && NOT Unplanted
                && LegComp._Foot.Get_Phase() == ECk_ProceduralLeg_FootPhase::Planted;
            const auto* RecoveryLease = InGaitComp._ParticipationLeases.FindByPredicate([Index](const auto& InLease)
                { return InLease._LegIndices.Contains(Index); });
            if (Withdrawn && (RecoveryLease == nullptr || NOT RecoveryLease->_Recovering
                || NOT RecoveryLease->_LandingProbePending))
            {
                Input.Set_TargetValid(false).Set_TargetTrusted(false).Set_TargetIsFoothold(false);
                continue;
            }
            const auto NativeRecoveryInput = NativeMotionObservation && NOT Withdrawn && NOT Unplanted
                && FMath::IsFinite(Params.Get_FootContactRadius()) && Params.Get_FootContactRadius() > 0.0f;
            Input.Set_NativeRecoveryEnabled(NativeRecoveryInput);
            const auto RequiredIndex = FreshStagingReplant ? Staging->_Required.IndexOfByPredicate([&](const auto& InRequired)
                { return InRequired._Leg == Leg && InRequired._Index == Index; }) : INDEX_NONE;
            const auto StagingLeg = RequiredIndex != INDEX_NONE && NativeRecoveryInput
                && UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(Leg) == InHandle.ConvertToHandle()
                && (NOT Staging->_TwoPlacement
                    || (StagingFence->_Legs[RequiredIndex]._Next != EProceduralContactStagingMilestone::Complete
                        && (StagingFence->_Legs[RequiredIndex]._Next != EProceduralContactStagingMilestone::Intermediate
                            || Staging->_Required[RequiredIndex]._IntermediateQuery.IsSet())));
            const auto NativeRecoveryPose = NativeRecoveryInput
                && InGaitComp._Solver.GetLegState(Index).Get_AutomaticRecovery() != EProceduralGaitAutomaticRecovery::None;
            if (HasPose && (NativeRecoveryPose
                    || (Planted && LegComp._Foot.Get_Contact() == ECk_ProceduralLeg_FootContact::Trusted))
                && UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(Leg) == InHandle.ConvertToHandle())
            {
                Input.Set_PosedHip(TOptional<FVector>{InverseBasis.RotateVector(
                    PresentationBody.TransformPosition(Placement.Get_HipLocal()))});
            }
            if (PaceSupport && Planted && LegComp._Foot.Get_Contact() == ECk_ProceduralLeg_FootContact::Trusted
                && UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(Leg) == InHandle.ConvertToHandle())
            {
                for (const auto& Feedback : PaceSupport->_ReachPaceFeedback)
                {
                    if (Feedback.Get_Leg() == Leg && Feedback.Get_FootWorld().Equals(LegComp._Foot.Get_Position(), 1.0e-3))
                    {
                        auto Trial = FProceduralGaitReachPaceTrial{}.Set_Hip(InverseBasis.RotateVector(Feedback.Get_TrialHipWorld()));
                        if (HasPose && Feedback.Get_TrialPosedHipWorld().IsSet())
                        { Trial.Set_PosedHip(TOptional<FVector>{InverseBasis.RotateVector(Feedback.Get_TrialPosedHipWorld().GetValue())}); }
                        Input.Set_ReachPaceTrial(TOptional<FProceduralGaitReachPaceTrial>{Trial});
                        break;
                    }
                }
            }
            auto RejectedReservation = false;
            const auto& LegState = InGaitComp._Solver.GetLegState(Index);
            const auto& Swing = LegState.Get_Swing();
            const auto CommittedPoint = Swing.Get_Active() && NOT Swing.Get_CatchStep() && Swing.Get_TargetTrusted()
                && NOT Swing.Get_CommittedLandingPoint().ContainsNaN()
                ? TOptional<FVector>{Basis.RotateVector(Swing.Get_CommittedLandingPoint())} : TOptional<FVector>{};
            const auto FootholdQuery = ck_procedural_gait::FFootholdQuery{
                .World = World,
                .Basis = Basis,
                .BodyLocation = Body.GetLocation(),
                .Hip = Hip,
                .Outboard = ck_procedural_gait::Get_Outboard(Hip, Neutral, Up),
                .MaxHipLateral = MaxHipLateral,
                .Ideal = Ideal,
                .Plant = LegComp._Foot.Get_Position(),
                .Planted = Planted,
                .RefreshPlantedHold = RefreshPlantedHolds && Planted && NOT Withdrawn && NOT Unplanted
                    && IsCurrentNativePlant(Leg, Index),
                .Travel = FVector::VectorPlaneProject(Velocity, Up),
                .BlockedFace = BlockedFace,
                .AdaptiveTop = AdaptiveTop.IsSet() && Planted && NOT Withdrawn && NOT Unplanted
                    && IsCurrentNativePlant(Leg, Index)
                    && LegState.Get_AutomaticRecovery() == EProceduralGaitAutomaticRecovery::None
                    ? AdaptiveTop : TOptional<ck_procedural_gait::FObservedBlockedFace>{},
                .Reach = Reach,
                .RestDrop = ck_procedural_gait::Get_RestDrop(Placement),
                .Probe = &Probe,
                .Step = &Step,
                .Foothold = &InTunables.Get_Foothold(),
                .FootContactRadius = Params.Get_FootContactRadius(),
                .LegIndex = Index,
                .Reservations = Reservations,
                .RejectedReservation = &RejectedReservation,
                .HeightObservations = CaptureHeight && NOT Withdrawn && NOT Unplanted ? &HeightObservations[Index] : nullptr,
                .CommittedPoint = CommittedPoint,
                .MatchingLandingReceipt = &MatchingLandingReceipts[Index],
                .ReceiptOwner = InHandle.ConvertToHandle(),
                .ReceiptInputSolve = ConsumerInputSolve,
                .ReceiptBody = Body,
                .CaptureTargetReceipt = StagingLeg};
            const auto SearchesLeft = SearchBudget.RemainingLegs;
            auto Foothold = ck_procedural_gait::Get_Foothold(FootholdQuery, InGaitComp._Footholds[Index], SearchBudget, RayCount,
                &DebugLeg);
            if (SearchBudget.RemainingLegs < SearchesLeft)
            { LastSearchedLeg = Index; }
            if (Foothold.Source == EProceduralFootholdSource::None && NOT Withdrawn && NOT Unplanted
                && InGaitComp._Solver.IsLegParticipating(Index) && Swing.Get_Active() && NOT Swing.Get_CatchStep()
                && Swing.Get_TargetTrusted() && NOT (MotionAirborne.IsSet() && MotionAirborne.GetValue()))
            {
                const auto Landing = Swing.Get_CommittedLandingPoint();
                const auto& LandingNormal = Swing.Get_TargetNormal();
                CK_ENSURE_IF_NOT(NOT Landing.ContainsNaN() && NOT LandingNormal.ContainsNaN() && LandingNormal.IsNormalized(),
                    TEXT("Procedural gait [{}] leg [{}] has a malformed committed landing; feature is failed."), InHandle, Index)
                {
                    InHandle.Add<FFragment_ProceduralGait_Failure>(ECk_ProceduralGait_Failure::SolverStep);
                    return;
                }
                // Search loss cannot alone replace a still-admissible committed native landing with untrusted rest.
                // Validate it afresh without restoring held-search state; ordinary search and explicit catches keep priority.
                const auto LandingWorld = Basis.RotateVector(Landing);
                const auto NormalWorld = Basis.RotateVector(LandingNormal).GetSafeNormal();
                const auto PosedHip = HasPose ? PresentationBody.TransformPosition(Placement.Get_HipLocal()) : Hip;
                constexpr auto PhysicalReachTolerance = 1.0e-3;
                if (UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(Leg) == InHandle.ConvertToHandle()
                    && FVector::DistSquared(LandingWorld, PosedHip)
                        <= FMath::Square(static_cast<double>(Reach) + PhysicalReachTolerance))
                {
                    const auto Hit = ck_procedural_gait::Get_GroundHit(World, LandingWorld, NormalWorld, FVector::ZeroVector,
                        Probe, ck_procedural_gait::EGroundProbeAttempts::RetryFromInsideSolid, RayCount, nullptr);
                    if (Hit.IsSet() && FVector::Dist(Hit->Get_Position(), LandingWorld)
                        <= InTunables.Get_Foothold().Get_OcclusionTolerance())
                    {
                        auto Candidate = ck_procedural_gait::FFootholdCandidate{LandingWorld, Hit->Get_Normal().GetSafeNormal(),
                            EProceduralFootholdSource::Held, EProceduralFootholdVerdict::Miss,
                            Swing.Get_TargetOnAFace() ? EProceduralFootholdCast::Face : EProceduralFootholdCast::Down};
                        Candidate.NativeHit = Candidate.Position == Hit->Get_Position();
                        ck_procedural_gait::DoValidate(FootholdQuery, Candidate, ck_procedural_gait::EFootholdStage::Search, RayCount);
                        DebugLeg.Get_Footholds().Add(FCk_ProceduralAnimation_DebugFoothold{}
                            .Set_Position(Candidate.Position).Set_Normal(Candidate.Normal)
                            .Set_Source(Candidate.Source).Set_Verdict(Candidate.Verdict));
                        if (Candidate.Verdict == EProceduralFootholdVerdict::Usable)
                        {
                            Foothold = ck_procedural_gait::DoFinish(Candidate, DebugLeg.Get_Footholds().Num() - 1,
                                Foothold.IdealVerdict, &DebugLeg);
                        }
                    }
                }
            }
            auto Trusted = Foothold.Source != EProceduralFootholdSource::None;
            auto PreserveCurrentPlant = false;
            auto FreshPlantPoint = TOptional<FVector>{};
            auto FreshPlantPlane = TOptional<ck_procedural_gait::FPlantSeedPlane>{};
            if (NOT Trusted && Planted && NOT Withdrawn && NOT Unplanted
                && InGaitComp._Solver.IsLegParticipating(Index) && NOT LegState.Get_Swing().Get_Active()
                && LegState.Get_Plant().Get_Trusted() && NOT (MotionAirborne.IsSet() && MotionAirborne.GetValue()))
            {
                const auto& Plant = LegState.Get_Plant();
                CK_ENSURE_IF_NOT(NOT Plant.Get_Position().ContainsNaN() && NOT Plant.Get_Normal().ContainsNaN()
                    && Plant.Get_Normal().IsNormalized(),
                    TEXT("Procedural gait [{}] leg [{}] has a malformed current plant; feature is failed."), InHandle, Index)
                {
                    InHandle.Add<FFragment_ProceduralGait_Failure>(ECk_ProceduralGait_Failure::SolverStep);
                    return;
                }
                // A retired future target is not evidence that the actual stance has lost contact. Revalidate the
                // solver plant itself, at physical chain reach, without reviving or republishing the retired target.
                const auto PlantWorld = Basis.RotateVector(Plant.Get_Position());
                const auto PlantNormal = Basis.RotateVector(Plant.Get_Normal()).GetSafeNormal();
                // Match StepProceduralSurfaceMotionPaced's physical reach boundary, including its numerical tolerance.
                constexpr auto PhysicalReachTolerance = 1.0e-3;
                const auto ReachSquared = FMath::Square(static_cast<double>(Reach) + PhysicalReachTolerance);
                const auto PosedHip = HasPose ? PresentationBody.TransformPosition(Placement.Get_HipLocal()) : Hip;
                const auto WithinReach = FVector::DistSquared(PlantWorld, Hip) <= ReachSquared
                    && FVector::DistSquared(PlantWorld, PosedHip) <= ReachSquared;
                if (WithinReach && UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(Leg) == InHandle.ConvertToHandle()
                    && Get_IsProceduralFootContactAvailable(PlantWorld, Params.Get_FootContactRadius(), Index, Reservations)
                    && NOT ck_procedural_gait::Get_Occluder(FootholdQuery, PlantWorld, RayCount).IsSet())
                {
                    const auto Hit = ck_procedural_gait::Get_GroundHit(World, PlantWorld, PlantNormal,
                        FVector::ZeroVector, Probe, ck_procedural_gait::EGroundProbeAttempts::RetryFromInsideSolid,
                        RayCount, nullptr);
                    PreserveCurrentPlant = Hit.IsSet() && FVector::Dist(Hit->Get_Position(), PlantWorld)
                        <= InTunables.Get_Foothold().Get_OcclusionTolerance();
                    if (PreserveCurrentPlant)
                    {
                        FreshPlantPoint = PlantWorld;
                        if (NOT Hit->Get_Position().ContainsNaN() && NOT Hit->Get_Normal().ContainsNaN()
                            && Hit->Get_Normal().IsNormalized())
                        { FreshPlantPlane = ck_procedural_gait::FPlantSeedPlane{Hit->Get_Position(), Hit->Get_Normal()}; }
                    }
                }
                if (PreserveCurrentPlant)
                { RevalidatedPlants |= ck_procedural_gait::Get_LegBit(Index); }
                else
                { InGaitComp._Solver.SetPlantedPose(Index, Plant.Get_Position(), Plant.Get_Normal(), false); }
            }

            // Only an unsuccessful budgeted search can grant one unused boundary slot. The actual stance was just
            // checked natively above; it seeds a NEW future query, never a restored held target or take-off authority.
            TOptional<FVector> BoundarySeedPoint;
            auto FailedFinalStraightPoint = TOptional<FVector>{};
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
            auto HipObservation = ck_procedural_gait_processor_hip_query_test::Begin(
                InHandle, Leg, Index, FootholdQuery.World, ConsumerInputSolve, StagingLeg);
#endif
            struct FFailedPlantSeedQuery
            {
                FVector SeedPoint;
                FCk_ProceduralGait_Probe Probe;
                FVector Plant;
                FVector Lead;
                TOptional<ck_procedural_gait::FPlantSeedPlane> Plane;
                float ForceReach;
                float SearchRadius;
                bool NativeDownUnreachable = false;
            };
            auto FailedPlantSeed = TOptional<FFailedPlantSeedQuery>{};
            const auto& PressureTrial = Input.Get_ReachPaceTrial();
            if (PreserveCurrentPlant && FreshPlantPoint.IsSet() && Foothold.UnusedBoundarySamples > 0 && HasFreshRequestedLead
                && FootholdQuery.RefreshPlantedHold && NOT FootholdQuery.BlockedFace.IsSet()
                && PressureTrial.IsSet() && NOT InGaitComp._Solver.HasPendingStep(Index))
            {
                const auto HalfLead = (Query - Neutral) * 0.5;
                const auto SearchRadius = InTunables.Get_Foothold().Get_SearchRadius() > 0.0f
                    ? InTunables.Get_Foothold().Get_SearchRadius() : ck_procedural_gait::DerivedSearchRadiusShareOfReach * Reach;
                // Preserve DoValidate's native float radius product; the predictor adds no posed-force constraint.
                const auto ForceReach = FootholdQuery.Step->Get_ForceStepReachFraction() * FootholdQuery.Reach;
                const auto SeedDisplacement = ck_procedural_gait::Get_PlantSeedDisplacement(
                    FreshPlantPoint.GetValue(), Query - Neutral, FootholdQuery.Hip, ForceReach, SearchRadius);
                const auto Limit = static_cast<double>(Reach) + 1.0e-3;
                const auto& Trial = PressureTrial.GetValue();
                const auto& Plant = LegState.Get_Plant();
                const auto PlantExceedsTrial = FVector::Dist(Plant.Get_Position(), Trial.Get_Hip()) > Limit
                    || (Trial.Get_PosedHip().IsSet()
                        && FVector::Dist(Plant.Get_Position(), Trial.Get_PosedHip().GetValue()) > Limit);
                const auto SeedX = FreshPlantPoint->X + SeedDisplacement.X;
                const auto SeedY = FreshPlantPoint->Y + SeedDisplacement.Y;
                const auto SeedZ = FreshPlantPoint->Z + SeedDisplacement.Z;
                const auto SeedIsFinite = FMath::IsFinite(SeedX) && FMath::IsFinite(SeedY) && FMath::IsFinite(SeedZ);
                if (PlantExceedsTrial && NOT SeedDisplacement.ContainsNaN() && SeedDisplacement.SizeSquared() > 0.0
                    && FMath::IsFinite(SearchRadius) && SearchRadius > 0.0f
                    && SeedDisplacement.SizeSquared() <= FMath::Square(static_cast<double>(SearchRadius))
                    && SeedIsFinite)
                {
                    auto SeedPoint = FVector{SeedX, SeedY, SeedZ};
                    if (FreshPlantPlane.IsSet())
                    {
                        SeedPoint = ck_procedural_gait::Get_OwnSidePlantSeedPoint(SeedPoint, FreshPlantPoint.GetValue(),
                            FootholdQuery.Hip, FootholdQuery.Outboard, FreshPlantPlane.GetValue(), Reach, ForceReach, SearchRadius);
                    }
                    const auto PaidSeedProbe = *FootholdQuery.Probe;
                    --Foothold.UnusedBoundarySamples;
                    DebugLeg.Set_ActualPlantSeedAttempted(true);
                    DebugLeg.Set_ActualPlantSeedForceAdjusted(SeedDisplacement != HalfLead);
                    const auto Candidate = ck_procedural_gait::DoProbe_Down(FootholdQuery, SeedPoint,
                        EProceduralFootholdSource::Ring, ck_procedural_gait::EGroundProbeAttempts::RetryFromInsideSolid,
                        ck_procedural_gait::EFootholdReprobe::None, ck_procedural_gait::EFootholdStage::Search, RayCount, nullptr);
                    const auto CandidateIndex = DebugLeg.Get_Footholds().Num();
                    DebugLeg.Get_Footholds().Add(FCk_ProceduralAnimation_DebugFoothold{}
                        .Set_Position(Candidate.Position).Set_Normal(Candidate.Normal)
                        .Set_Source(Candidate.Source).Set_Verdict(Candidate.Verdict));
                    const auto Target = InverseBasis.RotateVector(Candidate.Position);
                    const auto FitsTrial = FVector::Dist(Target, Trial.Get_Hip()) <= Limit
                        && (NOT Trial.Get_PosedHip().IsSet()
                            || FVector::Dist(Target, Trial.Get_PosedHip().GetValue()) <= Limit);
                    // The rejected current anchor and admitted trial target prove relief, excluding an equal-plant
                    // fallback. The ordinary Emergency/schedule and take-off guard still decide whether to step.
                    if (Candidate.Verdict == EProceduralFootholdVerdict::Usable && FitsTrial)
                    {
                        InGaitComp._Footholds[Index] = FProceduralFootholdState{}
                            .Set_Position(Candidate.Position).Set_Normal(Candidate.Normal)
                            .Set_Source(Candidate.Source).Set_Hold(EProceduralFootholdHold::Held).Set_Cast(Candidate.Cast);
                        Foothold = ck_procedural_gait::DoFinish(Candidate, CandidateIndex, Foothold.IdealVerdict, &DebugLeg);
                        Trusted = true;
                        PreserveCurrentPlant = false;
                        DebugLeg.Set_ActualPlantSeedAdmitted(true);
                    }
                    else
                    {
                        // This receipt exists only after the charged old seed actually failed native/trial admission.
                        FailedPlantSeed = FFailedPlantSeedQuery{SeedPoint, PaidSeedProbe,
                            FreshPlantPoint.GetValue(), Query - Neutral, FreshPlantPlane, ForceReach, SearchRadius,
                            Candidate.NativeHit && Candidate.Cast == EProceduralFootholdCast::Down
                                && Candidate.Verdict == EProceduralFootholdVerdict::Unreachable};
                        if (FreshPlantPlane.IsSet() && Foothold.UnusedBoundarySamples > 0)
                        {
                            BoundarySeedPoint = ck_procedural_gait::Get_PlantSeedBoundaryPoint(
                                FreshPlantPoint.GetValue(), Query - Neutral, FootholdQuery.Hip, FootholdQuery.Outboard,
                                FreshPlantPlane.GetValue(), Reach, ForceReach, SearchRadius);
                        }
                    }
                }
            }
            // Preserve every older result and the actual-plant seed's first opportunity. Only a still-valid
            // physical stance with no trusted future target may spend remaining shared slots on native top receipts.
            if (PreserveCurrentPlant && NOT Trusted && FreshPlantPoint.IsSet() && Foothold.UnusedBoundarySamples > 0
                && NOT Foothold.InwardOccluderTops.IsEmpty() && HasFreshRequestedLead
                && FootholdQuery.RefreshPlantedHold && NOT FootholdQuery.BlockedFace.IsSet()
                && PressureTrial.IsSet() && NOT InGaitComp._Solver.HasPendingStep(Index))
            {
                const auto Limit = static_cast<double>(Reach) + 1.0e-3;
                const auto& Trial = PressureTrial.GetValue();
                const auto& Plant = LegState.Get_Plant();
                const auto PlantExceedsTrial = FVector::Dist(Plant.Get_Position(), Trial.Get_Hip()) > Limit
                    || (Trial.Get_PosedHip().IsSet()
                        && FVector::Dist(Plant.Get_Position(), Trial.Get_PosedHip().GetValue()) > Limit);
                if (PlantExceedsTrial)
                {
                    // Copy the bounded receipts before DoFinish can replace Foothold and its array storage.
                    const auto OccluderTops = Foothold.InwardOccluderTops;
                    auto StraightProbe = *FootholdQuery.Probe;
                    StraightProbe.Set_OutwardLean(0.0f);
                    auto TopQuery = FootholdQuery;
                    TopQuery.Probe = &StraightProbe;
                    for (const auto& Receipt : OccluderTops)
                    {
                        if (Foothold.UnusedBoundarySamples <= 0)
                        { break; }
                        --Foothold.UnusedBoundarySamples;
                        // At most two ground attempts plus the existing visibility ray. No reach re-probe,
                        // recursive seed or new search budget is introduced; the fresh native hit owns admission.
                        const auto Candidate = ck_procedural_gait::DoProbe_Down(TopQuery, Receipt.Point,
                            EProceduralFootholdSource::Inward, ck_procedural_gait::EGroundProbeAttempts::RetryFromInsideSolid,
                            ck_procedural_gait::EFootholdReprobe::None, ck_procedural_gait::EFootholdStage::Search, RayCount, nullptr);
                        const auto CandidateIndex = DebugLeg.Get_Footholds().Num();
                        DebugLeg.Get_Footholds().Add(FCk_ProceduralAnimation_DebugFoothold{}
                            .Set_Position(Candidate.Position).Set_Normal(Candidate.Normal)
                            .Set_Source(Candidate.Source).Set_Verdict(Candidate.Verdict));
                        const auto Target = InverseBasis.RotateVector(Candidate.Position);
                        const auto FitsTrial = FVector::Dist(Target, Trial.Get_Hip()) <= Limit
                            && (NOT Trial.Get_PosedHip().IsSet()
                                || FVector::Dist(Target, Trial.Get_PosedHip().GetValue()) <= Limit);
                        if (Candidate.Verdict != EProceduralFootholdVerdict::Usable || NOT FitsTrial)
                        { continue; }

                        InGaitComp._Footholds[Index] = FProceduralFootholdState{}
                            .Set_Position(Candidate.Position).Set_Normal(Candidate.Normal)
                            .Set_Source(Candidate.Source).Set_Hold(EProceduralFootholdHold::Held).Set_Cast(Candidate.Cast);
                        Foothold = ck_procedural_gait::DoFinish(Candidate, CandidateIndex, Foothold.IdealVerdict, &DebugLeg);
                        Trusted = true;
                        PreserveCurrentPlant = false;
                        break;
                    }
                }
            }
            // This last receipt is query geometry from an original failed face, never a promoted face target.
            if (PreserveCurrentPlant && NOT Trusted && FreshPlantPoint.IsSet() && Foothold.UnusedBoundarySamples > 0
                && Foothold.FailedInwardFace.IsSet() && HasFreshRequestedLead
                && FootholdQuery.RefreshPlantedHold && NOT FootholdQuery.BlockedFace.IsSet()
                && PressureTrial.IsSet() && NOT InGaitComp._Solver.HasPendingStep(Index))
            {
                const auto Limit = static_cast<double>(Reach) + 1.0e-3;
                const auto& Trial = PressureTrial.GetValue();
                const auto& Plant = LegState.Get_Plant();
                const auto PlantExceedsTrial = FVector::Dist(Plant.Get_Position(), Trial.Get_Hip()) > Limit
                    || (Trial.Get_PosedHip().IsSet()
                        && FVector::Dist(Plant.Get_Position(), Trial.Get_PosedHip().GetValue()) > Limit);
                if (PlantExceedsTrial)
                {
                    const auto Receipt = Foothold.FailedInwardFace.GetValue();
                    const auto SearchRadius = InTunables.Get_Foothold().Get_SearchRadius() > 0.0f
                        ? InTunables.Get_Foothold().Get_SearchRadius() : ck_procedural_gait::DerivedSearchRadiusShareOfReach * Reach;
                    constexpr auto QueryMargin = 0.1;
                    const auto BodyBoundary = static_cast<double>(FootholdQuery.MaxHipLateral)
                        - ck_procedural_gait::UnderBodyMargin + QueryMargin;
                    const auto OwnSideMinimum = -static_cast<double>(ck_procedural_gait::InboardToleranceShareOfReach * Reach)
                        + QueryMargin;
                    const auto Point = Get_ProceduralFootholdFailedFaceQueryPoint(Receipt.Point, Receipt.Normal,
                        FootholdQuery.Basis.GetAxisZ(), FootholdQuery.BodyLocation, FootholdQuery.Hip,
                        FootholdQuery.Basis.GetAxisY(), FootholdQuery.Outboard, BodyBoundary, OwnSideMinimum, SearchRadius);
                    if (Point.IsSet())
                    {
                        auto StraightProbe = *FootholdQuery.Probe;
                        StraightProbe.Set_OutwardLean(0.0f);
                        auto TopQuery = FootholdQuery;
                        TopQuery.Probe = &StraightProbe;
                        --Foothold.UnusedBoundarySamples;
                        auto Candidate = ck_procedural_gait::DoProbe_Down(TopQuery, Point.GetValue(),
                            EProceduralFootholdSource::Inward, ck_procedural_gait::EGroundProbeAttempts::RetryFromInsideSolid,
                            ck_procedural_gait::EFootholdReprobe::None, ck_procedural_gait::EFootholdStage::Search, RayCount, nullptr);
                        // Unreachable here follows the trusted initial hit and its first reach rejection, with no
                        // visibility ray. Reserve a second four-ray slot before the at-most-two-ray native reprobe
                        // and its one visibility ray. One remaining initial slot retains the original behavior.
                        if (Candidate.Verdict == EProceduralFootholdVerdict::Unreachable && Foothold.UnusedBoundarySamples > 0)
                        {
                            --Foothold.UnusedBoundarySamples;
                            Candidate = ck_procedural_gait::DoReprobe_DownTowardHip(TopQuery, Candidate,
                                ck_procedural_gait::EGroundProbeAttempts::RetryFromInsideSolid,
                                ck_procedural_gait::EFootholdStage::Search, RayCount, nullptr);
                        }
                        const auto CandidateIndex = DebugLeg.Get_Footholds().Num();
                        DebugLeg.Get_Footholds().Add(FCk_ProceduralAnimation_DebugFoothold{}
                            .Set_Position(Candidate.Position).Set_Normal(Candidate.Normal)
                            .Set_Source(Candidate.Source).Set_Verdict(Candidate.Verdict));
                        const auto Target = InverseBasis.RotateVector(Candidate.Position);
                        const auto FitsTrial = FVector::Dist(Target, Trial.Get_Hip()) <= Limit
                            && (NOT Trial.Get_PosedHip().IsSet()
                                || FVector::Dist(Target, Trial.Get_PosedHip().GetValue()) <= Limit);
                        if (Candidate.Verdict == EProceduralFootholdVerdict::Usable && FitsTrial)
                        {
                            InGaitComp._Footholds[Index] = FProceduralFootholdState{}
                                .Set_Position(Candidate.Position).Set_Normal(Candidate.Normal)
                                .Set_Source(Candidate.Source).Set_Hold(EProceduralFootholdHold::Held).Set_Cast(Candidate.Cast);
                            Foothold = ck_procedural_gait::DoFinish(Candidate, CandidateIndex, Foothold.IdealVerdict, &DebugLeg);
                            Trusted = true;
                            PreserveCurrentPlant = false;
                        }
                    }
                }
            }
            // The old eight-point result owns its final attempt, including a later native rejection.
            // Only its absence and an actual failed paid seed permit this one-cell proposal.
            if (NOT BoundarySeedPoint.IsSet() && FailedPlantSeed.IsSet()
                && NOT Trusted && PreserveCurrentPlant && Foothold.UnusedBoundarySamples > 0
                && FreshPlantPoint.IsSet() && FreshPlantPlane.IsSet() && HasFreshRequestedLead
                && FootholdQuery.RefreshPlantedHold && NOT FootholdQuery.BlockedFace.IsSet()
                && PressureTrial.IsSet() && NOT InGaitComp._Solver.HasPendingStep(Index)
                && FailedPlantSeed->Plane.IsSet() && FootholdQuery.Probe != nullptr)
            {
                const auto& Failed = FailedPlantSeed.GetValue();
                auto ProposedProbe = *FootholdQuery.Probe;
                ProposedProbe.Set_OutwardLean(0.0f);
                // Both paid call sites use this unchanged const query and the same source/retry/reprobe/stage.
                // The existing final block clones that query and changes only this copied probe's lean.
                const auto IdenticalPaidPoint = ck_procedural_gait::Get_IsPlantSeedProbeIdentical(Failed.Probe, ProposedProbe)
                    ? TOptional<FVector>{Failed.SeedPoint} : TOptional<FVector>{};
                BoundarySeedPoint = ck_procedural_gait::Get_FinalPlantQueryPoint(Failed.Plant, Failed.Lead,
                    FootholdQuery.Hip, FootholdQuery.Outboard, Failed.Plane.GetValue(), FootholdQuery.Basis.GetAxisZ(),
                    Failed.ForceReach, Failed.SearchRadius, ck_procedural_gait::InboardToleranceShareOfReach * Reach,
                    FootholdQuery.FootContactRadius, Index, FootholdQuery.Reservations, IdenticalPaidPoint);
            }
            if (NOT BoundarySeedPoint.IsSet() && FailedPlantSeed.IsSet()
                && NOT Trusted && PreserveCurrentPlant && Foothold.UnusedBoundarySamples > 0
                && FreshPlantPoint.IsSet() && FreshPlantPlane.IsSet() && HasFreshRequestedLead
                && FootholdQuery.RefreshPlantedHold && NOT FootholdQuery.BlockedFace.IsSet()
                && PressureTrial.IsSet() && NOT InGaitComp._Solver.HasPendingStep(Index)
                && FailedPlantSeed->Plane.IsSet() && FootholdQuery.Probe != nullptr)
            {
                const auto& Failed = FailedPlantSeed.GetValue();
                auto ProposedProbe = *FootholdQuery.Probe;
                ProposedProbe.Set_OutwardLean(0.0f);
                const auto IdenticalPaidPoint = ck_procedural_gait::Get_IsPlantSeedProbeIdentical(Failed.Probe, ProposedProbe)
                    ? TOptional<FVector>{Failed.SeedPoint} : TOptional<FVector>{};
                const auto Context = ck_procedural_gait::FTiltedPlantSeedContext{
                    Failed.NativeDownUnreachable, SearchBudget.RemainingLegs < SearchesLeft,
                    Foothold.UnusedBoundarySamples,
                    StagingAtEntry.IsSet() && StagingAtEntry->_Phase != EProceduralContactStagingPhase::Dormant,
                    Staging != nullptr && Staging->_Phase != EProceduralContactStagingPhase::Dormant};
                BoundarySeedPoint = ck_procedural_gait::Get_TiltedPlantSeedQueryPoint(FootholdQuery,
                    Failed.SeedPoint, Failed.Plant, Failed.Lead, Failed.Plane.GetValue(), Failed.ForceReach,
                    Failed.SearchRadius, PressureTrial.GetValue(), Context, IdenticalPaidPoint);
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
                HipObservation.TiltedPoint = BoundarySeedPoint.IsSet();
#endif
            }
            // All older native opportunities keep priority. Geometry alone cannot publish a target.
            if (NOT Trusted && PreserveCurrentPlant && Foothold.UnusedBoundarySamples > 0 && BoundarySeedPoint.IsSet())
            {
                auto StraightProbe = *FootholdQuery.Probe;
                StraightProbe.Set_OutwardLean(0.0f);
                auto BoundaryQuery = FootholdQuery;
                BoundaryQuery.Probe = &StraightProbe;
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
                if (HipObservation.Enabled && HipObservation.Tilted && HipObservation.TiltedPoint)
                {
                    using namespace ck_procedural_gait_processor_hip_query_test;
                    HipObservation.OldSeen = true;
                    Put(HipObservation, 9, FailedPlantSeed->NativeDownUnreachable);
                    Vector(HipObservation, 11, BoundarySeedPoint.GetValue());
                    Put(HipObservation, 21, Foothold.UnusedBoundarySamples);
                    Put(HipObservation, 23, RayCount);
                    Vector(HipObservation, 25, Up);
                    uint64 Bits = 0;
                    const auto Z = FailedPlantSeed->Plane->Point.Z;
                    FMemory::Memcpy(&Bits, &Z, sizeof(Bits));
                    Put(HipObservation, 28, Bits);
                    Put(HipObservation, 38, 1);
                }
#endif
                --Foothold.UnusedBoundarySamples;
                DebugLeg.Set_ActualPlantSeedAttempted(true);
                DebugLeg.Set_ActualPlantSeedForceAdjusted(true);
                const auto Candidate = ck_procedural_gait::DoProbe_Down(BoundaryQuery, BoundarySeedPoint.GetValue(),
                    EProceduralFootholdSource::Ring, ck_procedural_gait::EGroundProbeAttempts::RetryFromInsideSolid,
                    ck_procedural_gait::EFootholdReprobe::None, ck_procedural_gait::EFootholdStage::Search, RayCount, nullptr);
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
                if (HipObservation.Enabled && NOT HipObservation.Tilted)
                {
                    using namespace ck_procedural_gait_processor_hip_query_test;
                    HipObservation.OldSeen = true;
                    Put(HipObservation, 9, Candidate.NativeHit);
                    Put(HipObservation, 10, static_cast<uint64>(Candidate.Verdict));
                    Vector(HipObservation, 11, BoundarySeedPoint.GetValue());
                    if (Candidate.NativeHit)
                    {
                        Vector(HipObservation, 14, Candidate.Position);
                    }
                }
#endif
                if (Candidate.NativeHit && Candidate.Verdict == EProceduralFootholdVerdict::Unreachable)
                {
                    FailedFinalStraightPoint = BoundarySeedPoint.GetValue();
                }
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
                if (HipObservation.Enabled && HipObservation.Tilted && HipObservation.TiltedPoint)
                {
                    using namespace ck_procedural_gait_processor_hip_query_test;
                    Put(HipObservation, 10, static_cast<uint64>(Candidate.Verdict));
                    Put(HipObservation, 18, Candidate.NativeHit);
                    Put(HipObservation, 22, Foothold.UnusedBoundarySamples);
                    Put(HipObservation, 24, RayCount);
                    if (Candidate.NativeHit)
                    {
                        Vector(HipObservation, 14, Candidate.Position);
                    }
                }
#endif
                const auto CandidateIndex = DebugLeg.Get_Footholds().Num();
                DebugLeg.Get_Footholds().Add(FCk_ProceduralAnimation_DebugFoothold{}
                    .Set_Position(Candidate.Position).Set_Normal(Candidate.Normal)
                    .Set_Source(Candidate.Source).Set_Verdict(Candidate.Verdict));
                const auto Limit = static_cast<double>(Reach) + 1.0e-3;
                const auto& Trial = PressureTrial.GetValue();
                const auto Target = InverseBasis.RotateVector(Candidate.Position);
                const auto FitsTrial = FVector::Dist(Target, Trial.Get_Hip()) <= Limit
                    && (NOT Trial.Get_PosedHip().IsSet()
                        || FVector::Dist(Target, Trial.Get_PosedHip().GetValue()) <= Limit);
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
                if (HipObservation.Enabled && HipObservation.Tilted && HipObservation.TiltedPoint)
                {
                    ck_procedural_gait_processor_hip_query_test::Put(HipObservation, 19, FitsTrial);
                    ck_procedural_gait_processor_hip_query_test::Put(HipObservation, 20, 0);
                }
#endif
                if (Candidate.Verdict == EProceduralFootholdVerdict::Usable && FitsTrial)
                {
                    InGaitComp._Footholds[Index] = FProceduralFootholdState{}
                        .Set_Position(Candidate.Position).Set_Normal(Candidate.Normal)
                        .Set_Source(Candidate.Source).Set_Hold(EProceduralFootholdHold::Held).Set_Cast(Candidate.Cast);
                    Foothold = ck_procedural_gait::DoFinish(Candidate, CandidateIndex, Foothold.IdealVerdict, &DebugLeg);
                    Trusted = true;
                    PreserveCurrentPlant = false;
                    DebugLeg.Set_ActualPlantSeedAdmitted(true);
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
                    if (HipObservation.Enabled && HipObservation.Tilted && HipObservation.TiltedPoint)
                    {
                        ck_procedural_gait_processor_hip_query_test::Put(HipObservation, 20, 1);
                    }
#endif
                }
            }
            const auto StagingTargetFits = [&](const FVector& InPoint, const FVector& InNormal, bool InSearchChecked) -> bool
            {
                if (NOT StagingLeg || NOT Planted || NOT LegState.Get_Plant().Get_Trusted()
                    || Swing.Get_Active() || InGaitComp._Solver.HasPendingStep(Index)
                    || NOT procedural_leg::Get_IsSupporting(Leg) || InPoint.ContainsNaN() || NOT InNormal.IsNormalized())
                { return false; }
                auto ClaimsRevision = TOptional<uint64>{};
                if (NOT ck_procedural_gait::Get_LandingClaimsRevision(InHandle, ClaimsRevision))
                { return false; }
                if (InHandle.Has<FFragment_ProceduralArticulation_Claims>())
                {
                    constexpr auto LimbChannel = false;
                    if (FCk_ProceduralArticulation_Control::GetClaimant(UCk_Utils_ProceduralArticulation_UE::Cast(InHandle),
                            Params.Get_Id(), LimbChannel) != FCk_ArticulationControlToken{})
                    { return false; }
                }
                if (NOT InSearchChecked)
                {
                    if (NOT Get_IsFootholdLevelEnough(InverseBasis.RotateVector(InNormal), InTunables.Get_Foothold().Get_MaxAngle())
                        || NOT Get_IsFootholdOnOwnSide(InPoint - Hip, FootholdQuery.Outboard,
                            ck_procedural_gait::InboardToleranceShareOfReach * Reach))
                    { return false; }
                    const auto FromBody = InverseBasis.RotateVector(InPoint - Body.GetLocation());
                    const auto FromHip = InverseBasis.RotateVector(InPoint - Hip);
                    if (Get_IsFootholdUnderBody(static_cast<float>(FromBody.Y), static_cast<float>(-FromHip.Z),
                            MaxHipLateral, FootholdQuery.RestDrop, ck_procedural_gait::UnderBodyMargin))
                    { return false; }
                }
                const auto Limit = static_cast<double>(Reach) + 1.0e-3;
                const auto PosedHip = HasPose ? PresentationBody.TransformPosition(Placement.Get_HipLocal()) : Hip;
                const auto ExitHip = Staging->_ExitBody.TransformPosition(Placement.Get_HipLocal());
                const auto ExitPosedBody = HasPose
                    ? UCk_Utils_ProceduralBodyPose_UE::Get_Offset(UCk_Utils_ProceduralBodyPose_UE::Cast(InHandle)) * Staging->_ExitBody
                    : Staging->_ExitBody;
                const auto ExitPosedHip = ExitPosedBody.TransformPosition(Placement.Get_HipLocal());
                const auto PlantWorld = Basis.RotateVector(LegState.Get_Plant().Get_Position());
                const auto PlantOutside = FVector::Dist(PlantWorld, ExitHip) > Limit || FVector::Dist(PlantWorld, ExitPosedHip) > Limit;
                const auto FinalFits = FVector::Dist(InPoint, ExitHip) <= Limit && FVector::Dist(InPoint, ExitPosedHip) <= Limit;
                const auto CurrentFits = FVector::Dist(InPoint, Hip) <= Limit && FVector::Dist(InPoint, PosedHip) <= Limit
                    && Get_IsProceduralFootContactAvailable(InPoint, Params.Get_FootContactRadius(), Index, Reservations);
                if (NOT Staging->_TwoPlacement)
                { return PlantOutside && FinalFits && CurrentFits; }
                const auto& Milestones = StagingFence->_Legs[RequiredIndex];
                const auto SearchRadius = InTunables.Get_Foothold().Get_SearchRadius() > 0.0f
                    ? InTunables.Get_Foothold().Get_SearchRadius() : ck_procedural_gait::DerivedSearchRadiusShareOfReach * Reach;
                const auto ForceReach = Step.Get_ForceStepReachFraction() * Reach;
                const auto PosedOutboard = HasPose
                    ? PresentationBody.TransformVectorNoScale(Body.InverseTransformVectorNoScale(FootholdQuery.Outboard))
                    : FootholdQuery.Outboard;
                if (NOT PlantOutside || NOT CurrentFits || Milestones._StartedCount >= 2
                    || Milestones._ActualPlant != LegComp._Foot.Get_Position()
                    || NOT FMath::IsFinite(ForceReach) || ForceReach <= 0.0f
                    || FVector::Dist(InPoint, Hip) > ForceReach || FVector::Dist(InPoint, PosedHip) > ForceReach
                    || NOT Get_IsFootholdOnOwnSide(InPoint - Hip, FootholdQuery.Outboard,
                        ck_procedural_gait::InboardToleranceShareOfReach * Reach)
                    || NOT Get_IsFootholdOnOwnSide(InPoint - PosedHip, PosedOutboard,
                        ck_procedural_gait::InboardToleranceShareOfReach * Reach)
                    || NOT FMath::IsFinite(SearchRadius) || SearchRadius <= 0.0f
                    || FVector::Dist(InPoint, Milestones._ActualPlant) > SearchRadius)
                { return false; }
                return FinalFits || (Milestones._Next == EProceduralContactStagingMilestone::Intermediate
                    && Milestones._StartedCount == 0
                    && Get_IsProceduralContactStagingIntermediateTargetValid(InPoint, Milestones._ActualPlant,
                        Staging->_Required[RequiredIndex]._Query, Staging->_Direction, SearchRadius));
            };
            // This last opportunity owns no extra search or boundary budget and cannot replace an older success.
            if (StagingLeg && NOT Trusted && PreserveCurrentPlant && FreshPlantPoint.IsSet()
                && SearchBudget.RemainingLegs < SearchesLeft && Foothold.UnusedBoundarySamples > 0
                && NOT Swing.Get_Active() && NOT InGaitComp._Solver.HasPendingStep(Index))
            {
                auto StraightProbe = Probe;
                StraightProbe.Set_OutwardLean(0.0f);
                auto StagingQuery = FootholdQuery;
                StagingQuery.Probe = &StraightProbe;
                --Foothold.UnusedBoundarySamples;
                const auto& Required = Staging->_Required[RequiredIndex];
                const auto QueryPoint = Staging->_TwoPlacement
                    && StagingFence->_Legs[RequiredIndex]._Next == EProceduralContactStagingMilestone::Intermediate
                    ? Required._IntermediateQuery.GetValue() : Required._Query;
                auto Candidate = ck_procedural_gait::DoProbe_Down(StagingQuery, QueryPoint,
                    EProceduralFootholdSource::Ring, ck_procedural_gait::EGroundProbeAttempts::RetryFromInsideSolid,
                    ck_procedural_gait::EFootholdReprobe::None, ck_procedural_gait::EFootholdStage::Search, RayCount, nullptr);
                const auto CandidateIndex = DebugLeg.Get_Footholds().Num();
                DebugLeg.Get_Footholds().Add(FCk_ProceduralAnimation_DebugFoothold{}
                    .Set_Position(Candidate.Position).Set_Normal(Candidate.Normal)
                    .Set_Source(Candidate.Source).Set_Verdict(Candidate.Verdict));
                if (Candidate.Verdict == EProceduralFootholdVerdict::Usable && Candidate.NativeReceipt.IsSet()
                    && StagingTargetFits(Candidate.Position, Candidate.Normal, true))
                {
                    auto PosedQuery = StagingQuery;
                    PosedQuery.Hip = HasPose ? PresentationBody.TransformPosition(Placement.Get_HipLocal()) : Hip;
                    if (NOT ck_procedural_gait::Get_Occluder(PosedQuery, Candidate.Position, RayCount).IsSet())
                    {
                        Candidate.NativeReceipt->VisiblePosedHip = PosedQuery.Hip;
                        Candidate.NativeReceipt->StagingGeometry = true;
                        InGaitComp._Footholds[Index] = FProceduralFootholdState{}
                            .Set_Position(Candidate.Position).Set_Normal(Candidate.Normal)
                            .Set_Source(Candidate.Source).Set_Hold(EProceduralFootholdHold::Held).Set_Cast(Candidate.Cast);
                        Foothold = ck_procedural_gait::DoFinish(Candidate, CandidateIndex, Foothold.IdealVerdict, &DebugLeg);
                        Trusted = true;
                        PreserveCurrentPlant = false;
                    }
                }
            }
            // All older opportunities, including staging, precede this additional ordinary family.
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
            if (HipObservation.Enabled && NOT HipObservation.Tilted)
            {
                ck_procedural_gait_processor_hip_query_test::Put(HipObservation, 30, Foothold.UnusedBoundarySamples);
                ck_procedural_gait_processor_hip_query_test::Put(HipObservation, 31, Foothold.UnusedBoundarySamples);
                ck_procedural_gait_processor_hip_query_test::Put(HipObservation, 32, RayCount);
                ck_procedural_gait_processor_hip_query_test::Put(HipObservation, 19, 0);
                ck_procedural_gait_processor_hip_query_test::Put(HipObservation, 29, 0);
            }
#endif
            const auto HipBoundaryEligible = NOT StagingLeg && NOT Trusted && PreserveCurrentPlant
                && SearchBudget.RemainingLegs < SearchesLeft && Foothold.UnusedBoundarySamples > 0
                && FailedFinalStraightPoint.IsSet() && FailedPlantSeed.IsSet()
                && FreshPlantPoint.IsSet() && FreshPlantPlane.IsSet() && HasFreshRequestedLead
                && FootholdQuery.RefreshPlantedHold && NOT FootholdQuery.BlockedFace.IsSet()
                && PressureTrial.IsSet() && NOT InGaitComp._Solver.HasPendingStep(Index)
                && FailedPlantSeed->Plane.IsSet() && FootholdQuery.Probe != nullptr;
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
            if (HipObservation.Enabled && NOT HipObservation.Tilted)
            {
                ck_procedural_gait_processor_hip_query_test::Put(HipObservation, 18, HipBoundaryEligible);
            }
#endif
            if (HipBoundaryEligible)
            {
                const auto& Failed = FailedPlantSeed.GetValue();
                // Same immutable base query and the same straight copied probe as the old final call.
                auto StraightProbe = *FootholdQuery.Probe;
                StraightProbe.Set_OutwardLean(0.0f);
                auto HipBoundaryQuery = FootholdQuery;
                HipBoundaryQuery.Probe = &StraightProbe;
                const auto Point = ck_procedural_gait::Get_HipBodyBoundaryQueryPoint(FootholdQuery,
                    Failed.Plane.GetValue(), FootholdQuery.Basis.GetAxisZ(), Failed.ForceReach,
                    ck_procedural_gait::InboardToleranceShareOfReach * Reach, FailedFinalStraightPoint
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
                    , HipObservation.Enabled ? &HipObservation.PointCount : nullptr
#endif
                    );
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
                if (HipObservation.Enabled && NOT HipObservation.Tilted)
                {
                    ck_procedural_gait_processor_hip_query_test::Put(HipObservation, 19, Point.IsSet());
                    ck_procedural_gait_processor_hip_query_test::Put(HipObservation, 34, HipObservation.PointCount);
                    if (Point.IsSet())
                    {
                        ck_procedural_gait_processor_hip_query_test::Vector(HipObservation, 20, Point.GetValue());
                    }
                }
#endif
                if (Point.IsSet())
                {
                    --Foothold.UnusedBoundarySamples;
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
                    if (HipObservation.Enabled && NOT HipObservation.Tilted)
                    {
                        ck_procedural_gait_processor_hip_query_test::Put(HipObservation, 31, Foothold.UnusedBoundarySamples);
                    }
#endif
                    DebugLeg.Set_ActualPlantSeedAttempted(true);
                    DebugLeg.Set_ActualPlantSeedForceAdjusted(true);
                    const auto Candidate = ck_procedural_gait::DoProbe_Down(HipBoundaryQuery, Point.GetValue(),
                        EProceduralFootholdSource::Ring, ck_procedural_gait::EGroundProbeAttempts::RetryFromInsideSolid,
                        ck_procedural_gait::EFootholdReprobe::None, ck_procedural_gait::EFootholdStage::Search, RayCount, nullptr);
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
                    if (HipObservation.Enabled && NOT HipObservation.Tilted)
                    {
                        using namespace ck_procedural_gait_processor_hip_query_test;
                        Put(HipObservation, 23, Candidate.NativeHit);
                        Put(HipObservation, 24, static_cast<uint64>(Candidate.Verdict));
                        if (Candidate.NativeHit)
                        {
                            Vector(HipObservation, 25, Candidate.Position);
                        }
                    }
#endif
                    const auto CandidateIndex = DebugLeg.Get_Footholds().Num();
                    DebugLeg.Get_Footholds().Add(FCk_ProceduralAnimation_DebugFoothold{}
                        .Set_Position(Candidate.Position).Set_Normal(Candidate.Normal)
                        .Set_Source(Candidate.Source).Set_Verdict(Candidate.Verdict));
                    const auto Limit = static_cast<double>(Reach) + 1.0e-3;
                    const auto& Trial = PressureTrial.GetValue();
                    const auto Target = InverseBasis.RotateVector(Candidate.Position);
                    const auto FitsTrial = FVector::Dist(Target, Trial.Get_Hip()) <= Limit
                        && (NOT Trial.Get_PosedHip().IsSet()
                            || FVector::Dist(Target, Trial.Get_PosedHip().GetValue()) <= Limit);
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
                    if (HipObservation.Enabled && NOT HipObservation.Tilted)
                    {
                        ck_procedural_gait_processor_hip_query_test::Put(HipObservation, 28, FitsTrial);
                    }
#endif
                    if (Candidate.Verdict == EProceduralFootholdVerdict::Usable && FitsTrial)
                    {
                        InGaitComp._Footholds[Index] = FProceduralFootholdState{}
                            .Set_Position(Candidate.Position).Set_Normal(Candidate.Normal)
                            .Set_Source(Candidate.Source).Set_Hold(EProceduralFootholdHold::Held).Set_Cast(Candidate.Cast);
                        Foothold = ck_procedural_gait::DoFinish(Candidate, CandidateIndex, Foothold.IdealVerdict, &DebugLeg);
                        Trusted = true;
                        PreserveCurrentPlant = false;
                        DebugLeg.Set_ActualPlantSeedAdmitted(true);
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
                        if (HipObservation.Enabled && NOT HipObservation.Tilted)
                        {
                            ck_procedural_gait_processor_hip_query_test::Put(HipObservation, 29, 1);
                        }
#endif
                    }
                }
            }
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
            if (HipObservation.Enabled && NOT HipObservation.Tilted)
            {
                ck_procedural_gait_processor_hip_query_test::Put(HipObservation, 33, RayCount);
                ck_procedural_gait_processor_hip_query_test::Emit(HipObservation);
            }
            else if (HipObservation.Enabled && HipObservation.Tilted)
            {
                ck_procedural_gait_processor_hip_query_test::Emit(HipObservation);
            }
#endif
            if (CaptureHeight)
            {
                const auto& Trial = Input.Get_ReachPaceTrial();
                const auto Target = InverseBasis.RotateVector(Foothold.Position);
                const auto TargetFitsTrial = Trial.IsSet()
                    && FVector::Dist(Target, Trial->Get_Hip()) <= Reach + 1.0e-3
                    && (NOT Trial->Get_PosedHip().IsSet()
                        || FVector::Dist(Target, Trial->Get_PosedHip().GetValue()) <= Reach + 1.0e-3);
                HeightRequiresRelief[Index] = Foothold.Source == EProceduralFootholdSource::None
                    || (Trial.IsSet() && NOT TargetFitsTrial);
            }
            const auto TargetIsFoothold = Trusted && Foothold.Source != EProceduralFootholdSource::Ideal;
            if (Trusted)
            {
                SelectedLandingReceipts[Index] = ck_procedural_gait::FNativeLandingReceipt{
                    Foothold.Position, Foothold.Normal, Foothold.Cast};
            }
            // Final selection owns the one grace update, including any newly observed occupied terrain.
            const auto ProbeAdvanced = InGaitComp._Probes[Index].Advance(Trusted || RejectedReservation, InDeltaT, Probe.Get_ContactGrace());
            CK_ENSURE_IF_NOT(ProbeAdvanced,
                TEXT("Procedural gait [{}] foot probe of leg [{}] rejected its elapsed time or contact grace; feature is failed."),
                InHandle, Index)
            {
                InHandle.Add<FFragment_ProceduralGait_Failure>(ECk_ProceduralGait_Failure::ProbeAdvance);
                return;
            }
            const auto ProbeState = InGaitComp._Probes[Index].Get_State();
            // A brief miss holds the plant by withholding a target. A prolonged loss lets the leg gather toward its CURRENT
            // rest pose only when no freshly validated stance remains. Explicit catch-step requests keep their existing
            // authority. A rest pose behind a solid is never offered; the fallback never becomes trusted ground.
            const auto GatherWouldCrossASolid = ProbeState == EProceduralFootProbeState::Lost
                && Foothold.IdealVerdict == EProceduralFootholdVerdict::Occluded;
            const auto GatherWouldOverlap = NOT Trusted && (RejectedReservation
                || NOT Get_IsProceduralFootContactAvailable(Ideal, Params.Get_FootContactRadius(), Index, Reservations));
            const auto TargetValid = ProbeState != EProceduralFootProbeState::Guessing
                && NOT GatherWouldCrossASolid && NOT GatherWouldOverlap && NOT PreserveCurrentPlant;
            AllLost &= ProbeState == EProceduralFootProbeState::Lost && NOT PreserveCurrentPlant;
            const auto Position = Trusted ? Foothold.Position : Ideal;
            const auto Normal = Trusted ? Foothold.Normal : Up;
            const auto PlantOccluded = Planted && ck_procedural_gait::Get_IsPlantOccluded(FootholdQuery, RayCount);

            Input.Set_IdealTarget(InverseBasis.RotateVector(Position))
                .Set_GroundNormal(InverseBasis.RotateVector(Normal))
                .Set_FacingDirection(FVector::ForwardVector)
                .Set_TargetValid(TargetValid)
                .Set_ClearanceGroundZ(-FLT_MAX)
                .Set_Hip(HipSupport)
                .Set_Reach(Reach)
                .Set_PlantOccluded(PlantOccluded)
                .Set_PlantCrowded(Planted && NOT Get_IsProceduralFootContactAvailable(
                    LegComp._Foot.Get_Position(), Params.Get_FootContactRadius(), Index, Reservations))
                .Set_TargetIsFoothold(TargetIsFoothold)
                .Set_TargetTrusted(Trusted);

            if (StagingLeg && Input.Get_TargetValid() && Input.Get_TargetTrusted() && Foothold.NativeReceipt.IsSet())
            {
                auto& Receipt = Foothold.NativeReceipt.GetValue();
                const auto& Filter = Probe.Get_QueryFilter();
                const auto ReceiptMatches = Receipt.Owner == InHandle.ConvertToHandle() && Receipt.Frame == GFrameCounter
                    && Receipt.InputSolve == ConsumerInputSolve && Receipt.Body.Equals(Body, 0.0)
                    && Receipt.LegIndex == Index && Receipt.Point == Position && Receipt.Normal == Normal
                    && Receipt.Cast == Foothold.Cast && Receipt.SimulationHip == Hip && Receipt.SimulationVisible
                    && Receipt.Filter.Get_Channel() == Filter.Get_Channel()
                    && Receipt.Filter.Get_MinResponse() == Filter.Get_MinResponse()
                    && Receipt.Filter.Get_BodySelection() == Filter.Get_BodySelection()
                    && Receipt.Filter.Get_CollideWithBackFacesConvex() == Filter.Get_CollideWithBackFacesConvex();
                if (ReceiptMatches && (Staging->_TwoPlacement
                        ? StagingTargetFits(Position, Normal, Receipt.SearchPredicates)
                        : (Receipt.StagingGeometry || StagingTargetFits(Position, Normal, Receipt.SearchPredicates))))
                {
                    const auto PosedHip = HasPose ? PresentationBody.TransformPosition(Placement.Get_HipLocal()) : Hip;
                    auto PosedVisible = PosedHip == Receipt.SimulationHip
                        || (Receipt.VisiblePosedHip.IsSet() && Receipt.VisiblePosedHip.GetValue() == PosedHip);
                    if (NOT PosedVisible)
                    {
                        auto& Support = InHandle.Get<FFragment_SurfaceMotion_Support>();
                        const auto Ticket = Staging->_PoseRayTickets.IndexOfByKey(Leg);
                        if (Ticket != INDEX_NONE && Staging->_ReservedRays > 0
                            && Staging->_ReservedRays == Staging->_PoseRayTickets.Num()
                            && Support._HeightPlanningRays >= 0 && Support._HeightPlanningRays < 2048
                            && Support._HeightPlanningRays + Staging->_ReservedRays <= 2048)
                        {
                            Staging->_PoseRayTickets.RemoveAt(Ticket);
                            --Staging->_ReservedRays;
                            ++Support._HeightPlanningRays;
                            auto PosedQuery = FootholdQuery;
                            PosedQuery.Hip = PosedHip;
                            PosedVisible = NOT ck_procedural_gait::Get_Occluder(PosedQuery, Position, RayCount).IsSet();
                        }
                    }
                    if (PosedVisible)
                    {
                        const auto ExitHip = Staging->_ExitBody.TransformPosition(Placement.Get_HipLocal());
                        const auto ExitPosedBody = HasPose
                            ? UCk_Utils_ProceduralBodyPose_UE::Get_Offset(UCk_Utils_ProceduralBodyPose_UE::Cast(InHandle)) * Staging->_ExitBody
                            : Staging->_ExitBody;
                        auto Intent = FProceduralGaitPlannedReplantIntent{}
                            .Set_Episode(Staging->_Episode).Set_ExpectedPlant(LegState.Get_Plant().Get_Position())
                            .Set_Target(InverseBasis.RotateVector(Position)).Set_ExitHip(InverseBasis.RotateVector(ExitHip))
                            .Set_Frame(GFrameCounter).Set_ConsumerInputSolve(ConsumerInputSolve);
                        if (HasPose)
                        { Intent.Set_ExitPosedHip(TOptional<FVector>{InverseBasis.RotateVector(
                            ExitPosedBody.TransformPosition(Placement.Get_HipLocal()))}); }
                        if (Staging->_TwoPlacement)
                        {
                            const auto& Milestones = StagingFence->_Legs[RequiredIndex];
                            const auto Limit = static_cast<double>(Reach) + 1.0e-3;
                            const auto FinalFits = FVector::Dist(Position, ExitHip) <= Limit
                                && FVector::Dist(Position, ExitPosedBody.TransformPosition(Placement.Get_HipLocal())) <= Limit;
                            const auto SearchRadius = InTunables.Get_Foothold().Get_SearchRadius() > 0.0f
                                ? InTunables.Get_Foothold().Get_SearchRadius() : ck_procedural_gait::DerivedSearchRadiusShareOfReach * Reach;
                            Intent.Set_Milestone(FinalFits ? EProceduralGaitReplantMilestone::Final : EProceduralGaitReplantMilestone::Intermediate)
                                .Set_PlacementSlot(Milestones._StartedCount)
                                .Set_FinalQuery(InverseBasis.RotateVector(Staging->_Required[RequiredIndex]._Query))
                                .Set_Forward(InverseBasis.RotateVector(Staging->_Direction)).Set_SearchRadius(SearchRadius);
                        }
                        Input.Set_PlannedReplantIntent(TOptional<FProceduralGaitPlannedReplantIntent>{Intent});
                    }
                }
            }

            if (LegComp._Foot.Get_Phase() == ECk_ProceduralLeg_FootPhase::Swinging)
            {
                const auto Foot = LegComp._Foot.Get_Position();
                const auto ClearanceHit = UCk_Utils_JoltQuery_UE::Get_RayCast(World,
                    Foot + Up * Probe.Get_Up(), Foot - Up * Probe.Get_Down(), Probe.Get_QueryFilter());
                ++RayCount;

                if (ck_procedural_gait::Get_TrustedHit(ClearanceHit, -Up))
                { Input.Set_ClearanceGroundZ(InverseBasis.RotateVector(ClearanceHit.Get_Position()).Z); }
            }

            const auto LandingGround = ck_procedural_gait::Get_LandingGround(World, Basis, Hip, Reach,
                InGaitComp._Solver.GetLegState(Index).Get_Swing(), Probe, Step, RayCount, DebugLeg);
            Input.Set_LandingGround(LandingGround.Report)
                .Set_LandingGroundZ(static_cast<float>(InverseBasis.RotateVector(LandingGround.Position).Z));
            DebugLeg.Set_LandingGround(LandingGround.Report);

            LegComp._Foot.Set_Foothold(ck_procedural_gait::Get_LegFoothold(Foothold.Source));
            LegComp._IdealVerdict = ck_procedural_gait::Get_LegFootholdVerdict(Foothold.IdealVerdict);
            DebugLeg.Set_PlantOccluded(PlantOccluded);
            DebugLeg.Get_Targeting().Set_IdealTarget(Position)
                .Set_TargetValid(TargetValid);
            DebugLeg.Get_Foot().Set_ContactTrusted(Trusted);
            DebugLeg.Get_Probe().Set_State(ProbeState)
                .Set_MissingContact(InGaitComp._Probes[Index].Get_MissingDuration());
        }
        InDebugComp._RaysLastSolve = RayCount;
        if (LastSearchedLeg != INDEX_NONE)
        { InGaitComp._NextSearchLeg = (LastSearchedLeg + 1) % LegCount; }

        if (EnabledMask != InGaitComp._EnabledMask)
        {
            InGaitComp._EnabledMask = EnabledMask;
            const auto Built = UCk_Utils_ProceduralGait_UE::DoBuild_SolverSettings(InTunables, InGaitComp._Legs, InGaitComp._EnabledMask);
            InGaitComp._Solver.Set_Settings(Built.Get_Settings());
            InDebugComp._ReachCadenceFloor = Built.Get_ReachCadenceFloor();
            InDebugComp._ReachSkippedLegs = Built.Get_ReachSkippedLegs();
            UUtils_Signal_OnProceduralGait_LegSetChanged::Broadcast(InHandle, MakePayload(InHandle, EnabledCount, LegCount));
        }

        auto Airborne = EnabledCount > 0 && AllLost;
        if (MotionAirborne.IsSet())
        { Airborne = MotionAirborne.GetValue(); }

        const auto CadenceSpeed = PlanarVelocity.Size() + FMath::Abs(YawRate) * MeanFootRadius / FMath::Max(EnabledCount, 1);
        auto CadenceDriveSpeed = TOptional<float>{};
        if (NOT Airborne && InHandle.Has<FFragment_SurfaceMotion>() && InHandle.Has<FFragment_SurfaceMotion_Support>())
        {
            const auto MotionHandle = UCk_Utils_SurfaceMotion_UE::Cast(InHandle);
            const auto& Support = InHandle.Get<FFragment_SurfaceMotion_Support>();
            if (UCk_Utils_SurfaceMotion_UE::Get_Status(MotionHandle) == ECk_ProceduralAnimation_Status::Ready
                && Support._EvaluatedFrame == GFrameCounter && Support._EvaluatedBody.Equals(Body, 1.0e-3)
                && Support._State.Get_Grounded() && Support._ReachPaceScale < 1.0f
                && Support._AttemptedStanceSpeed > 0.0f
                && (Support._ReachPaceState == ECk_SurfaceMotion_ReachPaceState::Pacing
                    || Support._ReachPaceState == ECk_SurfaceMotion_ReachPaceState::Blocked))
            {
                CadenceDriveSpeed = static_cast<float>(FMath::Max(CadenceSpeed,
                    static_cast<double>(Support._AttemptedStanceSpeed)));
            }
        }
        if (FreshStagingReplant && NOT Airborne)
        {
            CadenceDriveSpeed = static_cast<float>(FMath::Max(CadenceSpeed, static_cast<double>(Staging->_Speed)));
        }
        for (auto& Lease : InGaitComp._ParticipationLeases)
        {
            if (NOT Lease._Recovering || NOT Lease._LandingProbePending)
            { continue; }
            Lease._LatestInputs.SetNum(Lease._LegIndices.Num());
            auto AllPrepared = true;
            for (auto Entry = 0; Entry < Lease._LegIndices.Num(); ++Entry)
            {
                const auto Index = Lease._LegIndices[Entry];
                Lease._LatestInputs[Entry] = Inputs[Index];
                AllPrepared &= Inputs[Index].Get_TargetValid() && Inputs[Index].Get_TargetTrusted();
            }
            if (AllPrepared)
            {
                for (const auto Index : Lease._LegIndices)
                {
                    Lease._LandingWorld.Add(Basis.RotateVector(Inputs[Index].Get_IdealTarget()));
                    Lease._LandingNormalWorld.Add(Basis.RotateVector(Inputs[Index].Get_GroundNormal()));
                }
                Lease._PreparedSolveSequence = InGaitComp._SolveSequence;
                ++Lease._RecoveryGeneration;
            }
            Lease._LandingProbePending = false;
        }
        // A touchdown certifies the plant only at that instant. While a limb is borrowed, support protection must use
        // current contact at each remaining plant; otherwise removing ground leaves the old trusted bit latched and an
        // action can continue until its input timeout. Probe along the plant's own normal so wall and sloped holds use
        // their actual contact frame. The ordinary gait path retains its existing touchdown semantics and solve cost.
        if (InGaitComp._ParticipationLeases.ContainsByPredicate([](const auto& InLease)
            { return InLease._ProtectionActive; }))
        {
            const auto RaysBeforeSupport = RayCount;
            for (auto Index = 0; Index < LegCount; ++Index)
            {
                if (NOT Inputs[Index].Get_Enabled() || NOT Inputs[Index].Get_Participating())
                { continue; }
                const auto& Plant = InGaitComp._Solver.GetLegState(Index).Get_Plant();
                if (InGaitComp._Solver.GetLegState(Index).Get_Swing().Get_Active()
                    || NOT Plant.Get_Trusted() || (RevalidatedPlants & ck_procedural_gait::Get_LegBit(Index)) != 0)
                { continue; }
                const auto PlantWorld = Basis.RotateVector(Plant.Get_Position());
                const auto PlantNormalWorld = Basis.RotateVector(Plant.Get_Normal()).GetSafeNormal();
                const auto Hit = ck_procedural_gait::Get_GroundHit(World, PlantWorld, PlantNormalWorld,
                    FVector::ZeroVector, Probe, ck_procedural_gait::EGroundProbeAttempts::RetryFromInsideSolid,
                    RayCount, nullptr);
                if (Hit.IsSet() && FVector::Dist(Hit->Get_Position(), PlantWorld)
                    <= InTunables.Get_Foothold().Get_OcclusionTolerance())
                { continue; }
                InGaitComp._Solver.SetPlantedPose(Index, Plant.Get_Position(), Plant.Get_Normal(), false);
            }
            InDebugComp._RaysLastSolve += RayCount - RaysBeforeSupport;
        }
        auto Solved = false;
        auto DeniedTakeoffs = TArray<FProceduralGaitDeniedTakeoff>{};
        auto RecoveryTouchdownRays = int32{0};
        const auto ResolveNativeRecovery = [&](const FProceduralGaitSolver& InSolver, int32 InIndex,
            const FVector& InProposed, const FVector& InValidated, const FVector& InNormal)
            -> TOptional<FProceduralGaitNativeTouchdown>
        {
            if (InHandle.Has<FFragment_ProceduralBodyPose>() && NOT HasPose)
            { return {}; }
            const auto& Leg = InGaitComp._Legs[InIndex];
            const auto HipLocal = Leg.Get<FFragment_ProceduralLeg_Params>().Get_Placement().Get_HipLocal();
            const auto PosedHip = HasPose ? TOptional<FVector>{PresentationBody.TransformPosition(HipLocal)} : TOptional<FVector>{};
            const auto CurrentReservations = ck_procedural_gait::Get_FootReservations(InGaitComp._Legs, InSolver, Basis);
            const auto Touchdown = ck_procedural_gait::Get_Touchdown(World, Basis.RotateVector(InProposed),
                Basis.RotateVector(InValidated), Basis.RotateVector(InNormal), Up, Probe,
                Body.TransformPosition(HipLocal), PosedHip, Inputs[InIndex].Get_Reach(),
                Inputs[InIndex].Get_FootContactRadius(), InIndex, CurrentReservations);
            RecoveryTouchdownRays += Touchdown.Get_Rays();
            if (NOT Touchdown.Get_Trusted())
            { return {}; }
            return FProceduralGaitNativeTouchdown{InverseBasis.RotateVector(Touchdown.Get_Position()),
                InverseBasis.RotateVector(Touchdown.Get_Normal()).GetSafeNormal(), true};
        };
        const auto NativeResolver = TOptional<FProceduralGaitNativeRecoveryResolver>{ResolveNativeRecovery};
        if (InGaitComp._ParticipationLeases.IsEmpty())
        {
            Solved = InGaitComp._Solver.Step(InDeltaT, CadenceSpeed, PlanarVelocity, Inputs, Outputs,
                Airborne, CadenceDriveSpeed, NativeResolver);
        }
        else
        {
            auto Snapshot = FCk_ProceduralGaitParticipation_Snapshot{};
            const auto Fresh = FCk_ProceduralGaitParticipation_Control::GetSnapshot(InHandle, Snapshot)
                == ECk_ProceduralGaitParticipation_Result::Accepted;
            Solved = InGaitComp._Solver.StepWithTakeoffGuard(InDeltaT, CadenceSpeed, PlanarVelocity, Inputs, Outputs,
                Airborne, CadenceDriveSpeed,
                [InHandle, &Snapshot, Fresh](const FProceduralGaitSolver& InSolver, int32 InIndex, bool)
                { return Fresh && FCk_ProceduralGaitParticipation_Control::CanTakeoff(InHandle, InSolver, InIndex, Snapshot); },
                DeniedTakeoffs, NativeResolver);
        }
        CK_ENSURE_IF_NOT(Solved, TEXT("Procedural gait [{}] solver rejected runtime inputs; feature is failed, planted state retained."), InHandle)
        {
            InHandle.Add<FFragment_ProceduralGait_Failure>(ECk_ProceduralGait_Failure::SolverStep);
            return;
        }
        const auto ExhaustTwoPlacement = [&]()
        {
            auto* Current = GetCurrentStaging();
            if (Current != nullptr && Current->_TwoPlacement)
            {
                if (StagingFence != nullptr && StagingFence->_Spent && NOT StagingFence->_AbortRecorded
                    && StagingFence->_Episode == Current->_Episode)
                {
                    StagingFence->_AbortRecorded = true;
                    procedural_animation::Log(TEXT("[ContactStaging] event[abort] owner[{}] episode[{}] frame[{}] phase[{}] elapsed[{}] cycles[{}]"),
                        InHandle, Current->_Episode, GFrameCounter, static_cast<int32>(Current->_Phase),
                        StagingFence->_Elapsed, StagingFence->_Cycles);
                }
                Current->_Phase = EProceduralContactStagingPhase::Exhausted;
                Current->_IntentFrame = 0;
                Current->_PoseRayTickets.Reset();
                Current->_ReservedRays = 0;
                Current->_PendingBody.Reset();
            }
        };
        // The solver records every guarded begin-swing transition, including a same-solve native touchdown.
        auto* StartedStaging = GetCurrentStaging();
        if (StartedStaging != nullptr && StartedStaging->_TwoPlacement
            && StartedStaging->_Phase != EProceduralContactStagingPhase::Dormant
            && StartedStaging->_Phase != EProceduralContactStagingPhase::Exhausted)
        {
            auto ClaimsRevision = TOptional<uint64>{};
            const auto ValidFence = StagingFence != nullptr && StagingFence->_Spent
                && StagingFence->_Owner == InHandle.ConvertToHandle() && StagingFence->_World.Get() == World
                && StagingFence->_Episode == StartedStaging->_Episode
                && StartedStaging->_Required.Num() > 0 && StartedStaging->_Required.Num() <= 2
                && StagingFence->_RequiredCount == StartedStaging->_Required.Num()
                && ck_procedural_gait::Get_LandingClaimsRevision(InHandle, ClaimsRevision);
            if (NOT ValidFence)
            { ExhaustTwoPlacement(); }
            else
            {
                for (auto RequiredIndex = 0; RequiredIndex < StartedStaging->_Required.Num(); ++RequiredIndex)
                {
                    const auto& Required = StartedStaging->_Required[RequiredIndex];
                    if (NOT Inputs.IsValidIndex(Required._Index) || Required._Index >= InGaitComp._Solver.NumLegs())
                    { ExhaustTwoPlacement(); break; }
                    const auto& State = InGaitComp._Solver.GetLegState(Required._Index);
                    const auto Starts = State.Get_StartsThisStep();
                    if (Starts == 0)
                    { continue; }
                    const auto& Input = Inputs[Required._Index];
                    auto& Milestones = StagingFence->_Legs[RequiredIndex];
                    if (Starts != 1 || NOT FreshStagingReplant || NOT Input.Get_PlannedReplantIntent().IsSet()
                        || Milestones._StartedCount >= 2 || Milestones._Next == EProceduralContactStagingMilestone::Complete)
                    { ExhaustTwoPlacement(); break; }
                    const auto& Intent = Input.Get_PlannedReplantIntent().GetValue();
                    auto& Placement = Milestones._Placements[Milestones._StartedCount];
                    const auto IsFinal = Intent.Get_Milestone() == EProceduralGaitReplantMilestone::Final;
                    if (Placement._Started || Intent.Get_Episode() != StartedStaging->_Episode
                        || Intent.Get_Frame() != GFrameCounter || Intent.Get_ConsumerInputSolve() != ConsumerInputSolve
                        || Intent.Get_PlacementSlot() != Milestones._StartedCount
                        || State.Get_LastStartedTarget() != Intent.Get_Target()
                        || (NOT IsFinal && (Intent.Get_Milestone() != EProceduralGaitReplantMilestone::Intermediate
                            || Milestones._Next != EProceduralContactStagingMilestone::Intermediate || Milestones._StartedCount != 0)))
                    { ExhaustTwoPlacement(); break; }
                    Placement._Started = true;
                    Placement._Final = IsFinal;
                    Placement._Frame = GFrameCounter;
                    Placement._InputSolve = ConsumerInputSolve;
                    Placement._Body = Body;
                    Placement._Target = Basis.RotateVector(State.Get_LastStartedTarget());
                    Placement._ClaimsRevision = ClaimsRevision;
                    ++Milestones._StartedCount;
                    procedural_animation::Log(TEXT("[ContactStaging] event[start] owner[{}] episode[{}] leg[{}] slot[{}] final[{}] frame[{}] inputSolve[{}] target[{:.17g},{:.17g},{:.17g}]"),
                        InHandle, StartedStaging->_Episode, Required._Index, Intent.Get_PlacementSlot(), IsFinal,
                        GFrameCounter, ConsumerInputSolve, Placement._Target.X, Placement._Target.Y, Placement._Target.Z);
                    StagingStarted |= static_cast<uint8>(uint8{1} << RequiredIndex);
                }
            }
        }
        for (auto& Lease : InGaitComp._ParticipationLeases)
        {
            if (NOT Lease._Recovering || Lease._HasPreparedTrajectory || Lease._LandingProbePending
                || Lease._LandingWorld.Num() != Lease._LimbIds.Num()
                || Lease._LandingNormalWorld.Num() != Lease._LimbIds.Num()
                || Lease._LatestInputs.Num() != Lease._LimbIds.Num())
            { continue; }
            Lease._RecoverySettings = InGaitComp._Solver.Get_Settings();
            Lease._RecoveryBasis = Basis;
            Lease._RecoveryCadenceScale = InGaitComp._Solver.Get_LastCadenceScale();
            Lease._HasPreparedTrajectory = true;
        }
        if (InDeltaT > FCk_Time{})
        {
            auto SupportSnapshot = FCk_ProceduralGaitParticipation_Snapshot{};
            const auto SupportReady = InGaitComp._ParticipationLeases.IsEmpty()
                || FCk_ProceduralGaitParticipation_Control::GetSnapshot(InHandle, SupportSnapshot)
                    == ECk_ProceduralGaitParticipation_Result::Accepted;
            const auto SupportsRemain = SupportReady && (InGaitComp._ParticipationLeases.IsEmpty()
                || FCk_ProceduralGaitParticipation_Control::CanTakeoff(
                    InHandle, InGaitComp._Solver, INDEX_NONE, SupportSnapshot));
            for (auto& Lease : InGaitComp._ParticipationLeases)
            {
                if (NOT Lease._ProtectionActive)
                { continue; }
                const auto Denied = DeniedTakeoffs.ContainsByPredicate([&Lease](const auto& InDenial)
                    { return NOT Lease._LegIndices.Contains(InDenial.Get_LegIndex()); });
                Lease._SupportHold = (Denied || NOT SupportsRemain) ? Lease._SupportHold + InDeltaT : FCk_Time{};
                if (Lease._SupportHold >= Lease._SupportPolicy.Get_SupportHoldTimeout())
                {
                    Lease._Interrupted = true;
                    if (Lease._Reason.IsNone())
                    { Lease._Reason = TEXT("SupportHoldTimeout"); }
                }
            }
        }

        auto TouchdownRays = RecoveryTouchdownRays;
        for (auto Index = 0; Index < LegCount; ++Index)
        {
            if (NOT Inputs[Index].Get_Enabled() || NOT Inputs[Index].Get_Participating())
            { continue; }

            const auto& Output = Outputs[Index];
            auto& Leg = InGaitComp._Legs[Index];
            auto& LegComp = Leg.Get<FFragment_ProceduralLeg>();
            const auto PreviousPhase = LegComp._Foot.Get_Phase();
            const auto PreviousPosition = LegComp._Foot.Get_Position();

            // The ground under a touchdown decides the plant's trust and normal; the landing report describes the landing
            // point of the solve before.
            if (Output.Get_Planted() && PreviousPhase == ECk_ProceduralLeg_FootPhase::Swinging
                && NOT Output.Get_NativeTouchdownResolved())
            {
                const auto& Landed = InGaitComp._Solver.GetLegState(Index);
                const auto HipLocal = Leg.Get<FFragment_ProceduralLeg_Params>().Get_Placement().Get_HipLocal();
                const auto PresentedHip = HasPose
                    ? TOptional<FVector>{PresentationBody.TransformPosition(HipLocal)} : TOptional<FVector>{};
                // Include earlier native touchdown corrections; test each hit before the resolver chooses a fallback.
                const auto TouchdownReservations = ck_procedural_gait::Get_FootReservations(
                    InGaitComp._Legs, InGaitComp._Solver, Basis);
                const auto Touchdown = ck_procedural_gait::Get_Touchdown(World, Basis.RotateVector(Landed.Get_Plant().Get_Position()),
                    Basis.RotateVector(Landed.Get_Swing().Get_ValidatedTarget()), Basis.RotateVector(Landed.Get_Swing().Get_TargetNormal()),
                    Up, Probe, Body.TransformPosition(HipLocal), PresentedHip, Inputs[Index].Get_Reach(),
                    Inputs[Index].Get_FootContactRadius(), Index, TouchdownReservations);
                TouchdownRays += Touchdown.Get_Rays();
                InGaitComp._Solver.SetPlantedPose(Index, InverseBasis.RotateVector(Touchdown.Get_Position()),
                    InverseBasis.RotateVector(Touchdown.Get_Normal()).GetSafeNormal(), Touchdown.Get_Trusted());
            }

            // A planted foot reports the ground it touched down on; a swinging one, the target this solve found.
            const auto& Plant = InGaitComp._Solver.GetLegState(Index).Get_Plant();
            const auto Waiting = InGaitComp._Solver.GetLegState(Index).Get_AutomaticRecovery()
                == EProceduralGaitAutomaticRecovery::WaitingForContact;
            const auto ContactTrusted = NOT Waiting
                && (Output.Get_Planted() ? Plant.Get_Trusted() : Inputs[Index].Get_TargetTrusted());
            const auto* ContinuingPlant = ContinuingWorldPlants[Index].IsSet()
                ? &ContinuingWorldPlants[Index].GetValue() : nullptr;
            const auto KeepWorldPlant = ContinuingPlant != nullptr && Output.Get_Planted() && ContactTrusted
                && NOT Output.Get_NativeTouchdownAttempted() && NOT Output.Get_NativeTouchdownResolved()
                && ContinuingPlant->Leg == Leg && HasCurrentWorldPlant(Leg, Index)
                && LegComp._Foot.Get_Position() == ContinuingPlant->WorldPosition
                && LegComp._Foot.Get_Normal() == ContinuingPlant->WorldNormal
                && Plant.Get_Position() == ContinuingPlant->SupportPosition
                && Plant.Get_Normal() == ContinuingPlant->SupportNormal
                && Plant.Get_Trusted() == ContinuingPlant->Trusted;
            LegComp._Foot
                .Set_Position(KeepWorldPlant ? ContinuingPlant->WorldPosition
                    : Basis.RotateVector(Output.Get_Planted() ? Plant.Get_Position() : Output.Get_Position()))
                .Set_Normal(KeepWorldPlant ? ContinuingPlant->WorldNormal
                    : Basis.RotateVector(Output.Get_Planted() ? Plant.Get_Normal() : Output.Get_Normal()))
                .Set_Rotation((Basis * (Output.Get_Planted() ? Plant.Get_Rotation() : Output.Get_Rotation())).GetNormalized())
                .Set_SwingAlpha(Output.Get_SwingAlpha())
                .Set_Phase(Output.Get_Planted() ? ECk_ProceduralLeg_FootPhase::Planted : ECk_ProceduralLeg_FootPhase::Swinging)
                .Set_Contact(ContactTrusted ? ECk_ProceduralLeg_FootContact::Trusted : ECk_ProceduralLeg_FootContact::Guessed);
            InDebugComp._ScratchLegs[Index].Get_Foot().Set_ContactTrusted(ContactTrusted);

            auto* CompletionStaging = FreshStagingReplant ? GetCurrentStaging() : nullptr;
            if (CompletionStaging != nullptr && Inputs[Index].Get_NativeRecoveryEnabled()
                && Inputs[Index].Get_Enabled() && Inputs[Index].Get_Participating())
            {
                const auto Required = CompletionStaging->_Required.IndexOfByPredicate([&](const auto& InRequired)
                    { return InRequired._Leg == Leg && InRequired._Index == Index; });
                if (Required != INDEX_NONE)
                {
                    if (NOT CompletionStaging->_TwoPlacement && (InGaitComp._Solver.GetLegState(Index).Get_Swing().Get_Active()
                        || (Output.Get_Planted() && PreviousPhase == ECk_ProceduralLeg_FootPhase::Swinging))
                        )
                    { StagingStarted |= static_cast<uint8>(uint8{1} << Required); }
                    const auto CompletedTransition = PreviousPhase == ECk_ProceduralLeg_FootPhase::Swinging
                        || (CompletionStaging->_TwoPlacement && InGaitComp._Solver.GetLegState(Index).Get_StartsThisStep() > 0);
                    if (Output.Get_Planted() && ContactTrusted && CompletedTransition)
                    {
                        const auto HipLocal = Leg.Get<FFragment_ProceduralLeg_Params>().Get_Placement().Get_HipLocal();
                        const auto ExitHip = CompletionStaging->_ExitBody.TransformPosition(HipLocal);
                        const auto ExitPosedBody = HasPose
                            ? UCk_Utils_ProceduralBodyPose_UE::Get_Offset(UCk_Utils_ProceduralBodyPose_UE::Cast(InHandle)) * CompletionStaging->_ExitBody
                            : CompletionStaging->_ExitBody;
                        const auto Point = LegComp._Foot.Get_Position();
                        const auto Limit = static_cast<double>(Inputs[Index].Get_Reach()) + 1.0e-3;
                        auto Completed = FVector::Dist(Point, ExitHip) <= Limit
                            && FVector::Dist(Point, ExitPosedBody.TransformPosition(HipLocal)) <= Limit;
                        if (CompletionStaging->_TwoPlacement)
                        {
                            Completed = false;
                            if (StagingFence != nullptr && StagingFence->_Spent && StagingFence->_Episode == CompletionStaging->_Episode
                                && StagingFence->_RequiredCount == CompletionStaging->_Required.Num())
                            {
                                auto& Milestones = StagingFence->_Legs[Required];
                                if (Milestones._StartedCount > 0 && Milestones._StartedCount <= 2)
                                {
                                    const auto& Placement = Milestones._Placements[Milestones._StartedCount - 1];
                                    auto ClaimsRevision = TOptional<uint64>{};
                                    const auto& RequiredContact = CompletionStaging->_Required[Required];
                                    const auto SearchRadius = InTunables.Get_Foothold().Get_SearchRadius() > 0.0f
                                        ? InTunables.Get_Foothold().Get_SearchRadius()
                                        : ck_procedural_gait::DerivedSearchRadiusShareOfReach * Inputs[Index].Get_Reach();
                                    const auto GeometryFits = Placement._Final
                                        ? FVector::Dist(Point, ExitHip) <= Limit
                                            && FVector::Dist(Point, ExitPosedBody.TransformPosition(HipLocal)) <= Limit
                                        : Get_IsProceduralContactStagingIntermediateTargetValid(Point, Milestones._ActualPlant,
                                            RequiredContact._Query, CompletionStaging->_Direction, SearchRadius);
                                    Completed = Placement._Started && NOT Placement._Completed
                                        && Placement._Frame <= GFrameCounter && Placement._InputSolve <= ConsumerInputSolve
                                        && Output.Get_NativeTouchdownAttempted() && Output.Get_NativeTouchdownResolved()
                                        && LegComp._Foot.Get_Normal() == CompletionStaging->_Up && GeometryFits
                                        && FVector::Dist(Point, Milestones._ActualPlant) <= SearchRadius
                                        && ck_procedural_gait::Get_LandingClaimsRevision(InHandle, ClaimsRevision)
                                        && ClaimsRevision == Placement._ClaimsRevision;
                                }
                            }
                            if (NOT Completed)
                            { ExhaustTwoPlacement(); }
                        }
                        if (Completed)
                        {
                            StagingCompletions |= static_cast<uint8>(uint8{1} << Required);
                            StagingCompletionReceipts[Required] = ck_procedural_gait::FPlantSeedPlane{
                                Point, LegComp._Foot.Get_Normal()};
                        }
                    }
                }
            }

            ck_procedural_gait::DoPublish_FootPhaseChange(Leg, LegComp, PreviousPhase, PreviousPosition, Dt);
        }
        InDebugComp._RaysLastSolve += TouchdownRays;

        DoUpdate_FeetPlane(InHandle, InDeltaT, InTunables, InGaitComp, Inputs, Body, InFeetPlaneComp);

        if (InDeltaT > FCk_Time{})
        {
            ++InGaitComp._SolveSequence;
            auto& Stance = InGaitComp._ReachStance;
            Stance._Anchors.Reset();
            Stance._BodyAtSolve = Body;
            Stance._SolveSequence = InGaitComp._SolveSequence;
            Stance._HasSample = true;
            auto& LandingPlanning = InGaitComp._LandingPlanning;
            auto ClaimsRevision = TOptional<uint64>{};
            const auto CanPublishLandings = NativeMotionObservation && MotionAirborne.IsSet() && NOT MotionAirborne.GetValue()
                && NOT Airborne
                && (NOT InHandle.Has<FFragment_ProceduralBodyPose>() || HasPose)
                && ck_procedural_gait::Get_LandingClaimsRevision(InHandle, ClaimsRevision);
            LandingPlanning._BodyAtSolve = Body;
            LandingPlanning._SolveSequence = InGaitComp._SolveSequence;
            LandingPlanning._Frame = GFrameCounter;
            LandingPlanning._ClaimsRevision = ClaimsRevision;
            LandingPlanning._HasSample = CanPublishLandings;
            const auto LandingReservations = ck_procedural_gait::Get_FootReservations(InGaitComp._Legs, InGaitComp._Solver, Basis);
            for (auto Index = 0; CanPublishLandings && Index < LegCount; ++Index)
            {
                const auto& Leg = InGaitComp._Legs[Index];
                if (NOT ck_procedural_gait::Get_IsLandingLegEligible(InHandle, Leg, InGaitComp._Solver, Index))
                { continue; }
                const auto& Swing = InGaitComp._Solver.GetLegState(Index).Get_Swing();
                const auto Point = Basis.RotateVector(Swing.Get_CommittedLandingPoint());
                const auto Normal = Basis.RotateVector(Swing.Get_TargetNormal());
                const auto& Params = Leg.Get<FFragment_ProceduralLeg_Params>();
                const auto Matches = [&](const auto& InReceipt) -> bool
                {
                    return InReceipt.IsSet() && InReceipt->Point.Equals(Point, 1.0e-3)
                        && InReceipt->Normal.Equals(Normal, 1.0e-3);
                };
                const auto& Receipt = Matches(SelectedLandingReceipts[Index])
                    ? SelectedLandingReceipts[Index] : MatchingLandingReceipts[Index];
                if (NOT Matches(Receipt) || NOT Get_IsProceduralFootContactAvailable(
                        Point, Params.Get_FootContactRadius(), Index, LandingReservations))
                { continue; }
                const auto Fraction = Receipt->Cast == EProceduralFootholdCast::Face
                    ? Step.Get_TargetReachFraction() : Step.Get_ForceStepReachFraction();
                const auto Constraint = FProceduralSurfaceLandingConstraint{Point, Params.Get_Placement().Get_HipLocal(),
                    Fraction * Inputs[Index].Get_Reach(), Inputs[Index].Get_Reach()};
                const auto PoseOffset = HasPose ? TOptional<FTransform>{
                    UCk_Utils_ProceduralBodyPose_UE::Get_Offset(UCk_Utils_ProceduralBodyPose_UE::Cast(InHandle))} : TOptional<FTransform>{};
                if (Get_IsProceduralSurfaceLandingReachValid(Body, PoseOffset, Constraint))
                { LandingPlanning._Landings.Emplace(Leg, Constraint, Normal, Receipt->Cast); }
            }
            for (auto Index = 0; Index < LegCount; ++Index)
            {
                const auto& Leg = InGaitComp._Legs[Index];
                if (NOT Inputs[Index].Get_Enabled() || NOT Inputs[Index].Get_Participating()
                    || NOT Outputs[Index].Get_Planted() || NOT procedural_leg::Get_IsSupporting(Leg))
                { continue; }

                const auto& Foot = Leg.Get<FFragment_ProceduralLeg>()._Foot;
                if (Foot.Get_Contact() != ECk_ProceduralLeg_FootContact::Trusted)
                { continue; }

                Stance._Anchors.Add(FProceduralGaitReachAnchor{Leg, Foot.Get_Position(),
                    Leg.Get<FFragment_ProceduralLeg_Params>().Get_Placement().Get_HipLocal(), Inputs[Index].Get_Reach()});
            }

            if (CaptureHeight && InHandle.Has<FFragment_SurfaceMotion>())
            {
                auto& Planning = InGaitComp._HeightPlanning;
                Planning._BodyAtSolve = Body;
                Planning._SolveSequence = InGaitComp._SolveSequence;
                Planning._Frame = GFrameCounter;
                Planning._Direction = InHandle.Get<FFragment_SurfaceMotion>()._Direction;
                Planning._Speed = InHandle.Get<FFragment_SurfaceMotion>()._Speed;
                Planning._HasSample = true;
                for (auto Index = 0; Index < LegCount; ++Index)
                {
                    const auto& Leg = InGaitComp._Legs[Index];
                    if (ck_procedural_gait::Get_IsLegEnabled(Leg))
                    { Planning._EnabledMask |= ck_procedural_gait::Get_LegBit(Index); }
                    if (NOT procedural_leg::Get_IsSupporting(Leg))
                    { continue; }
                    Planning._SupportingMask |= ck_procedural_gait::Get_LegBit(Index);
                    if (UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(Leg) != InHandle.ConvertToHandle())
                    { continue; }
                    const auto& Params = Leg.Get<FFragment_ProceduralLeg_Params>();
                    const auto& Foot = Leg.Get<FFragment_ProceduralLeg>().Get_Foot();
                    for (const auto& Observation : HeightObservations[Index])
                    {
                        const auto Candidate = FProceduralGaitHeightCandidate{Leg, Params.Get_Placement().Get_HipLocal(),
                            Foot.Get_Position(), Observation.Point, Observation.Normal, Inputs[Index].Get_Reach(),
                            Step.Get_ForceStepReachFraction() * Inputs[Index].Get_Reach(), HeightRequiresRelief[Index]};
                        const auto Distance = FVector::VectorPlaneProject(Observation.Point
                            - Body.TransformPosition(Candidate.Get_HipLocal()), Up).SizeSquared();
                        const auto Insert = Planning._Candidates.IndexOfByPredicate([&](const auto& InExisting)
                        {
                            if (Candidate.Get_RequiresRelief() != InExisting.Get_RequiresRelief())
                            { return Candidate.Get_RequiresRelief(); }
                            return Distance < FVector::VectorPlaneProject(InExisting.Get_PointWorld()
                                - Body.TransformPosition(InExisting.Get_HipLocal()), Up).SizeSquared();
                        });
                        if (Insert != INDEX_NONE)
                        { Planning._Candidates.Insert(Candidate, Insert); }
                        else if (Planning._Candidates.Num() < 2 * LegCount)
                        { Planning._Candidates.Add(Candidate); }
                        if (Planning._Candidates.Num() > 2 * LegCount)
                        { Planning._Candidates.SetNum(2 * LegCount); }
                    }
                }
            }

            auto* PublishedStaging = GetCurrentStaging();
            if (PublishedStaging != nullptr && PublishedStaging->_Phase != EProceduralContactStagingPhase::Dormant
                && PublishedStaging->_Phase != EProceduralContactStagingPhase::Exhausted
                && ConsumerInputSolve != MAX_uint64
                && InHandle.Get<FFragment_ProceduralGait>()._SolveSequence == ConsumerInputSolve + 1
                && PublishedStaging->_IntentBody.Equals(Body, 1.0e-3))
            {
                auto& PublishedGait = InHandle.Get<FFragment_ProceduralGait>();
                PublishedStaging->_ProducedFrame = GFrameCounter;
                PublishedStaging->_ProducedSolve = PublishedGait._SolveSequence;
                PublishedStaging->_ProducedBody = Body;
                if (FreshStagingReplant)
                {
                    for (auto Required = 0; Required < PublishedStaging->_Required.Num(); ++Required)
                    {
                        const auto Bit = static_cast<uint8>(uint8{1} << Required);
                        if ((StagingCompletions & Bit) == 0)
                        { continue; }
                        const auto& Leg = PublishedStaging->_Required[Required]._Leg;
                        if (ck::Is_NOT_Valid(Leg) || NOT Leg.Has<FFragment_ProceduralLeg>()
                            || NOT Leg.Has<FFragment_ProceduralLeg_Params>() || NOT Leg.Has<FFragment_LifetimeOwner>()
                            || NOT procedural_leg::Get_IsSupporting(Leg)
                            || UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(Leg) != InHandle.ConvertToHandle())
                        { continue; }
                        const auto& Foot = Leg.Get<FFragment_ProceduralLeg>().Get_Foot();
                        const auto& Receipt = StagingCompletionReceipts[Required];
                        if (Receipt.IsSet() && Foot.Get_Phase() == ECk_ProceduralLeg_FootPhase::Planted
                            && Foot.Get_Contact() == ECk_ProceduralLeg_FootContact::Trusted
                            && Foot.Get_Position() == Receipt->Point && Foot.Get_Normal() == Receipt->Normal)
                        {
                            if (PublishedStaging->_TwoPlacement)
                            {
                                auto& Milestones = StagingFence->_Legs[Required];
                                if (Milestones._StartedCount == 0 || Milestones._StartedCount > 2)
                                { ExhaustTwoPlacement(); break; }
                                auto& Placement = Milestones._Placements[Milestones._StartedCount - 1];
                                auto PublicationClaimsRevision = TOptional<uint64>{};
                                if (NOT Placement._Started || Placement._Completed
                                    || NOT ck_procedural_gait::Get_LandingClaimsRevision(InHandle, PublicationClaimsRevision)
                                    || PublicationClaimsRevision != Placement._ClaimsRevision)
                                { ExhaustTwoPlacement(); break; }
                                Placement._Completed = true;
                                Placement._CompletedFrame = GFrameCounter;
                                Placement._CompletedSolve = PublishedGait._SolveSequence;
                                Placement._Plant = Foot.Get_Position();
                                Placement._Normal = Foot.Get_Normal();
                                Milestones._ActualPlant = Foot.Get_Position();
                                Milestones._Next = Placement._Final ? EProceduralContactStagingMilestone::Complete
                                    : EProceduralContactStagingMilestone::Final;
                                procedural_animation::Log(TEXT("[ContactStaging] event[touchdown] owner[{}] episode[{}] leg[{}] slot[{}] final[{}] frame[{}] producedSolve[{}] native[1,1] plant[{:.17g},{:.17g},{:.17g}]"),
                                    InHandle, PublishedStaging->_Episode, PublishedStaging->_Required[Required]._Index,
                                    Milestones._StartedCount - 1, Placement._Final, GFrameCounter, PublishedGait._SolveSequence,
                                    Placement._Plant.X, Placement._Plant.Y, Placement._Plant.Z);
                                if (Placement._Final)
                                { PublishedStaging->_LandedMask |= Bit; }
                            }
                            else
                            { PublishedStaging->_LandedMask |= Bit; }
                        }
                    }
                    const auto NewStarts = PublishedStaging->_TwoPlacement ? StagingStarted
                        : StagingStarted & static_cast<uint8>(~PublishedStaging->_StartedMask);
                    PublishedStaging->_StartedMask |= StagingStarted;
                    const auto Clock = PublishedGait._Solver.GetGaitClock();
                    const auto Advance = PublishedGait._Solver.GetLastClockAdvance();
                    if (NewStarts != 0)
                    {
                        PublishedStaging->_ClockTravelWithoutProgress = 0.0;
                    }
                    else
                    { PublishedStaging->_ClockTravelWithoutProgress += Advance; }
                    const auto AllRequired = static_cast<uint8>((uint8{1} << PublishedStaging->_Required.Num()) - 1);
                    auto HasUnstarted = ((PublishedStaging->_StartedMask | PublishedStaging->_LandedMask) & AllRequired) != AllRequired;
                    if (PublishedStaging->_TwoPlacement)
                    {
                        HasUnstarted = false;
                        for (auto Required = 0; Required < PublishedStaging->_Required.Num(); ++Required)
                        {
                            const auto& Milestones = StagingFence->_Legs[Required];
                            if (Milestones._StartedCount > 2)
                            { ExhaustTwoPlacement(); break; }
                            HasUnstarted |= Milestones._Next != EProceduralContactStagingMilestone::Complete
                                && (Milestones._StartedCount == 0
                                    || Milestones._Placements[Milestones._StartedCount - 1]._Completed);
                        }
                    }
                    const auto ValidClock = FMath::IsFinite(Clock) && Clock >= 0.0f && Clock < 1.0f
                        && FMath::IsFinite(Advance) && Advance >= 0.0
                        && FMath::IsFinite(PublishedStaging->_ClockTravelWithoutProgress)
                        && PublishedStaging->_ClockTravelWithoutProgress >= 0.0;
                    if (NOT ValidClock || (HasUnstarted && (PublishedStaging->_ClockTravelWithoutProgress >= 1.0
                        || (NewStarts == 0 && Advance <= 0.0))))
                    {
                        PublishedStaging->_Phase = EProceduralContactStagingPhase::Exhausted;
                        PublishedStaging->_IntentFrame = 0;
                        PublishedStaging->_PoseRayTickets.Reset();
                        PublishedStaging->_ReservedRays = 0;
                    }
                }
                if (PublishedStaging->_TwoPlacement)
                {
                    const auto Advance = PublishedGait._Solver.GetLastClockAdvance();
                    if (StagingFence == nullptr || NOT StagingFence->_Spent
                        || StagingFence->_Episode != PublishedStaging->_Episode
                        || NOT StagingFence->TryAcknowledge(GFrameCounter, ConsumerInputSolve, PublishedGait._SolveSequence, Advance))
                    { ExhaustTwoPlacement(); }
                }
            }

            const auto PublishedReservations = ck_procedural_gait::Get_FootReservations(InGaitComp._Legs, InGaitComp._Solver, Basis);
            for (auto Index = 0; Index < LegCount; ++Index)
            {
                const auto& Output = Outputs[Index];
                const auto& State = InGaitComp._Solver.GetLegState(Index);
                const auto& Leg = InGaitComp._Legs[Index];
                const auto PublishedFoot = procedural_leg::Get_IsLive(Leg) && Leg.Has<FFragment_ProceduralLeg>()
                    ? TOptional<FCk_ProceduralLeg_Foot>{Leg.Get<FFragment_ProceduralLeg>().Get_Foot()}
                    : TOptional<FCk_ProceduralLeg_Foot>{};
                InDebugComp._ScratchLegs[Index].Get_Foot().Set_PlantedPosition(Basis.RotateVector(State.Get_Plant().Get_Position()))
                    .Set_SwingTarget(Basis.RotateVector(State.Get_Swing().Get_Target()))
                    .Set_SwingValidatedTarget(Basis.RotateVector(State.Get_Swing().Get_ValidatedTarget()))
                    .Set_SwingActive(State.Get_Swing().Get_Active())
                    .Set_SwingTargetTrusted(State.Get_Swing().Get_TargetTrusted())
                    .Set_SwingTargetFrozen(State.Get_Swing().Get_TargetFrozen())
                    .Set_Position(PublishedFoot.IsSet() ? PublishedFoot->Get_Position() : Basis.RotateVector(Output.Get_Position()))
                    .Set_Rotation(PublishedFoot.IsSet() ? PublishedFoot->Get_Rotation()
                        : (Basis * Output.Get_Rotation()).GetNormalized())
                    .Set_Normal(PublishedFoot.IsSet() ? PublishedFoot->Get_Normal() : Basis.RotateVector(Output.Get_Normal()))
                    .Set_Planted(PublishedFoot.IsSet() ? PublishedFoot->Get_Phase() == ECk_ProceduralLeg_FootPhase::Planted
                        : Output.Get_Planted())
                    .Set_SwingAlpha(PublishedFoot.IsSet() ? PublishedFoot->Get_SwingAlpha() : Output.Get_SwingAlpha())
                    .Set_PhaseOffset(InGaitComp._Solver.GetEffectivePhaseOffset(Index));
                InDebugComp._ScratchLegs[Index].Get_Targeting().Set_StepThreshold(InGaitComp._Solver.Get_Settings().Get_Step().Get_Threshold()
                    * Inputs[Index].Get_StepThresholdScale());
                const auto* CommittedLanding = InGaitComp._LandingPlanning.Get_Landings().FindByPredicate(
                    [&Leg](const auto& InLanding) { return InLanding.Get_Leg() == Leg; });
                InDebugComp._ScratchLegs[Index].Set_HasCommittedLanding(CommittedLanding != nullptr)
                    .Set_CommittedLandingWorld(CommittedLanding ? CommittedLanding->Get_Constraint().Get_PointWorld() : FVector::ZeroVector)
                    .Set_CommittedLandingAdmissionReach(CommittedLanding ? CommittedLanding->Get_Constraint().Get_AdmissionReach() : 0.0f);
                InDebugComp._ScratchLegs[Index].Set_AutomaticRecovery(State.Get_AutomaticRecovery())
                    .Set_NativeTouchdownAttempted(Output.Get_NativeTouchdownAttempted())
                    .Set_NativeTouchdownResolved(Output.Get_NativeTouchdownResolved())
                    .Set_HasReservation(PublishedReservations.ContainsByPredicate([Index](const auto& InReservation)
                        { return InReservation.Get_LegIndex() == Index; }))
                    .Set_HasReachAnchor(InGaitComp._ReachStance.Get_Anchors().ContainsByPredicate([&Leg](const auto& InAnchor)
                        { return InAnchor.Get_Leg() == Leg; }));
            }

            auto& Snapshot = InDebugComp._Snapshot;
            Snapshot.Get_Status().Set_HasAcceptedSample(true);
            Snapshot.Get_Sample().Set_FrameNumber(GFrameCounter)
                .Set_Sequence(InGaitComp._SolveSequence)
                .Set_Time(FCk_Time{World->GetTimeSeconds()});
            Snapshot.Get_Gait().Set_BodyTransform(Body)
                .Set_Velocity(Velocity)
                .Set_CadenceSpeed(CadenceSpeed)
                .Set_CadenceScale(InGaitComp._Solver.Get_LastCadenceScale())
                .Set_Clock(InGaitComp._Solver.GetGaitClock())
                .Set_Airborne(InGaitComp._Solver.IsAirborne())
                .Set_RestTime(InGaitComp._Solver.GetRestTime())
                .Set_SupportNormal(Up)
                .Set_CadenceSpeedRef(InGaitComp._Solver.Get_Settings().Get_Cadence().Get_CadenceSpeedRef())
                .Set_ReachCadenceFloor(InDebugComp._ReachCadenceFloor)
                .Set_ReachSkippedLegs(InDebugComp._ReachSkippedLegs)
                .Set_MissedLandingLifts(InGaitComp._Solver.Get_MissedLandingLifts())
                .Set_RaysLastSolve(InDebugComp._RaysLastSolve)
                .Set_FeetPlane(InFeetPlaneComp.Get_State())
                .Set_FeetPlanePoint(InFeetPlaneComp.Get_Support().Get_Point())
                .Set_FeetPlaneNormal(InFeetPlaneComp.Get_Support().Get_Normal())
                .Set_FeetSupport(InFeetPlaneComp.Get_Support());
            Snapshot.Set_Legs(InDebugComp._ScratchLegs);

            if (InHandle.Has<FFragment_SurfaceMotion>() && InHandle.Has<FFragment_SurfaceMotion_Support>())
            {
                const auto& Motion = InHandle.Get<FFragment_SurfaceMotion>();
                const auto& MotionSupport = InHandle.Get<FFragment_SurfaceMotion_Support>();
                const auto& Support = MotionSupport._State;
                const auto MotionHandle = UCk_Utils_SurfaceMotion_UE::CastChecked(InHandle);
                Snapshot.Get_Motion().Set_Velocity(Support.Get_Velocity())
                    .Set_RequestedDirection(Motion._Direction)
                    .Set_RequestedSpeed(Motion._Speed)
                    .Set_Grounded(Support.Get_Grounded())
                    .Set_TrustedContact(Support.Get_ContactTrusted())
                    .Set_MissingContact(Support.Get_MissingContact())
                    .Set_ContactSource(UCk_Utils_SurfaceMotion_UE::Get_ContactSource(MotionHandle))
                    .Set_HeightSource(UCk_Utils_SurfaceMotion_UE::Get_HeightSource(MotionHandle))
                    .Set_CandidateNormal(Support.Get_CandidateNormal())
                    .Set_CandidateSeen(Support.Get_CandidateSeen())
                    .Set_WallPolicy(UCk_Utils_SurfaceMotion_UE::Get_WallPolicy(MotionHandle))
                    .Set_MaxStepHeight(UCk_Utils_SurfaceMotion_UE::Get_MaxStepHeight(MotionHandle))
                    .Set_Obstruction(UCk_Utils_SurfaceMotion_UE::Get_Obstruction(MotionHandle))
                    .Set_ObstructionNormal(UCk_Utils_SurfaceMotion_UE::Get_ObstructionNormal(MotionHandle))
                    .Set_ReachPaceScale(MotionSupport.Get_ReachPaceScale())
                    .Set_ReachPaceState(MotionSupport.Get_ReachPaceState())
                    .Set_ReachPaceTrials(MotionSupport.Get_ReachPaceTrials())
                    .Set_ReachPaceRays(MotionSupport.Get_ReachPaceRays());
                Snapshot.Get_Gait().Set_SupportNormal(Support.Get_SupportNormal());
            }
        }

        InGaitComp._Basis = Basis;
    }

    auto
        FProcessor_ProceduralGait_Update::
        DoUpdate_FeetPlane(
            HandleType InHandle,
            TimeType InDeltaT,
            const FFragment_ProceduralGait_Tunables& InTunables,
            const FFragment_ProceduralGait& InGaitComp,
            TArrayView<const FProceduralGaitLegInput> InInputs,
            const FTransform& InBody,
            FFragment_ProceduralGait_FeetPlane& InOutFeetPlane)
        -> void
    {
        const auto Ground = ck_procedural_gait::Get_FeetPlaneGround(InGaitComp._Solver, InInputs);
        const auto Fit = ck_procedural_gait::Get_FeetPlane(Ground, InBody);
        const auto GroundFinite = Fit.Result != EProceduralFeetPlaneResult::Malformed;
        CK_ENSURE_IF_NOT(GroundFinite, TEXT("Procedural gait [{}] solved a non-finite plant or landing point; it publishes no feet plane."), InHandle)
        {
            InOutFeetPlane = FFragment_ProceduralGait_FeetPlane{};
            return;
        }

        if (Fit.Result == EProceduralFeetPlaneResult::Fitted)
        {
            InOutFeetPlane._Support = Fit.Support;
            InOutFeetPlane._State = EProceduralGaitFeetPlane::Fitted;
            InOutFeetPlane._SinceFit = FCk_Time{};
            return;
        }

        const auto SinceFit = InOutFeetPlane._SinceFit + InDeltaT;
        const auto Holds = Fit.Result == EProceduralFeetPlaneResult::Underdetermined
            && InOutFeetPlane._State != EProceduralGaitFeetPlane::None
            && SinceFit <= InTunables.Get_Timing().Get_StepDuration() * ck_procedural_gait::FeetPlaneHoldStepDurations;
        if (NOT Holds)
        {
            InOutFeetPlane = FFragment_ProceduralGait_FeetPlane{};
            return;
        }

        InOutFeetPlane._State = EProceduralGaitFeetPlane::Held;
        InOutFeetPlane._SinceFit = SinceFit;
    }

    // --------------------------------------------------------------------------------------------------------------------

    auto
        FProcessor_ProceduralGait_CancelPendingRequests::
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_ProceduralGait_Requests& InRequestsComp)
        -> void
    {
        request::FireCancelledForPending(InHandle, InRequestsComp.Get_Requests());
    }
}

// --------------------------------------------------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS
#include "../../../Private/Tests/CkProceduralPlantQuery.spec.inl"
#endif
