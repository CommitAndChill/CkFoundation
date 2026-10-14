#pragma once

#include "CkProceduralAnimation/Action/CkProceduralAction_Data.h"
#include "CkProceduralAnimation/Core/CkProceduralActionProgress.h"

struct FCk_ProceduralAction_ClipDraft;
struct FCk_ProceduralAction_MirrorSpec;
struct FCk_ProceduralAction_SlotBinding;
struct FCk_Handle_ProceduralArticulation;

// --------------------------------------------------------------------------------------------------------------------

namespace ck::procedural_action
{
    namespace content_limits
    {
        constexpr int32 MaxSlots = 64;
        constexpr int32 MaxPhases = 256;
        constexpr int32 MaxTracksPerPhase = 64;
        constexpr int64 MaxKeys = 65536;
        constexpr int32 MaxMarkers = 4096;
        constexpr int32 MaxWindows = 1024;
        constexpr int32 MaxDiagnostics = 64;
    }
    enum class EDiagnosticSeverity : uint8 { Error, Warning };

    struct CKPROCEDURALANIMATION_API FDiagnostic
    {
        CK_GENERATED_BODY(FDiagnostic);
    private:
        EDiagnosticSeverity _Severity = EDiagnosticSeverity::Error;
        FName _Code;
        FString _AssetPath;
        FString _FieldPath;
        FName _PhaseId;
        FName _Slot;
        int32 _KeyIndex = INDEX_NONE;
        FString _Expected;
        FString _Actual;
    public:
        CK_PROPERTY_GET(_Severity);
        CK_PROPERTY_GET(_Code);
        CK_PROPERTY_GET(_AssetPath);
        CK_PROPERTY_GET(_FieldPath);
        CK_PROPERTY_GET(_PhaseId);
        CK_PROPERTY_GET(_Slot);
        CK_PROPERTY_GET(_KeyIndex);
        CK_PROPERTY_GET(_Expected);
        CK_PROPERTY_GET(_Actual);
        CK_DEFINE_CONSTRUCTORS(FDiagnostic, _Severity, _Code, _AssetPath, _FieldPath, _PhaseId,
            _Slot, _KeyIndex, _Expected, _Actual);
    };

    struct CKPROCEDURALANIMATION_API FTransformKey
    {
        CK_GENERATED_BODY(FTransformKey);
    private:
        FCk_FloatRange_0to1 _Time;
        FVector _Translation = FVector::ZeroVector;
        FQuat _Rotation = FQuat::Identity;
        FVector _ArriveTangent = FVector::ZeroVector;
        FVector _LeaveTangent = FVector::ZeroVector;
    public:
        CK_PROPERTY_GET(_Time);
        CK_PROPERTY_GET(_Translation);
        CK_PROPERTY_GET(_Rotation);
        CK_PROPERTY_GET(_ArriveTangent);
        CK_PROPERTY_GET(_LeaveTangent);
        CK_DEFINE_CONSTRUCTORS(FTransformKey, _Time, _Translation, _Rotation, _ArriveTangent, _LeaveTangent);
    };

    struct CKPROCEDURALANIMATION_API FVectorKey
    {
        CK_GENERATED_BODY(FVectorKey);
    private:
        FCk_FloatRange_0to1 _Time;
        FVector _Value = FVector::ZeroVector;
        FVector _ArriveTangent = FVector::ZeroVector;
        FVector _LeaveTangent = FVector::ZeroVector;
    public:
        CK_PROPERTY_GET(_Time);
        CK_PROPERTY_GET(_Value);
        CK_PROPERTY_GET(_ArriveTangent);
        CK_PROPERTY_GET(_LeaveTangent);
        CK_DEFINE_CONSTRUCTORS(FVectorKey, _Time, _Value, _ArriveTangent, _LeaveTangent);
    };

    struct CKPROCEDURALANIMATION_API FOrientationKey
    {
        CK_GENERATED_BODY(FOrientationKey);
    private:
        FCk_FloatRange_0to1 _Time;
        FQuat _Value = FQuat::Identity;
    public:
        CK_PROPERTY_GET(_Time);
        CK_PROPERTY_GET(_Value);
        CK_DEFINE_CONSTRUCTORS(FOrientationKey, _Time, _Value);
    };

