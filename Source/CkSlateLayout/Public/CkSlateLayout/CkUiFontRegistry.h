#pragma once

#include "CkSlateLayout/CkUiDocument.h"
#include "Fonts/SlateFontInfo.h"

/** Trusted host resources; authored CSS selects registered names rather than file paths. */
struct FCkUiFontFamilyRegistration
{
    FString Name;
    FString RegularFile;
    TOptional<FString> BoldFile;
    TOptional<FString> FallbackRegularFile;
    TOptional<FString> FallbackBoldFile;
};

/** Immutable font families. Resolved fonts retain the admitted bytes and all fallback faces. */
class CKSLATELAYOUT_API FCkUiFontRegistrySnapshot final
{
public:
    /** Regular/Bold must be explicitly present. Failure preserves OutFont; success uses size 10. */
    auto TryResolve(const FString& InFamily, FName InFace, FSlateFontInfo& OutFont) const -> bool;
    auto HasFamily(const FString& InFamily) const -> bool;

private:
    friend class FCkUiFontRegistry;
    TMap<FString, TSharedPtr<const FCompositeFont>> _Families;
};

/** Setup, snapshot creation and mounted font ownership use the game thread. */
class CKSLATELAYOUT_API FCkUiFontRegistry final
{
public:
    /** Shared CSS/registration grammar; reserved generic family names are excluded. */
    static auto IsFamilyName(const FString& InName) -> bool;
    /**
     * Trusted immutable composite admission on the game thread. The shared composite must own/collect
     * every face asset throughout its lifetime; FCkUiFontResources provides the supported file bridge.
     * Only inline in-memory Regular/Bold primary/fallback faces at sub-face zero are admitted;
     * sub-typefaces are unsupported. Failure preserves existing families.
     */
    auto Register(FString InName, TSharedPtr<const FCompositeFont> InFont) -> FCkUiLoadResult;
    auto CreateSnapshot() const -> TSharedRef<const FCkUiFontRegistrySnapshot>;

private:
    TMap<FString, TSharedPtr<const FCompositeFont>> _Families;
};
