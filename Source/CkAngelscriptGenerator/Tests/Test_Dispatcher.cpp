// Classification / action-plan logic only. The live OnAngelscriptReloadHadErrors
// path has engine-state side effects; its coverage is the `_probe_*.bat` smoke runs.

#include "CkAngelscriptGenerator/SelfHeal/CkAngelscriptGenerator_Dispatcher.h"
#include "CkAngelscriptGenerator/SelfHeal/CkAngelscriptGenerator_AsErrorParser.h"

#include "CkCore/Macros/CkMacros.h"

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace ck::angelscriptgenerator::self_heal;

namespace
{
    auto Make_NoMatchingSignatures(
        const TCHAR* InNamespace,
        const TCHAR* InFunctionName,
        const TCHAR* InArgsList = TEXT("")) -> FCk_AsParsedError
    {
        auto E = FCk_AsParsedError{};
        E.Kind             = ECk_AsParsedError_Kind::NoMatchingSignatures;
        E.TargetNamespace  = InNamespace;
        E.FunctionName     = InFunctionName;
        E.ArgsList         = InArgsList;
        E.FilePath         = TEXT("D:/Test/Caller.as");
        E.Line             = 10;
        E.Column           = 5;
        return E;
    }

    auto Make_IdentifierNotADataType(
        const TCHAR* InMissingIdentifier,
        const TCHAR* InLookupScope = TEXT(""),
        const TCHAR* InFilePath    = TEXT("D:/Test/Caller.as")) -> FCk_AsParsedError
    {
        auto E = FCk_AsParsedError{};
        E.Kind              = ECk_AsParsedError_Kind::IdentifierNotADataType;
        E.MissingIdentifier = InMissingIdentifier;
        E.LookupScope       = InLookupScope;
        E.FilePath          = InFilePath;
        E.Line              = 10;
        E.Column            = 5;
        return E;
    }

    auto Make_AdjacentStringLiteral() -> FCk_AsParsedError
    {
        auto E      = FCk_AsParsedError{};
        E.Kind      = ECk_AsParsedError_Kind::AdjacentStringLiteral;
        E.FilePath  = TEXT("D:/Test/Caller.as");
        E.Line      = 44;
        E.Column    = 13;
        return E;
    }

    auto Make_NotAMemberOfStruct(
        const TCHAR* InMissingMember,
        const TCHAR* InOwningStruct,
        const TCHAR* InFilePath = TEXT("D:/Test/Caller.as")) -> FCk_AsParsedError
    {
        auto E              = FCk_AsParsedError{};
        E.Kind              = ECk_AsParsedError_Kind::NotAMemberOfStruct;
        E.MissingIdentifier = InMissingMember;
        E.LookupScope       = InOwningStruct;
        E.FilePath          = InFilePath;
        E.Line              = 6948;
        E.Column            = 15;
        return E;
    }

