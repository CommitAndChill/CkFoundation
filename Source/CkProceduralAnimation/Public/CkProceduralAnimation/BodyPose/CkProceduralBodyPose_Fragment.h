#pragma once

#include "CkProceduralAnimation/BodyPose/CkProceduralBodyPose_Fragment_Data.h"

#include "CkCore/Macros/CkMacros.h"

#include <Kismet/KismetMathLibrary.h>

#include <variant>

// --------------------------------------------------------------------------------------------------------------------

class UCk_Utils_ProceduralBodyPose_UE;
class UCk_Utils_ProceduralAnimation_Debug_UE;

// --------------------------------------------------------------------------------------------------------------------

namespace ck
{
    class FProcessor_ProceduralBodyPose_HandleRequests;
    class FProcessor_ProceduralBodyPose_Update;

    // --------------------------------------------------------------------------------------------------------------------

    struct CKPROCEDURALANIMATION_API FFragment_ProceduralBodyPose_Params
    {
    public:
        CK_GENERATED_BODY(FFragment_ProceduralBodyPose_Params);

    private:
        FCk_Handle_Transform _Presentation;
        FCk_ProceduralBodyPose_Spring _Spring;
        FCk_ProceduralBodyPose_Support _Support;

    public:
        CK_PROPERTY_GET(_Presentation);
        CK_PROPERTY_GET(_Spring);
        CK_PROPERTY_GET(_Support);

    public:
        CK_DEFINE_CONSTRUCTORS(FFragment_ProceduralBodyPose_Params, _Presentation, _Spring, _Support);
    };

    // --------------------------------------------------------------------------------------------------------------------

    // Present only while the body pose conforms to its planted feet.
    struct CKPROCEDURALANIMATION_API FFragment_ProceduralBodyPose_Conform
    {
    public:
        CK_GENERATED_BODY(FFragment_ProceduralBodyPose_Conform);

    private:
        float _MaxTilt = 20.0f;
        float _HeightWeight = 0.5f;
        float _MaxHeight = 10.0f;
        float _MaxTiltRate = 80.0f;
        float _MaxHeightRate = 60.0f;

    public:
        CK_PROPERTY_GET(_MaxTilt);
        CK_PROPERTY_GET(_HeightWeight);
        CK_PROPERTY_GET(_MaxHeight);
        CK_PROPERTY_GET(_MaxTiltRate);
        CK_PROPERTY_GET(_MaxHeightRate);

    public:
        CK_DEFINE_CONSTRUCTORS(FFragment_ProceduralBodyPose_Conform, _MaxTilt, _HeightWeight, _MaxHeight, _MaxTiltRate, _MaxHeightRate);
    };

    // --------------------------------------------------------------------------------------------------------------------

    // Body-local conform targets: the last fitted one, which the update holds while the feet cannot determine a plane, and
    // the applied one the springs chase, which follows the held one at the conform's max tilt and height rates.
    struct CKPROCEDURALANIMATION_API FFragment_ProceduralBodyPose_ConformState
    {
    public:
        CK_GENERATED_BODY(FFragment_ProceduralBodyPose_ConformState);

    public:
        friend class FProcessor_ProceduralBodyPose_Update;
        friend class FProcessor_ProceduralArticulation_Observe;

    private:
        FTransform _HeldTarget = FTransform::Identity;
        FTransform _AppliedTarget = FTransform::Identity;

    public:
        CK_PROPERTY_GET(_HeldTarget);
        CK_PROPERTY_GET(_AppliedTarget);
    };

    // --------------------------------------------------------------------------------------------------------------------

    struct CKPROCEDURALANIMATION_API FFragment_ProceduralBodyPose_Failure
    {
    public:
        CK_GENERATED_BODY(FFragment_ProceduralBodyPose_Failure);

    private:
        ECk_ProceduralBodyPose_Failure _Reason = ECk_ProceduralBodyPose_Failure::None;

    public:
        CK_PROPERTY_GET(_Reason);

