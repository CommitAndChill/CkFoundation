#include "CkAutoTestWrapperGenerator.h"

#include "CkAngelscriptGenerator/AutoTests/CkAutoTestNetStubGenerator.h"
#include "CkAngelscriptGenerator/CkAngelscriptGenerator_Log.h"
#include "CkAngelscriptGenerator/CkAngelscriptGenerator_PluginLookup.h"
#include "CkAngelscriptGenerator/CkAngelscriptGenerator_RegenOwnership.h"
#include "CkAngelscriptGenerator/SelfHeal/CkAngelscriptGenerator_AsSourceScanner.h"

#include "CkCore/Ensure/CkEnsure.h"
#include "CkCore/Format/CkFormat.h"
#include "CkCore/Reflection/CkReflection_Utils.h"

#include <HAL/FileManager.h>
#include <Hash/Blake3.h>
#include <Internationalization/Regex.h>
#include <Misc/App.h>
#include <Misc/FileHelper.h>
#include <Misc/Paths.h>
#include <ProfilingDebugging/CpuProfilerTrace.h>
#include <UObject/Class.h>
#include <UObject/UObjectIterator.h>

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include <Misc/AutomationTest.h>
#include <Misc/ScopeExit.h>
#endif

#if WITH_ANGELSCRIPT_CK
#include "ClassGenerator/ASClass.h"
#endif

// --------------------------------------------------------------------------------------------------------------------

namespace ck_autotest_wrapper_generator
{
    auto
        Collect_DeclaredWrappersFromFile(
            const FString& InContents,
            const FString& InSourceFilePath,
            TSet<FString>& OutNames)
        -> void
    {
        using ck::angelscriptgenerator::self_heal::FCkAsSourceScanner;

        // The regex finds candidates in raw text. The parser rejects comments and string literals.
        static const auto WrapperDeclarationPattern = FRegexPattern{TEXT(R"(class\s+(A[A-Za-z0-9_]+_Actor)\b)")};
        auto Candidates = TSet<FString>{};
        auto Matcher = FRegexMatcher{WrapperDeclarationPattern, InContents};
        while (Matcher.FindNext())
        {
            Candidates.Add(Matcher.GetCaptureGroup(1));
        }

        for (const auto& Candidate : Candidates)
        {
            if (FCkAsSourceScanner::Parse_ClassDeclaration(InContents, Candidate, InSourceFilePath).Found)
            { OutNames.Add(Candidate.RightChop(1)); }
        }
    }

#if WITH_EDITOR

    // ---- Constants -----------------------------------------------------

    // UE object storage drops the type prefix, and AS-defined classes follow suit — source-form
    // `UCk_AutoTest_Base` (Plugins/CkTests/Script/Common) is stored as `Ck_AutoTest_Base`.
    static const TCHAR* AutoTestBaseBareName = TEXT("Ck_AutoTest_Base");

    // Source form, because this one is EMITTED into .as files rather than looked up.
    static const TCHAR* AutoTestRunnerSourceName = TEXT("ACk_AutoTestRunner");

    // A class whose source path contains this was emitted by us — never a hand-authored collision.
    static const TCHAR* GeneratedDirSegment = TEXT("/Script/Generated/");

    // ---- Locating UCk_AutoTest_Base ------------------------------------

    auto Find_AutoTestBaseClass() -> UClass*
    {
        // AS-defined, so there is no C++ symbol to link against. The prefixed fallback only
        // matters if a future AS-UE revision changes the storage convention.
        if (auto* Found = FindFirstObject<UClass>(AutoTestBaseBareName, EFindFirstObjectOptions::None))
        { return Found; }
        return FindFirstObject<UClass>(TEXT("UCk_AutoTest_Base"), EFindFirstObjectOptions::None);
    }

    // ---- Filtering -----------------------------------------------------

    auto Is_IncludedAutoTestClass(UClass* InClass, UClass* InAutoTestBase) -> bool
    {
        if (ck::Is_NOT_Valid(InClass))
        { return false; }

        if (InClass == InAutoTestBase)
        { return false; }

        if (NOT InClass->IsChildOf(InAutoTestBase))
        { return false; }

        constexpr auto DisqualifyingFlags =
            CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists;

        if (InClass->HasAnyClassFlags(DisqualifyingFlags))
        { return false; }

        // A class deleted from .as source lingers in TObjectIterator until GC runs, and would
        // otherwise keep reappearing in the generated file.
        if (InClass->IsUnreachable() ||
            InClass->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed))
        { return false; }

#if WITH_ANGELSCRIPT_CK
        // A vanished source FILE is the strong signal the class is gone; a class merely deleted
        // out of a still-present file leaves its stale UASClass behind and is not caught here.
        if (auto* ASClass = UASClass::GetFirstASClass(InClass))
        {
            if (ASClass->NewerVersion != nullptr)
            { return false; }

            const auto SourcePath = ASClass->GetSourceFilePath();
            if (NOT SourcePath.IsEmpty() && NOT FPaths::FileExists(SourcePath))
            { return false; }
        }
#endif