    auto Make_BareCtor(
        const TCHAR* InMissingIdentifier,
        const TCHAR* InArgsList = TEXT("")) -> FCk_AsParsedError
    {
        auto E              = FCk_AsParsedError{};
        E.Kind              = ECk_AsParsedError_Kind::BareCtorNoMatchingSignatures;
        E.MissingIdentifier = InMissingIdentifier;
        E.ArgsList          = InArgsList;
        E.FilePath          = TEXT("D:/Test/Caller.as");
        E.Line              = 10;
        E.Column            = 5;
        return E;
    }
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_Dispatcher_Classify_EntitySpawnParams,
    "CkAngelscriptGenerator.UnitTests.Dispatcher.Classify_EntitySpawnParams",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkTest_Dispatcher_Classify_EntitySpawnParams::RunTest(const FString&)
{
    TestEqual(TEXT("UBb_X::Params(...) -> SynthesizeStub"),
        static_cast<int32>(FCkAsRecoveryDispatcher::Classify(
            Make_NoMatchingSignatures(TEXT("UBb_DeliveryTruck_EntityScript"), TEXT("Params"), TEXT("const FTransform")))),
        static_cast<int32>(ECk_RecoveryStrategy::SynthesizeStub_EntitySpawnParams));

    TestEqual(TEXT("UCk_X::Params() -> SynthesizeStub"),
        static_cast<int32>(FCkAsRecoveryDispatcher::Classify(
            Make_NoMatchingSignatures(TEXT("UCk_Foo_EntityScript"), TEXT("Params")))),
        static_cast<int32>(ECk_RecoveryStrategy::SynthesizeStub_EntitySpawnParams));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_Dispatcher_Classify_AssetRegistry,
    "CkAngelscriptGenerator.UnitTests.Dispatcher.Classify_AssetRegistry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkTest_Dispatcher_Classify_AssetRegistry::RunTest(const FString&)
{
    TestEqual(TEXT("assets::MALE_SKEL_NEW() -> KickGenerator_AssetRegistry"),
        static_cast<int32>(FCkAsRecoveryDispatcher::Classify(
            Make_NoMatchingSignatures(TEXT("assets"), TEXT("MALE_SKEL_NEW")))),
        static_cast<int32>(ECk_RecoveryStrategy::KickGenerator_AssetRegistry));

    TestEqual(TEXT("assets::load::X() -> KickGenerator_AssetRegistry"),
        static_cast<int32>(FCkAsRecoveryDispatcher::Classify(
            Make_NoMatchingSignatures(TEXT("assets::load"), TEXT("Foo")))),
        static_cast<int32>(ECk_RecoveryStrategy::KickGenerator_AssetRegistry));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_Dispatcher_Classify_DynamicHandle,
    "CkAngelscriptGenerator.UnitTests.Dispatcher.Classify_DynamicHandle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkTest_Dispatcher_Classify_DynamicHandle::RunTest(const FString&)
{
    TestEqual(TEXT("FCk_Handle_CheckoutCounter (global) -> DynamicHandle"),
        static_cast<int32>(FCkAsRecoveryDispatcher::Classify(
            Make_IdentifierNotADataType(TEXT("FCk_Handle_CheckoutCounter")))),
        static_cast<int32>(ECk_RecoveryStrategy::KickGenerator_DynamicHandle));

    TestEqual(TEXT("FCk_Handle_X (namespaced scope) -> DynamicHandle"),
        static_cast<int32>(FCkAsRecoveryDispatcher::Classify(
            Make_IdentifierNotADataType(TEXT("FCk_Handle_X"), TEXT("bb_checkout_cheats")))),
        static_cast<int32>(ECk_RecoveryStrategy::KickGenerator_DynamicHandle));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_Dispatcher_Classify_SpawnParamsDirectConstruction,
    "CkAngelscriptGenerator.UnitTests.Dispatcher.Classify_SpawnParamsDirectConstruction",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkTest_Dispatcher_Classify_SpawnParamsDirectConstruction::RunTest(const FString&)
{
    TestEqual(TEXT("F<X>_SpawnParams as missing declared type -> SynthesizeStub"),
        static_cast<int32>(FCkAsRecoveryDispatcher::Classify(
            Make_IdentifierNotADataType(TEXT("FBb_DayCycle_EntityScript_SpawnParams")))),
        static_cast<int32>(ECk_RecoveryStrategy::SynthesizeStub_EntitySpawnParams));

    TestEqual(TEXT("F<X>_SpawnParams bare ctor (no args) -> SynthesizeStub"),
        static_cast<int32>(FCkAsRecoveryDispatcher::Classify(
            Make_BareCtor(TEXT("FBb_CombatReceiver_DamageReceiver_SpawnParams")))),
        static_cast<int32>(ECk_RecoveryStrategy::SynthesizeStub_EntitySpawnParams));

    TestEqual(TEXT("F<X>_SpawnParams bare ctor (typed args) -> SynthesizeStub"),
        static_cast<int32>(FCkAsRecoveryDispatcher::Classify(
            Make_BareCtor(TEXT("FBb_Npc_EntityScript_SpawnParams"), TEXT("const FTransform")))),
        static_cast<int32>(ECk_RecoveryStrategy::SynthesizeStub_EntitySpawnParams));

    TestEqual(TEXT("bare ctor on a non-SpawnParams type -> Unrecognized (authoring error)"),
        static_cast<int32>(FCkAsRecoveryDispatcher::Classify(
            Make_BareCtor(TEXT("FVector"), TEXT("float32")))),
        static_cast<int32>(ECk_RecoveryStrategy::Unrecognized));

    TestEqual(TEXT("non-F-prefixed *_SpawnParams -> Unrecognized"),
        static_cast<int32>(FCkAsRecoveryDispatcher::Classify(
            Make_BareCtor(TEXT("Bb_SpawnParams")))),
        static_cast<int32>(ECk_RecoveryStrategy::Unrecognized));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_Dispatcher_Classify_AdjacentStringLiteral,
    "CkAngelscriptGenerator.UnitTests.Dispatcher.Classify_AdjacentStringLiteral",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkTest_Dispatcher_Classify_AdjacentStringLiteral::RunTest(const FString&)
{
    TestEqual(TEXT("AdjacentStringLiteral -> Author_FixupRequired_AdjacentStringLiteral"),
        static_cast<int32>(FCkAsRecoveryDispatcher::Classify(Make_AdjacentStringLiteral())),
        static_cast<int32>(ECk_RecoveryStrategy::Author_FixupRequired_AdjacentStringLiteral));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_Dispatcher_Classify_Unrecognized,
    "CkAngelscriptGenerator.UnitTests.Dispatcher.Classify_Unrecognized",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkTest_Dispatcher_Classify_Unrecognized::RunTest(const FString&)
{
    TestEqual(TEXT("NoMatch with non-Params function on U namespace -> Unrecognized"),
        static_cast<int32>(FCkAsRecoveryDispatcher::Classify(
            Make_NoMatchingSignatures(TEXT("UBb_X_EntityScript"), TEXT("NotParams")))),
        static_cast<int32>(ECk_RecoveryStrategy::Unrecognized));

    TestEqual(TEXT("NoMatch with non-U / non-assets namespace -> Unrecognized"),
        static_cast<int32>(FCkAsRecoveryDispatcher::Classify(
            Make_NoMatchingSignatures(TEXT("FCk_Handle"), TEXT("As_CheckoutCounter")))),
        static_cast<int32>(ECk_RecoveryStrategy::Unrecognized));

    TestEqual(TEXT("Identifier not a data type but not FCk_Handle_ -> Unrecognized"),
        static_cast<int32>(FCkAsRecoveryDispatcher::Classify(
            Make_IdentifierNotADataType(TEXT("FBb_SomeRandomStruct")))),
        static_cast<int32>(ECk_RecoveryStrategy::Unrecognized));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------
// LOCATION is the discriminator for a deleted type that is neither a dynamic
// handle nor a SpawnParams struct: only gitignored *_EntitySpawnParams.as
// canonicals are delete-safe, so only they quarantine.
// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_Dispatcher_Classify_StaleEspCanonical,
    "CkAngelscriptGenerator.UnitTests.Dispatcher.Classify_StaleEspCanonical",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkTest_Dispatcher_Classify_StaleEspCanonical::RunTest(const FString&)
{
    TestEqual(TEXT("deleted enum inside project ESP canonical -> Quarantine"),
        static_cast<int32>(FCkAsRecoveryDispatcher::Classify(
            Make_IdentifierNotADataType(TEXT("EBb_NamedNpc"), TEXT(""),
                TEXT("D:/Repos/BusterBlock/Script/Generated/BusterBlock_EntitySpawnParams.as")))),
        static_cast<int32>(ECk_RecoveryStrategy::Quarantine_StaleEspCanonical));

    TestEqual(TEXT("deleted enum inside plugin ESP canonical (backslashes) -> Quarantine"),
        static_cast<int32>(FCkAsRecoveryDispatcher::Classify(
            Make_IdentifierNotADataType(TEXT("EBb_Employee"), TEXT("UBb_Npc_EntityScript"),
                TEXT("D:\\Repos\\BusterBlock\\Plugins\\BusterBlockTests\\Script\\Generated\\BusterBlockTests_EntitySpawnParams.as")))),
        static_cast<int32>(ECk_RecoveryStrategy::Quarantine_StaleEspCanonical));

    // The identical error in AUTHOR source is a real authoring bug — never
    // quarantine user code.
    TestEqual(TEXT("deleted type in author source -> Unrecognized"),
        static_cast<int32>(FCkAsRecoveryDispatcher::Classify(
            Make_IdentifierNotADataType(TEXT("EBb_NamedNpc"), TEXT(""),
                TEXT("D:/Repos/BusterBlock/Script/Npc/BB_Npc_EntityScript.as")))),
        static_cast<int32>(ECk_RecoveryStrategy::Unrecognized));

    // Tracked generated files are not ESP canonicals — deleting them would
    // destroy committed state.
    TestEqual(TEXT("deleted type in tracked generated Assets file -> Unrecognized"),
        static_cast<int32>(FCkAsRecoveryDispatcher::Classify(
            Make_IdentifierNotADataType(TEXT("EBb_NamedNpc"), TEXT(""),
                TEXT("D:/Repos/BusterBlock/Script/Generated/BusterBlockAssets.as")))),
        static_cast<int32>(ECk_RecoveryStrategy::Unrecognized));

    TestEqual(TEXT("deleted type in tracked AutoTestActors file -> Unrecognized"),
        static_cast<int32>(FCkAsRecoveryDispatcher::Classify(
            Make_IdentifierNotADataType(TEXT("EBb_NamedNpc"), TEXT(""),
                TEXT("D:/Repos/BusterBlock/Script/Generated/BusterBlock_AutoTestActors.as")))),
        static_cast<int32>(ECk_RecoveryStrategy::Unrecognized));

    // A sibling recovery stub carries the canonical suffix but is our OWN output.
    // Pinned for THIS kind too: the guard lives in the shared path predicate, so
    // moving it into either Classify arm alone must fail here.
    // Its canonical must be absent (a present one turns this into a discard), so the path is one no
    // checkout on this machine can supply.
    TestEqual(TEXT("deleted type in _StubRecovery_ sibling (canonical absent) -> Unrecognized"),
        static_cast<int32>(FCkAsRecoveryDispatcher::Classify(
            Make_IdentifierNotADataType(TEXT("EBb_NamedNpc"), TEXT(""),
                *(FPaths::ConvertRelativePathToFull(FPaths::ProjectIntermediateDir())
                    / TEXT("CkDispatcherTest_NoCanonical/Script/Generated/_StubRecovery_BusterBlock_EntitySpawnParams.as"))))),
        static_cast<int32>(ECk_RecoveryStrategy::Unrecognized));

    // The location check is a FALLBACK after the handle / SpawnParams checks.
    TestEqual(TEXT("FCk_Handle_ inside canonical still -> DynamicHandle (not Quarantine)"),
        static_cast<int32>(FCkAsRecoveryDispatcher::Classify(
            Make_IdentifierNotADataType(TEXT("FCk_Handle_CheckoutCounter"), TEXT(""),
                TEXT("D:/Repos/BusterBlock/Script/Generated/BusterBlock_EntitySpawnParams.as")))),
        static_cast<int32>(ECk_RecoveryStrategy::KickGenerator_DynamicHandle));

    TestEqual(TEXT("F<X>_SpawnParams inside canonical still -> SynthesizeStub (not Quarantine)"),
        static_cast<int32>(FCkAsRecoveryDispatcher::Classify(
            Make_IdentifierNotADataType(TEXT("FBb_Npc_EntityScript_SpawnParams"), TEXT(""),
                TEXT("D:/Repos/BusterBlock/Script/Generated/BusterBlock_EntitySpawnParams.as")))),
        static_cast<int32>(ECk_RecoveryStrategy::SynthesizeStub_EntitySpawnParams));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------
// Same LOCATION discriminator for a deleted FIELD (the 2026-08-07 OpenSign wedge):
// `LocalRotationOffset` was dropped from the params struct while machine-local
// canonicals kept assigning it, and the boot had no recognized root cause.
// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_Dispatcher_Classify_StaleEspCanonical_DeletedField,
    "CkAngelscriptGenerator.UnitTests.Dispatcher.Classify_StaleEspCanonical_DeletedField",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkTest_Dispatcher_Classify_StaleEspCanonical_DeletedField::RunTest(const FString&)
{
    TestEqual(TEXT("deleted field inside project ESP canonical -> Quarantine"),
        static_cast<int32>(FCkAsRecoveryDispatcher::Classify(
            Make_NotAMemberOfStruct(TEXT("LocalRotationOffset"), TEXT("FBb_Fragment_OpenSign_ParamsData"),
                TEXT("D:/Repos/BusterBlock/Script/Generated/BusterBlock_EntitySpawnParams.as")))),
        static_cast<int32>(ECk_RecoveryStrategy::Quarantine_StaleEspCanonical));

    TestEqual(TEXT("deleted field inside plugin ESP canonical (backslashes) -> Quarantine"),
        static_cast<int32>(FCkAsRecoveryDispatcher::Classify(
            Make_NotAMemberOfStruct(TEXT("LocalRotationOffset"), TEXT("FBb_Fragment_OpenSign_ParamsData"),
                TEXT("D:\\Repos\\BusterBlock\\Plugins\\BusterBlockTests\\Script\\Generated\\BusterBlockTests_EntitySpawnParams.as")))),
        static_cast<int32>(ECk_RecoveryStrategy::Quarantine_StaleEspCanonical));

    // The identical error in AUTHOR source is a real authoring bug.
    TestEqual(TEXT("deleted field in author source -> Unrecognized"),
        static_cast<int32>(FCkAsRecoveryDispatcher::Classify(
            Make_NotAMemberOfStruct(TEXT("LocalRotationOffset"), TEXT("FBb_Fragment_OpenSign_ParamsData"),
                TEXT("D:/Repos/BusterBlock/Script/ECS/OpenSign/BB_OpenSign_EntityScript.as")))),
        static_cast<int32>(ECk_RecoveryStrategy::Unrecognized));

    // Tracked generated files are never delete-safe.
    TestEqual(TEXT("deleted field in tracked generated Assets file -> Unrecognized"),
        static_cast<int32>(FCkAsRecoveryDispatcher::Classify(
            Make_NotAMemberOfStruct(TEXT("DeadField"), TEXT("FBb_Fragment_Foo_ParamsData"),
                TEXT("D:/Repos/BusterBlock/Script/Generated/BusterBlockAssets.as")))),
        static_cast<int32>(ECk_RecoveryStrategy::Unrecognized));

    // A sibling recovery stub carries the canonical suffix but is our OWN output —
    // quarantining it would derive a doubly-prefixed sibling from a stub.
    TestEqual(TEXT("deleted field in _StubRecovery_ sibling (canonical absent) -> Unrecognized"),
        static_cast<int32>(FCkAsRecoveryDispatcher::Classify(
            Make_NotAMemberOfStruct(TEXT("DeadField"), TEXT("FBb_Fragment_Foo_ParamsData"),
                *(FPaths::ConvertRelativePathToFull(FPaths::ProjectIntermediateDir())
                    / TEXT("CkDispatcherTest_NoCanonical/Script/Generated/_StubRecovery_BusterBlock_EntitySpawnParams.as"))))),
        static_cast<int32>(ECk_RecoveryStrategy::Unrecognized));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------
// The ADDED-field twin, reported at the CALLER (the 2026-11 Mars `SpawnAtWorld`
// wedge): a change adds an ExposeOnSpawn field and a caller that sets it, and every
// machine-local canonical generated before it lacks the field. The fixture is a
// throwaway project tree, because the evidence lives on disk, in both the source and the canonical.
// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_Dispatcher_Classify_StaleEspCanonical_AddedField,
    "CkAngelscriptGenerator.UnitTests.Dispatcher.Classify_StaleEspCanonical_AddedField",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkTest_Dispatcher_Classify_StaleEspCanonical_AddedField::RunTest(const FString&)
{
    const auto TempRoot = FPaths::ConvertRelativePathToFull(
        FPaths::ProjectIntermediateDir() / TEXT("CkDispatcherTest_AddedField"));
    IFileManager::Get().DeleteDirectory(*TempRoot, /*RequireExists=*/false, /*Tree=*/true);

    const auto ScriptRoot = TempRoot / TEXT("Script");
    const auto Canonical  = ScriptRoot / TEXT("Generated/Probe_EntitySpawnParams.as");
    const auto Caller     = ScriptRoot / TEXT("Probe/Probe_Caller.as");

    const auto Write = [](const FString& InPath, const TCHAR* InContents)
    {
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(InPath), /*Tree=*/true);
        FFileHelper::SaveStringToFile(FString{InContents}, *InPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    };

    // Current source: NewField was added after the canonical below was generated.
    Write(ScriptRoot / TEXT("Probe/Probe_EntityScript.as"), TEXT(
        "class UCkProbe_AddedField_EntityScript\n"
        "{\n"
        "    UPROPERTY(ExposeOnSpawn)\n"
        "    int32 OldField = 0;\n"
        "\n"
        "    UPROPERTY(ExposeOnSpawn)\n"
        "    TOptional<FTransform> NewField;\n"
        "\n"
        "    UPROPERTY()\n"
        "    int32 NotExposed = 0;\n"
        "}\n"));

    // The stale machine-local canonical, generated before NewField existed.
    Write(Canonical, TEXT(
        "namespace UCkProbe_AddedField_EntityScript\n"
        "{\n"
        "    FCkProbe_AddedField_EntityScript_SpawnParams Params(int32 OldField) { return FCkProbe_AddedField_EntityScript_SpawnParams(); }\n"
        "}\n"
        "struct FCkProbe_AddedField_EntityScript_SpawnParams\n"
        "{\n"
        "    int32 OldField = 0;\n"
        "}\n"));

    const auto Candidates = TArray<FString>{Canonical};
    const auto ScanRoots  = TArray<FString>{ScriptRoot};

    const auto ClassifyAt = [&](const FCk_AsParsedError& InError, const TArray<FString>& InCandidates) -> int32
    {
        auto Cache = FCk_AsSourceScanCache{};
        return static_cast<int32>(FCkAsRecoveryDispatcher::Classify(InError, InCandidates, ScanRoots, &Cache));
    };

    const auto AddedField = Make_NotAMemberOfStruct(
        TEXT("NewField"), TEXT("FCkProbe_AddedField_EntityScript_SpawnParams"), *Caller);

    TestEqual(TEXT("added ExposeOnSpawn field set by a caller, canonical predates it -> Quarantine"),
        ClassifyAt(AddedField, Candidates),
        static_cast<int32>(ECk_RecoveryStrategy::Quarantine_StaleEspCanonical));

    {
        auto Cache = FCk_AsSourceScanCache{};
        TestEqual(TEXT("added field resolves the canonical defining the struct (not the caller)"),
            FCkAsRecoveryDispatcher::Find_StaleCanonical_ForAddedField(AddedField, Candidates, ScanRoots, &Cache),
            Canonical);
    }

    // Apply deletes the canonical Classify resolved, carried on the action, so it must be the canonical and not the caller.
    {
        auto Cache    = FCk_AsSourceScanCache{};
        auto Resolved = FString{TEXT("<stale>")};
        FCkAsRecoveryDispatcher::Classify(AddedField, Candidates, ScanRoots, &Cache, &Resolved);
        TestEqual(TEXT("Classify carries the added-field canonical out"), Resolved, Canonical);

        FCkAsRecoveryDispatcher::Classify(Make_NotAMemberOfStruct(
            TEXT("NewFeild"), TEXT("FCkProbe_AddedField_EntityScript_SpawnParams"), *Caller), Candidates, ScanRoots, &Cache, &Resolved);
        TestTrue(TEXT("Unrecognized clears the carried canonical"), Resolved.IsEmpty());

        FCkAsRecoveryDispatcher::Classify(Make_NotAMemberOfStruct(
            TEXT("DeadField"), TEXT("FCkProbe_AddedField_EntityScript_SpawnParams"), *Canonical), {}, {}, nullptr, &Resolved);
        TestEqual(TEXT("located-in-canonical carries the error's own file"), Resolved, Canonical);
    }

    // A genuine authoring typo must never delete anything.
    TestEqual(TEXT("member absent from source -> Unrecognized"),
        ClassifyAt(Make_NotAMemberOfStruct(
            TEXT("NewFeild"), TEXT("FCkProbe_AddedField_EntityScript_SpawnParams"), *Caller), Candidates),
        static_cast<int32>(ECk_RecoveryStrategy::Unrecognized));

    TestEqual(TEXT("member declared without ExposeOnSpawn -> Unrecognized (the generator never emits it)"),
        ClassifyAt(Make_NotAMemberOfStruct(
            TEXT("NotExposed"), TEXT("FCkProbe_AddedField_EntityScript_SpawnParams"), *Caller), Candidates),
        static_cast<int32>(ECk_RecoveryStrategy::Unrecognized));

    TestEqual(TEXT("member case differs from source -> Unrecognized (AS identifiers are case-sensitive)"),
        ClassifyAt(Make_NotAMemberOfStruct(
            TEXT("newfield"), TEXT("FCkProbe_AddedField_EntityScript_SpawnParams"), *Caller), Candidates),
        static_cast<int32>(ECk_RecoveryStrategy::Unrecognized));

    TestEqual(TEXT("non-SpawnParams struct -> Unrecognized"),
        ClassifyAt(Make_NotAMemberOfStruct(
            TEXT("NewField"), TEXT("FCkProbe_AddedField_Fragment_ParamsData"), *Caller), Candidates),
        static_cast<int32>(ECk_RecoveryStrategy::Unrecognized));

    TestEqual(TEXT("SpawnParams of a class with no source -> Unrecognized"),
        ClassifyAt(Make_NotAMemberOfStruct(
            TEXT("NewField"), TEXT("FCkProbe_NoSuchClass_EntityScript_SpawnParams"), *Caller), Candidates),
        static_cast<int32>(ECk_RecoveryStrategy::Unrecognized));

    // On a fresh clone the struct comes from a recovery stub and no canonical exists, so nothing is stale.
    TestEqual(TEXT("no present canonical -> Unrecognized"),
        ClassifyAt(AddedField, TArray<FString>{}),
        static_cast<int32>(ECk_RecoveryStrategy::Unrecognized));

    // Two canonicals that both define the struct are ambiguous. The fix must not choose one to delete.
    const auto SecondCanonical = ScriptRoot / TEXT("Generated/ProbeTwo_EntitySpawnParams.as");
    Write(SecondCanonical, TEXT(
        "struct FCkProbe_AddedField_EntityScript_SpawnParams\n"
        "{\n"
        "    int32 OldField = 0;\n"
        "}\n"));
    TestEqual(TEXT("struct defined in two canonicals -> Unrecognized"),
        ClassifyAt(AddedField, TArray<FString>{Canonical, SecondCanonical}),
        static_cast<int32>(ECk_RecoveryStrategy::Unrecognized));

    // A sibling recovery stub is never a quarantine target, even if passed as a candidate.
    const auto Sibling = ScriptRoot / TEXT("Generated/_StubRecovery_Probe_EntitySpawnParams.as");
    Write(Sibling, TEXT(
        "struct FCkProbe_AddedField_EntityScript_SpawnParams\n"
        "{\n"
        "    int32 OldField = 0;\n"
        "}\n"));
    TestEqual(TEXT("struct defined only in a _StubRecovery_ sibling -> Unrecognized"),
        ClassifyAt(AddedField, TArray<FString>{Sibling}),
        static_cast<int32>(ECk_RecoveryStrategy::Unrecognized));

    // The located-in-canonical (deleted-field) rule still wins without any probe input.
    TestEqual(TEXT("error inside the canonical still -> Quarantine with no probe inputs"),
        static_cast<int32>(FCkAsRecoveryDispatcher::Classify(
            Make_NotAMemberOfStruct(TEXT("NewField"), TEXT("FCkProbe_AddedField_EntityScript_SpawnParams"), *Canonical),
            {}, {}, nullptr)),
        static_cast<int32>(ECk_RecoveryStrategy::Quarantine_StaleEspCanonical));

    IFileManager::Get().DeleteDirectory(*TempRoot, /*RequireExists=*/false, /*Tree=*/true);
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_Dispatcher_BuildActionPlan,
    "CkAngelscriptGenerator.UnitTests.Dispatcher.BuildActionPlan",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkTest_Dispatcher_BuildActionPlan::RunTest(const FString&)
{
    const auto Roots = TArray<FCk_AsParsedError>{
        Make_NoMatchingSignatures(TEXT("UBb_Foo_EntityScript"), TEXT("Params"), TEXT("FTransform")),
        Make_IdentifierNotADataType(TEXT("FCk_Handle_X")),
        Make_NoMatchingSignatures(TEXT("assets"), TEXT("MALE_SKEL_NEW")),
        Make_NoMatchingSignatures(TEXT("FCk_Handle"), TEXT("As_X")),  // -> Unrecognized
    };

    const auto Plan = FCkAsRecoveryDispatcher::BuildActionPlan(Roots);

    TestEqual(TEXT("plan size matches roots"), Plan.Num(), 4);
    if (Plan.Num() < 4)
    { return false; }

    TestEqual(TEXT("[0] strategy"), static_cast<int32>(Plan[0].Strategy), static_cast<int32>(ECk_RecoveryStrategy::SynthesizeStub_EntitySpawnParams));
    TestEqual(TEXT("[1] strategy"), static_cast<int32>(Plan[1].Strategy), static_cast<int32>(ECk_RecoveryStrategy::KickGenerator_DynamicHandle));
    TestEqual(TEXT("[2] strategy"), static_cast<int32>(Plan[2].Strategy), static_cast<int32>(ECk_RecoveryStrategy::KickGenerator_AssetRegistry));
    TestEqual(TEXT("[3] strategy"), static_cast<int32>(Plan[3].Strategy), static_cast<int32>(ECk_RecoveryStrategy::Unrecognized));

    TestEqual(TEXT("[0] error preserved"), Plan[0].Error.TargetNamespace, FString{TEXT("UBb_Foo_EntityScript")});
    TestEqual(TEXT("[1] error preserved"), Plan[1].Error.MissingIdentifier, FString{TEXT("FCk_Handle_X")});

    return true;
}

// --------------------------------------------------------------------------------------------------------------------
// A stub kept beside a present canonical can outlive the source it was derived from (another
// branch's failed run), and then fails to compile by itself. LOCATION decides, ahead of every
// kind-specific branch, but only while the canonical is on disk: without it the stub is the whole
// bootstrap. The fixture is a throwaway tree, because that evidence lives on disk.
// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_Dispatcher_Classify_StaleEspRecoveryStub,
    "CkAngelscriptGenerator.UnitTests.Dispatcher.Classify_StaleEspRecoveryStub",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkTest_Dispatcher_Classify_StaleEspRecoveryStub::RunTest(const FString&)
{
    const auto TempRoot = FPaths::ConvertRelativePathToFull(
        FPaths::ProjectIntermediateDir() / TEXT("CkDispatcherTest_RecoveryStub"));
    IFileManager::Get().DeleteDirectory(*TempRoot, /*RequireExists=*/false, /*Tree=*/true);

    const auto Generated = TempRoot / TEXT("Script/Generated");
    const auto Canonical = Generated / TEXT("Probe_EntitySpawnParams.as");
    const auto Stub      = Generated / TEXT("_StubRecovery_Probe_EntitySpawnParams.as");

    const auto Write = [](const FString& InPath)
    {
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(InPath), /*Tree=*/true);
        FFileHelper::SaveStringToFile(FString{TEXT("// probe\n")}, *InPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    };
    const auto ClassifyAt = [](const TCHAR* InMissingIdentifier, const FString& InFilePath) -> int32
    {
        return static_cast<int32>(FCkAsRecoveryDispatcher::Classify(
            Make_IdentifierNotADataType(InMissingIdentifier, TEXT(""), *InFilePath)));
    };

    Write(Stub);

    TestEqual(TEXT("missing type inside an ESP stub, canonical ABSENT -> Unrecognized (the stub is the bootstrap)"),
        ClassifyAt(TEXT("EBb_OtherBranchOnly"), Stub),
        static_cast<int32>(ECk_RecoveryStrategy::Unrecognized));

    TestEqual(TEXT("F<X>_SpawnParams inside an ESP stub, canonical ABSENT -> SynthesizeStub as before"),
        ClassifyAt(TEXT("FBb_Npc_EntityScript_SpawnParams"), Stub),
        static_cast<int32>(ECk_RecoveryStrategy::SynthesizeStub_EntitySpawnParams));

    Write(Canonical);

    TestEqual(TEXT("missing type inside an ESP stub, canonical present -> Discard"),
        ClassifyAt(TEXT("EBb_OtherBranchOnly"), Stub),
        static_cast<int32>(ECk_RecoveryStrategy::Discard_StaleEspRecoveryStub));

    TestEqual(TEXT("missing type inside an ESP stub (backslashes), canonical present -> Discard"),
        ClassifyAt(TEXT("EBb_OtherBranchOnly"), Stub.Replace(TEXT("/"), TEXT("\\"))),
        static_cast<int32>(ECk_RecoveryStrategy::Discard_StaleEspRecoveryStub));

    TestEqual(TEXT("F<X>_SpawnParams inside an ESP stub, canonical present -> Discard (decided before the SynthesizeStub branch)"),
        ClassifyAt(TEXT("FBb_Npc_EntityScript_SpawnParams"), Stub),
        static_cast<int32>(ECk_RecoveryStrategy::Discard_StaleEspRecoveryStub));

    TestEqual(TEXT("deleted field inside an ESP stub, canonical present -> Discard"),
        static_cast<int32>(FCkAsRecoveryDispatcher::Classify(
            Make_NotAMemberOfStruct(TEXT("DeadField"), TEXT("FBb_Fragment_Foo_ParamsData"), *Stub))),
        static_cast<int32>(ECk_RecoveryStrategy::Discard_StaleEspRecoveryStub));

    // A stub field may name a dynamic handle the JSON registry lacks; the stub is sound.
    TestEqual(TEXT("missing FCk_Handle_ inside an ESP stub -> still DynamicHandle"),
        ClassifyAt(TEXT("FCk_Handle_CombatReceiver"), Stub),
        static_cast<int32>(ECk_RecoveryStrategy::KickGenerator_DynamicHandle));

    TestEqual(TEXT("same missing type inside the canonical -> still Quarantine"),
        ClassifyAt(TEXT("EBb_OtherBranchOnly"), Canonical),
        static_cast<int32>(ECk_RecoveryStrategy::Quarantine_StaleEspCanonical));

    TestEqual(TEXT("same missing type in author source -> still Unrecognized"),
        ClassifyAt(TEXT("EBb_OtherBranchOnly"), TempRoot / TEXT("Script/Npc/Probe_EntityScript.as")),
        static_cast<int32>(ECk_RecoveryStrategy::Unrecognized));

    const auto AssetsStub = Generated / TEXT("_StubRecovery_ProbeAssets.as");
    Write(Generated / TEXT("ProbeAssets.as"));
    Write(AssetsStub);
    TestEqual(TEXT("same missing type in an AssetRegistry stub, its canonical present -> unchanged (Unrecognized)"),
        ClassifyAt(TEXT("EBb_OtherBranchOnly"), AssetsStub),
        static_cast<int32>(ECk_RecoveryStrategy::Unrecognized));

    const auto OutsideGenerated = TempRoot / TEXT("Script/Npc/_StubRecovery_Probe_EntitySpawnParams.as");
    Write(TempRoot / TEXT("Script/Npc/Probe_EntitySpawnParams.as"));
    Write(OutsideGenerated);
    TestEqual(TEXT("ESP-stub-shaped name outside Script/Generated -> Unrecognized"),
        ClassifyAt(TEXT("EBb_OtherBranchOnly"), OutsideGenerated),
        static_cast<int32>(ECk_RecoveryStrategy::Unrecognized));

    IFileManager::Get().DeleteDirectory(*TempRoot, /*RequireExists=*/false, /*Tree=*/true);
    return true;
}

// --------------------------------------------------------------------------------------------------------------------
// The discard and the drain's synthesis target the same sibling file. Discards go first so the fresh
// stubs land in a clean file, and every error inside one stub is one discard.
// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_Dispatcher_BuildActionPlan_StubDiscard,
    "CkAngelscriptGenerator.UnitTests.Dispatcher.BuildActionPlan_StubDiscard",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkTest_Dispatcher_BuildActionPlan_StubDiscard::RunTest(const FString&)
{
    const auto TempRoot = FPaths::ConvertRelativePathToFull(
        FPaths::ProjectIntermediateDir() / TEXT("CkDispatcherTest_StubDiscardPlan"));
    IFileManager::Get().DeleteDirectory(*TempRoot, /*RequireExists=*/false, /*Tree=*/true);

    const auto Write = [](const FString& InPath)
    {
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(InPath), /*Tree=*/true);
        FFileHelper::SaveStringToFile(FString{TEXT("// probe\n")}, *InPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    };

    const auto StubA       = TempRoot / TEXT("Script/Generated/_StubRecovery_ProbeA_EntitySpawnParams.as");
    const auto StubB       = TempRoot / TEXT("Plugins/Foo/Script/Generated/_StubRecovery_Foo_EntitySpawnParams.as");
    const auto StubNoCanon = TempRoot / TEXT("Plugins/Bar/Script/Generated/_StubRecovery_Bar_EntitySpawnParams.as");
    Write(StubA);
    Write(TempRoot / TEXT("Script/Generated/ProbeA_EntitySpawnParams.as"));
    Write(StubB);
    Write(TempRoot / TEXT("Plugins/Foo/Script/Generated/Foo_EntitySpawnParams.as"));
    Write(StubNoCanon);

    auto SecondErrorInStubA = Make_IdentifierNotADataType(TEXT("EBb_OtherBranchOnlyB"), TEXT(""), *StubA);
    SecondErrorInStubA.Line = 77;

    const auto Errors = TArray<FCk_AsParsedError>{
        Make_NoMatchingSignatures(TEXT("UBb_BrandNew_EntityScript"), TEXT("Params"), TEXT("int32")),
        Make_IdentifierNotADataType(TEXT("EBb_OtherBranchOnlyA"), TEXT(""), *StubA),
        Make_IdentifierNotADataType(TEXT("FBb_AlsoNew_EntityScript_SpawnParams")),
        Make_IdentifierNotADataType(TEXT("EBb_CascadeInBootstrap"), TEXT(""), *StubNoCanon),
        SecondErrorInStubA,
        Make_IdentifierNotADataType(TEXT("EBb_OtherBranchOnlyC"), TEXT(""), *StubB),
    };

    const auto Roots = FCkAsErrorParser::DeduplicateRoots(Errors);
    TestEqual(TEXT("DeduplicateRoots keeps distinct identifiers (the per-stub collapse is the plan's)"), Roots.Num(), 6);

    const auto Plan = FCkAsRecoveryDispatcher::BuildActionPlan(Roots);

    TestEqual(TEXT("two discards + two syntheses + one bootstrap-stub error -> five actions"), Plan.Num(), 5);
    if (Plan.Num() != 5)
    {
        IFileManager::Get().DeleteDirectory(*TempRoot, /*RequireExists=*/false, /*Tree=*/true);
        return false;
    }

    TestEqual(TEXT("[0] discard"), static_cast<int32>(Plan[0].Strategy), static_cast<int32>(ECk_RecoveryStrategy::Discard_StaleEspRecoveryStub));
    TestEqual(TEXT("[0] first stub"), Plan[0].Error.FilePath, StubA);
    TestEqual(TEXT("[1] discard"), static_cast<int32>(Plan[1].Strategy), static_cast<int32>(ECk_RecoveryStrategy::Discard_StaleEspRecoveryStub));
    TestEqual(TEXT("[1] second stub"), Plan[1].Error.FilePath, StubB);

    TestEqual(TEXT("[2] synthesis keeps its relative order"), static_cast<int32>(Plan[2].Strategy), static_cast<int32>(ECk_RecoveryStrategy::SynthesizeStub_EntitySpawnParams));
    TestEqual(TEXT("[2] is the Params miss"), Plan[2].Error.TargetNamespace, FString{TEXT("UBb_BrandNew_EntityScript")});
    TestEqual(TEXT("[3] synthesis"), static_cast<int32>(Plan[3].Strategy), static_cast<int32>(ECk_RecoveryStrategy::SynthesizeStub_EntitySpawnParams));
    TestEqual(TEXT("[3] is the direct construction"), Plan[3].Error.MissingIdentifier, FString{TEXT("FBb_AlsoNew_EntityScript_SpawnParams")});
    TestEqual(TEXT("[4] bootstrap stub (no canonical) is left alone"), static_cast<int32>(Plan[4].Strategy), static_cast<int32>(ECk_RecoveryStrategy::Unrecognized));
    TestEqual(TEXT("[4] is the bootstrap stub"), Plan[4].Error.FilePath, StubNoCanon);

    IFileManager::Get().DeleteDirectory(*TempRoot, /*RequireExists=*/false, /*Tree=*/true);
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_Dispatcher_CycleCounter_Reset,
    "CkAngelscriptGenerator.UnitTests.Dispatcher.CycleCounter_Reset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkTest_Dispatcher_CycleCounter_Reset::RunTest(const FString&)
{
    FCkAsRecoveryDispatcher::Reset_CyclesRun();
    TestEqual(TEXT("post-reset counter is 0"), FCkAsRecoveryDispatcher::Get_CyclesRun(), 0);

    TestEqual(TEXT("MaxCycles is 3 (CTO Rev 10 pushback #2)"),
        FCkAsRecoveryDispatcher::MaxCycles, 3);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------
// NOTE: mutates global session state (sBootstrapComplete + sCyclesRun) and does
// NOT restore it — the bootstrap flag has editor-session lifetime by design.
// Safe only while no other test reads it.
// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_Dispatcher_BootstrapMode_FlipFlop,
    "CkAngelscriptGenerator.UnitTests.Dispatcher.BootstrapMode_FlipFlop",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkTest_Dispatcher_BootstrapMode_FlipFlop::RunTest(const FString&)
{
    // Only the transition is asserted, not the initial value — another test may
    // already have called Mark_BootstrapComplete.
    const auto WasBootstrap = FCkAsRecoveryDispatcher::Is_BootstrapMode();

    FCkAsRecoveryDispatcher::Mark_BootstrapComplete();
    TestFalse(TEXT("after Mark_BootstrapComplete, Is_BootstrapMode returns false"),
        FCkAsRecoveryDispatcher::Is_BootstrapMode());

    TestEqual(TEXT("cycle counter reset to 0 at bootstrap->mid-session transition"),
        FCkAsRecoveryDispatcher::Get_CyclesRun(), 0);

    (void)WasBootstrap;
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
