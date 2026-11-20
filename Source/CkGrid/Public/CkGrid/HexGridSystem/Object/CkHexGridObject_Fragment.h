#pragma once

#include "CkHexGridObject_Fragment_Data.h"
#include "CkEcs/Persistence/CkPersistenceInitialization.h"
#include "CkEcs/Tag/CkTag.h"

namespace ck
{
    CK_DEFINE_ECS_TAG(FTag_HexGridObject_RecipeRejected);
    struct CKGRID_API FFragment_HexGridObjectIdentity
    {
        CK_GENERATED_BODY(FFragment_HexGridObjectIdentity);

    private:
        FGuid _BindingId;
        bool _HasNetBaseline = false;

    public:
        CK_PROPERTY(_BindingId);
        CK_PROPERTY(_HasNetBaseline);
    };

    struct CKGRID_API FFragment_HexGridObject_Params
    {
        CK_GENERATED_BODY(FFragment_HexGridObject_Params);
    private:
        FCk_HexGridObject_Spec _Spec;
    public:
        CK_PROPERTY_GET(_Spec);
        CK_DEFINE_CONSTRUCTORS(FFragment_HexGridObject_Params, _Spec);
    };
}

namespace ck::hex_grid
{
    // Zero means this generation has no accepted identity. No provisional key is exposed while restoring.
    CKGRID_API auto GetAcceptedObjectBindingId(const FCk_Handle& InObject) -> FGuid;
}
