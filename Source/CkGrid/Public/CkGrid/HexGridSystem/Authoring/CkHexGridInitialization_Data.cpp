#include "CkHexGridInitialization_Data.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Persistence/CkPersistenceInitialization.h"

auto
    FCk_HexGridInitialization::
    TryResolve(
        const FCk_Handle& InEntity,
        ck::persistence::FInitializationIdentity& OutIdentity) const
    -> bool
{
    using EIdentity = ck::persistence::EInitializationIdentity;
    OutIdentity = {};
    switch (_Kind)
    {
        case ECk_HexGridInitialization::Automatic:
            return true;
        case ECk_HexGridInitialization::Fresh:
            OutIdentity._Kind = EIdentity::Fresh;
            return true;
        case ECk_HexGridInitialization::ExistingSaveKey:
            if (!_SaveKey.IsValid()) { return false; }
            OutIdentity._Kind = EIdentity::SaveKey;
            OutIdentity._SaveKey = _SaveKey;
            return true;
        case ECk_HexGridInitialization::PendingOwnedLabel:
        {
            if (!_Label.IsValid() || _Occurrence < 0 || ck::Is_NOT_Valid(InEntity)) { return false; }
            const auto Owner = UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(InEntity);
            if (ck::Is_NOT_Valid(Owner)) { return false; }
            OutIdentity._Kind = EIdentity::OwnedLabel;
            OutIdentity._Owner = Owner;
            OutIdentity._Label = _Label.ToString();
            OutIdentity._Occurrence = _Occurrence;
            return true;
        }
        case ECk_HexGridInitialization::Player:
            // The snapshot capture encodes standalone player zero with an empty string.
            OutIdentity._Kind = EIdentity::Player;
            OutIdentity._PlayerId = _PlayerId;
            return true;
        default:
            return false;
    }
}
