#include "CkGameSettings_IniStorageProvider.h"

#include "CkCore/Ensure/CkEnsure.h"
#include "CkCore/Format/CkFormat.h"

#include "CkGameSettings/CkGameSettings_Log.h"

#include <HAL/FileManager.h>
#include <HAL/PlatformProperties.h>
#include <Misc/FileHelper.h>
#include <Misc/Paths.h>

#if PLATFORM_WINDOWS
#include "Microsoft/WindowsHWrapper.h"
#elif PLATFORM_MAC || PLATFORM_LINUX
#include <cerrno>
#include <cstdio>
#include <sys/stat.h>
#endif

namespace ck_game_settings_ini_storage
{
    auto
        Get_PhysicalPath(
            const FString& InPath)
        -> FString
    {
        auto Result = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*InPath);
#if PLATFORM_WINDOWS
        Result.ReplaceInline(TEXT("/"), TEXT("\\"));
        if (Result.StartsWith(TEXT("\\\\?\\")))
        { return Result; }
        Result = FPaths::ConvertRelativePathToFull(Result);
        FPaths::CollapseRelativeDirectories(Result);
        Result.ReplaceInline(TEXT("/"), TEXT("\\"));
        if (Result.StartsWith(TEXT("\\\\")))
        { return FString{TEXT("\\\\?\\UNC\\")} + Result.Mid(2); }
        return FString{TEXT("\\\\?\\")} + Result;
#else
        return Result;
#endif
    }

    auto
        Get_IsPhysicallyAbsent(
            const FString& InPath)
        -> bool
    {
        const auto Path = Get_PhysicalPath(InPath);
#if PLATFORM_WINDOWS
        if (::GetFileAttributesW(*Path) != INVALID_FILE_ATTRIBUTES)
        { return false; }
        const auto Error = ::GetLastError();
        return Error == ERROR_FILE_NOT_FOUND || Error == ERROR_PATH_NOT_FOUND;
#elif PLATFORM_MAC || PLATFORM_LINUX
        struct stat Info{};
        return ::stat(TCHAR_TO_UTF8(*Path), &Info) != 0 && errno == ENOENT;
#else
        return false;
#endif
    }

    auto
        Get_IsSettingsSection(
            const FString& InName)
        -> bool
    {
        if (InName.Equals(TEXT("Machine"), ESearchCase::CaseSensitive))
        { return true; }
        if (NOT InName.StartsWith(TEXT("Player."), ESearchCase::CaseSensitive))
        { return false; }
        const auto UserIdText = InName.Mid(7);
        auto UserId = int32{};
        return LexTryParseString(UserId, *UserIdText) && LexToString(UserId).Equals(UserIdText, ESearchCase::CaseSensitive);
    }

    auto
        Request_Install(
            const FString& InDestination,
            const FString& InTemporary)
        -> bool
    {
        const auto Destination = Get_PhysicalPath(InDestination);
        const auto Temporary = Get_PhysicalPath(InTemporary);
#if PLATFORM_WINDOWS
        return ::MoveFileExW(*Temporary, *Destination, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#elif PLATFORM_MAC || PLATFORM_LINUX
        return ::rename(TCHAR_TO_UTF8(*Temporary), TCHAR_TO_UTF8(*Destination)) == 0;
#else
        return false;
#endif
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_GameSettings_IniStorageProvider_UE::
    Get_StorageFilePath() const
    -> FString
{
    if (NOT _FilePathOverride.IsEmpty())
    { return _FilePathOverride; }

    return FPaths::Combine(FPaths::GeneratedConfigDir(), FPlatformProperties::PlatformName(), TEXT("CkGameSettings.ini"));
}

auto
    UCk_GameSettings_IniStorageProvider_UE::
    Set_FilePathOverride(
        const FString& InFilePath)
    -> void
{
    _FilePathOverride = InFilePath;
    _PreambleLines.Reset();
    _Sections.Reset();
    _Loaded = false;
    _LoadResult = ECk_GameSettings_StorageReadResult::Failed;
    _Dirty = false;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_GameSettings_IniStorageProvider_UE::
    Get_StoredValues_Implementation(
        ECk_GameSettings_Scope InScope,
        int32 InPlatformUserId,
        TArray<FCk_GameSettings_StoredValue>& OutValues) const
    -> ECk_GameSettings_StorageReadResult
{
    OutValues.Reset();
    CK_ENSURE_IF_NOT(InScope == ECk_GameSettings_Scope::Machine || InScope == ECk_GameSettings_Scope::Player, TEXT("Invalid GameSettings storage scope"))
    { return ECk_GameSettings_StorageReadResult::Failed; }
    const auto ReadResult = DoEnsureLoaded();
    if (ReadResult == ECk_GameSettings_StorageReadResult::Failed)
    { return ECk_GameSettings_StorageReadResult::Failed; }

    const auto* Section = DoFindSection(Get_SectionName(InScope, InPlatformUserId));
    if (Section != nullptr)
    {
        for (const auto& Line : Section->_Lines)
        {
            if (Line._Key.IsNone())
            { continue; }
            auto EqualsIndex = int32{INDEX_NONE};
            Line._Raw.FindChar(TEXT('='), EqualsIndex);
            OutValues.Add(FCk_GameSettings_StoredValue{Line._Key, Line._Raw.Mid(EqualsIndex + 1)});
        }
    }
    return ReadResult;
}

auto
    UCk_GameSettings_IniStorageProvider_UE::
    Request_StoreValues_Implementation(
        const TArray<FCk_GameSettings_StoreValue>& InValues)
    -> bool
{
    if (InValues.IsEmpty())
    { return true; }


#if NOT CK_DISABLE_ENSURE_CHECKS
    auto Identities = TSet<FString>{};
    for (const auto& Value : InValues)
    {
        CK_ENSURE_IF_NOT(Value.Get_Scope() == ECk_GameSettings_Scope::Machine || Value.Get_Scope() == ECk_GameSettings_Scope::Player,
            TEXT("Invalid GameSettings storage scope"))
        { return false; }
        CK_ENSURE_IF_NOT(NOT Value.Get_Key().IsNone() && NOT Value.Get_Key().ToString().Contains(TEXT("="))
            && NOT Value.Get_Key().ToString().Contains(TEXT("\n")) && NOT Value.Get_Key().ToString().Contains(TEXT("\r")),
            TEXT("Invalid GameSettings storage key [{}]"), Value.Get_Key())
        { return false; }
        CK_ENSURE_IF_NOT(NOT Value.Get_Value().Contains(TEXT("\n")) && NOT Value.Get_Value().Contains(TEXT("\r")),
            TEXT("GameSettings value for key [{}] contains a newline and cannot be stored in the ini provider"), Value.Get_Key())
        { return false; }
        const auto Identity = ck::Format_UE(TEXT("{}:{}"), Get_SectionName(Value.Get_Scope(), Value.Get_PlatformUserId()), Value.Get_Key().ToString().ToLower());
        CK_ENSURE_IF_NOT(NOT Identities.Contains(Identity), TEXT("Duplicate GameSettings storage identity [{}]"), Identity)
        { return false; }
        Identities.Add(Identity);
    }
#endif

    const auto ReadResult = DoEnsureLoaded();
    if (ReadResult == ECk_GameSettings_StorageReadResult::Failed)
    { return false; }

    auto Sections = _Sections;
    auto Changed = false;
    for (const auto& Value : InValues)
    {
        const auto SectionName = Get_SectionName(Value.Get_Scope(), Value.Get_PlatformUserId());
        auto* Section = Sections.FindByPredicate([&](const FIniSection& InSection) { return InSection._Name.Equals(SectionName, ESearchCase::CaseSensitive); });
        if (Section == nullptr)
        {
            const auto Index = Sections.Emplace(FIniSection{SectionName, {}});
            Section = &Sections[Index];
        }
        const auto Raw = ck::Format_UE(TEXT("{}={}"), Value.Get_Key(), Value.Get_Value());
        if (auto* Line = Section->_Lines.FindByPredicate([&](const FIniLine& InLine) { return InLine._Key == Value.Get_Key(); }))
        {
            Changed = Changed || Line->_Raw != Raw;
            Line->_Raw = Raw;
        }
        else
        {
            Section->_Lines.Add(FIniLine{Raw, Value.Get_Key()});
            Changed = true;
        }
    }
    _Sections = MoveTemp(Sections);
    _Dirty = _Dirty || Changed;
    _LoadResult = ECk_GameSettings_StorageReadResult::Succeeded;
    return true;
}

auto
    UCk_GameSettings_IniStorageProvider_UE::
    Request_RemoveValue_Implementation(
        ECk_GameSettings_Scope InScope,
        int32 InPlatformUserId,
        FName InKey)
    -> bool
{
    CK_ENSURE_IF_NOT(InScope == ECk_GameSettings_Scope::Machine || InScope == ECk_GameSettings_Scope::Player, TEXT("Invalid GameSettings storage scope"))
    { return false; }
    const auto ReadResult = DoEnsureLoaded();
    if (ReadResult == ECk_GameSettings_StorageReadResult::Failed)
    { return false; }
    if (auto* Section = DoFindSection(Get_SectionName(InScope, InPlatformUserId)))
    {
        const auto RemovedCount = Section->_Lines.RemoveAll([&](const FIniLine& InLine) { return InLine._Key == InKey; });
        _Dirty = _Dirty || RemovedCount > 0;
    }
    return true;
}

auto
    UCk_GameSettings_IniStorageProvider_UE::
    Request_Flush_Implementation()
    -> bool
{
    const auto ReadResult = DoEnsureLoaded();
    if (ReadResult == ECk_GameSettings_StorageReadResult::Failed)
    { return false; }
    if (NOT _Dirty)
    { return true; }

    auto Lines = TArray<FString>{};
    Lines.Append(_PreambleLines);

    for (const auto& Section : _Sections)
    {
        Lines.Add(ck::Format_UE(TEXT("[{}]"), Section._Name));

        for (const auto& Line : Section._Lines)
        { Lines.Add(Line._Raw); }
    }

    const auto Content = FString::Join(Lines, LINE_TERMINATOR) + LINE_TERMINATOR;
    const auto FilePath = Get_StorageFilePath();
    const auto TempFilePath = FilePath + TEXT(".tmp");

    constexpr auto CreateParentTree = true;
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(FilePath), CreateParentTree);

    const auto TempFileSaved = FFileHelper::SaveStringToFile(Content, *TempFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    if (NOT TempFileSaved)
    { return false; }

    const auto TempFileMoved = ck_game_settings_ini_storage::Request_Install(FilePath, TempFilePath);
    if (NOT TempFileMoved)
    { return false; }

    _Dirty = false;
    ck::game_settings::Verbose(TEXT("Flushed GameSettings store to [{}]"), FilePath);
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_GameSettings_IniStorageProvider_UE::
    DoEnsureLoaded() const
    -> ECk_GameSettings_StorageReadResult
{
    if (_Loaded)
    { return _LoadResult; }

    auto PreambleLines = TArray<FString>{};
    auto Sections = TArray<FIniSection>{};

    auto FileContent = FString{};
    if (NOT FFileHelper::LoadFileToString(FileContent, *Get_StorageFilePath()))
    {
        if (ck_game_settings_ini_storage::Get_IsPhysicallyAbsent(Get_StorageFilePath()))
        {
            _Loaded = true;
            _LoadResult = ECk_GameSettings_StorageReadResult::NotFound;
            return _LoadResult;
        }
        return ECk_GameSettings_StorageReadResult::Failed;
    }

    auto FileLines = TArray<FString>{};
    constexpr auto CullEmptyLines = false;
    FileContent.ParseIntoArrayLines(FileLines, CullEmptyLines);

    auto CurrentSectionIndex = int32{INDEX_NONE};
    auto SettingsSections = TSet<FString>{};

    for (const auto& FileLine : FileLines)
    {
        const auto TrimmedLine = FileLine.TrimStartAndEnd();

        if (TrimmedLine.StartsWith(TEXT("[")) && TrimmedLine.EndsWith(TEXT("]")))
        {
            const auto SectionName = TrimmedLine.Mid(1, TrimmedLine.Len() - 2);
            if (ck_game_settings_ini_storage::Get_IsSettingsSection(SectionName))
            {
                const auto Identity = SectionName;
                if (SettingsSections.Contains(Identity))
                {
                    ck::game_settings::Error(TEXT("GameSettings storage contains repeated settings section [{}], refusing load [{}]"),
                        SectionName, Get_StorageFilePath());
                    return ECk_GameSettings_StorageReadResult::Failed;
                }
                SettingsSections.Add(Identity);
            }
            CurrentSectionIndex = Sections.Emplace(FIniSection{SectionName, {}});
            continue;
        }

        auto Key = FName{};
        const auto IsCommentLine = TrimmedLine.StartsWith(TEXT(";")) || TrimmedLine.StartsWith(TEXT("#"));

        if (NOT IsCommentLine)
        {
            auto EqualsIndex = int32{INDEX_NONE};
            if (FileLine.FindChar(TEXT('='), EqualsIndex) && EqualsIndex > 0)
            { Key = FName{*FileLine.Left(EqualsIndex).TrimStartAndEnd()}; }
        }

        if (CurrentSectionIndex == INDEX_NONE)
        {
            PreambleLines.Add(FileLine);
            continue;
        }

        Sections[CurrentSectionIndex]._Lines.Add(FIniLine{FileLine, Key});
    }
    _PreambleLines = MoveTemp(PreambleLines);
    _Sections = MoveTemp(Sections);
    _Loaded = true;
    _LoadResult = ECk_GameSettings_StorageReadResult::Succeeded;
    return _LoadResult;
}

auto
    UCk_GameSettings_IniStorageProvider_UE::
    DoFindSection(
        const FString& InSectionName) const
    -> const FIniSection*
{
    return _Sections.FindByPredicate([&](const FIniSection& InSection) { return InSection._Name.Equals(InSectionName); });
}

auto
    UCk_GameSettings_IniStorageProvider_UE::
    DoFindSection(
        const FString& InSectionName)
    -> FIniSection*
{
    return _Sections.FindByPredicate([&](const FIniSection& InSection) { return InSection._Name.Equals(InSectionName); });
}

auto
    UCk_GameSettings_IniStorageProvider_UE::
    DoFindOrAddSection(
        const FString& InSectionName)
    -> FIniSection&
{
    if (auto* ExistingSection = _Sections.FindByPredicate([&](const FIniSection& InSection) { return InSection._Name.Equals(InSectionName); }))
    { return *ExistingSection; }

    const auto NewSectionIndex = _Sections.Emplace(FIniSection{InSectionName, {}});
    return _Sections[NewSectionIndex];
}

auto
    UCk_GameSettings_IniStorageProvider_UE::
    Get_SectionName(
        ECk_GameSettings_Scope InScope,
        int32 InPlatformUserId)
    -> FString
{
    switch (InScope)
    {
        case ECk_GameSettings_Scope::Machine:
        {
            return TEXT("Machine");
        }
        case ECk_GameSettings_Scope::Player:
        {
            return ck::Format_UE(TEXT("Player.{}"), InPlatformUserId);
        }
        default:
        {
            CK_INVALID_ENUM(InScope);
            return TEXT("Machine");
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------
