#pragma once

#include "CkCore/Macros/CkMacros.h"
#include "CkWidgets/Authoring/CkAuthoredCollection.h"
#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "CkAuthoredSession.generated.h"

class FCkUiAuthoredImageStorage;
class UTexture2D;
class FCkUiView;
class FCkUiCollection;
class FCkUiAuthoredInputReleaseGuard;
class FSlateUser;
class SWidget;
class SCkUiWorldMap;
class UCommonActivatableWidget;
class ULocalPlayer;
class UWorld;
class UCk_UI_AuthoredSurface_UE;

USTRUCT(BlueprintType)
struct CKWIDGETS_API FCk_UI_AuthoredText
{
    GENERATED_BODY()
public:
    CK_GENERATED_BODY(FCk_UI_AuthoredText);
private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ck|UI|Authoring", meta = (AllowPrivateAccess = true))
    FString _Name;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ck|UI|Authoring", meta = (AllowPrivateAccess = true))
    FText _Value;
public:
    CK_PROPERTY(_Name);
    CK_PROPERTY(_Value);
    CK_DEFINE_CONSTRUCTORS(FCk_UI_AuthoredText, _Name, _Value);
};

USTRUCT(BlueprintType)
struct CKWIDGETS_API FCk_UI_AuthoredNumber
{
    GENERATED_BODY()
public:
    CK_GENERATED_BODY(FCk_UI_AuthoredNumber);
private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ck|UI|Authoring", meta = (AllowPrivateAccess = true))
    FString _Name;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ck|UI|Authoring", meta = (AllowPrivateAccess = true))
    float _Value = 0.0f;
public:
    CK_PROPERTY(_Name);
    CK_PROPERTY(_Value);
    CK_DEFINE_CONSTRUCTORS(FCk_UI_AuthoredNumber, _Name, _Value);
};

USTRUCT(BlueprintType)
struct CKWIDGETS_API FCk_UI_AuthoredBool
{
    GENERATED_BODY()
public:
    CK_GENERATED_BODY(FCk_UI_AuthoredBool);
private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ck|UI|Authoring", meta = (AllowPrivateAccess = true))
    FString _Name;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ck|UI|Authoring", meta = (AllowPrivateAccess = true))
    bool _Value = false;
public:
    CK_PROPERTY(_Name);
    CK_PROPERTY(_Value);
    CK_DEFINE_CONSTRUCTORS(FCk_UI_AuthoredBool, _Name, _Value);
};

/** Named texture resource; object handles are copied and validated at admission. */
USTRUCT(BlueprintType)
struct CKWIDGETS_API FCk_UI_AuthoredImage
{
    GENERATED_BODY()
public:
    CK_GENERATED_BODY(FCk_UI_AuthoredImage);
private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ck|UI|Authoring", meta = (AllowPrivateAccess = true))
    FString _Name;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ck|UI|Authoring", meta = (AllowPrivateAccess = true))
    UTexture2D* _Texture = nullptr;
public:
    CK_PROPERTY(_Name);
    CK_DEFINE_CONSTRUCTORS(FCk_UI_AuthoredImage, _Name);
    auto Get_Texture() const -> UTexture2D*;
    auto Set_Texture(UTexture2D* InValue) -> ThisType&;
};

/** Named font resources are admitted at initialization; empty optional paths omit those faces. */
USTRUCT(BlueprintType)
struct CKWIDGETS_API FCk_UI_AuthoredFontFamily
{
    GENERATED_BODY()
public:
    CK_GENERATED_BODY(FCk_UI_AuthoredFontFamily);
private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ck|UI|Authoring", meta = (AllowPrivateAccess = true))
    FString _Name;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ck|UI|Authoring", meta = (AllowPrivateAccess = true))
    FString _RegularFile;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ck|UI|Authoring", meta = (AllowPrivateAccess = true))
    FString _BoldFile;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ck|UI|Authoring", meta = (AllowPrivateAccess = true))
    FString _FallbackRegularFile;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ck|UI|Authoring", meta = (AllowPrivateAccess = true))
    FString _FallbackBoldFile;
public:
    CK_PROPERTY(_Name);
    CK_PROPERTY(_RegularFile);
    CK_PROPERTY(_BoldFile);
    CK_PROPERTY(_FallbackRegularFile);
    CK_PROPERTY(_FallbackBoldFile);
    CK_DEFINE_CONSTRUCTORS(FCk_UI_AuthoredFontFamily, _Name, _RegularFile);
};

