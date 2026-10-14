#pragma once

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Math/ValueRange/CkValueRange.h"
#include "CkCore/Time/CkTime.h"
#include "CkCore/Types/DataAsset/CkDataAsset.h"

#include "CkProceduralAction_Data.generated.h"

// --------------------------------------------------------------------------------------------------------------------

UENUM(BlueprintType)
enum class ECk_ProceduralAction_InitialSource : uint8
{
    CapturedAcceptedPose,
    AuthoredRest,
    IdentityOffset
};
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_ProceduralAction_InitialSource);

UENUM(BlueprintType)
enum class ECk_ProceduralAction_TrackSpace : uint8
{
    BodyLocal,
    RootAtStart,
    AnchorAtStart,
    LimbRestNormalized
};
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_ProceduralAction_TrackSpace);

UENUM(BlueprintType)
enum class ECk_ProceduralAction_Interpolation : uint8
{
    Linear,
    Smooth
};
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_ProceduralAction_Interpolation);

UENUM(BlueprintType)
enum class ECk_ProceduralAction_Easing : uint8
{
    Linear,
    InQuad,
    OutQuad,
    InOutQuad,
    InCubic,
    OutCubic,
    InOutCubic
};
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_ProceduralAction_Easing);

UENUM(BlueprintType)
enum class ECk_ProceduralAction_ProgressMode : uint8
{
    Preset,
    CustomCurve
};
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_ProceduralAction_ProgressMode);

UENUM(BlueprintType)
enum class ECk_ProceduralAction_ReachPolicy : uint8
{
    RejectUnreachable,
    ProjectToReach
};
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_ProceduralAction_ReachPolicy);

UENUM(BlueprintType)
enum class ECk_ProceduralAction_ClearanceResponse : uint8
{
    AbortOnCrossing,
    ObserveCrossing
};
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_ProceduralAction_ClearanceResponse);

UENUM(BlueprintType)
enum class ECk_ProceduralAction_PoseProvenance : uint8
{
    Requested,
    Solved
};
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_ProceduralAction_PoseProvenance);

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_Sampling
{
    GENERATED_BODY()
public:
    CK_GENERATED_BODY(FCk_ProceduralAction_Sampling);
private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true))
    FCk_Time _MaxAdvance = FCk_Time{1.0 / 120.0};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true, Units = "cm"))
    float _MaxEndpointTravelCm = 2.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true, Units = "deg"))
    float _MaxSegmentAngularStepDegrees = 5.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true, ClampMin = "1"))
    int32 _MaxSamples = 16;
public:
    CK_PROPERTY(_MaxAdvance);
    CK_PROPERTY(_MaxEndpointTravelCm);
    CK_PROPERTY(_MaxSegmentAngularStepDegrees);
    CK_PROPERTY(_MaxSamples);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_TransformKey
{
    GENERATED_BODY()
public:
    CK_GENERATED_BODY(FCk_ProceduralAction_TransformKey);
private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true))
    FCk_FloatRange_0to1 _Time;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true, Units = "cm"))
    FVector _Translation = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true))
    FQuat _Rotation = FQuat::Identity;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true, Units = "cm"))
    FVector _ArriveTangent = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true, Units = "cm"))
    FVector _LeaveTangent = FVector::ZeroVector;
public:
    CK_PROPERTY(_Time);
    CK_PROPERTY(_Translation);
    CK_PROPERTY(_Rotation);
    CK_PROPERTY(_ArriveTangent);
    CK_PROPERTY(_LeaveTangent);
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralAction_TransformKey, _Time, _Translation, _Rotation);
};

USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_VectorKey
{
    GENERATED_BODY()
public:
    CK_GENERATED_BODY(FCk_ProceduralAction_VectorKey);
private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true))
    FCk_FloatRange_0to1 _Time;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true))
    FVector _Value = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true))
    FVector _ArriveTangent = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true))
    FVector _LeaveTangent = FVector::ZeroVector;
public:
    CK_PROPERTY(_Time);
    CK_PROPERTY(_Value);
    CK_PROPERTY(_ArriveTangent);
    CK_PROPERTY(_LeaveTangent);
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralAction_VectorKey, _Time, _Value);
};

USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_OrientationKey
{
    GENERATED_BODY()
public:
    CK_GENERATED_BODY(FCk_ProceduralAction_OrientationKey);
private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true))
    FCk_FloatRange_0to1 _Time;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true))
    FQuat _Value = FQuat::Identity;
public:
    CK_PROPERTY(_Time);
    CK_PROPERTY(_Value);
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralAction_OrientationKey, _Time, _Value);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_ProgressKey
{
    GENERATED_BODY()
public:
    CK_GENERATED_BODY(FCk_ProceduralAction_ProgressKey);
private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true))
    FCk_FloatRange_0to1 _Time;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true))
    FCk_FloatRange_0to1 _Value;
public:
    CK_PROPERTY(_Time);
    CK_PROPERTY(_Value);
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralAction_ProgressKey, _Time, _Value);
};

