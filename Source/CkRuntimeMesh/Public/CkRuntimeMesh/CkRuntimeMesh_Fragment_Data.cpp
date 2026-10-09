#include "CkRuntimeMesh/CkRuntimeMesh_Fragment_Data.h"

#include "CkRuntimeMesh/CkRuntimeMesh_Fragment.h"
#include "CkRuntimeMesh/Internal/CkRuntimeMesh_Geometry.h"
#include "CkRuntimeMesh/Internal/CkRuntimeMesh_Slice.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_RuntimeMesh_Spec::
    Get_IsValid() const
    -> bool
{
    const auto HasSource = NOT _SourceMesh.IsNull();
    const auto HasValidLimits = ck::runtimemesh::geometry::ValidateOptions(ck::runtimemesh::MakeImportOptions(_Import));

    return HasSource && HasValidLimits;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_Request_RuntimeMesh_Slice::
    Get_IsValid() const
    -> bool
{
    const auto HasOperationID = _OperationID.IsValid();
    const auto HasValidCut = ck::runtimemesh::slice::ValidateOptions(ck::runtimemesh::MakeCutOptions(*this));

    return HasOperationID && HasValidCut;
}

// --------------------------------------------------------------------------------------------------------------------