/** Exact ordinary-button source contract within one declared region. */
USTRUCT(BlueprintType)
struct CKWIDGETS_API FCk_UI_AuthoredButtonRequirement
{
    GENERATED_BODY()
public:
    CK_GENERATED_BODY(FCk_UI_AuthoredButtonRequirement);
private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ck|UI|Authoring", meta = (AllowPrivateAccess = true))
    FString _Id;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ck|UI|Authoring", meta = (AllowPrivateAccess = true))
    FString _Action;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ck|UI|Authoring", meta = (AllowPrivateAccess = true))
    FString _EnabledBool;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ck|UI|Authoring", meta = (AllowPrivateAccess = true))
    FString _Region = TEXT("main");
public:
    CK_PROPERTY(_Id);
    CK_PROPERTY(_Action);
    CK_PROPERTY(_EnabledBool);
    CK_PROPERTY(_Region);
    CK_DEFINE_CONSTRUCTORS(FCk_UI_AuthoredButtonRequirement, _Id, _Action, _EnabledBool);
};

/** Declarations freeze after successful initialization; reloads must define exactly the declared regions. */
USTRUCT(BlueprintType)
struct CKWIDGETS_API FCk_UI_AuthoredSession_Config
{
    GENERATED_BODY()
public:
    CK_GENERATED_BODY(FCk_UI_AuthoredSession_Config);
private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ck|UI|Authoring", meta = (AllowPrivateAccess = true))
    TArray<FCk_UI_AuthoredText> _Text;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ck|UI|Authoring", meta = (AllowPrivateAccess = true))
    TArray<FCk_UI_AuthoredNumber> _Number;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ck|UI|Authoring", meta = (AllowPrivateAccess = true))
    TArray<FCk_UI_AuthoredBool> _Bool;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ck|UI|Authoring", meta = (AllowPrivateAccess = true))
    TArray<FCk_UI_AuthoredFontFamily> _FontFamilies;
    /** Native var(--name) values; frozen when the session is initialized. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ck|UI|Authoring", meta = (AllowPrivateAccess = true))
    TMap<FString, FString> _StyleTokens;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ck|UI|Authoring", meta = (AllowPrivateAccess = true))
    TArray<FCk_UI_AuthoredImage> _Images;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ck|UI|Authoring", meta = (AllowPrivateAccess = true))
    bool _RegisterHudControls = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ck|UI|Authoring", meta = (AllowPrivateAccess = true))
    bool _RegisterRadialControls = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ck|UI|Authoring", meta = (AllowPrivateAccess = true))
    bool _RegisterItemControls = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ck|UI|Authoring", meta = (AllowPrivateAccess = true))
    bool _RegisterMapControls = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ck|UI|Authoring", meta = (AllowPrivateAccess = true))
    TArray<FString> _Actions;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ck|UI|Authoring", meta = (AllowPrivateAccess = true))
    TArray<FCk_UI_AuthoredCollection> _Collections;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ck|UI|Authoring", meta = (AllowPrivateAccess = true))
    TArray<FString> _ItemActions;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ck|UI|Authoring", meta = (AllowPrivateAccess = true))
    TArray<FString> _Regions = {TEXT("main")};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ck|UI|Authoring", meta = (AllowPrivateAccess = true))
    TArray<FCk_UI_AuthoredButtonRequirement> _RequiredButtons;
    /** Optional CommonUI input owner. Its local player and Slate user freeze at admission. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ck|UI|Authoring", meta = (AllowPrivateAccess = true))
    UCommonActivatableWidget* _InteractionOwner = nullptr;
public:
    CK_PROPERTY(_Text);
    CK_PROPERTY(_Number);
    CK_PROPERTY(_Bool);
    CK_PROPERTY(_FontFamilies);
    CK_PROPERTY(_StyleTokens);
    CK_PROPERTY(_Images);
    CK_PROPERTY(_RegisterHudControls);
    CK_PROPERTY(_RegisterRadialControls);
    CK_PROPERTY(_RegisterItemControls);
    CK_PROPERTY(_RegisterMapControls);
    CK_PROPERTY(_Actions);
    CK_PROPERTY(_Collections);
    CK_PROPERTY(_ItemActions);
    CK_PROPERTY(_Regions);
    CK_PROPERTY(_RequiredButtons);
    /** Copies a nullable object handle; initialization validates the selected owner. */
    auto Get_InteractionOwner() const -> UCommonActivatableWidget*;
    auto Set_InteractionOwner(UCommonActivatableWidget* InValue) -> ThisType&;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCk_UI_AuthoredAction, const FString&, Name);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCk_UI_AuthoredItemAction, const FString&, ActionName, const FString&, Key);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCk_UI_AuthoredTextChanged, const FString&, Name, const FText&, Proposed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCk_UI_AuthoredNumberChanged, const FString&, Name, float, Proposed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCk_UI_AuthoredBoolChanged, const FString&, Name, bool, Proposed);

