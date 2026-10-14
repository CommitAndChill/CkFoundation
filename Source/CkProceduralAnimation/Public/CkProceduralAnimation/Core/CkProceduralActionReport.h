#pragma once

#include "CkProceduralAnimation/Action/CkProceduralAction_Fragment_Data.h"
#include "CkProceduralAnimation/Core/CkProceduralActionValidation.h"

namespace ck::procedural_action
{
    inline auto
    MakePublicReport(
        ECk_ProceduralAction_ReportStatus InStatus,
        bool InAdvisory,
        const TArray<FDiagnostic>& InDiagnostics)
        -> FCk_ProceduralAction_ValidationReport
    {
        auto PublicDiagnostics = TArray<FCk_ProceduralAction_Diagnostic>{};
        PublicDiagnostics.Reserve(FMath::Min(InDiagnostics.Num(), content_limits::MaxDiagnostics));
        for (const auto& Diagnostic : InDiagnostics)
        {
            if (PublicDiagnostics.Num() >= content_limits::MaxDiagnostics) { break; }
            PublicDiagnostics.Emplace(
                Diagnostic.Get_Severity() == EDiagnosticSeverity::Error
                    ? ECk_ProceduralAction_DiagnosticSeverity::Error
                    : ECk_ProceduralAction_DiagnosticSeverity::Warning,
                Diagnostic.Get_Code(), Diagnostic.Get_AssetPath(), Diagnostic.Get_FieldPath(),
                Diagnostic.Get_PhaseId(), Diagnostic.Get_Slot(), Diagnostic.Get_KeyIndex(),
                Diagnostic.Get_Expected(), Diagnostic.Get_Actual());
        }
        return FCk_ProceduralAction_ValidationReport{
            InStatus, InAdvisory, MoveTemp(PublicDiagnostics)};
    }

    inline auto
    MakeReportDiagnostic(
        FName InCode,
        const FString& InAssetPath,
        const FString& InField,
        FName InSlot,
        const FString& InExpected,
        const FString& InActual)
        -> FDiagnostic
    {
        return FDiagnostic{EDiagnosticSeverity::Error, InCode, InAssetPath, InField,
            {}, InSlot, INDEX_NONE, InExpected, InActual};
    }
}
