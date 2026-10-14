#pragma once

#include "CkProceduralAnimation/LimbControl/CkProceduralLimbControl_Fragment_Data.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck::procedural_support
{
    enum class EResult : uint8
    {
        Accepted,
        Disabled,
        InvalidPolicy,
        InvalidFrame,
        InvalidContact,
        InsufficientContacts,
        IncoherentNormals,
        DegenerateRegion,
        OutsideRegion
    };

    struct CKPROCEDURALANIMATION_API FContact
    {
        CK_GENERATED_BODY(FContact);

    private:
        FVector _Position = FVector::ZeroVector;
        FVector _Normal = FVector::UpVector;
        bool _Trusted = false;

    public:
        CK_PROPERTY_GET(_Position);
        CK_PROPERTY_GET(_Normal);
        CK_PROPERTY_GET(_Trusted);
        CK_DEFINE_CONSTRUCTORS(FContact, _Position, _Normal, _Trusted);
    };

    struct CKPROCEDURALANIMATION_API FObservation
    {
        CK_GENERATED_BODY(FObservation);

    private:
        EResult _Result = EResult::InvalidPolicy;
        int32 _TrustedContacts = 0;
        double _MinimumEdgeDistanceCm = 0.0;

    public:
        CK_PROPERTY_GET(_Result);
        CK_PROPERTY_GET(_TrustedContacts);
        CK_PROPERTY_GET(_MinimumEdgeDistanceCm);
        CK_DEFINE_CONSTRUCTORS(FObservation, _Result, _TrustedContacts, _MinimumEdgeDistanceCm);
    };

    CKPROCEDURALANIMATION_API auto
        ValidatePolicy(
            const FCk_ProceduralLimbControl_Support& InPolicy)
        -> bool;

    // InContacts is the prospective remaining stance after all withdrawals, including earlier takeoffs this solve.
    // Untrusted entries contribute nothing. Callers must obtain trust from actual contact provenance.
    // Optional diagnostic hull lies on the plane through InRootProxy. Normal callers allocate no heap storage.
    CKPROCEDURALANIMATION_API auto
        Evaluate(
            const FCk_ProceduralLimbControl_Support& InPolicy,
            TArrayView<const FContact> InContacts,
            const FVector& InRootProxy,
            const FVector& InSupportUp,
            TArray<FVector>* OutProjectedRegion = nullptr)
        -> FObservation;
}

// --------------------------------------------------------------------------------------------------------------------
