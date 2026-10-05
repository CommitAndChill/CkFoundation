#include "CkWidgets/Authoring/CkUiFontResources.h"

#include "CkSlateLayout/CkUiFontRegistry.h"
#include "Engine/FontFace.h"
#include "Fonts/CompositeFont.h"
#include "Fonts/FontCache.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/FileHelper.h"
#include "Rendering/SlateRenderer.h"
#include "UObject/GCObject.h"
#include "UObject/UObjectGlobals.h"

namespace ck_ui_font_resources
{
    /** Unlike FStandaloneCompositeFont, this also roots FallbackTypeface's face assets. */
    class FOwnedCompositeFont final : public FCompositeFont, public FGCObject
    {
    public:
        void AddReferencedObjects(FReferenceCollector& InCollector) override
        {
            for (FTypefaceEntry& Entry : DefaultTypeface.Fonts)
            { Entry.Font.AddReferencedObjects(InCollector); }
            for (FTypefaceEntry& Entry : FallbackTypeface.Typeface.Fonts)
            { Entry.Font.AddReferencedObjects(InCollector); }
            for (FCompositeSubFont& SubFont : SubTypefaces)
            {
                for (FTypefaceEntry& Entry : SubFont.Typeface.Fonts)
                { Entry.Font.AddReferencedObjects(InCollector); }
            }
        }

        auto GetReferencerName() const -> FString override
        { return TEXT("CkUiFontResources"); }
    };

    auto LoadFailure(const FString& InMessage) -> FCkUiLoadResult
    {
        auto Result = FCkUiLoadResult{};
        Result.Errors.Add(InMessage);
        return Result;
    }

    struct FPreparedFace
    {
        FName Name;
        FString File;
        bool bFallback = false;
        FFontFaceDataPtr Data;
    };
}

auto FCkUiFontResources::Register(FCkUiFontRegistry& InRegistry, FCkUiFontFamilyRegistration InRegistration) -> FCkUiLoadResult
{
    using namespace ck_ui_font_resources;
    if (!IsInGameThread())
    { return LoadFailure(TEXT("Font family registration requires the game thread.")); }
    if (!FCkUiFontRegistry::IsFamilyName(InRegistration.Name))
    { return LoadFailure(TEXT("Font family names must start with a-z, contain only a-z, 0-9, or hyphen, and may not be monospace or sans-serif.")); }
    if (InRegistry.CreateSnapshot()->HasFamily(InRegistration.Name))
    { return LoadFailure(FString::Printf(TEXT("Font family '%s' is already registered."), *InRegistration.Name)); }
    if (InRegistration.FallbackBoldFile.IsSet()
        && (!InRegistration.FallbackRegularFile.IsSet() || !InRegistration.BoldFile.IsSet()))
    { return LoadFailure(TEXT("A bold fallback requires both a regular fallback and a primary bold face.")); }
    if (!FSlateApplication::IsInitialized())
    { return LoadFailure(TEXT("Font family registration requires initialized Slate font services.")); }
    FSlateRenderer* Renderer = FSlateApplication::Get().GetRenderer();
    if (Renderer == nullptr)
    { return LoadFailure(TEXT("Font family registration requires a Slate renderer.")); }
    const TSharedRef<FSlateFontCache> Cache = Renderer->GetFontCache();

    auto Faces = TArray<FPreparedFace>{};
    Faces.Add({FName{TEXT("Regular")}, MoveTemp(InRegistration.RegularFile), false, {}});
    if (InRegistration.BoldFile.IsSet())
    { Faces.Add({FName{TEXT("Bold")}, InRegistration.BoldFile.GetValue(), false, {}}); }
    if (InRegistration.FallbackRegularFile.IsSet())
    { Faces.Add({FName{TEXT("Regular")}, InRegistration.FallbackRegularFile.GetValue(), true, {}}); }
    if (InRegistration.FallbackBoldFile.IsSet())
    { Faces.Add({FName{TEXT("Bold")}, InRegistration.FallbackBoldFile.GetValue(), true, {}}); }

    for (FPreparedFace& Face : Faces)
    {
        auto Bytes = TArray<uint8>{};
        if (Face.File.IsEmpty() || !FFileHelper::LoadFileToArray(Bytes, *Face.File, FILEREAD_Silent) || Bytes.IsEmpty())
        {
            return LoadFailure(FString::Printf(TEXT("Font family '%s' %s%s face could not read non-empty font data: '%s'."),
                *InRegistration.Name, Face.bFallback ? TEXT("fallback ") : TEXT(""), *Face.Name.ToString(), *Face.File));
        }
        Face.Data = FFontFaceData::MakeFontFaceData(MoveTemp(Bytes));
        // FreeType-backed face opening, not an exhaustive OpenType outline/conformance audit.
        // The memory overload avoids streaming/file-path ensures for malformed resources.
        if (Cache->GetAvailableFontSubFaces(Face.Data.ToSharedRef()).IsEmpty())
        {
            return LoadFailure(FString::Printf(TEXT("Font family '%s' %s%s face contains invalid font data: '%s'."),
                *InRegistration.Name, Face.bFallback ? TEXT("fallback ") : TEXT(""), *Face.Name.ToString(), *Face.File));
        }
    }

    const TSharedRef<FOwnedCompositeFont> Composite = MakeShared<FOwnedCompositeFont>();
    for (const FPreparedFace& Face : Faces)
    {
        UFontFace* Asset = NewObject<UFontFace>(GetTransientPackage(), NAME_None, RF_Transient);
        if (Asset == nullptr)
        { return LoadFailure(TEXT("Font family could not create an owning font face.")); }
        Asset->FontFaceData = Face.Data.ToSharedRef();
        Asset->LoadingPolicy = EFontLoadingPolicy::Inline;
        Asset->Hinting = EFontHinting::Default;
        FTypeface& Typeface = Face.bFallback ? Composite->FallbackTypeface.Typeface : Composite->DefaultTypeface;
        FTypefaceEntry& Entry = Typeface.Fonts.Emplace_GetRef(Face.Name);
        Entry.Font = FFontData(Asset);
    }
    return InRegistry.Register(MoveTemp(InRegistration.Name), Composite);
}
