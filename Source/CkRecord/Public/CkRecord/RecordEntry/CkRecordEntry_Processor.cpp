#include "CkRecordEntry_Processor.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"

#include "CkRecord/Record/CkRecord_Fragment.h"
#include "CkRecord/Record/CkRecord_Utils.h"

#include "CkEcs/Scheduler/CkProcessorRegistration.h"

CK_REGISTER_PROCESSOR(ck::FProcessor_RecordEntry_Destructor);

// --------------------------------------------------------------------------------------------------------------------

namespace ck
{
    auto
        FProcessor_RecordEntry_Destructor::
        ForEachEntity(
            TimeType,
            HandleType InHandle,
            FFragment_RecordEntry& InRecordEntry) const
        -> void
    {
        const auto Disconnections = InRecordEntry._DisconnectionFuncs;
        for (const auto& Kvp : Disconnections)
        {
            const auto& RecordEntity = Kvp.Key.Key;

            if (ck::Is_NOT_Valid(RecordEntity))
            { continue; }

            const auto& DestructionCleanupFunc = Kvp.Value;

            DestructionCleanupFunc(RecordEntity, InHandle);
        }
        InRecordEntry._Records.RemoveAll([](const auto& InRecord) { return ck::Is_NOT_Valid(InRecord); });
        for (auto It = InRecordEntry._DisconnectionFuncs.CreateIterator(); It; ++It)
        {
            if (ck::Is_NOT_Valid(It.Key().Key))
            { It.RemoveCurrent(); }
        }
        CK_ENSURE_IF_NOT(InRecordEntry._Records.IsEmpty() && InRecordEntry._DisconnectionFuncs.IsEmpty(),
            TEXT("RecordEntry [{}] teardown left connections to live records"), InHandle)
        { return; }
    }
}

// --------------------------------------------------------------------------------------------------------------------