        if (UCk_Utils_Reflection_UE::Is_PlaceholderClass(InClass))
        { return false; }

        // Net-mode tests belong to FCkAutoTestNetStubGenerator: a wrapper emitted for one would
        // silently fail in single-PIE, its AS body needing the multi-PIE harness' cross-world
        // coordination.
        if (FCkAutoTestNetStubGenerator::Read_NetMode(InClass) !=
            FCkAutoTestNetStubGenerator::ENetMode::Standalone)
        { return false; }

        return true;
    }

    // ---- Naming --------------------------------------------------------

    auto Get_WrapperSourceName(const UClass* InEntityScriptClass) -> FString
    {
        return FString::Printf(TEXT("A%s_Actor"), *InEntityScriptClass->GetName());
    }

    // No `A` prefix — this form is for FindFirstObject, which sees UE's stripped storage names.
    auto Get_WrapperBareName(const UClass* InEntityScriptClass) -> FString
    {
        return FString::Printf(TEXT("%s_Actor"), *InEntityScriptClass->GetName());
    }

    // ---- Hand-authored wrapper detection -------------------------------

    // TWO authorities answer "does a hand-authored wrapper already own this name": this one, which
    // asks the live object table, and `FCkAutoTestWrapperGenerator::Collect_SourceDeclaredWrapperNames`,
    // which asks the .as source. The source authority exists because a wrapper whose file failed to
    // compile has NO live class to answer for it — emitting beside it lands a second class of the
    // same name in Script/Generated/, and the next successful compile aborts the AS boot on the
    // duplicate until someone deletes the generated entry by hand.
    //
    // This one is true only for a USER-authored wrapper. Our own previous output is ignored (we would
    // otherwise deadlock ourselves), as are stale UClasses of just-removed AS classes.
    auto Has_HandAuthoredWrapper(const FString& InWrapperBareName) -> bool
    {
        auto* Existing = FindFirstObject<UClass>(*InWrapperBareName, EFindFirstObjectOptions::None);
        if (ck::Is_NOT_Valid(Existing))
        { return false; }

        // A just-deleted wrapper lingers until GC; without this it keeps tripping the collision
        // check and deleting a hand-authored wrapper would never hand emission back to us.
        if (Existing->IsUnreachable() ||
            Existing->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed))
        { return false; }

        constexpr auto DisqualifyingFlags = CLASS_NewerVersionExists | CLASS_Deprecated;
        if (Existing->HasAnyClassFlags(DisqualifyingFlags))
        { return false; }

#if WITH_ANGELSCRIPT_CK
        if (auto* ExistingAS = UASClass::GetFirstASClass(Existing))
        {
            // The class has been replaced by a recompile; its slot is dead.
            if (ExistingAS->NewerVersion != nullptr)
            { return false; }

            const auto SourcePath = ExistingAS->GetSourceFilePath();
            if (NOT SourcePath.IsEmpty() && NOT FPaths::FileExists(SourcePath))
            { return false; }

            // Not an AS class we can reason about — assume hand-authored and stay out of the way.
            if (SourcePath.IsEmpty())
            { return true; }

            auto Normalized = FPaths::ConvertRelativePathToFull(SourcePath);
            FPaths::NormalizeFilename(Normalized);
            return NOT Normalized.Contains(GeneratedDirSegment, ESearchCase::IgnoreCase);
        }
