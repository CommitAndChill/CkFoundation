#include "CkSlateLayout/CkUiFontRegistry.h"

#include "Fonts/CompositeFont.h"
#include "Fonts/FontFaceInterface.h"
#include "Templates/Casts.h"
#include "UObject/UObjectGlobals.h"

namespace ck_ui_font_registry
{
    auto LoadFailure(const FString& InMessage) -> FCkUiLoadResult
    {
        auto Result = FCkUiLoadResult{};
        Result.Errors.Add(InMessage);
        return Result;
    }

    auto IsName(const FString& InName) -> bool
    {
        if (InName.IsEmpty() || InName[0] < TEXT('a') || InName[0] > TEXT('z'))
        { return false; }
        for (const TCHAR Character : InName)
        {
            if (!((Character >= TEXT('a') && Character <= TEXT('z'))
                || (Character >= TEXT('0') && Character <= TEXT('9')) || Character == TEXT('-')))
            { return false; }
        }
        return true;
    }

    auto ValidateTypeface(const FTypeface& InTypeface, TSet<FName>& OutFaces) -> FCkUiLoadResult
    {
        for (const FTypefaceEntry& Entry : InTypeface.Fonts)
        {
            if (Entry.Name != FName{TEXT("Regular")} && Entry.Name != FName{TEXT("Bold")})
            { return LoadFailure(TEXT("Registered font typefaces support only Regular and Bold.")); }
            if (OutFaces.Contains(Entry.Name))
            { return LoadFailure(TEXT("Registered font typefaces may not contain duplicate faces.")); }
            const UObject* Asset = Entry.Font.GetFontFaceAsset();
            if (!IsValid(Asset))
            { return LoadFailure(TEXT("Registered font faces require a valid owned font-face asset; file paths are not admitted.")); }
            const IFontFaceInterface* Face = Cast<const IFontFaceInterface>(Asset);
            if (Face == nullptr)
            { return LoadFailure(TEXT("Registered font face assets must implement the font-face interface.")); }
            if (!Face->GetFontFaceData()->HasData())
            { return LoadFailure(TEXT("Registered font faces require non-empty owned memory data.")); }
            if (Face->GetLoadingPolicy() != EFontLoadingPolicy::Inline)
            { return LoadFailure(TEXT("Registered font faces require inline owned memory data.")); }
            if (Entry.Font.GetSubFaceIndex() != 0)
            { return LoadFailure(TEXT("Registered font faces support only the first font sub-face.")); }
            OutFaces.Add(Entry.Name);
        }
        auto Result = FCkUiLoadResult{};
        Result.Succeeded = true;
        return Result;
    }
}

auto FCkUiFontRegistrySnapshot::TryResolve(const FString& InFamily, const FName InFace,
    FSlateFontInfo& OutFont) const -> bool
{
    if (InFace != FName{TEXT("Regular")} && InFace != FName{TEXT("Bold")}) { return false; }
    const TSharedPtr<const FCompositeFont>* Font = _Families.Find(InFamily);
    if (Font == nullptr || !Font->IsValid()) { return false; }
    for (const FTypefaceEntry& Entry : (*Font)->DefaultTypeface.Fonts)
    {
        if (Entry.Name == InFace)
        {
            OutFont = FSlateFontInfo(*Font, 10.0f, InFace);
            return true;
        }
    }
    return false;
}

auto FCkUiFontRegistrySnapshot::HasFamily(const FString& InFamily) const -> bool
{
    return _Families.Contains(InFamily);
}

auto FCkUiFontRegistry::IsFamilyName(const FString& InName) -> bool
{
    return ck_ui_font_registry::IsName(InName)
        && InName != TEXT("monospace") && InName != TEXT("sans-serif");
}

auto FCkUiFontRegistry::Register(FString InName, TSharedPtr<const FCompositeFont> InFont) -> FCkUiLoadResult
{
    using namespace ck_ui_font_registry;
    if (!IsInGameThread())
    { return LoadFailure(TEXT("Font family registration requires the game thread.")); }
    if (!IsFamilyName(InName))
    { return LoadFailure(TEXT("Font family names must start with a-z, contain only a-z, 0-9, or hyphen, and may not be monospace or sans-serif.")); }
    if (_Families.Contains(InName))
    { return LoadFailure(FString::Printf(TEXT("Font family '%s' is already registered."), *InName)); }
    if (!InFont.IsValid())
    { return LoadFailure(TEXT("Font family registration requires an owned immutable composite font.")); }
    if (!FMath::IsFinite(InFont->FallbackTypeface.ScalingFactor) || InFont->FallbackTypeface.ScalingFactor <= 0.0f)
    { return LoadFailure(TEXT("Registered font families require a finite positive fallback scale.")); }
    if (!InFont->SubTypefaces.IsEmpty())
    { return LoadFailure(TEXT("Registered font families support primary and fallback typefaces only.")); }
    auto PrimaryFaces = TSet<FName>{};
    const FCkUiLoadResult PrimaryResult = ValidateTypeface(InFont->DefaultTypeface, PrimaryFaces);
    if (!PrimaryResult.Succeeded) { return PrimaryResult; }
    if (!PrimaryFaces.Contains(FName{TEXT("Regular")}))
    { return LoadFailure(TEXT("Font family registration requires a regular primary face.")); }
    auto FallbackFaces = TSet<FName>{};
    const FCkUiLoadResult FallbackResult = ValidateTypeface(InFont->FallbackTypeface.Typeface, FallbackFaces);
    if (!FallbackResult.Succeeded) { return FallbackResult; }
    if (FallbackFaces.Contains(FName{TEXT("Bold")})
        && (!FallbackFaces.Contains(FName{TEXT("Regular")}) || !PrimaryFaces.Contains(FName{TEXT("Bold")})))
    { return LoadFailure(TEXT("A bold fallback requires both a regular fallback and a primary bold face.")); }
    _Families.Add(MoveTemp(InName), MoveTemp(InFont));
    auto Result = FCkUiLoadResult{};
    Result.Succeeded = true;
    return Result;
}

auto FCkUiFontRegistry::CreateSnapshot() const -> TSharedRef<const FCkUiFontRegistrySnapshot>
{
    const TSharedRef<FCkUiFontRegistrySnapshot> Snapshot = MakeShared<FCkUiFontRegistrySnapshot>();
    Snapshot->_Families = _Families;
    return Snapshot;
}
