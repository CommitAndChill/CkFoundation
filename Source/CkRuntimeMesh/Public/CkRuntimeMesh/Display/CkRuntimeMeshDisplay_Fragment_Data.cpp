#include "CkRuntimeMesh/Display/CkRuntimeMeshDisplay_Fragment_Data.h"

#include "CkCore/Algorithms/CkAlgorithms.h"
#include "CkCore/Validation/CkIsValid.h"

#include "Components/PrimitiveComponent.h"
#include "Materials/MaterialInterface.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_RuntimeMeshDisplay_Spec::
    Get_IsValid() const
    -> bool
{
    const auto& Materials = _Visuals.Get_Materials();
    const auto HasGeometry = ck::IsValid(_Geometry);
    const auto HasSlotCountInRange = Materials.Num() >= 1 && Materials.Num() <= ck::runtimemesh::display::MaxMaterialSlots;
    const auto HasNoEmptySlot = ck::algo::NoneOf(Materials,
        [](const TSoftObjectPtr<UMaterialInterface>& InMaterial) { return InMaterial.IsNull(); });

    return HasGeometry && HasSlotCountInRange && HasNoEmptySlot;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_Request_RuntimeMeshDisplay_SetCustomPrimitiveData::
    Get_IsValid() const
    -> bool
{
    const auto DataIndex = _Data.Get_CustomDataIndex();
    const auto FloatCount = _Data.Get_Value().Get_FloatCount();

    return FloatCount > 0 && DataIndex >= 0 &&
        DataIndex <= FCustomPrimitiveData::NumCustomPrimitiveDataFloats - FloatCount;
}

// --------------------------------------------------------------------------------------------------------------------
