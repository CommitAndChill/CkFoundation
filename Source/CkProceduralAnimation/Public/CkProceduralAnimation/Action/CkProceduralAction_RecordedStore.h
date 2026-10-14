#pragma once

#include "CkProceduralAnimation/Action/CkProceduralAction_Receipt.h"

#include "CkCore/Ensure/CkEnsure.h"

// A copied, consumer-owned recording. It never retains the source player or its registry.
class CKPROCEDURALANIMATION_API FCk_ProceduralActionRecordedStore
{
    CK_GENERATED_BODY(FCk_ProceduralActionRecordedStore);

public:
    static constexpr int32 MaximumCopiedRecords = 4096;
    static constexpr int64 DefaultMaximumCopiedPoseTransforms = 262144;

    explicit FCk_ProceduralActionRecordedStore(
        int64 InMaximumCopiedPoseTransforms = DefaultMaximumCopiedPoseTransforms)
        : _MaximumCopiedPoseTransforms(InMaximumCopiedPoseTransforms)
    {
        const auto BudgetValid = InMaximumCopiedPoseTransforms >= 0;
        CK_ENSURE_IF_NOT(BudgetValid,
            TEXT("Recorded store rejected a negative pose-transform budget [{}]; it records nothing."),
            InMaximumCopiedPoseTransforms)
        { _MaximumCopiedPoseTransforms = 0; }
    }

    struct FProvenance
    {
        CK_GENERATED_BODY(FProvenance);

    private:
        FString _Fixture;
        FString _SourceAsset;
        FString _InputDescription;
        FCk_Time _FixedStep;
        uint64 _PreviewRevision = 0;

    public:
        CK_PROPERTY(_Fixture);
        CK_PROPERTY(_SourceAsset);
        CK_PROPERTY(_InputDescription);
        CK_PROPERTY(_FixedStep);
        CK_PROPERTY(_PreviewRevision);
        CK_DEFINE_CONSTRUCTORS(FProvenance, _Fixture, _SourceAsset, _InputDescription, _FixedStep, _PreviewRevision);
    };

    struct FGap
    {
        CK_GENERATED_BODY(FGap);

    private:
        int64 _First = 0;
        int64 _Last = 0;

    public:
        CK_PROPERTY_GET(_First);
        CK_PROPERTY_GET(_Last);
        CK_DEFINE_CONSTRUCTORS(FGap, _First, _Last);
    };

    auto
        Reset(
            FCk_ProceduralAction_Identity InIdentity,
            FProvenance InProvenance)
        -> void;

    auto
        AppendPage(
            const FCk_ProceduralAction_ReadPage& InPage)
        -> bool;

private:
    FCk_ProceduralAction_Identity _Identity;
    FProvenance _Provenance;
    TArray<FCk_ProceduralAction_Record> _Records;
    TArray<FGap> _Gaps;
    int64 _LastCopiedSequence = 0;
    int64 _MaximumCopiedPoseTransforms;
    int64 _CopiedPoseTransforms = 0;
    ECk_ProceduralAction_ReadResult _LastReadResult = ECk_ProceduralAction_ReadResult::Pending;
    FString _LastError;
    bool _Terminal = false;

public:
    CK_PROPERTY_GET(_Identity);
    CK_PROPERTY_GET(_Provenance);
    CK_PROPERTY_GET(_Records);
    CK_PROPERTY_GET(_Gaps);
    CK_PROPERTY_GET(_LastCopiedSequence);
    CK_PROPERTY_GET(_CopiedPoseTransforms);
    CK_PROPERTY_GET(_MaximumCopiedPoseTransforms);
    CK_PROPERTY_GET(_Terminal);
    CK_PROPERTY_GET(_LastReadResult);
    CK_PROPERTY_GET(_LastError);
};
