#include "CkRuntimeMesh/Settings/CkRuntimeMesh_ProjectSettings.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_RuntimeMesh_Settings_UE::
    Get()
    -> const UCk_RuntimeMesh_ProjectSettings_UE*
{
    return GetDefault<UCk_RuntimeMesh_ProjectSettings_UE>();
}

auto
    UCk_Utils_RuntimeMesh_Settings_UE::
    Get_DrainTimeBudget()
    -> FCk_Time
{
    return Get()->Get_DrainTimeBudget();
}

auto
    UCk_Utils_RuntimeMesh_Settings_UE::
    Get_MaximumSlicesPerDrain()
    -> int32
{
    return Get()->Get_MaximumSlicesPerDrain();
}

// --------------------------------------------------------------------------------------------------------------------