/** Remaps a phase's elapsed fraction to motion progress. Preset uses the phase's _Easing and requires no _Keys.
 *  CustomCurve requires _Easing Linear and 2..64 keys with strictly ascending times, nondecreasing values and exact
 *  (0,0) and (1,1) endpoints; it is compiled to a monotone cubic. Markers and windows stay on elapsed time either way. */
USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_ProgressCurve
{
    GENERATED_BODY()
public:
    CK_GENERATED_BODY(FCk_ProceduralAction_ProgressCurve);
private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true))
    ECk_ProceduralAction_ProgressMode _Mode = ECk_ProceduralAction_ProgressMode::Preset;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true,
        EditCondition = "_Mode == ECk_ProceduralAction_ProgressMode::CustomCurve"))
    TArray<FCk_ProceduralAction_ProgressKey> _Keys;
public:
    CK_PROPERTY(_Mode);
    CK_PROPERTY(_Keys);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_Slot
{
    GENERATED_BODY()
public:
    CK_GENERATED_BODY(FCk_ProceduralAction_Slot);
private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true))
    FName _Id;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true))
    ECk_ProceduralAction_InitialSource _InitialSource = ECk_ProceduralAction_InitialSource::CapturedAcceptedPose;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true))
    bool _RequiresTerminalOrientation = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true))
    bool _HasExplicitRestTarget = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true))
    FTransform _ExplicitRestTarget = FTransform::Identity;
public:
    CK_PROPERTY_GET(_Id);
    CK_PROPERTY(_InitialSource);
    CK_PROPERTY(_RequiresTerminalOrientation);
    CK_PROPERTY(_HasExplicitRestTarget);
    CK_PROPERTY(_ExplicitRestTarget);
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralAction_Slot, _Id);
};

USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_BodyChannel
{
    GENERATED_BODY()
public:
    CK_GENERATED_BODY(FCk_ProceduralAction_BodyChannel);
private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true))
    bool _Enabled = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true))
    ECk_ProceduralAction_InitialSource _InitialSource = ECk_ProceduralAction_InitialSource::IdentityOffset;
public:
    CK_PROPERTY(_Enabled);
    CK_PROPERTY(_InitialSource);
};

USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_LimbTrack
{
    GENERATED_BODY()
public:
    CK_GENERATED_BODY(FCk_ProceduralAction_LimbTrack);
private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true))
    FName _Slot;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true))
    ECk_ProceduralAction_TrackSpace _Space = ECk_ProceduralAction_TrackSpace::BodyLocal;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true))
    FName _AnchorId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true))
    ECk_ProceduralAction_Interpolation _Interpolation = ECk_ProceduralAction_Interpolation::Linear;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true))
    ECk_ProceduralAction_ReachPolicy _ReachPolicy = ECk_ProceduralAction_ReachPolicy::RejectUnreachable;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true))
    ECk_ProceduralAction_ClearanceResponse _ClearanceResponse = ECk_ProceduralAction_ClearanceResponse::AbortOnCrossing;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion|Position", meta = (AllowPrivateAccess = true))
    TArray<FCk_ProceduralAction_VectorKey> _PositionKeys;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion|Orientation", meta = (AllowPrivateAccess = true))
    TArray<FCk_ProceduralAction_OrientationKey> _OrientationKeys;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion|Bend", meta = (AllowPrivateAccess = true))
    TArray<FCk_ProceduralAction_VectorKey> _BendKeys;
public:
    CK_PROPERTY_GET(_Slot);
    CK_PROPERTY(_Space);
    CK_PROPERTY(_AnchorId);
    CK_PROPERTY(_Interpolation);
    CK_PROPERTY(_ReachPolicy);
    CK_PROPERTY(_ClearanceResponse);
    CK_PROPERTY(_PositionKeys);
    CK_PROPERTY(_OrientationKeys);
    CK_PROPERTY(_BendKeys);
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralAction_LimbTrack, _Slot);
};

USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_Phase
{
    GENERATED_BODY()
public:
    CK_GENERATED_BODY(FCk_ProceduralAction_Phase);
private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true))
    FName _Id;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true))
    FCk_Time _Duration;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true))
    ECk_ProceduralAction_Easing _Easing = ECk_ProceduralAction_Easing::Linear;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true))
    FCk_ProceduralAction_ProgressCurve _Progress;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true))
    ECk_ProceduralAction_Interpolation _BodyInterpolation = ECk_ProceduralAction_Interpolation::Linear;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion|Body", meta = (AllowPrivateAccess = true))
    TArray<FCk_ProceduralAction_TransformKey> _BodyKeys;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion|Limbs", meta = (AllowPrivateAccess = true))
    TArray<FCk_ProceduralAction_LimbTrack> _LimbTracks;
public:
    CK_PROPERTY_GET(_Id);
    CK_PROPERTY(_Duration);
    CK_PROPERTY(_Easing);
    CK_PROPERTY(_Progress);
    CK_PROPERTY(_BodyInterpolation);
    CK_PROPERTY(_BodyKeys);
    CK_PROPERTY(_LimbTracks);
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralAction_Phase, _Id, _Duration);
};

USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_TimeRef
{
    GENERATED_BODY()
public:
    CK_GENERATED_BODY(FCk_ProceduralAction_TimeRef);
private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true))
    FName _PhaseId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true))
    FCk_FloatRange_0to1 _Time;
public:
    CK_PROPERTY(_PhaseId);
    CK_PROPERTY(_Time);
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralAction_TimeRef, _PhaseId, _Time);
};

USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_Marker
{
    GENERATED_BODY()
public:
    CK_GENERATED_BODY(FCk_ProceduralAction_Marker);
private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true))
    FName _Id;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true))
    FCk_ProceduralAction_TimeRef _At;
public:
    CK_PROPERTY_GET(_Id);
    CK_PROPERTY(_At);
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralAction_Marker, _Id, _At);
};

USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_Window
{
    GENERATED_BODY()
public:
    CK_GENERATED_BODY(FCk_ProceduralAction_Window);
private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true))
    FName _Id;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true))
    FCk_ProceduralAction_TimeRef _Start;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value", meta = (AllowPrivateAccess = true))
    FCk_ProceduralAction_TimeRef _End;
public:
    CK_PROPERTY_GET(_Id);
    CK_PROPERTY(_Start);
    CK_PROPERTY(_End);
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralAction_Window, _Id, _Start, _End);
};

// --------------------------------------------------------------------------------------------------------------------

/** Editor-only label for the keys one whole-pose capture wrote: the phase, time and channels they share. It holds no
 *  transforms; the keys in the phases stay the only motion. The body channel is named Get_BodyChannelName(). */
USTRUCT()
struct CKPROCEDURALANIMATION_API FCk_ProceduralAction_PoseGroup
{
    GENERATED_BODY()
public:
    CK_GENERATED_BODY(FCk_ProceduralAction_PoseGroup);
private:
    UPROPERTY()
    FGuid _Id;
    UPROPERTY()
    FName _Label;
    UPROPERTY()
    FName _PhaseId;
    UPROPERTY()
    FCk_FloatRange_0to1 _Time;
    UPROPERTY()
    TArray<FName> _Channels;
    UPROPERTY()
    ECk_ProceduralAction_PoseProvenance _Provenance = ECk_ProceduralAction_PoseProvenance::Requested;
public:
    static auto
        Get_BodyChannelName()
        -> FName
    { return FName{TEXT("(Body)")}; }

    CK_PROPERTY_GET(_Id);
    CK_PROPERTY(_Label);
    CK_PROPERTY(_PhaseId);
    CK_PROPERTY(_Time);
    CK_PROPERTY(_Channels);
    CK_PROPERTY(_Provenance);
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralAction_PoseGroup, _Id, _PhaseId, _Time, _Channels);
};

// --------------------------------------------------------------------------------------------------------------------

UCLASS(BlueprintType)
class CKPROCEDURALANIMATION_API UCk_ProceduralAction_Data : public UCk_DataAsset_PDA
{
    GENERATED_BODY()
public:
    CK_GENERATED_BODY(UCk_ProceduralAction_Data);
protected:
#if WITH_EDITOR
    auto
        IsDataValid(
            class FDataValidationContext& InContext) const
        -> EDataValidationResult override;
#endif
private:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Content", meta = (AllowPrivateAccess = true))
    int32 _CompatibilityVersion = 1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body", meta = (AllowPrivateAccess = true))
    FCk_ProceduralAction_BodyChannel _BodyChannel;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slots", meta = (AllowPrivateAccess = true, TitleProperty = "_Id"))
    TArray<FCk_ProceduralAction_Slot> _Slots;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Phases", meta = (AllowPrivateAccess = true, TitleProperty = "_Id"))
    TArray<FCk_ProceduralAction_Phase> _Phases;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Markers", meta = (AllowPrivateAccess = true, TitleProperty = "_Id"))
    TArray<FCk_ProceduralAction_Marker> _Markers;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Windows", meta = (AllowPrivateAccess = true, TitleProperty = "_Id"))
    TArray<FCk_ProceduralAction_Window> _Windows;
#if WITH_EDITORONLY_DATA
    // Read by the editor only; Compile never sees it, so cooked motion data and content hashes ignore it.
    UPROPERTY()
    TArray<FCk_ProceduralAction_PoseGroup> _EditorPoseGroups;
#endif
public:
    CK_PROPERTY(_CompatibilityVersion);
    CK_PROPERTY(_BodyChannel);
    CK_PROPERTY(_Slots);
    CK_PROPERTY(_Phases);
    CK_PROPERTY(_Markers);
    CK_PROPERTY(_Windows);
#if WITH_EDITORONLY_DATA
    auto
        Get_EditorPoseGroups() const
        -> const TArray<FCk_ProceduralAction_PoseGroup>&
    { return _EditorPoseGroups; }

    auto
        Set_EditorPoseGroups(
            const TArray<FCk_ProceduralAction_PoseGroup>& InGroups)
        -> void
    { _EditorPoseGroups = InGroups; }
#endif
};

// --------------------------------------------------------------------------------------------------------------------
