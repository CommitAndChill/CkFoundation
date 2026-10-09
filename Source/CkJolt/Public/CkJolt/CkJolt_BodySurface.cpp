#include "CkJolt_BodySurface.h"

#include "CkCore/Validation/CkIsValid.h"

#include "CkJolt/Body/CkJoltBody_Fragment.h"
#include "CkJolt/Body/CkJoltBody_Utils.h"
#include "CkJolt/StaticWorld/CkJoltStaticActor_Utils.h"

#include <PhysicalMaterials/PhysicalMaterial.h>

// --------------------------------------------------------------------------------------------------------------------

auto
    ck::jolt::
    TryGet_BodyPhysicalMaterial(
        const FCk_Handle& InEntity,
        uint32 InBodyIndexAndSequence)
    -> UPhysicalMaterial*
{
    if (ck::Is_NOT_Valid(InEntity))
    { return nullptr; }

    if (const auto JoltBody = UCk_Utils_JoltBody_UE::Cast(InEntity); ck::IsValid(JoltBody))
    {
        if (JoltBody.Get<ck::FFragment_JoltBody>().Get_BodyId().GetIndexAndSequenceNumber() != InBodyIndexAndSequence)
        { return nullptr; }

        return UCk_Utils_JoltBody_UE::Get_PhysicalMaterial(JoltBody);
    }

    if (const auto StaticActor = UCk_Utils_JoltStaticActor_UE::Cast(InEntity); ck::IsValid(StaticActor))
    { return UCk_Utils_JoltStaticActor_UE::Get_BodyPhysicalMaterial(StaticActor, InBodyIndexAndSequence); }

    return nullptr;
}

// --------------------------------------------------------------------------------------------------------------------
