#include "CkPixelSprite_File.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkPixelSprite/CkPixelSprite_Data.h"
#include "Containers/StringConv.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/PlatformFileManager.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#elif PLATFORM_LINUX || PLATFORM_MAC
#include <sys/stat.h>
#endif

namespace ck::pixel_sprite::file_private
{
    constexpr int64 MaxSourceBytes = 8 * 1024 * 1024;
    constexpr int32 MaxSourcePathCharacters = 1024;

    static auto
    Is_AsciiLetter(
        const TCHAR InCharacter)
        -> bool
    {
        return (InCharacter >= TEXT('A') && InCharacter <= TEXT('Z')) ||
               (InCharacter >= TEXT('a') && InCharacter <= TEXT('z'));
    }

    static auto
    Is_ForbiddenSourceCharacter(
        const TCHAR InCharacter)
        -> bool
    {
        return InCharacter < 0x20 ||
               (InCharacter >= 0x7F && InCharacter <= 0x9F) ||
               InCharacter == TEXT(':') || InCharacter == TEXT('<') ||
               InCharacter == TEXT('>') || InCharacter == TEXT('"') ||
               InCharacter == TEXT('|') || InCharacter == TEXT('?') ||
               InCharacter == TEXT('*') || InCharacter == TEXT('\\');
    }

    static auto
    Is_ReservedDeviceComponent(
        const FString& InComponent)
        -> bool
    {
        const int32 Dot = InComponent.Find(TEXT("."));
        FString Stem = InComponent.Left(Dot == INDEX_NONE ? InComponent.Len() : Dot).ToUpper();
        // Windows treats ASCII spaces immediately before the extension as
        // insignificant for device names (for example, "CON .xpm").
        while (Stem.EndsWith(TEXT(" ")))
        {
            Stem = Stem.Left(Stem.Len() - 1);
        }
        if (Stem == TEXT("CON") || Stem == TEXT("PRN") ||
            Stem == TEXT("AUX") || Stem == TEXT("NUL"))
        {
            return true;
        }
        if (Stem.Len() == 4 &&
            (Stem.StartsWith(TEXT("COM")) || Stem.StartsWith(TEXT("LPT"))))
        {
            const TCHAR Suffix = Stem[3];
            return (Suffix >= TEXT('1') && Suffix <= TEXT('9')) ||
                   Suffix == 0x00B9 || Suffix == 0x00B2 || Suffix == 0x00B3;
        }
        return false;
    }

    static auto
    Is_SafeSourceComponent(
        const FString& InComponent)
        -> bool
    {
        if (InComponent.IsEmpty() || InComponent == TEXT(".") ||
            InComponent == TEXT("..") || InComponent.EndsWith(TEXT(" ")) ||
            InComponent.EndsWith(TEXT(".")) || Is_ReservedDeviceComponent(InComponent))
        {
            return false;
        }
        for (const TCHAR Character : InComponent)
        {
            if (Is_ForbiddenSourceCharacter(Character))
            {
                return false;
            }
        }
        return true;
    }

    // FString is UTF-16 on Windows and UTF-32 on some other platforms.
    // Count Unicode scalar values, matching Python len() on valid strings.
    // A negative result denotes malformed surrogate data.
    static auto
    Count_SourceCodePoints(
        const FString& InText)
        -> int32
    {
        int32 Count = 0;
        for (int32 Index = 0; Index < InText.Len(); ++Index)
        {
            const uint32 CodeUnit = static_cast<uint32>(InText[Index]);
            if constexpr (sizeof(TCHAR) == 2)
            {
                if (CodeUnit >= 0xD800 && CodeUnit <= 0xDBFF)
                {
                    if (Index + 1 >= InText.Len())
                    {
                        return INDEX_NONE;
                    }
                    const uint32 Low = static_cast<uint32>(InText[++Index]);
                    if (Low < 0xDC00 || Low > 0xDFFF)
                    {
                        return INDEX_NONE;
                    }
                }
                else if (CodeUnit >= 0xDC00 && CodeUnit <= 0xDFFF)
                {
                    return INDEX_NONE;
                }
            }
            else if (CodeUnit > 0x10FFFF ||
                     (CodeUnit >= 0xD800 && CodeUnit <= 0xDFFF))
            {
                return INDEX_NONE;
            }
            ++Count;
        }
        return Count;
    }