    struct CKPROCEDURALANIMATION_API FCompiledSlot
    {
        CK_GENERATED_BODY(FCompiledSlot);
    private:
        FName _Id;
        ECk_ProceduralAction_InitialSource _InitialSource = ECk_ProceduralAction_InitialSource::CapturedAcceptedPose;
        bool _RequiresTerminalOrientation = false;
        bool _HasExplicitRestTarget = false;
        FTransform _ExplicitRestTarget = FTransform::Identity;
        ECk_ProceduralAction_TrackSpace _Space = ECk_ProceduralAction_TrackSpace::BodyLocal;
        FName _AnchorId;
    public:
        CK_PROPERTY_GET(_Id);
        CK_PROPERTY_GET(_InitialSource);
        CK_PROPERTY_GET(_RequiresTerminalOrientation);
        CK_PROPERTY_GET(_HasExplicitRestTarget);
        CK_PROPERTY_GET(_ExplicitRestTarget);
        CK_PROPERTY(_Space);
        CK_PROPERTY(_AnchorId);
        CK_DEFINE_CONSTRUCTORS(FCompiledSlot, _Id, _InitialSource, _RequiresTerminalOrientation,
            _HasExplicitRestTarget, _ExplicitRestTarget, _Space, _AnchorId);
    };

    struct CKPROCEDURALANIMATION_API FCompiledLimbTrack
    {
        CK_GENERATED_BODY(FCompiledLimbTrack);
    private:
        FName _Slot;
        ECk_ProceduralAction_TrackSpace _Space = ECk_ProceduralAction_TrackSpace::BodyLocal;
        FName _AnchorId;
        ECk_ProceduralAction_Interpolation _Interpolation = ECk_ProceduralAction_Interpolation::Linear;
        ECk_ProceduralAction_ReachPolicy _ReachPolicy = ECk_ProceduralAction_ReachPolicy::RejectUnreachable;
        ECk_ProceduralAction_ClearanceResponse _ClearanceResponse = ECk_ProceduralAction_ClearanceResponse::AbortOnCrossing;
        TArray<FVectorKey> _PositionKeys;
        TArray<FOrientationKey> _OrientationKeys;
        TArray<FVectorKey> _BendKeys;
    public:
        CK_PROPERTY(_Slot);
        CK_PROPERTY(_Space);
        CK_PROPERTY(_AnchorId);
        CK_PROPERTY(_Interpolation);
        CK_PROPERTY(_ReachPolicy);
        CK_PROPERTY(_ClearanceResponse);
        CK_PROPERTY(_PositionKeys);
        CK_PROPERTY(_OrientationKeys);
        CK_PROPERTY(_BendKeys);
        CK_DEFINE_CONSTRUCTORS(FCompiledLimbTrack, _Slot, _Space, _AnchorId, _Interpolation,
            _ReachPolicy, _ClearanceResponse, _PositionKeys, _OrientationKeys, _BendKeys);
    };

    struct CKPROCEDURALANIMATION_API FCompiledPhase
    {
        CK_GENERATED_BODY(FCompiledPhase);
    private:
        FName _Id;
        FCk_Time _Start;
        FCk_Time _Duration;
        ECk_ProceduralAction_Easing _Easing = ECk_ProceduralAction_Easing::Linear;
        ECk_ProceduralAction_Interpolation _BodyInterpolation = ECk_ProceduralAction_Interpolation::Linear;
        TArray<FTransformKey> _BodyKeys;
        TArray<FCompiledLimbTrack> _LimbTracks;
        TArray<FCk_Time> _CriticalTimes;
        FCompiledProgressCurve _Progress;
    public:
        CK_PROPERTY(_Id);
        CK_PROPERTY(_Start);
        CK_PROPERTY(_Duration);
        CK_PROPERTY(_Easing);
        CK_PROPERTY(_BodyInterpolation);
        CK_PROPERTY(_BodyKeys);
        CK_PROPERTY(_LimbTracks);
        CK_PROPERTY(_CriticalTimes);
        CK_PROPERTY(_Progress);
        CK_DEFINE_CONSTRUCTORS(FCompiledPhase, _Id, _Start, _Duration, _Easing,
            _BodyInterpolation, _BodyKeys, _LimbTracks);
    };

    enum class EEventKind : uint8 { WindowExit, Point, WindowEnter };

    struct CKPROCEDURALANIMATION_API FCompiledEvent
    {
        CK_GENERATED_BODY(FCompiledEvent);
    private:
        FCk_Time _Time;
        EEventKind _Kind = EEventKind::Point;
        FName _Id;
        int32 _AuthoredIndex = 0;
    public:
        CK_PROPERTY_GET(_Time);
        CK_PROPERTY_GET(_Kind);
        CK_PROPERTY_GET(_Id);
        CK_PROPERTY_GET(_AuthoredIndex);
        CK_DEFINE_CONSTRUCTORS(FCompiledEvent, _Time, _Kind, _Id, _AuthoredIndex);
    };

    struct FCompiledClip;