#endif

        // C++ class with the wrapper name — definitely hand-authored.
        return true;
    }

    // ---- Block formatting ----------------------------------------------

    // Unset when the field is missing, unreadable, OR equal to the harness default — the caller
    // emits nothing in that case, keeping the generated file tight for the common test.
    auto Get_TimeoutOverride(const UClass* InEntityScriptClass) -> TOptional<float>
    {
        constexpr auto HarnessDefault = 5.0f;

        const auto* CDO = InEntityScriptClass->GetDefaultObject();
        if (ck::Is_NOT_Valid(CDO))
        { return {}; }

        const auto* Property = InEntityScriptClass->FindPropertyByName(TEXT("_TimeoutSeconds"));
        if (Property == nullptr)
        { return {}; }

        // AS-UE binds a script-side `float` to EITHER FFloatProperty or FDoubleProperty depending
        // on the property, so both have to be handled.
        auto Value = HarnessDefault;
        if (const auto* FloatProp = CastField<FFloatProperty>(Property))
        {
            Value = FloatProp->GetPropertyValue_InContainer(CDO);
        }
        else if (const auto* DoubleProp = CastField<FDoubleProperty>(Property))
        {
            Value = static_cast<float>(DoubleProp->GetPropertyValue_InContainer(CDO));
        }
        else
        {
            return {};
        }

        if (FMath::IsNearlyEqual(Value, HarnessDefault, KINDA_SMALL_NUMBER))
        { return {}; }

        return Value;
    }

    struct FWrapperInput
    {
        FString _SourceName;
        FString _TestClassPath;
        FString _TimeoutLiteral;

        auto operator==(const FWrapperInput& InOther) const -> bool
        {
            return _SourceName.Equals(InOther._SourceName, ESearchCase::CaseSensitive) &&
                _TestClassPath.Equals(InOther._TestClassPath, ESearchCase::CaseSensitive) &&
                _TimeoutLiteral.Equals(InOther._TimeoutLiteral, ESearchCase::CaseSensitive);
        }
    };

    auto
        Make_WrapperInput(
            const UClass& InClass)
        -> FWrapperInput
    {
        const auto TimeoutOverride = Get_TimeoutOverride(&InClass);
        // MinFractionalDigits=1 forces "2.0f" over "2f".
        return FWrapperInput{
            Get_WrapperSourceName(&InClass),
            InClass.GetPathName(),
            TimeoutOverride.IsSet() ? FString::SanitizeFloat(*TimeoutOverride, /*MinFractionalDigits=*/1) : FString{}};
    }

    auto Format_WrapperBlock(const FWrapperInput& InWrapper) -> FString
    {
        auto Block = FString{};
        Block += ck::Format_UE(TEXT("class {} : {}\n"), InWrapper._SourceName, AutoTestRunnerSourceName);
        Block += TEXT("{\n");
        if (NOT InWrapper._TimeoutLiteral.IsEmpty())
        {
            Block += ck::Format_UE(TEXT("    default _TimeoutSeconds = {}f;\n"), InWrapper._TimeoutLiteral);
        }
        Block += TEXT("    UFUNCTION(BlueprintOverride)\n");
        Block += TEXT("    TSubclassOf<UCk_EntityScript_UE> Get_TestEntityScriptClass() const\n");
        Block += TEXT("    {\n");
        Block += ck::Format_UE(TEXT("        auto Path = FSoftClassPath(\"{}\");\n"), InWrapper._TestClassPath);
        Block += TEXT("        TSubclassOf<UCk_EntityScript_UE> ResolvedClass;\n");
        Block += TEXT("        ResolvedClass = Path.TryLoadClass();\n");
        Block += TEXT("        return ResolvedClass;\n");
        Block += TEXT("    }\n");
        Block += TEXT("}\n\n");
        return Block;
    }

    // ---- Plugin bucketing ----------------------------------------------

    struct FPluginBucket
    {
        FString PluginName;
        FString OutputFilePath;
        bool _IsProjectBucket = false;
        TArray<FWrapperInput> _Wrappers;

        auto operator==(const FPluginBucket& InOther) const -> bool
        {
            return PluginName.Equals(InOther.PluginName, ESearchCase::CaseSensitive) &&
                OutputFilePath.Equals(InOther.OutputFilePath, ESearchCase::CaseSensitive) &&
                _IsProjectBucket == InOther._IsProjectBucket &&
                _Wrappers == InOther._Wrappers;
        }
    };

    auto Bucket_ClassesByPlugin(const TArray<UClass*>& InClasses) -> TArray<FPluginBucket>
    {
        TRACE_CPUPROFILER_EVENT_SCOPE(CkAS_AutoTestWrappers_BucketClassesByPlugin);
        const auto PluginLookup = FCkAngelscriptGenerator_PluginLookup{};

        auto BucketMap = TMap<FString, FPluginBucket>{};

        for (auto* Class : InClasses)
        {
            auto PluginName = FString{};
            auto OutputDir = FString{};

            if (const auto* Plugin = PluginLookup.Find_PluginForClass(Class))
            {
                PluginName = Plugin->GetName();
                OutputDir = Plugin->GetBaseDir() / TEXT("Script") / TEXT("Generated");
            }
            else
            {
                PluginName = FApp::GetProjectName();
                OutputDir = FPaths::ProjectDir() / TEXT("Script") / TEXT("Generated");
            }

            if (NOT BucketMap.Contains(PluginName))
            {
                auto Bucket = FPluginBucket{};
                Bucket.PluginName = PluginName;
                Bucket.OutputFilePath = OutputDir / (PluginName + TEXT("_AutoTestActors.as"));
                Bucket._IsProjectBucket = PluginName == FApp::GetProjectName();
                BucketMap.Add(PluginName, MoveTemp(Bucket));
            }

            BucketMap[PluginName]._Wrappers.Add(Make_WrapperInput(*Class));
        }

        auto Result = TArray<FPluginBucket>{};
        BucketMap.GenerateValueArray(Result);
        Result.Sort([](const FPluginBucket& A, const FPluginBucket& B)
        {
            return A.PluginName < B.PluginName;
        });
        return Result;
    }

    // ---- File content --------------------------------------------------

    static const TCHAR* FileHeader =
        TEXT("// Auto-generated AutoTest actor wrappers - DO NOT EDIT.\n")
        TEXT("// Regenerated on editor startup and after every AngelScript recompile.\n")
        TEXT("//\n")
        TEXT("// =====================================================================\n")
        TEXT("// WHY DO THESE WRAPPERS LOOK SO WEIRD?\n")
        TEXT("// =====================================================================\n")
        TEXT("//\n")
        TEXT("// You'd normally write a wrapper like this - short, type-safe:\n")
        TEXT("//\n")
        TEXT("//   class A<TestName>_Actor : ACk_AutoTestRunner\n")
        TEXT("//   {\n")
        TEXT("//       default _TestEntityScriptClass = U<TestName>;   // compile-time ref\n")
        TEXT("//   }\n")
        TEXT("//\n")
        TEXT("// We don't, because that compile-time reference creates a deadlock when\n")
        TEXT("// the entity-script .as file is deleted while the editor is running:\n")
        TEXT("//\n")
        TEXT("//   1. AS file watcher misses the delete for one cycle.\n")
        TEXT("//   2. Generator emits a wrapper still referencing U<TestName>.\n")
        TEXT("//   3. AS recompiles the generated file -> fails because U<TestName> is\n")
        TEXT("//      gone -> PostCompile stops firing -> generator can't fix the file\n")
        TEXT("//      it just emitted. Editor stays broken until manual recovery.\n")
        TEXT("//\n")
        TEXT("// The runtime-resolved form below sidesteps the deadlock: the entity-\n")
        TEXT("// script class is referenced as a string literal inside an override of\n")
        TEXT("// Get_TestEntityScriptClass, looked up at runtime via FSoftClassPath.\n")
        TEXT("// AS doesn't resolve the string at compile time, so the wrapper compiles\n")
        TEXT("// regardless of whether U<TestName> exists. If it's gone, the lookup\n")
        TEXT("// returns null and the test reports a clear runtime failure; one sync\n")
        TEXT("// pass later the wrapper is removed entirely. Self-healing.\n")
        TEXT("//\n")
        TEXT("// =====================================================================\n")
        TEXT("// HAND-AUTHORED OPT-OUT\n")
        TEXT("// =====================================================================\n")
        TEXT("//\n")
        TEXT("// For tests that need a custom _TimeoutSeconds or any other wrapper\n")
        TEXT("// customization, hand-author your own A<TestName>_Actor class anywhere\n")
        TEXT("// OUTSIDE Script/Generated/ using the simpler compile-time form:\n")
        TEXT("//\n")
        TEXT("//   class A<TestName>_Actor : ACk_AutoTestRunner\n")
        TEXT("//   {\n")
        TEXT("//       default _TestEntityScriptClass = U<TestName>;\n")
        TEXT("//       default _TimeoutSeconds = 2.0f;\n")
        TEXT("//   }\n")
        TEXT("//\n")
        TEXT("// The generator detects hand-authored wrappers by class name + source\n")
        TEXT("// path and skips emission for that test, leaving yours authoritative.\n")
        TEXT("// (Hand-authored wrappers don't carry the deletion-race risk because\n")
        TEXT("// deleting the .as file removes BOTH classes atomically - no stale\n")
        TEXT("// generated file to get out of sync.)\n\n");


    enum class EStartupPass
    {
        None,
        PostCompile,
        PostInit
    };

    struct FSourceFingerprint
    {
        FString _Path;
        FBlake3Hash _Hash;
    };

    struct FStartupSnapshot
    {
        TArray<FSourceFingerprint> _Sources;
        TSet<FString> _SourceDeclared;
        TArray<FPluginBucket> _Buckets;
        TArray<FBlake3Hash> _OutputHashes;
    };

    TOptional<FStartupSnapshot> sStartupSnapshot;

    auto
        Try_GetFileHash(
            const FString& InPath)
        -> TOptional<FBlake3Hash>
    {
        auto Bytes = TArray<uint8>{};
        if (NOT FFileHelper::LoadFileToArray(Bytes, *InPath, FILEREAD_Silent))
        { return {}; }

        return FBlake3::HashBuffer(Bytes.GetData(), Bytes.Num());
    }

    auto
        Capture_StartupSources(
            const TArray<FString>& InSourcePaths,
            FStartupSnapshot& OutSnapshot)
        -> bool
    {
        auto Complete = true;
        for (const auto& Path : InSourcePaths)
        {
            auto Bytes = TArray<uint8>{};
            if (NOT FFileHelper::LoadFileToArray(Bytes, *Path, FILEREAD_Silent))
            {
                Complete = false;
                continue;
            }

            // Parse the same bytes whose hash is used by the follow-up.
            OutSnapshot._Sources.Add(FSourceFingerprint{Path, FBlake3::HashBuffer(Bytes.GetData(), Bytes.Num())});
            auto Contents = FString{};
            FFileHelper::BufferToString(Contents, Bytes.GetData(), Bytes.Num());
            Collect_DeclaredWrappersFromFile(Contents, Path, OutSnapshot._SourceDeclared);
        }
        return Complete;
    }

    auto
        Are_StartupSourcesUnchanged(
            const TArray<FString>& InSourcePaths,
            const TArray<FSourceFingerprint>& InPreviousSources)
        -> bool
    {
        if (InSourcePaths.Num() != InPreviousSources.Num())
        { return false; }

        for (auto Index = 0; Index < InSourcePaths.Num(); ++Index)
        {
            const auto& Path = InSourcePaths[Index];
            const auto& Previous = InPreviousSources[Index];
            if (NOT Path.Equals(Previous._Path, ESearchCase::CaseSensitive))
            { return false; }

            const auto CurrentHash = Try_GetFileHash(Path);
            if (NOT CurrentHash.IsSet() || *CurrentHash != Previous._Hash)
            { return false; }
        }
        return true;
    }

    auto
        Are_StartupOutputsUnchanged(
            const TArray<FPluginBucket>& InBuckets,
            const FStartupSnapshot& InPrevious)
        -> bool
    {
        if (InBuckets != InPrevious._Buckets || InBuckets.Num() != InPrevious._OutputHashes.Num())
        { return false; }

        for (auto Index = 0; Index < InBuckets.Num(); ++Index)
        {
            const auto CurrentHash = Try_GetFileHash(InBuckets[Index].OutputFilePath);
            if (NOT CurrentHash.IsSet() || *CurrentHash != InPrevious._OutputHashes[Index])
            { return false; }
        }
        return true;
    }

    auto
        Try_CaptureValidatedOutput(
            const FString& InPath,
            const FString& InExpectedContent)
        -> TOptional<FBlake3Hash>
    {
        auto Bytes = TArray<uint8>{};
        if (NOT FFileHelper::LoadFileToArray(Bytes, *InPath, FILEREAD_Silent))
        { return {}; }

        auto Contents = FString{};
        FFileHelper::BufferToString(Contents, Bytes.GetData(), Bytes.Num());
        Contents.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
        if (NOT Contents.Equals(InExpectedContent, ESearchCase::CaseSensitive))
        { return {}; }

        return FBlake3::HashBuffer(Bytes.GetData(), Bytes.Num());
    }

    auto GenerateAll(EStartupPass InStartupPass) -> void;