    // InRelativePath is the decoded, non-null JSON string. The schema layer
    // rejects a JSON null before calling this helper; no FString sentinel is
    // used to conflate null with an empty source string.
    auto
    Validate_SourcePath(
        const FString& InRelativePath,
        FString& OutDiagnostic)
        -> bool
    {
        const int32 CodePointCount = Count_SourceCodePoints(InRelativePath);
        if (InRelativePath.IsEmpty() || CodePointCount == INDEX_NONE ||
            CodePointCount > MaxSourcePathCharacters ||
            InRelativePath.StartsWith(TEXT("/")) ||
            NOT InRelativePath.EndsWith(TEXT(".xpm"), ESearchCase::IgnoreCase))
        {
            OutDiagnostic = TEXT("source must be a forward-slash relative .xpm path of at most 1024 characters");
            return false;
        }

        int32 SegmentStart = 0;
        for (int32 Index = 0; Index <= InRelativePath.Len(); ++Index)
        {
            if (Index != InRelativePath.Len() && InRelativePath[Index] != TEXT('/'))
            {
                continue;
            }
            if (NOT Is_SafeSourceComponent(InRelativePath.Mid(SegmentStart, Index - SegmentStart)))
            {
                OutDiagnostic = TEXT("source contains an empty, traversal, control, device, or unsafe character component");
                return false;
            }
            SegmentStart = Index + 1;
        }
        OutDiagnostic.Empty();
        return true;
    }

    static auto
    Is_StrictUtf8(
        const TArray<uint8>& InBytes,
        const int32 InStart)
        -> bool
    {
        for (int32 Index = InStart; Index < InBytes.Num();)
        {
            const uint8 First = InBytes[Index++];
            if (First < 0x80)
            {
                continue;
            }

            int32 Following = 0;
            uint32 CodePoint = 0;
            uint32 Minimum = 0;
            if (First >= 0xC2 && First <= 0xDF)
            {
                Following = 1;
                CodePoint = First & 0x1F;
                Minimum = 0x80;
            }
            else if (First >= 0xE0 && First <= 0xEF)
            {
                Following = 2;
                CodePoint = First & 0x0F;
                Minimum = 0x800;
            }
            else if (First >= 0xF0 && First <= 0xF4)
            {
                Following = 3;
                CodePoint = First & 0x07;
                Minimum = 0x10000;
            }
            else
            {
                return false;
            }
            if (Following > InBytes.Num() - Index)
            {
                return false;
            }

            for (int32 Continuation = 0; Continuation < Following; ++Continuation)
            {
                const uint8 Next = InBytes[Index++];
                if ((Next & 0xC0) != 0x80)
                {
                    return false;
                }
                CodePoint = (CodePoint << 6) | (Next & 0x3F);
            }
            if (CodePoint < Minimum || CodePoint > 0x10FFFF ||
                (CodePoint >= 0xD800 && CodePoint <= 0xDFFF))
            {
                return false;
            }
        }
        return true;
    }