    public:
        CK_DEFINE_CONSTRUCTORS(FFragment_ProceduralBodyPose_Failure, _Reason);
    };

    // --------------------------------------------------------------------------------------------------------------------

    // Hip positions of the gait's captured legs, by captured index, taken at Add. A detached leg's entity is
    // gone, so its hip must come from here for the support gap to keep pointing at it.
    struct CKPROCEDURALANIMATION_API FFragment_ProceduralBodyPose_SupportLayout
    {
    public:
        CK_GENERATED_BODY(FFragment_ProceduralBodyPose_SupportLayout);

    private:
        TArray<FVector> _HipLocals;

    public:
        CK_PROPERTY_GET(_HipLocals);

    public:
        CK_DEFINE_CONSTRUCTORS(FFragment_ProceduralBodyPose_SupportLayout, _HipLocals);
    };

    // --------------------------------------------------------------------------------------------------------------------

    struct CKPROCEDURALANIMATION_API FFragment_ProceduralBodyPose
    {
    public:
        CK_GENERATED_BODY(FFragment_ProceduralBodyPose);

    public:
        friend class FProcessor_ProceduralBodyPose_Update;
        friend class ::UCk_Utils_ProceduralBodyPose_UE;
        friend class ::UCk_Utils_ProceduralAnimation_Debug_UE;

    private:
        // Body-local; the accepted offset the presentation is posed at, _Offset * Body: the base offset with the action
        // composed and reach-projected.
        FTransform _Offset = FTransform::Identity;
        // Body-local; the environmental offset the springs carry toward _TargetOffset. The springs, the swing transport and
        // _LastBodyRotation belong to it; the action never feeds them.
        FTransform _BaseOffset = FTransform::Identity;
        // Body-local; the pose the springs followed on the last update.
        FTransform _TargetOffset = FTransform::Identity;
        FVectorSpringState _TranslationSpring;
        FQuaternionSpringState _RotationSpring;
        FQuat _LastBodyRotation = FQuat::Identity;

    public:
        CK_PROPERTY_GET(_Offset);
        CK_PROPERTY_GET(_BaseOffset);
        CK_PROPERTY_GET(_TargetOffset);

    public:
        CK_DEFINE_CONSTRUCTORS(FFragment_ProceduralBodyPose, _LastBodyRotation);
    };

    // --------------------------------------------------------------------------------------------------------------------

    struct CKPROCEDURALANIMATION_API FFragment_ProceduralBodyPose_Action
    {
    public:
        CK_GENERATED_BODY(FFragment_ProceduralBodyPose_Action);

    public:
        friend class FProcessor_ProceduralBodyPose_HandleRequests;
        friend class FProcessor_ProceduralBodyPose_Update;
        friend class FProcessor_ProceduralArticulation_Observe;
        friend class ::UCk_Utils_ProceduralBodyPose_UE;

    private:
        // Body-local, in the simulation body's axes; the last admitted action input, kept until replaced.
        FTransform _Offset = FTransform::Identity;
        int64 _SampleId = 0;
        FCk_ProceduralBodyPose_ActionSample _Sample;

    public:
        CK_PROPERTY_GET(_Offset);
        CK_PROPERTY_GET(_SampleId);
        CK_PROPERTY_GET(_Sample);
    };

    // --------------------------------------------------------------------------------------------------------------------

    struct CKPROCEDURALANIMATION_API FFragment_ProceduralBodyPose_Requests
    {
    public:
        CK_GENERATED_BODY(FFragment_ProceduralBodyPose_Requests);

    public:
        friend class FProcessor_ProceduralBodyPose_HandleRequests;
        friend class ::UCk_Utils_ProceduralBodyPose_UE;

    public:
        using RequestType = std::variant<FCk_Request_ProceduralBodyPose_SetActionOffset>;
        using RequestList = TArray<RequestType>;

    private:
        RequestList _Requests;

    public:
        CK_PROPERTY_GET(_Requests);
    };
}

// --------------------------------------------------------------------------------------------------------------------
