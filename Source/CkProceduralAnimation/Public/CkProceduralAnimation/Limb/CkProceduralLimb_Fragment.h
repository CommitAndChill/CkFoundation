#pragma once

#include "CkProceduralAnimation/Limb/CkProceduralLimb_Fragment_Data.h"

namespace ck
{
    // The root articulation owns geometry and bindings. A limb has stable identity only.
    struct CKPROCEDURALANIMATION_API FFragment_ProceduralLimb
    {
        CK_GENERATED_BODY(FFragment_ProceduralLimb);

    private:
        FName _Id;
        int32 _Index = INDEX_NONE;

    public:
        CK_PROPERTY_GET(_Id);
        CK_PROPERTY_GET(_Index);
        CK_DEFINE_CONSTRUCTORS(FFragment_ProceduralLimb, _Id, _Index);
    };
}
