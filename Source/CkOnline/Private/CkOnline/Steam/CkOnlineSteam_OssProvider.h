#pragma once

#include "CkOnline/Steam/CkOnlineSteam_Session.h"

namespace ck::steam
{
    auto CreateSteamTransport(const FCk_OnlineSteam_Spec& InSpec) -> TSharedPtr<ITransport>;
}