    // Non-owning view of the five authored clip fields plus their version. The asset and the builder draft both produce
    // one, so there is a single compiler. The viewed arrays must outlive the Compile call.
    struct CKPROCEDURALANIMATION_API FClipSource
    {
        int32 CompatibilityVersion = 1;
        const FCk_ProceduralAction_BodyChannel* BodyChannel = nullptr;
        const TArray<FCk_ProceduralAction_Slot>* Slots = nullptr;
        const TArray<FCk_ProceduralAction_Phase>* Phases = nullptr;
        const TArray<FCk_ProceduralAction_Marker>* Markers = nullptr;
        const TArray<FCk_ProceduralAction_Window>* Windows = nullptr;
        FString AssetPath;
    };

    CKPROCEDURALANIMATION_API auto
    MakeClipSource(
        const UCk_ProceduralAction_Data& InAsset)
        -> FClipSource;

    CKPROCEDURALANIMATION_API auto
    MakeClipSource(
        const FCk_ProceduralAction_ClipDraft& InDraft)
        -> FClipSource;

    CKPROCEDURALANIMATION_API auto
    Compile(
        const FClipSource& InSource,
        FCompiledClip& OutClip,
        TArray<FDiagnostic>& OutDiagnostics)
        -> bool;

    CKPROCEDURALANIMATION_API auto
    Compile(
        const UCk_ProceduralAction_Data& InAsset,
        FCompiledClip& OutClip,
        TArray<FDiagnostic>& OutDiagnostics)
        -> bool;

    // Appends the compiler's diagnostics for InPhase's progress curve (InPhasePath like "Phases[2]"); true when it is valid.
    CKPROCEDURALANIMATION_API auto
    ValidateProgress(
        const FCk_ProceduralAction_Phase& InPhase,
        const FString& InAssetPath,
        const FString& InPhasePath,
        TArray<FDiagnostic>& OutDiagnostics)
        -> bool;

    /** Writes into OutClip a copy of InSource with every mirror's destination slot added, its tracks reflected from the
     *  source slot's across the mirror plane in the body frame. Rest-normalized tracks are converted through the bound
     *  limbs' rest frames on InSubject (the frames the clip's initial capture uses), so the destination stays
     *  rest-normalized in its own limb's frame; body-local and root-relative tracks reflect as declared. Rejects an
     *  occupied or repeated destination, a source the clip does not declare or never tracks, an anchor-relative source,
     *  a destination without a binding to a limb of InSubject other than the source's, and content beyond the compiler
     *  ceilings. Body motion and events are copied unchanged. On rejection OutClip is reset. InSource is never changed. */
    CKPROCEDURALANIMATION_API auto
    ExpandMirrors(
        const FCompiledClip& InSource,
        const TArray<FCk_ProceduralAction_MirrorSpec>& InMirrors,
        const TArray<FCk_ProceduralAction_SlotBinding>& InSlotBindings,
        const FCk_Handle_ProceduralArticulation& InSubject,
        FCompiledClip& OutClip,
        TArray<FDiagnostic>& OutDiagnostics)
        -> bool;

    // Detached compiled revision. Consumers retain a const shared copy; authored assets are not sampled per frame.
    struct CKPROCEDURALANIMATION_API FCompiledClip
    {
        CK_GENERATED_BODY(FCompiledClip);
        friend CKPROCEDURALANIMATION_API auto Compile(const FClipSource& InSource, FCompiledClip& OutClip,
            TArray<FDiagnostic>& OutDiagnostics) -> bool;
        friend CKPROCEDURALANIMATION_API auto ExpandMirrors(const FCompiledClip& InSource,
            const TArray<FCk_ProceduralAction_MirrorSpec>& InMirrors,
            const TArray<FCk_ProceduralAction_SlotBinding>& InSlotBindings,
            const FCk_Handle_ProceduralArticulation& InSubject, FCompiledClip& OutClip,
            TArray<FDiagnostic>& OutDiagnostics) -> bool;
    private:
        int32 _CompatibilityVersion = 1;
        bool _HasBodyChannel = false;
        ECk_ProceduralAction_InitialSource _BodyInitialSource = ECk_ProceduralAction_InitialSource::IdentityOffset;
        FCk_Time _Duration;
        uint64 _ContentHash = 0;
        TArray<FCompiledSlot> _Slots;
        TArray<FCompiledPhase> _Phases;
        TArray<FCompiledEvent> _Events;
    public:
        CK_PROPERTY_GET(_CompatibilityVersion);
        CK_PROPERTY_GET(_HasBodyChannel);
        CK_PROPERTY_GET(_BodyInitialSource);
        CK_PROPERTY_GET(_Duration);
        CK_PROPERTY_GET(_ContentHash);
        CK_PROPERTY_GET(_Slots);
        CK_PROPERTY_GET(_Phases);
        CK_PROPERTY_GET(_Events);
    };

    // On failure OutClip resets; no partially compiled clip escapes.
}

// --------------------------------------------------------------------------------------------------------------------
