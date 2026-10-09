#pragma once

#include "CkEcs/Handle/CkHandle.h"

namespace ck
{
    // A failed fresh Add is terminal for this entity. Tuner children are kept
    // for normal owner-lifetime cleanup, but no camera processor may run them.
    CK_DEFINE_ECS_TAG(FTag_Camera_AdmissionClosed);
}

struct FCk_Handle_Camera;

namespace ck::camera
{
    // Processors use the same total structural tuner preflight as Get_Profile.
    auto CanReadCameraProfile(const FCk_Handle_Camera& InCamera) -> bool;
}
