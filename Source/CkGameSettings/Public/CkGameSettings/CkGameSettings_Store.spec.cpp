#include "CkGameSettings/Storage/CkGameSettings_IniStorageProvider.h"

#include "Misc/AutomationTest.h"

#include <Misc/FileHelper.h>
#include <Misc/Paths.h>
#include <Misc/ScopeExit.h>
#include <HAL/FileManager.h>

#if WITH_DEV_AUTOMATION_TESTS && PLATFORM_WINDOWS
#include "Microsoft/WindowsHWrapper.h"
#endif

#if WITH_DEV_AUTOMATION_TESTS

// --------------------------------------------------------------------------------------------------------------------

namespace ck_game_settings_store_spec
{
    auto
        Read_Values(
            FAutomationTestBase& InTest,
            UCk_GameSettings_StorageProvider_UE* InProvider,
            ECk_GameSettings_Scope InScope,
            int32 InUserId)
        -> TArray<FCk_GameSettings_StoredValue>
    {
        auto Values = TArray<FCk_GameSettings_StoredValue>{};
        InTest.TestTrue(TEXT("storage read succeeds"), InProvider->Get_StoredValues(InScope, InUserId, Values)
            != ECk_GameSettings_StorageReadResult::Failed);
        return Values;
    }

    auto Make_Provider(const FString& InFilePath) -> UCk_GameSettings_IniStorageProvider_UE*
    {
        auto* Provider = NewObject<UCk_GameSettings_IniStorageProvider_UE>();
        Provider->Set_FilePathOverride(InFilePath);
        return Provider;
    }

