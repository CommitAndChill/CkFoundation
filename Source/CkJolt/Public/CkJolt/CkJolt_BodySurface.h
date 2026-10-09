#pragma once

#include "CkEcs/Handle/CkHandle.h"

// --------------------------------------------------------------------------------------------------------------------

class UPhysicalMaterial;

// --------------------------------------------------------------------------------------------------------------------

namespace ck::jolt
{
    /** The phys mat of the Jolt body InBodyIndexAndSequence (JPH::BodyID::GetIndexAndSequenceNumber) owned by InEntity:
        a JoltBody's spec phys mat (SurfaceSource PhysicalMaterial only), or a JoltStaticActor's baked body phys mat.
        Null when InEntity is invalid, owns neither, or the body is not the JoltBody's own (an entity may also own a
        probe body under the same user data). */
    CKJOLT_API auto
        TryGet_BodyPhysicalMaterial(
            const FCk_Handle& InEntity,
            uint32 InBodyIndexAndSequence)
        -> UPhysicalMaterial*;
}

// --------------------------------------------------------------------------------------------------------------------