/**
 * Game-thread-owned scalar and flat collection models with a native authored view. Initialization succeeds once; release is terminal.
 * Retain this object through a reflected owner while mounted. Native attributes and callbacks retain it weakly.
 */
UCLASS(BlueprintType)
class CKWIDGETS_API UCk_UI_AuthoredSession_UE : public UObject
{
    GENERATED_BODY()
public:
    CK_GENERATED_BODY(UCk_UI_AuthoredSession_UE);

    UFUNCTION(BlueprintCallable, Category = "Ck|UI|Authoring")
    bool TryInitialize(const FCk_UI_AuthoredSession_Config& InConfig, TArray<FString>& OutErrors);
    UFUNCTION(BlueprintCallable, Category = "Ck|UI|Authoring")
    bool TryReload(const FString& InMarkup, const FString& InStylesheet, TArray<FString>& OutErrors);
    UFUNCTION(BlueprintCallable, Category = "Ck|UI|Authoring")
    bool TryReloadFiles(const FString& InMarkupPath, const FString& InStylesheetPath, TArray<FString>& OutErrors);
    UFUNCTION(BlueprintCallable, Category = "Ck|UI|Authoring")
    bool TryMount(UCk_UI_AuthoredSurface_UE* InSurface, const FString& InRegionName, TArray<FString>& OutErrors);

    UFUNCTION(BlueprintCallable, Category = "Ck|UI|Authoring")
    bool TrySetText(const FString& InName, const FText& InValue);
    UFUNCTION(BlueprintCallable, Category = "Ck|UI|Authoring")
    bool TrySetNumber(const FString& InName, float InValue);
    UFUNCTION(BlueprintCallable, Category = "Ck|UI|Authoring")
    bool TrySetBool(const FString& InName, bool InValue);
    /** Replaces a declared image atomically; null/stale textures and unknown names reject. */
    UFUNCTION(BlueprintCallable, Category = "Ck|UI|Authoring")
    bool TrySetImage(const FString& InName, UTexture2D* InTexture);
    UFUNCTION(BlueprintCallable, Category = "Ck|UI|Authoring")
    bool TryGetText(const FString& InName, FText& OutValue) const;
    UFUNCTION(BlueprintCallable, Category = "Ck|UI|Authoring")
    bool TryGetNumber(const FString& InName, float& OutValue) const;
    UFUNCTION(BlueprintCallable, Category = "Ck|UI|Authoring")
    bool TryGetBool(const FString& InName, bool& OutValue) const;

    /** Replaces every record atomically in the retained model; presenter refresh occurs separately.
     * A listener releasing the session causes failure after retirement without rolling back accepted data. */
    UFUNCTION(BlueprintCallable, Category = "Ck|UI|Authoring")
    bool TrySetRecords(const FString& InName, const TArray<FCk_UI_AuthoredRecord>& InRecords, TArray<FString>& OutErrors);
    /** Copies supplied fields in schema order. Failure preserves OutRecords. */
    UFUNCTION(BlueprintCallable, Category = "Ck|UI|Authoring")
    bool TryGetRecords(const FString& InName, TArray<FCk_UI_AuthoredRecord>& OutRecords) const;
    UFUNCTION(BlueprintCallable, Category = "Ck|UI|Authoring")
    bool TryGetCollectionRevision(const FString& InName, int64& OutRevision) const;