    auto Make_TempFilePath() -> FString
    {
        const auto TempDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Temp"));
        constexpr auto CreateParentTree = true;
        IFileManager::Get().MakeDirectory(*TempDir, CreateParentTree);
        return FPaths::CreateTempFilename(*TempDir, TEXT("CkGameSettingsStoreSpec"), TEXT(".ini"));
    }
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_GameSettings_Store_RoundTripPreservesOrder,
    "Ck.CkGameSettings.Store.RoundTripPreservesOrder",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkTest_GameSettings_Store_RoundTripPreservesOrder::RunTest(const FString&)
{
    const auto TempFilePath = ck_game_settings_store_spec::Make_TempFilePath();

    auto* Writer = ck_game_settings_store_spec::Make_Provider(TempFilePath);
    Writer->Request_StoreValue(ECk_GameSettings_Scope::Machine, 0, TEXT("sg.quality"), TEXT("3"));
    Writer->Request_StoreValue(ECk_GameSettings_Scope::Machine, 0, TEXT("sg.shadows"), TEXT("2"));
    Writer->Request_StoreValue(ECk_GameSettings_Scope::Machine, 0, TEXT("sg.textures"), TEXT("1"));
    Writer->Request_StoreValue(ECk_GameSettings_Scope::Player, 7, TEXT("audio.master"), TEXT("0.8"));

    Writer->Request_StoreValue(ECk_GameSettings_Scope::Machine, 0, TEXT("sg.shadows"), TEXT("0"));

    Writer->Request_Flush();

    auto* Reader = ck_game_settings_store_spec::Make_Provider(TempFilePath);

    const auto MachineValues = ck_game_settings_store_spec::Read_Values(*this, Reader, ECk_GameSettings_Scope::Machine, 0);
    TestEqual(TEXT("three Machine values round-tripped"), MachineValues.Num(), 3);
    if (MachineValues.Num() == 3)
    {
        TestEqual(TEXT("first key keeps insertion order"), MachineValues[0].Get_Key(), FName{TEXT("sg.quality")});
        TestEqual(TEXT("updated key keeps its ORIGINAL position"), MachineValues[1].Get_Key(), FName{TEXT("sg.shadows")});
        TestEqual(TEXT("updated key carries the new value"), MachineValues[1].Get_Value(), TEXT("0"));
        TestEqual(TEXT("third key keeps insertion order"), MachineValues[2].Get_Key(), FName{TEXT("sg.textures")});
    }

    const auto PlayerValues = ck_game_settings_store_spec::Read_Values(*this, Reader, ECk_GameSettings_Scope::Player, 7);
    TestEqual(TEXT("Player.7 section round-tripped"), PlayerValues.Num(), 1);
    if (PlayerValues.Num() == 1)
    {
        TestEqual(TEXT("player value round-tripped"), PlayerValues[0].Get_Value(), TEXT("0.8"));
    }

    TestEqual(TEXT("other player ids are empty"), ck_game_settings_store_spec::Read_Values(*this, Reader, ECk_GameSettings_Scope::Player, 0).Num(), 0);

    IFileManager::Get().Delete(*TempFilePath);
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_GameSettings_Store_UnknownLinesSurviveRewrite,
    "Ck.CkGameSettings.Store.UnknownLinesSurviveRewrite",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkTest_GameSettings_Store_UnknownLinesSurviveRewrite::RunTest(const FString&)
{
    const auto TempFilePath = ck_game_settings_store_spec::Make_TempFilePath();

    const auto HandWrittenContent = FString{
        TEXT("; hand-written preamble comment\r\n")
        TEXT("[Machine]\r\n")
        TEXT("# section comment\r\n")
        TEXT("sg.quality=3\r\n")
        TEXT("some unknown line without equals\r\n")
        TEXT("[FutureSection]\r\n")
        TEXT("future.key=42\r\n")};
    TestTrue(TEXT("fixture written"), FFileHelper::SaveStringToFile(HandWrittenContent, *TempFilePath));

    auto* Provider = ck_game_settings_store_spec::Make_Provider(TempFilePath);
    Provider->Request_StoreValue(ECk_GameSettings_Scope::Machine, 0, TEXT("sg.quality"), TEXT("1"));
    Provider->Request_StoreValue(ECk_GameSettings_Scope::Machine, 0, TEXT("sg.new"), TEXT("added"));
    Provider->Request_Flush();

    auto RewrittenContent = FString{};
    TestTrue(TEXT("rewritten file readable"), FFileHelper::LoadFileToString(RewrittenContent, *TempFilePath));

    TestTrue(TEXT("preamble comment preserved"), RewrittenContent.Contains(TEXT("; hand-written preamble comment")));
    TestTrue(TEXT("section comment preserved"), RewrittenContent.Contains(TEXT("# section comment")));
    TestTrue(TEXT("unknown non-KV line preserved"), RewrittenContent.Contains(TEXT("some unknown line without equals")));
    TestTrue(TEXT("unknown section preserved"), RewrittenContent.Contains(TEXT("[FutureSection]")));
    TestTrue(TEXT("unknown section content preserved"), RewrittenContent.Contains(TEXT("future.key=42")));
    TestTrue(TEXT("updated value written"), RewrittenContent.Contains(TEXT("sg.quality=1")));
    TestTrue(TEXT("appended value written"), RewrittenContent.Contains(TEXT("sg.new=added")));

    const auto CommentIndex = RewrittenContent.Find(TEXT("# section comment"));
    const auto QualityIndex = RewrittenContent.Find(TEXT("sg.quality=1"));
    const auto UnknownLineIndex = RewrittenContent.Find(TEXT("some unknown line"));
    TestTrue(TEXT("line order preserved: comment before KV"), CommentIndex < QualityIndex);
    TestTrue(TEXT("line order preserved: KV before unknown line"), QualityIndex < UnknownLineIndex);

    IFileManager::Get().Delete(*TempFilePath);
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_GameSettings_Store_NewlineValueRejected,
    "Ck.CkGameSettings.Store.NewlineValueRejected",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkTest_GameSettings_Store_NewlineValueRejected::RunTest(const FString&)
{
    const auto TempFilePath = ck_game_settings_store_spec::Make_TempFilePath();

    auto* Provider = ck_game_settings_store_spec::Make_Provider(TempFilePath);
    Provider->Request_StoreValue(ECk_GameSettings_Scope::Machine, 0, TEXT("good.key"), TEXT("fine"));

    AddExpectedError(TEXT("contains a newline"), EAutomationExpectedErrorFlags::Contains, 0);
    Provider->Request_StoreValue(ECk_GameSettings_Scope::Machine, 0, TEXT("bad.key"), TEXT("line one\nline two"));

    const auto StoredValues = ck_game_settings_store_spec::Read_Values(*this, Provider, ECk_GameSettings_Scope::Machine, 0);
    TestEqual(TEXT("only the good value is stored"), StoredValues.Num(), 1);
    if (StoredValues.Num() == 1)
    {
        TestEqual(TEXT("good value intact"), StoredValues[0].Get_Key(), FName{TEXT("good.key")});
    }

    Provider->Request_Flush();

    auto FlushedContent = FString{};
    TestTrue(TEXT("flushed file readable"), FFileHelper::LoadFileToString(FlushedContent, *TempFilePath));
    TestFalse(TEXT("rejected value never reaches the file"), FlushedContent.Contains(TEXT("bad.key")));

    IFileManager::Get().Delete(*TempFilePath);
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

#if PLATFORM_WINDOWS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_GameSettings_TenetIntegrity_UnreadableExistingFilePreserved,
    "Ck.CkGameSettings.TenetIntegrity.UnreadableExistingFilePreserved",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkTest_GameSettings_TenetIntegrity_UnreadableExistingFilePreserved::
    RunTest(
        const FString&)
    -> bool
{
    const auto TempFilePath = FPaths::ConvertRelativePathToFull(ck_game_settings_store_spec::Make_TempFilePath());
    ON_SCOPE_EXIT
    {
        IFileManager::Get().Delete(*(TempFilePath + TEXT(".tmp")));
        IFileManager::Get().Delete(*TempFilePath);
    };

    const auto OriginalContent = FString{
        TEXT("; preserve this original preamble\r\n")
        TEXT("[Machine]\r\n")
        TEXT("existing.key=original\r\n")
        TEXT("[FutureSection]\r\n")
        TEXT("future.key=untouched\r\n")};
    if (NOT TestTrue(TEXT("fixture written"), FFileHelper::SaveStringToFile(
        OriginalContent, *TempFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)))
    { return false; }

    auto OriginalBytes = TArray<uint8>{};
    if (NOT TestTrue(TEXT("original bytes captured"), FFileHelper::LoadFileToArray(OriginalBytes, *TempFilePath)))
    { return false; }

    // Deny new readers but permit replacement, so a flush-only failure cannot hide a poisoned empty cache.
    auto FileLock = ::CreateFileW(*TempFilePath, GENERIC_READ, FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    ON_SCOPE_EXIT
    {
        if (FileLock != INVALID_HANDLE_VALUE)
        { ::CloseHandle(FileLock); }
    };
    if (NOT TestTrue(TEXT("read-denial lock acquired"), FileLock != INVALID_HANDLE_VALUE))
    { return false; }

    const auto ReadProbe = ::CreateFileW(*TempFilePath, GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    const auto ReadWasDenied = ReadProbe == INVALID_HANDLE_VALUE;
    if (ReadProbe != INVALID_HANDLE_VALUE)
    { ::CloseHandle(ReadProbe); }
    if (NOT TestTrue(TEXT("fixture deterministically denies a new read"), ReadWasDenied))
    { return false; }

    auto* Provider = ck_game_settings_store_spec::Make_Provider(TempFilePath);
    auto RejectedValues = TArray<FCk_GameSettings_StoredValue>{};
    TestTrue(TEXT("unreadable existing file reports failure"), Provider->Get_StoredValues(
        ECk_GameSettings_Scope::Machine, 0, RejectedValues) == ECk_GameSettings_StorageReadResult::Failed);
    TestTrue(TEXT("failed read publishes no values"), RejectedValues.IsEmpty());
    TestFalse(TEXT("store rejects unreadable original"), Provider->Request_StoreValue(
        ECk_GameSettings_Scope::Machine, 0, TEXT("rejected.key"), TEXT("must-not-persist")));
    TestFalse(TEXT("remove rejects unreadable original"), Provider->Request_RemoveValue(
        ECk_GameSettings_Scope::Machine, 0, TEXT("existing.key")));
    TestFalse(TEXT("flush rejects unreadable original"), Provider->Request_Flush());

    ::CloseHandle(FileLock);
    FileLock = INVALID_HANDLE_VALUE;

    auto AfterFailureBytes = TArray<uint8>{};
    TestTrue(TEXT("original path remains readable after failed-load operations"),
        FFileHelper::LoadFileToArray(AfterFailureBytes, *TempFilePath));
    TestTrue(TEXT("failed-load read/store/remove/flush preserve every original byte"),
        AfterFailureBytes == OriginalBytes);

    const auto RecoveredValues = ck_game_settings_store_spec::Read_Values(*this, Provider, ECk_GameSettings_Scope::Machine, 0);
    TestEqual(TEXT("same provider retries loading after read access recovers"), RecoveredValues.Num(), 1);
    if (RecoveredValues.Num() == 1)
    {
        TestEqual(TEXT("recovered original key"), RecoveredValues[0].Get_Key(), FName{TEXT("existing.key")});
        TestEqual(TEXT("recovered original value"), RecoveredValues[0].Get_Value(), TEXT("original"));
    }

    Provider->Request_StoreValue(ECk_GameSettings_Scope::Machine, 0, TEXT("accepted.key"), TEXT("after-recovery"));
    Provider->Request_Flush();

    auto RecoveredContent = FString{};
    TestTrue(TEXT("recovered write readable"), FFileHelper::LoadFileToString(RecoveredContent, *TempFilePath));
    TestTrue(TEXT("recovered write preserves original value"), RecoveredContent.Contains(TEXT("existing.key=original")));
    TestTrue(TEXT("recovered write preserves unknown section"), RecoveredContent.Contains(TEXT("[FutureSection]")));
    TestTrue(TEXT("recovered write preserves unknown content"), RecoveredContent.Contains(TEXT("future.key=untouched")));
    TestTrue(TEXT("recovered write accepts new value"), RecoveredContent.Contains(TEXT("accepted.key=after-recovery")));
    TestFalse(TEXT("rejected write never leaks into recovered store"), RecoveredContent.Contains(TEXT("rejected.key")));
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_GameSettings_TenetIntegrity_InstallFailurePreservesOriginal,
    "Ck.CkGameSettings.TenetIntegrity.InstallFailurePreservesOriginal",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkTest_GameSettings_TenetIntegrity_InstallFailurePreservesOriginal::
    RunTest(
        const FString&)
    -> bool
{
    const auto Path = FPaths::ConvertRelativePathToFull(ck_game_settings_store_spec::Make_TempFilePath());
    const auto Temporary = Path + TEXT(".tmp");
    ON_SCOPE_EXIT
    {
        IFileManager::Get().Delete(*Temporary);
        IFileManager::Get().Delete(*Path);
    };
    TestTrue(TEXT("original fixture written"), FFileHelper::SaveStringToFile(TEXT("; original\n[Machine]\noriginal.key=old\n"), *Path));
    auto Original = TArray<uint8>{};
    TestTrue(TEXT("original bytes captured"), FFileHelper::LoadFileToArray(Original, *Path));
    auto* Provider = ck_game_settings_store_spec::Make_Provider(Path);
    TestTrue(TEXT("new value accepted"), Provider->Request_StoreValue(ECk_GameSettings_Scope::Machine, 0, TEXT("new.key"), TEXT("new")));
    TestTrue(TEXT("temporary source fixture written"), FFileHelper::SaveStringToFile(TEXT("not-the-flush-output"), *Temporary));
    // Deny replacement of the original while leaving the separate temporary file free for UE's writer.
    auto Lock = ::CreateFileW(*Path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    ON_SCOPE_EXIT
    {
        if (Lock != INVALID_HANDLE_VALUE)
        { ::CloseHandle(Lock); }
    };
    if (NOT TestTrue(TEXT("destination replacement obstruction acquired"), Lock != INVALID_HANDLE_VALUE))
    { return false; }
    TestFalse(TEXT("installation failure reported"), Provider->Request_Flush());
    auto TemporaryContent = FString{};
    TestTrue(TEXT("temporary source can be inspected"), FFileHelper::LoadFileToString(TemporaryContent, *Temporary));
    TestTrue(TEXT("temporary write completed before installation failed"), TemporaryContent.Contains(TEXT("new.key=new")));
    auto After = TArray<uint8>{};
    TestTrue(TEXT("original path remains readable"), FFileHelper::LoadFileToArray(After, *Path));
    TestTrue(TEXT("installation failure preserves original bytes"), Original == After);
    ::CloseHandle(Lock);
    Lock = INVALID_HANDLE_VALUE;
    TestTrue(TEXT("same provider retains dirty data and retries"), Provider->Request_Flush());
    const auto Values = ck_game_settings_store_spec::Read_Values(*this, ck_game_settings_store_spec::Make_Provider(Path), ECk_GameSettings_Scope::Machine, 0);
    TestEqual(TEXT("fresh provider sees both accepted entries"), Values.Num(), 2);
    return true;
}

#endif // PLATFORM_WINDOWS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_GameSettings_TenetIntegrity_StoreBatchRejectsAll,
    "Ck.CkGameSettings.TenetIntegrity.StoreBatchRejectsAll",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkTest_GameSettings_TenetIntegrity_StoreBatchRejectsAll::
    RunTest(
        const FString&)
    -> bool
{
    const auto Path = ck_game_settings_store_spec::Make_TempFilePath();
    ON_SCOPE_EXIT
    {
        IFileManager::Get().Delete(*Path);
        IFileManager::Get().Delete(*(Path + TEXT(".tmp")));
    };
    auto* Provider = ck_game_settings_store_spec::Make_Provider(Path);
    auto Values = TArray<FCk_GameSettings_StoredValue>{};
    TestTrue(TEXT("missing file is explicit absence"), Provider->Get_StoredValues(ECk_GameSettings_Scope::Machine, 0, Values)
        == ECk_GameSettings_StorageReadResult::NotFound);
    AddExpectedError(TEXT("contains a newline"), EAutomationExpectedErrorFlags::Contains, 0);
    TestFalse(TEXT("one invalid required row rejects batch"), Provider->Request_StoreValues({
        FCk_GameSettings_StoreValue{ECk_GameSettings_Scope::Machine, 0, TEXT("good.key"), TEXT("good")},
        FCk_GameSettings_StoreValue{ECk_GameSettings_Scope::Player, 0, TEXT("bad.key"), TEXT("bad\nline")}}));
    TestTrue(TEXT("valid prefix did not materialize"), ck_game_settings_store_spec::Read_Values(*this, Provider, ECk_GameSettings_Scope::Machine, 0).IsEmpty());
    AddExpectedError(TEXT("Duplicate GameSettings storage identity"), EAutomationExpectedErrorFlags::Contains, 0);
    TestFalse(TEXT("duplicate identity rejects entire batch"), Provider->Request_StoreValues({
        FCk_GameSettings_StoreValue{ECk_GameSettings_Scope::Machine, 0, TEXT("same.key"), TEXT("one")},
        FCk_GameSettings_StoreValue{ECk_GameSettings_Scope::Machine, 0, TEXT("same.key"), TEXT("two")}}));
    TestTrue(TEXT("empty batch succeeds"), Provider->Request_StoreValues({}));
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_GameSettings_TenetIntegrity_RepeatedSettingsSectionsRejected,
    "Ck.CkGameSettings.TenetIntegrity.RepeatedSettingsSectionsRejected",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkTest_GameSettings_TenetIntegrity_RepeatedSettingsSectionsRejected::
    RunTest(
        const FString&)
    -> bool
{
    AddExpectedError(TEXT("GameSettings storage contains repeated settings section"), EAutomationExpectedErrorFlags::Contains, 0);
    for (const auto Scope : {ECk_GameSettings_Scope::Machine, ECk_GameSettings_Scope::Player})
    {
        for (const auto DuplicateKey : {false, true})
        {
            const auto Path = ck_game_settings_store_spec::Make_TempFilePath();
            ON_SCOPE_EXIT
            {
                IFileManager::Get().Delete(*(Path + TEXT(".tmp")));
                IFileManager::Get().Delete(*Path);
            };
            const auto Section = Scope == ECk_GameSettings_Scope::Machine ? FString{TEXT("Machine")} : FString{TEXT("Player.0")};
            const auto SecondKey = DuplicateKey ? FString{TEXT("first.key")} : FString{TEXT("second.key")};
            AddInfo(FString::Printf(TEXT("Repeated-section case: [%s], second key [%s]"), *Section, *SecondKey));
            const auto Content = FString::Printf(TEXT("; original\n[%s]\nfirst.key=one\n[%s]\n%s=two\n"),
                *Section, *Section, *SecondKey);
            if (NOT TestTrue(TEXT("repeated-section fixture written"), FFileHelper::SaveStringToFile(Content, *Path)))
            {
                return false;
            }
            auto OriginalBytes = TArray<uint8>{};
            if (NOT TestTrue(TEXT("repeated-section original bytes captured"), FFileHelper::LoadFileToArray(OriginalBytes, *Path)))
            {
                return false;
            }
            auto* Provider = ck_game_settings_store_spec::Make_Provider(Path);
            auto Values = TArray<FCk_GameSettings_StoredValue>{FCk_GameSettings_StoredValue{TEXT("stale.key"), TEXT("stale")}};
            TestTrue(TEXT("repeated recognized section rejects whole read"), Provider->Get_StoredValues(Scope, 0, Values)
                == ECk_GameSettings_StorageReadResult::Failed);
            TestTrue(TEXT("failed repeated-section read clears output"), Values.IsEmpty());
            TestFalse(TEXT("repeated-section store rejected"), Provider->Request_StoreValue(Scope, 0, TEXT("rejected.key"), TEXT("rejected")));
            TestFalse(TEXT("repeated-section flush rejected"), Provider->Request_Flush());
            auto AfterBytes = TArray<uint8>{};
            TestTrue(TEXT("repeated-section original remains readable"), FFileHelper::LoadFileToArray(AfterBytes, *Path));
            TestTrue(TEXT("repeated-section failures preserve original bytes"), AfterBytes == OriginalBytes);

            const auto Corrected = FString::Printf(TEXT("; corrected\n[%s]\nfirst.key=one\nsecond.key=two\n"), *Section);
            if (NOT TestTrue(TEXT("corrected fixture written"), FFileHelper::SaveStringToFile(Corrected, *Path)))
            {
                return false;
            }
            TestTrue(TEXT("same provider retries after corrected sections"), Provider->Get_StoredValues(Scope, 0, Values)
                == ECk_GameSettings_StorageReadResult::Succeeded);
            TestEqual(TEXT("corrected whole section loaded"), Values.Num(), 2);
            TestTrue(TEXT("corrected provider accepts new value"), Provider->Request_StoreValue(Scope, 0, TEXT("accepted.key"), TEXT("accepted")));
            TestTrue(TEXT("corrected provider flush succeeds"), Provider->Request_Flush());
        }
    }
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_GameSettings_TenetIntegrity_UnknownSectionsRemainOpaque,
    "Ck.CkGameSettings.TenetIntegrity.UnknownSectionsRemainOpaque",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkTest_GameSettings_TenetIntegrity_UnknownSectionsRemainOpaque::
    RunTest(
        const FString&)
    -> bool
{
    const auto Path = ck_game_settings_store_spec::Make_TempFilePath();
    ON_SCOPE_EXIT
    {
        IFileManager::Get().Delete(*(Path + TEXT(".tmp")));
        IFileManager::Get().Delete(*Path);
    };
    const auto Content = FString{TEXT("; preamble\n[FutureSection]\n=value\nfuture.key=one\n[machine]\nexisting.key=opaque-machine\n[player.0]\nexisting.key=opaque-player\n[Machine]\nexisting.key=old\n[FutureSection]\nfuture.key=two\n")};
    if (NOT TestTrue(TEXT("unknown-section fixture written"), FFileHelper::SaveStringToFile(Content, *Path)))
    {
        return false;
    }
    auto* Provider = ck_game_settings_store_spec::Make_Provider(Path);
    auto Values = TArray<FCk_GameSettings_StoredValue>{};
    TestTrue(TEXT("repeated unknown sections do not reject recognized settings"), Provider->Get_StoredValues(ECk_GameSettings_Scope::Machine, 0, Values)
        == ECk_GameSettings_StorageReadResult::Succeeded);
    TestEqual(TEXT("recognized section remains readable"), Values.Num(), 1);
    TestTrue(TEXT("known update accepted"), Provider->Request_StoreValue(ECk_GameSettings_Scope::Machine, 0, TEXT("existing.key"), TEXT("new")));
    TestTrue(TEXT("canonical Player update accepted"), Provider->Request_StoreValue(ECk_GameSettings_Scope::Player, 0, TEXT("existing.key"), TEXT("new-player")));
    TestTrue(TEXT("opaque sections permit flush"), Provider->Request_Flush());
    auto Flushed = FString{};
    TestTrue(TEXT("opaque rewrite readable"), FFileHelper::LoadFileToString(Flushed, *Path));
    Flushed.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
    TestTrue(TEXT("opaque raw line and first section preserved"), Flushed.Contains(TEXT("[FutureSection]\n=value\nfuture.key=one")));
    TestTrue(TEXT("second unknown section preserved"), Flushed.Contains(TEXT("[FutureSection]\nfuture.key=two")));
    TestTrue(TEXT("lowercase Machine section remains opaque"), Flushed.Contains(TEXT("[machine]\nexisting.key=opaque-machine")));
    TestTrue(TEXT("lowercase Player section remains opaque"), Flushed.Contains(TEXT("[player.0]\nexisting.key=opaque-player")));
    TestTrue(TEXT("known value updated"), Flushed.Contains(TEXT("[Machine]\nexisting.key=new")));
    TestTrue(TEXT("canonical Player section created separately"), Flushed.Contains(TEXT("[Player.0]\nexisting.key=new-player")));
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