#endif // WITH_EDITOR
}

// --------------------------------------------------------------------------------------------------------------------

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_AutoTestWrapperGenerator_StartupSnapshot,
    "CkAngelscriptGenerator.UnitTests.AutoTestWrapperGenerator.StartupSnapshot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkTest_AutoTestWrapperGenerator_StartupSnapshot::RunTest(const FString&)
{
    using namespace ck_autotest_wrapper_generator;

    auto& FileManager = IFileManager::Get();
    const auto Directory = FPaths::ProjectIntermediateDir();
    if (NOT TestTrue(TEXT("Fixture directory exists"), FileManager.MakeDirectory(*Directory, true)))
    { return false; }

    const auto SourcePath = FPaths::CreateTempFilename(*Directory, TEXT("CkWrapperStartupSource"), TEXT(".as"));
    const auto OutputPath = FPaths::CreateTempFilename(*Directory, TEXT("CkWrapperStartupOutput"), TEXT(".as"));
    ON_SCOPE_EXIT
    {
        FileManager.Delete(*SourcePath);
        FileManager.Delete(*OutputPath);
    };

    const auto SaveFixture = [](const FString& InPath, const TCHAR* InContents)
    {
        return FFileHelper::SaveStringToFile(InContents, *InPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    };
    if (NOT TestTrue(TEXT("Source fixture written"),
            SaveFixture(SourcePath, TEXT("class ATestOne_Actor : ACk_AutoTestRunner {}\n"))) ||
        NOT TestTrue(TEXT("Output fixture written"), SaveFixture(OutputPath, TEXT("// expected\n"))))
    { return false; }

    const auto SourcePaths = TArray<FString>{SourcePath};
    auto Snapshot = FStartupSnapshot{};
    if (NOT TestTrue(TEXT("Source inputs captured"), Capture_StartupSources(SourcePaths, Snapshot)))
    { return false; }
    TestTrue(TEXT("Source declarations came from captured bytes"), Snapshot._SourceDeclared.Contains(TEXT("TestOne_Actor")));
    TestTrue(TEXT("Unchanged source matches"), Are_StartupSourcesUnchanged(SourcePaths, Snapshot._Sources));

    const auto SourceTime = FileManager.GetTimeStamp(*SourcePath);
    const auto SourceSize = FileManager.FileSize(*SourcePath);
    if (NOT TestTrue(TEXT("Same-length source edit written"),
            SaveFixture(SourcePath, TEXT("class ATestTwo_Actor : ACk_AutoTestRunner {}\n"))) ||
        NOT TestTrue(TEXT("Original timestamp restored"), FileManager.SetTimeStamp(*SourcePath, SourceTime)))
    { return false; }
    TestTrue(TEXT("Edit preserved size and timestamp"),
        FileManager.FileSize(*SourcePath) == SourceSize && FileManager.GetTimeStamp(*SourcePath) == SourceTime);
    TestFalse(TEXT("Source edit invalidates the snapshot despite equal metadata"),
        Are_StartupSourcesUnchanged(SourcePaths, Snapshot._Sources));

    auto Bucket = FPluginBucket{};
    Bucket.PluginName = TEXT("StartupSnapshotTest");
    Bucket.OutputFilePath = OutputPath;
    Bucket._Wrappers.Add(FWrapperInput{TEXT("ATestOne_Actor"), TEXT("/Script/Angelscript.TestOne"), TEXT("2.0")});
    Snapshot._Buckets.Add(MoveTemp(Bucket));
    const auto Hash = Try_CaptureValidatedOutput(OutputPath, TEXT("// expected\n"));
    if (NOT TestTrue(TEXT("Expected output captured"), Hash.IsSet()))
    { return false; }

    Snapshot._OutputHashes.Add(*Hash);
    TestTrue(TEXT("Unchanged output matches"), Are_StartupOutputsUnchanged(Snapshot._Buckets, Snapshot));
    auto ChangedBuckets = Snapshot._Buckets;
    ChangedBuckets[0]._Wrappers[0]._TimeoutLiteral = TEXT("3.0");
    TestFalse(TEXT("Changed effective timeout invalidates the snapshot"), Are_StartupOutputsUnchanged(ChangedBuckets, Snapshot));

    if (NOT TestTrue(TEXT("Modified output written"), SaveFixture(OutputPath, TEXT("// modified\n"))))
    { return false; }
    TestFalse(TEXT("Modified output cannot be captured as expected content"),
        Try_CaptureValidatedOutput(OutputPath, TEXT("// expected\n")).IsSet());
    TestFalse(TEXT("Modified output invalidates the snapshot"), Are_StartupOutputsUnchanged(Snapshot._Buckets, Snapshot));
    if (NOT TestTrue(TEXT("Generated output removed"), FileManager.Delete(*OutputPath, /*RequireExists=*/true)))
    { return false; }
    TestFalse(TEXT("Missing output invalidates the snapshot"), Are_StartupOutputsUnchanged(Snapshot._Buckets, Snapshot));
    return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkAutoTestWrapperGenerator::
    Collect_SourceDeclaredWrapperNames(
        const TArray<FString>& InAsSourceFiles)
    -> TSet<FString>
{
    auto Result = TSet<FString>{};
    for (const auto& SourceFilePath : InAsSourceFiles)
    {
        auto Contents = FString{};
        if (FFileHelper::LoadFileToString(Contents, *SourceFilePath))
        { ck_autotest_wrapper_generator::Collect_DeclaredWrappersFromFile(Contents, SourceFilePath, Result); }
    }
    return Result;
}

// --------------------------------------------------------------------------------------------------------------------

#if WITH_EDITOR

auto
    ck_autotest_wrapper_generator::
    GenerateAll(
        EStartupPass InStartupPass)
    -> void
{
    using ck::angelscriptgenerator::self_heal::FCkAsSourceScanner;

    auto Previous = InStartupPass == EStartupPass::PostInit
        ? MoveTemp(sStartupSnapshot)
        : TOptional<FStartupSnapshot>{};
    sStartupSnapshot.Reset();

    // Single-writer gate (G4): a secondary instance must not rewrite <Plugin>_AutoTestActors.as.
    if (NOT FCkAngelscriptGenerator_RegenOwnership::Try_AcquireOrGet_IsOwner(
            TEXT("AutoTestWrapperGenerator.GenerateAll")))
    { return; }

    auto* AutoTestBase = Find_AutoTestBaseClass();
    if (ck::Is_NOT_Valid(AutoTestBase))
    {
        // Normal during early startup. Log rather than VeryVerbose so the silent-no-write case
        // stays diagnosable.
        ck::angelscriptgenerator::Log(
            TEXT("[CkAS AutoTest Wrappers] Ck_AutoTest_Base not found in object table - ")
            TEXT("skipping pass (CkTests not loaded yet, or AS not yet compiled)."));
        return;
    }

    auto AllSubclasses = TArray<UClass*>{};
    for (TObjectIterator<UClass> ClassIterator; ClassIterator; ++ClassIterator)
    {
        auto* Class = *ClassIterator;
        if (Is_IncludedAutoTestClass(Class, AutoTestBase))
        {
            AllSubclasses.Add(Class);
        }
    }

    AllSubclasses.Sort([](const UClass& InA, const UClass& InB)
    {
        return InA.GetPathName() < InB.GetPathName();
    });

    const auto SourcePaths = FCkAsSourceScanner::Enumerate_AsSourceFiles(FCkAsSourceScanner::Get_DefaultScanRoots());
    auto Current = FStartupSnapshot{};
    const auto CanReuseSourceDeclarations = Previous.IsSet() &&
        Are_StartupSourcesUnchanged(SourcePaths, Previous->_Sources);
    auto CaptureComplete = false;
    if (InStartupPass == EStartupPass::PostCompile)
    {
        CaptureComplete = Capture_StartupSources(SourcePaths, Current);
    }
    else
    {
        Current._SourceDeclared = CanReuseSourceDeclarations
            ? MoveTemp(Previous->_SourceDeclared)
            : FCkAutoTestWrapperGenerator::Collect_SourceDeclaredWrapperNames(SourcePaths);
    }
    const auto& SourceDeclared = Current._SourceDeclared;

    auto Emittable = TArray<UClass*>{};
    auto SkippedCount = int32{0};
    auto SkippedBySourceOnlyCount = int32{0};
    for (auto* Class : AllSubclasses)
    {
        const auto WrapperBareName = Get_WrapperBareName(Class);

        const auto HasLiveWrapper = Has_HandAuthoredWrapper(WrapperBareName);
        const auto HasSourceDeclaredWrapper = SourceDeclared.Contains(WrapperBareName);

        if (HasLiveWrapper || HasSourceDeclaredWrapper)
        {
            ++SkippedCount;

            if (NOT HasLiveWrapper && HasSourceDeclaredWrapper)
            { ++SkippedBySourceOnlyCount; }
            const auto Authority = HasLiveWrapper
                ? FString{TEXT("live class")}
                : FString{TEXT("declared in source")};
            ck::angelscriptgenerator::VeryVerbose(
                TEXT("[CkAS AutoTest Wrappers] Skipping {} - hand-authored {} already exists ({})."),
                Class->GetName(),
                Get_WrapperSourceName(Class),
                Authority);
            continue;
        }
        Emittable.Add(Class);
    }

    auto Buckets = Bucket_ClassesByPlugin(Emittable);
    if (CanReuseSourceDeclarations && Are_StartupOutputsUnchanged(Buckets, *Previous))
    {
        ck::angelscriptgenerator::Log(
            TEXT("[CkAS AutoTest Wrappers] Reused startup pass ({} output files)."), Buckets.Num());
        return;
    }

    ck::angelscriptgenerator::Log(TEXT("[CkAS AutoTest Wrappers] === Generating AutoTest actor wrappers ==="));
    ck::angelscriptgenerator::Log(
        TEXT("[CkAS AutoTest Wrappers] Discovered {} subclasses of Ck_AutoTest_Base - ")
        TEXT("{} will be emitted, {} have hand-authored wrappers (skipped; {} of those found only by ")
        TEXT("the source scan); {} wrapper classes are declared in .as source."),
        AllSubclasses.Num(), Emittable.Num(), SkippedCount, SkippedBySourceOnlyCount, SourceDeclared.Num());

    auto TotalEmitted = int32{0};
    for (const auto& Bucket : Buckets)
    {
        auto Content = FString{FileHeader};

        // Only the project bucket is guarded. Its Script root is always compiled, while a disabled
        // test plugin drops its whole Script root. Plugin Assets.as references these types outside EDITOR.
        if (Bucket._IsProjectBucket)
        { Content += TEXT("#if EDITOR\n\n"); }

        for (const auto& Wrapper : Bucket._Wrappers)
        {
            Content += Format_WrapperBlock(Wrapper);
        }

        if (Bucket._IsProjectBucket)
        { Content += TEXT("#endif // EDITOR\n"); }

        const auto OutputDir = FPaths::GetPath(Bucket.OutputFilePath);
        IFileManager::Get().MakeDirectory(*OutputDir, true);

        // Writing unchanged content re-triggers the AngelScript PostCompile hook, which calls us
        // again, ad infinitum. Compare LF-normalized — the on-disk file may be CRLF.
        auto ExistingContent = FString{};
        const auto HasExisting = FFileHelper::LoadFileToString(ExistingContent, *Bucket.OutputFilePath);

        auto ContentForCompare = Content;
        ContentForCompare.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
        ExistingContent.ReplaceInline(TEXT("\r\n"), TEXT("\n"));

        if (HasExisting && ExistingContent.Equals(ContentForCompare, ESearchCase::CaseSensitive))
        {
            ck::angelscriptgenerator::VeryVerbose(
                TEXT("[CkAS AutoTest Wrappers] [{}] up-to-date ({} wrappers)"),
                Bucket.PluginName, Bucket._Wrappers.Num());
        }
        else
        {
#if PLATFORM_WINDOWS
            // Match the CRLF on-disk convention on Windows.
            Content.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
            Content.ReplaceInline(TEXT("\n"), TEXT("\r\n"));
#endif

            if (FFileHelper::SaveStringToFile(Content, *Bucket.OutputFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
            {
                ck::angelscriptgenerator::Log(
                    TEXT("[CkAS AutoTest Wrappers] [{}] {} wrappers -> {}"),
                    Bucket.PluginName, Bucket._Wrappers.Num(), Bucket.OutputFilePath);
            }
            else
            {
                CaptureComplete = false;
                ck::angelscriptgenerator::Error(
                    TEXT("[CkAS AutoTest Wrappers] Failed to write to [{}]"), Bucket.OutputFilePath);
            }
        }

        if (CaptureComplete)
        {
            const auto OutputHash = Try_CaptureValidatedOutput(Bucket.OutputFilePath, ContentForCompare);
            CaptureComplete = OutputHash.IsSet();
            if (CaptureComplete)
            { Current._OutputHashes.Add(*OutputHash); }
        }
        TotalEmitted += Bucket._Wrappers.Num();
    }

    ck::angelscriptgenerator::Log(
        TEXT("[CkAS AutoTest Wrappers] Done - {} plugins, {} wrappers emitted, {} skipped (hand-authored)"),
        Buckets.Num(), TotalEmitted, SkippedCount);

    if (CaptureComplete)
    {
        Current._Buckets = MoveTemp(Buckets);
        sStartupSnapshot.Emplace(MoveTemp(Current));
    }
}

#endif // WITH_EDITOR

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkAutoTestWrapperGenerator::
    GenerateAll()
    -> void
{
#if WITH_EDITOR
    ck_autotest_wrapper_generator::GenerateAll(ck_autotest_wrapper_generator::EStartupPass::None);
#endif
}

auto
    FCkAutoTestWrapperGenerator::
    GenerateAllForStartupPostCompile()
    -> void
{
#if WITH_EDITOR
    ck_autotest_wrapper_generator::GenerateAll(ck_autotest_wrapper_generator::EStartupPass::PostCompile);
#endif
}

auto
    FCkAutoTestWrapperGenerator::
    GenerateAllForStartupPostInit()
    -> void
{
#if WITH_EDITOR
    ck_autotest_wrapper_generator::GenerateAll(ck_autotest_wrapper_generator::EStartupPass::PostInit);
#endif
}

// --------------------------------------------------------------------------------------------------------------------