    /** Requires an active admitted owner and a mounted surface in that owner's widget tree. */
    UFUNCTION(BlueprintCallable, Category = "Ck|UI|Authoring")
    bool TryBeginInput();
    /** Suspends events and drains matching releases. False discards saved focus without restoring it; retains model/view. */
    UFUNCTION(BlueprintCallable, Category = "Ck|UI|Authoring")
    void Request_EndInput(bool InRestoreFocus = true);
    UFUNCTION(BlueprintCallable, Category = "Ck|UI|Authoring")
    bool TryFocusButton(const FString& InId);
    /** Focus a visible, enabled native control by its full authored tag, including repeat scopes. */
    UFUNCTION(BlueprintCallable, Category = "Ck|UI|Authoring")
    bool TryFocusControl(const FString& InId);
    /** Operate a unique visible map in the active owned surface. Rejected requests preserve its camera. */
    UFUNCTION(BlueprintCallable, Category = "Ck|UI|Authoring")
    bool TrySetMapCamera(const FString& InId, FVector2D InCenter, float InZoom);
    UFUNCTION(BlueprintCallable, Category = "Ck|UI|Authoring")
    bool TryZoomMap(const FString& InId, float InFactor);
    UFUNCTION(BlueprintCallable, Category = "Ck|UI|Authoring")
    bool TryRevealMapMarker(const FString& InId, const FString& InKey);
    /** Failure preserves both outputs. Input ownership is required, including for this query. */
    UFUNCTION(BlueprintCallable, Category = "Ck|UI|Authoring")
    bool TryGetMapCamera(const FString& InId, FVector2D& OutCenter, float& OutZoom);
    /** Queries exact current button focus for the active admitted user without refreshing attributes. */
    UFUNCTION(BlueprintPure, Category = "Ck|UI|Authoring")
    bool Get_IsButtonFocused(const FString& InId) const;
    /** Direction must be -1 or 1; IDs must be unique current ordinary authored buttons. */
    UFUNCTION(BlueprintCallable, Category = "Ck|UI|Authoring")
    bool TryNavigateButtons(const TArray<FString>& InIds, int32 InDirection);
    UFUNCTION(BlueprintPure, Category = "Ck|UI|Authoring")
    bool Get_IsInputActive() const;
    UFUNCTION(BlueprintPure, Category = "Ck|UI|Authoring")
    int32 Get_SlateUserIndex() const;

    UFUNCTION(BlueprintCallable, Category = "Ck|UI|Authoring")
    void Request_Release();
    UFUNCTION(BlueprintPure, Category = "Ck|UI|Authoring")
    bool Get_IsUsable() const;
    UFUNCTION(BlueprintPure, Category = "Ck|UI|Authoring")
    int64 Get_Revision() const;

    UPROPERTY(BlueprintAssignable, Category = "Ck|UI|Authoring")
    FCk_UI_AuthoredAction OnAction;
    UPROPERTY(BlueprintAssignable, Category = "Ck|UI|Authoring")
    FCk_UI_AuthoredItemAction OnItemAction;
    UPROPERTY(BlueprintAssignable, Category = "Ck|UI|Authoring")
    FCk_UI_AuthoredTextChanged OnTextChanged;
    /** Slider interaction commits emit proposals; an authored changed binding also emits live proposals. */
    UPROPERTY(BlueprintAssignable, Category = "Ck|UI|Authoring")
    FCk_UI_AuthoredNumberChanged OnNumberChanged;
    UPROPERTY(BlueprintAssignable, Category = "Ck|UI|Authoring")
    FCk_UI_AuthoredBoolChanged OnBoolChanged;

    /** Native interop only. Releasing the session retires this view even when native callers retain it. */
    auto Get_View() const -> TSharedPtr<FCkUiView>;
    /** Read-only native observation of an admitted model. Retained readers may outlive the session;
     * they do not keep it usable or restore event eligibility after release. */
    auto Get_Collection(const FString& InName) const -> TSharedPtr<const FCkUiCollection>;

protected:
    void BeginDestroy() override;

private:
    auto CanDispatchEvents() const -> bool;
    auto HasOwnedInputContext() const -> bool;
    auto FindFocusableButton(const FString& InId) -> TSharedPtr<SWidget>;
    auto FindWorldMap(const FString& InId) -> TSharedPtr<SCkUiWorldMap>;
    auto DoFocusButton(const FString& InId, const TSharedPtr<SWidget>& InButton) -> bool;
    bool _Initialized = false;
    bool _Released = false;
    bool _OperationActive = false;
    bool _HasMountedSurface = false;
    TMap<FString, FText> _Text;
    TMap<FString, float> _Number;
    TMap<FString, bool> _Bool;
    UPROPERTY(Transient)
    TMap<FString, TObjectPtr<UTexture2D>> _ImageTextures;
    TMap<FString, TSharedPtr<FCkUiAuthoredImageStorage>> _Images;
    TMap<FString, TSharedPtr<FCkUiCollection>> _Collections;
    TSharedPtr<FCkUiView> _View;
    TWeakObjectPtr<UCk_UI_AuthoredSurface_UE> _MountedSurface;
    bool _InputOwnerBound = false;
    bool _InputActive = false;
    bool _InputOperationActive = false;
    uint64 _InputGeneration = 0;
    int32 _SlateUserIndex = INDEX_NONE;
    TWeakObjectPtr<UCommonActivatableWidget> _InteractionOwner;
    TWeakObjectPtr<ULocalPlayer> _InputPlayer;
    TWeakObjectPtr<UWorld> _InputWorld;
    TWeakPtr<FSlateUser> _SlateUser;
    TWeakPtr<SWidget> _PreviousInputFocus;
    TSharedPtr<FCkUiAuthoredInputReleaseGuard> _InputReleaseGuard;
};