    // Check existence and type before link inspection. The physical platform
    // file is still used for opening, regardless of the platform-specific
    // attribute check below. Unsupported inspection always rejects.
    static auto
    Is_ExistingOrdinaryComponent(
        IPlatformFile& InFile,
        const FString& InPath,
        const bool bInFinalFile)
        -> bool
    {
        const FFileStatData Data = InFile.GetStatData(*InPath);
        if (NOT Data.bIsValid || Data.bIsDirectory == bInFinalFile ||
            (bInFinalFile ? NOT InFile.FileExists(*InPath)
                          : NOT InFile.DirectoryExists(*InPath)))
        {
            return false;
        }

#if PLATFORM_WINDOWS
        if (InFile.IsSymlink(*InPath) != ESymlinkResult::NonSymlink)
        {
            return false;
        }
        const uint32 Attributes = ::GetFileAttributesW(*InPath);
        return Attributes != INVALID_FILE_ATTRIBUTES &&
               (Attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0 &&
               (bInFinalFile ? (Attributes & FILE_ATTRIBUTE_DIRECTORY) == 0
                             : (Attributes & FILE_ATTRIBUTE_DIRECTORY) != 0);
#elif PLATFORM_LINUX || PLATFORM_MAC
        struct stat NativeStat;
        if (::lstat(TCHAR_TO_UTF8(*InPath), &NativeStat) != 0)
        {
            return false;
        }
        return bInFinalFile ? S_ISREG(NativeStat.st_mode)
                            : S_ISDIR(NativeStat.st_mode);
#else
        return false;
#endif
    }

    // OutText and OutByteCount are published only after full validation.
    // Failures write OutDiagnostic and preserve both other outputs.
    auto
    Load_NativeSource(
        const FString& InAbsolutePath,
        FString& OutText,
        int64& OutByteCount,
        FCk_PixelSprite_Diagnostic& OutDiagnostic)
        -> bool
    {
        const auto Fail = [&InAbsolutePath, &OutDiagnostic](
            const FString& InMessage,
            const FString& InCode = TEXT("io")) -> bool
        {
            OutDiagnostic = {InAbsolutePath, InCode, InMessage};
            return false;
        };

#if !(PLATFORM_WINDOWS || PLATFORM_LINUX || PLATFORM_MAC)
        return Fail(TEXT("native link inspection is unsupported on this platform"), TEXT("platform"));
#else
        FString Path = InAbsolutePath;
#if PLATFORM_WINDOWS
        Path.ReplaceCharInline(TEXT('\\'), TEXT('/'));
        // Local drive only; reject UNC, device namespaces and drive-relative.
        if (Path.Len() < 4 || NOT Is_AsciiLetter(Path[0]) ||
            Path[1] != TEXT(':') || Path[2] != TEXT('/') || Path[3] == TEXT('/'))
        {
            return Fail(TEXT("source path must be an absolute local drive path"));
        }
        FString Prefix = Path.Left(3);
        constexpr int32 RootLength = 3;
#else
        if (Path.Len() < 2 || Path[0] != TEXT('/') || Path[1] == TEXT('/'))
        {
            return Fail(TEXT("source path must be an absolute local POSIX path"));
        }
        FString Prefix = TEXT("/");
        constexpr int32 RootLength = 1;
#endif
        auto& File = FPlatformFileManager::Get().GetPlatformPhysical();
        if (NOT Is_ExistingOrdinaryComponent(File, Prefix, false))
        {
            return Fail(TEXT("source root is missing, linked, or cannot be inspected"));
        }

        int32 SegmentStart = RootLength;
        for (int32 Index = RootLength; Index <= Path.Len(); ++Index)
        {
            if (Index != Path.Len() && Path[Index] != TEXT('/'))
            {
                continue;
            }
            const FString Segment = Path.Mid(SegmentStart, Index - SegmentStart);
            if (Segment.IsEmpty() || Segment == TEXT(".") || Segment == TEXT(".."))
            {
                return Fail(TEXT("source path has an empty or traversal component"));
            }
            for (const TCHAR Character : Segment)
            {
                if (Character < 0x20 || (Character >= 0x7F && Character <= 0x9F))
                {
                    return Fail(TEXT("source path has a control character"));
                }
            }
#if PLATFORM_WINDOWS
            if (NOT Is_SafeSourceComponent(Segment))
            {
                return Fail(TEXT("source path has a nonordinary Windows component"));
            }
#endif
            Prefix += Segment;
            const bool bFinalFile = Index == Path.Len();
            if (NOT Is_ExistingOrdinaryComponent(File, Prefix, bFinalFile))
            {
                return Fail(TEXT("source path component is missing, linked, or cannot be inspected"));
            }
            Prefix += TEXT("/");
            SegmentStart = Index + 1;
        }

        auto Opened = File.OpenRead(*Path, IPlatformFile::EOpenReadFlags::None);
        if (NOT Opened.HasValue())
        {
            return Fail(TEXT("source could not be opened through the physical platform file"));
        }
        auto& Handle = Opened.GetValue(); // Owned TUniquePtr<IFileHandle>.
        const int64 Size = Handle->Size();
        if (Size < 0)
        {
            return Fail(TEXT("source size could not be determined"));
        }
        if (Size > MaxSourceBytes)
        {
            return Fail(TEXT("source exceeds 8 MiB"), TEXT("source_limit"));
        }
        if (Handle->Tell() != 0)
        {
            return Fail(TEXT("source handle is not at the beginning"));
        }

        TArray<uint8> Bytes;
        Bytes.SetNumUninitialized(static_cast<int32>(Size));
        if (Size != 0 &&
            (NOT Handle->Read(Bytes.GetData(), Size) || Handle->Tell() != Size))
        {
            return Fail(TEXT("source read was short or failed"));
        }
        if (Handle->Size() != Size || Handle->Tell() != Size)
        {
            return Fail(TEXT("source size changed during read"));
        }

        const int32 Offset = Bytes.Num() >= 3 && Bytes[0] == 0xEF &&
            Bytes[1] == 0xBB && Bytes[2] == 0xBF ? 3 : 0;
        if (NOT Is_StrictUtf8(Bytes, Offset))
        {
            return Fail(TEXT("source is not strict UTF-8"), TEXT("utf8"));
        }
        const int32 TextBytes = Bytes.Num() - Offset;
        FString Candidate;
        if (TextBytes > 0)
        {
            const FUTF8ToTCHAR Converted(
                reinterpret_cast<const ANSICHAR*>(Bytes.GetData() + Offset), TextBytes);
            Candidate = FString::ConstructFromPtrSize(Converted.Get(), Converted.Length());
        }
        OutText = MoveTemp(Candidate);
        OutByteCount = Size; // Includes the optional UTF-8 BOM.
        OutDiagnostic = {};
        return true;
#endif
    }
}
