#include "CkWidgets/Authoring/CkAuthoredSession.h"

#include "CkWidgets/Authoring/CkAuthoredSurface_Widget.h"
#include "CkAuthoredCollectionAdapter.h"
#include "CkWidgets/Authoring/CkUiFontResources.h"
#include "CkSlateLayout/CkUiFontRegistry.h"
#include "CkSlateLayout/CkUiCheckbox.h"
#include "CkSlateLayout/CkUiSlider.h"
#include "CkSlateLayout/CkUiHudControls.h"
#include "CkSlateLayout/CkUiRadialSelector.h"
#include "CkCore/Ensure/CkEnsure.h"
#include "Engine/Texture2D.h"
#include "UObject/StrongObjectPtr.h"
#include "CkSlateLayout/CkUiWidgetRegistry.h"
#include "CkSlateLayout/SCkUiSurface.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateUser.h"
#include "CommonActivatableWidget.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Input/Events.h"
#include "Misc/Char.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "Templates/UnrealTemplate.h"
#include "Types/SlateAttributeMetaData.h"
#include "Widgets/Input/SButton.h"

#if WITH_ANGELSCRIPT_CK
#include "AngelscriptBinds.h"
#include "StaticJIT/StaticJITBinds.h"
#endif

/** Attribute copies retain the brush address and resource after session retirement. */
class FCkUiAuthoredImageStorage
{
public:
    explicit FCkUiAuthoredImageStorage(UTexture2D* InTexture) : Texture(InTexture)
    {
        Brush.DrawAs = ESlateBrushDrawType::Image;
        SetTexture(InTexture);
    }
    void SetTexture(UTexture2D* InTexture)
    {
        Texture.Reset(InTexture);
        Brush.SetResourceObject(InTexture);
        Brush.ImageSize = FVector2D(InTexture->GetSizeX(), InTexture->GetSizeY());
    }
    FSlateBrush Brush;
    TStrongObjectPtr<UTexture2D> Texture;
};

auto
    FCk_UI_AuthoredImage::
    Get_Texture() const
    -> UTexture2D*
{
    return _Texture;
}

auto
    FCk_UI_AuthoredImage::
    Set_Texture(
        UTexture2D* InValue)
    -> ThisType&
{
    _Texture = InValue;
    return *this;
}

auto
    FCk_UI_AuthoredSession_Config::
    Get_InteractionOwner() const
    -> UCommonActivatableWidget*
{
    return _InteractionOwner;
}

auto
    FCk_UI_AuthoredSession_Config::
    Set_InteractionOwner(
        UCommonActivatableWidget* InValue)
    -> ThisType&
{
    _InteractionOwner = InValue;
    return *this;
}

namespace ck_ui_authored_session
{
#if WITH_ANGELSCRIPT_CK
    auto
        RegisterObjectHandleAccessors()
        -> void
    {
        auto ImageClass = FAngelscriptBinds::ExistingClass("FCk_UI_AuthoredImage");
        const auto* ImageType = ImageClass.GetTypeInfo();
        if (ImageType != nullptr)
        {
            if (ImageType->GetMethodByName("Get_Texture") == nullptr)
            {
                ImageClass.Method("UTexture2D Get_Texture() const",
                    [](const FCk_UI_AuthoredImage* Self) -> UTexture2D* { return Self->Get_Texture(); });
                FAngelscriptBinds::SetPreviousBindNoDiscard(true);
                FScriptFunctionNativeForm::BindNativeMethod(ImageClass, "Get_Texture", true);
            }
            if (ImageType->GetMethodByName("Set_Texture") == nullptr)
            {
                ImageClass.Method("FCk_UI_AuthoredImage& Set_Texture(UTexture2D InValue)",
                    METHODPR_TRIVIAL(FCk_UI_AuthoredImage&, FCk_UI_AuthoredImage, Set_Texture, (UTexture2D*)));
                FAngelscriptBinds::SetPreviousBindNoDiscard(false);
                FScriptFunctionNativeForm::BindNativeMethod(ImageClass, "Set_Texture", true);
            }
            FAngelscriptBinds::SetPreviousBindNoDiscard(false);
        }
        auto ExistingClass = FAngelscriptBinds::ExistingClass("FCk_UI_AuthoredSession_Config");
        const auto* TypeInfo = ExistingClass.GetTypeInfo();
        if (TypeInfo == nullptr) { return; }
        // Object handles are copied; the generic CK value-reference binding rejects derived handles.
        if (TypeInfo->GetMethodByName("Get_InteractionOwner") == nullptr)
        {
            ExistingClass.Method("UCommonActivatableWidget Get_InteractionOwner() const",
                [](const FCk_UI_AuthoredSession_Config* Self) -> UCommonActivatableWidget*
                { return Self->Get_InteractionOwner(); });
            FAngelscriptBinds::SetPreviousBindNoDiscard(true);
            FScriptFunctionNativeForm::BindNativeMethod(ExistingClass, "Get_InteractionOwner", true);
        }
        if (TypeInfo->GetMethodByName("Set_InteractionOwner") == nullptr)
        {
            ExistingClass.Method("FCk_UI_AuthoredSession_Config& Set_InteractionOwner(UCommonActivatableWidget InValue)",
                METHODPR_TRIVIAL(FCk_UI_AuthoredSession_Config&, FCk_UI_AuthoredSession_Config,
                    Set_InteractionOwner, (UCommonActivatableWidget*)));
            FAngelscriptBinds::SetPreviousBindNoDiscard(false);
            FScriptFunctionNativeForm::BindNativeMethod(ExistingClass, "Set_InteractionOwner", true);
        }
        FAngelscriptBinds::SetPreviousBindNoDiscard(false);
    }

    const bool ObjectHandleAccessorsRegistered = []()
    {
        FCkAngelScriptPropertyFunctionRegistration::RegisterPropertyFunction(&RegisterObjectHandleAccessors);
        return true;
    }();
#endif

    auto
        Fail(
            TArray<FString>& OutErrors,
            const FString& InMessage)
        -> bool
    {
        OutErrors.Add(InMessage);
        return false;
    }

    // Binding names follow the document parser's identifier grammar, including case sensitivity.
    auto
        IsName(
            const FString& InValue)
        -> bool
    {
        if (InValue.IsEmpty() || !(FChar::IsAlpha(InValue[0]) || InValue[0] == TEXT('_') || InValue[0] == TEXT('-')))
        { return false; }
        for (const TCHAR Character : InValue)
        {
            if (!(FChar::IsAlnum(Character) || Character == TEXT('_') || Character == TEXT('-')))
            { return false; }
        }
        return true;
    }

    auto
        AdmitName(
            const FString& InName,
            TSet<FString>& InOutNames,
            TArray<FString>& OutErrors)
        -> bool
    {
        if (!IsName(InName))
        { return Fail(OutErrors, FString::Printf(TEXT("Authored session declaration '%s' is not an identifier."), *InName)); }
        if (InOutNames.Contains(InName))
        { return Fail(OutErrors, FString::Printf(TEXT("Authored session declaration '%s' is duplicated or conflicts with another type/action."), *InName)); }
        InOutNames.Add(InName);
        return true;
    }
}

/** Observes this input admission only; after suspension it consumes matching outstanding releases. */
class FCkUiAuthoredInputReleaseGuard final : public IInputProcessor, public TSharedFromThis<FCkUiAuthoredInputReleaseGuard>
{
public:
    FCkUiAuthoredInputReleaseGuard(UCk_UI_AuthoredSession_UE* InSession, UWorld* InWorld, int32 InUserIndex)
        : _Session(InSession), _World(InWorld), _UserIndex(InUserIndex)
    {}

    auto BeginDrain() -> void { _Draining = true; }
    auto IsDraining() const -> bool { return _Draining; }

    auto Tick(float, FSlateApplication& InSlate, TSharedRef<ICursor>) -> void override
    {
        UpdateAdmission();
        if (_Draining && (_Held.IsEmpty() || !_World.IsValid() || !InSlate.IsActive()))
        { InSlate.UnregisterInputPreProcessor(AsShared()); }
    }

    auto HandleKeyDownEvent(FSlateApplication&, const FKeyEvent& InEvent) -> bool override
    {
        UpdateAdmission();
        if (InEvent.GetUserIndex() != _UserIndex) { return false; }
        if (!_Draining) { _Held.Add(InEvent.GetKey()); return false; }
        if (InEvent.IsRepeat()) { return _Held.Contains(InEvent.GetKey()); }
        _Held.Remove(InEvent.GetKey());
        return false;
    }

    auto HandleKeyUpEvent(FSlateApplication&, const FKeyEvent& InEvent) -> bool override
    { return Release(InEvent.GetUserIndex(), InEvent.GetKey()); }

    auto HandleMouseButtonDownEvent(FSlateApplication&, const FPointerEvent& InEvent) -> bool override
    {
        UpdateAdmission();
        if (InEvent.GetUserIndex() == _UserIndex)
        {
            if (_Draining) { _Held.Remove(InEvent.GetEffectingButton()); }
            else { _Held.Add(InEvent.GetEffectingButton()); }
        }
        return false;
    }

    auto HandleMouseButtonUpEvent(FSlateApplication&, const FPointerEvent& InEvent) -> bool override
    { return Release(InEvent.GetUserIndex(), InEvent.GetEffectingButton()); }

private:
    auto UpdateAdmission() -> void
    {
        const auto* Session = _Session.Get();
        if (Session == nullptr || !Session->Get_IsInputActive()) { _Draining = true; }
    }
    auto Release(int32 InUserIndex, FKey InKey) -> bool
    {
        UpdateAdmission();
        if (InUserIndex != _UserIndex) { return false; }
        const bool WasHeld = _Held.Remove(InKey) > 0;
        return _Draining && WasHeld;
    }
    TWeakObjectPtr<UCk_UI_AuthoredSession_UE> _Session;
    TWeakObjectPtr<UWorld> _World;
    int32 _UserIndex;
    TSet<FKey> _Held;
    bool _Draining = false;
};

auto
    UCk_UI_AuthoredSession_UE::
    TryInitialize(
        const FCk_UI_AuthoredSession_Config& InConfig,
        TArray<FString>& OutErrors)
    -> bool
{
    using namespace ck_ui_authored_session;
    OutErrors.Reset();
    if (!IsInGameThread()) { return Fail(OutErrors, TEXT("Authored session initialization requires the game thread.")); }
    if (_OperationActive) { return Fail(OutErrors, TEXT("Authored session initialization cannot reenter another operation.")); }
    if (_Released || _Initialized) { return Fail(OutErrors, TEXT("Authored sessions may initialize once and cannot initialize after release.")); }
    if (!FSlateApplication::IsInitialized())
    { return Fail(OutErrors, TEXT("Authored session initialization requires an initialized Slate application.")); }
    TGuardValue<bool> OperationGuard(_OperationActive, true);

    UCommonActivatableWidget* InteractionOwner = InConfig.Get_InteractionOwner();
    ULocalPlayer* InputPlayer = nullptr;
    UWorld* InputWorld = nullptr;
    TSharedPtr<FSlateUser> SlateUser;
    if (InteractionOwner != nullptr)
    {
        if (!IsValid(InteractionOwner))
        { return Fail(OutErrors, TEXT("Authored interaction owner must be valid.")); }
        APlayerController* Controller = InteractionOwner->GetOwningPlayer();
        if (!IsValid(Controller) || !Controller->IsLocalController())
        { return Fail(OutErrors, TEXT("Authored interaction owner requires a local player controller.")); }
        InputPlayer = InteractionOwner->GetOwningLocalPlayer();
        if (!IsValid(InputPlayer) || Controller->GetLocalPlayer() != InputPlayer)
        { return Fail(OutErrors, TEXT("Authored interaction owner requires its controller's local player.")); }
        InputWorld = InteractionOwner->GetWorld();
        if (!IsValid(InputWorld) || InputWorld->bIsTearingDown || Controller->GetWorld() != InputWorld || InputPlayer->GetWorld() != InputWorld)
        { return Fail(OutErrors, TEXT("Authored interaction owner requires a consistent live world.")); }
        SlateUser = InputPlayer->GetSlateUser();
        if (!SlateUser.IsValid() || FSlateApplication::Get().GetUser(SlateUser->GetUserIndex()) != SlateUser)
        { return Fail(OutErrors, TEXT("Authored interaction owner requires an existing Slate user.")); }
    }

    auto Names = TSet<FString>{};
    auto Text = TMap<FString, FText>{};
    auto Number = TMap<FString, float>{};
    auto Bool = TMap<FString, bool>{};
    for (const FCk_UI_AuthoredText& Entry : InConfig.Get_Text())
    {
        if (!AdmitName(Entry.Get_Name(), Names, OutErrors)) { return false; }
        Text.Add(Entry.Get_Name(), Entry.Get_Value());
    }
    for (const FCk_UI_AuthoredNumber& Entry : InConfig.Get_Number())
    {
        if (!AdmitName(Entry.Get_Name(), Names, OutErrors)) { return false; }
        if (!FMath::IsFinite(Entry.Get_Value()))
        { return Fail(OutErrors, FString::Printf(TEXT("Authored number '%s' must be finite."), *Entry.Get_Name())); }
        Number.Add(Entry.Get_Name(), Entry.Get_Value());
    }
    for (const FCk_UI_AuthoredBool& Entry : InConfig.Get_Bool())
    {
        if (!AdmitName(Entry.Get_Name(), Names, OutErrors)) { return false; }
        Bool.Add(Entry.Get_Name(), Entry.Get_Value());
    }
    auto Images = TMap<FString, TSharedPtr<FCkUiAuthoredImageStorage>>{};
    auto ImageTextures = TMap<FString, TObjectPtr<UTexture2D>>{};
    for (const FCk_UI_AuthoredImage& Entry : InConfig.Get_Images())
    {
        const bool NameValid = IsName(Entry.Get_Name()) && !Names.Contains(Entry.Get_Name());
        CK_ENSURE_IF_NOT(NameValid, TEXT("Authored image requires a unique identifier.")) {}
        if (!NameValid) { return Fail(OutErrors, TEXT("Authored image requires a unique identifier across declarations.")); }
        UTexture2D* Texture = Entry.Get_Texture();
        const bool TextureValid = IsValid(Texture);
        CK_ENSURE_IF_NOT(TextureValid, TEXT("Authored image requires a valid texture.")) {}
        if (!TextureValid) { return Fail(OutErrors, TEXT("Authored image requires a valid texture.")); }
        const bool DimensionsValid = Texture->GetSizeX() > 0 && Texture->GetSizeY() > 0;
        CK_ENSURE_IF_NOT(DimensionsValid, TEXT("Authored image requires positive texture dimensions.")) {}
        if (!DimensionsValid) { return Fail(OutErrors, TEXT("Authored image requires positive texture dimensions.")); }
        Names.Add(Entry.Get_Name());
        Images.Add(Entry.Get_Name(), MakeShared<FCkUiAuthoredImageStorage>(Texture));
        ImageTextures.Add(Entry.Get_Name(), Texture);
    }
    for (const FString& Name : InConfig.Get_Actions())
    { if (!AdmitName(Name, Names, OutErrors)) { return false; } }

    auto Collections = TMap<FString, TSharedPtr<FCkUiCollection>>{};
    for (const FCk_UI_AuthoredCollection& Declaration : InConfig.Get_Collections())
    {
        if (!AdmitName(Declaration.Get_Name(), Names, OutErrors)) { return false; }
        TSharedPtr<FCkUiCollection> Collection;
        if (!ck_ui_authored_collection::TryCreate(Declaration, Collection, OutErrors)) { return false; }
        Collections.Add(Declaration.Get_Name(), MoveTemp(Collection));
    }
    for (const FString& Name : InConfig.Get_ItemActions())
    { if (!AdmitName(Name, Names, OutErrors)) { return false; } }

    if (InConfig.Get_Regions().IsEmpty())
    { return Fail(OutErrors, TEXT("Authored session requires at least one declared region.")); }
    auto Regions = TSet<FString>{};
    for (const FString& Name : InConfig.Get_Regions())
    {
        if (!IsName(Name))
        { return Fail(OutErrors, FString::Printf(TEXT("Authored session region '%s' is not an identifier."), *Name)); }
        if (Regions.Contains(Name))
        { return Fail(OutErrors, FString::Printf(TEXT("Authored session region '%s' is duplicated."), *Name)); }
        Regions.Add(Name);
    }

    auto RequiredButtons = TArray<FCkUiView::FDataBindings::FButtonRequirement>{};
    auto RequiredIds = TSet<FString>{};
    for (const FCk_UI_AuthoredButtonRequirement& Entry : InConfig.Get_RequiredButtons())
    {
        if (!IsName(Entry.Get_Id()) || !IsName(Entry.Get_Action()) || !IsName(Entry.Get_EnabledBool())
            || !IsName(Entry.Get_Region()))
        { return Fail(OutErrors, TEXT("Authored button requirements need identifier IDs, actions, enabled bools and regions.")); }
        if (RequiredIds.Contains(Entry.Get_Id()))
        { return Fail(OutErrors, FString::Printf(TEXT("Authored button requirement '%s' is duplicated."), *Entry.Get_Id())); }
        if (!InConfig.Get_Actions().Contains(Entry.Get_Action()) || !Bool.Contains(Entry.Get_EnabledBool())
            || !Regions.Contains(Entry.Get_Region()))
        { return Fail(OutErrors, FString::Printf(TEXT("Authored button requirement '%s' needs a declared action, bool and region."), *Entry.Get_Id())); }
        RequiredIds.Add(Entry.Get_Id());
        RequiredButtons.Add({Entry.Get_Id(), Entry.Get_Action(), Entry.Get_EnabledBool(), Entry.Get_Region()});
    }

    // Reject structural mistakes across the full catalog before loading any font bytes.
    const TArray<FCk_UI_AuthoredFontFamily> FontFamilies = InConfig.Get_FontFamilies();
    auto FontNames = TSet<FString>{};
    for (const FCk_UI_AuthoredFontFamily& Entry : FontFamilies)
    {
        const FString& Name = Entry.Get_Name();
        if (!FCkUiFontRegistry::IsFamilyName(Name))
        { return Fail(OutErrors, TEXT("Font family names must start with a-z, contain only a-z, 0-9, or hyphen, and may not be monospace or sans-serif.")); }
        if (FontNames.Contains(Name))
        { return Fail(OutErrors, FString::Printf(TEXT("Font family '%s' is already registered."), *Name)); }
        if (!Entry.Get_FallbackBoldFile().IsEmpty()
            && (Entry.Get_FallbackRegularFile().IsEmpty() || Entry.Get_BoldFile().IsEmpty()))
        { return Fail(OutErrors, TEXT("A bold fallback requires both a regular fallback and a primary bold face.")); }
        FontNames.Add(Name);
    }

    FCkUiFontRegistry Fonts;
    for (const FCk_UI_AuthoredFontFamily& Entry : FontFamilies)
    {
        FCkUiFontFamilyRegistration Registration;
        Registration.Name = Entry.Get_Name();
        Registration.RegularFile = Entry.Get_RegularFile();
        if (!Entry.Get_BoldFile().IsEmpty()) { Registration.BoldFile = Entry.Get_BoldFile(); }
        if (!Entry.Get_FallbackRegularFile().IsEmpty()) { Registration.FallbackRegularFile = Entry.Get_FallbackRegularFile(); }
        if (!Entry.Get_FallbackBoldFile().IsEmpty()) { Registration.FallbackBoldFile = Entry.Get_FallbackBoldFile(); }
        const FCkUiLoadResult FontResult = FCkUiFontResources::Register(Fonts, MoveTemp(Registration));
        if (!FontResult.Succeeded) { OutErrors = FontResult.Errors; return false; }
    }

    FCkUiWidgetRegistry Registry;
    const FCkUiLoadResult SliderResult = FCkUiSlider::Register(Registry);
    if (!SliderResult.Succeeded) { OutErrors = SliderResult.Errors; return false; }
    const FCkUiLoadResult CheckboxResult = FCkUiCheckbox::Register(Registry);
    if (!CheckboxResult.Succeeded) { OutErrors = CheckboxResult.Errors; return false; }

    if (InConfig.Get_RegisterHudControls())
    {
        const FCkUiLoadResult Result = FCkUiHudControls::Register(Registry);
        const bool Registered = Result.Succeeded;
        CK_ENSURE_IF_NOT(Registered, TEXT("Authored HUD controls could not be registered.")) {}
        if (!Registered) { OutErrors = Result.Errors; return false; }
    }
    if (InConfig.Get_RegisterRadialControls())
    {
        const FCkUiLoadResult Result = RegisterRadialUiControls(Registry);
        const bool Registered = Result.Succeeded;
        CK_ENSURE_IF_NOT(Registered, TEXT("Authored radial controls could not be registered.")) {}
        if (!Registered) { OutErrors = Result.Errors; return false; }
    }

    const TWeakObjectPtr<UCk_UI_AuthoredSession_UE> Weak{this};
    auto Data = FCkUiView::FDataBindings{};
    Data.RequiredButtons = MoveTemp(RequiredButtons);
    Data.Collections = Collections;
    for (const FString& Name : InConfig.Get_ItemActions())
    {
        Data.StringChanged.Add(Name, FCkUiOnStringChanged::CreateLambda([Weak, Name](const FString& Proposed)
        {
            // A listener may mutate the model or retire the session; copy the key before dispatch.
            const FString Key = Proposed;
            auto* Owner = Weak.Get();
            if (Owner != nullptr && Owner->CanDispatchEvents()) { Owner->OnItemAction.Broadcast(Name, Key); }
        }));
        Data.ItemActions.Add(Name, FCkUiOnItemAction::CreateLambda([Weak, Name](FString Key)
        {
            auto* Owner = Weak.Get();
            if (Owner != nullptr && Owner->CanDispatchEvents()) { Owner->OnItemAction.Broadcast(Name, Key); }
        }));
    }
    if (SlateUser.IsValid())
    {
        Data.SlateUserIndex = SlateUser->GetUserIndex();
        Data.RestoreButtonFocusOnReload = true;
    }
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([Weak]()
    {
        const auto* Owner = Weak.Get();
        // Keep committed buttons eligible for focus repair during a source transaction.
        // Native reload guards and the reflected callbacks below still suppress event dispatch.
        return Owner != nullptr && Owner->Get_IsUsable() && (!Owner->_InputOwnerBound || Owner->Get_IsInputActive());
    });
    for (const auto& Entry : Images)
    {
        const TSharedPtr<FCkUiAuthoredImageStorage> Storage = Entry.Value;
        Data.Images.Add(Entry.Key, TAttribute<const FSlateBrush*>::CreateLambda([Weak, Storage]() -> const FSlateBrush*
        {
            const auto* Owner = Weak.Get();
            return Owner != nullptr && Owner->Get_IsUsable() ? &Storage->Brush : nullptr;
        }));
    }
    for (const auto& Entry : Text)
    {
        const FString Name = Entry.Key;
        Data.Text.Add(Name, TAttribute<FText>::CreateLambda([Weak, Name]()
        {
            FText Value;
            const auto* Owner = Weak.Get();
            if (Owner != nullptr) { Owner->TryGetText(Name, Value); }
            return Value;
        }));
        Data.String.Add(Name, TAttribute<FString>::CreateLambda([Weak, Name]()
        {
            FText Value;
            const auto* Owner = Weak.Get();
            if (Owner != nullptr) { Owner->TryGetText(Name, Value); }
            return Value.ToString();
        }));
        Data.TextChanged.Add(Name, FOnTextChanged::CreateLambda([Weak, Name](const FText& Proposed)
        {
            auto* Owner = Weak.Get();
            if (Owner != nullptr && Owner->CanDispatchEvents()) { Owner->OnTextChanged.Broadcast(Name, Proposed); }
        }));
    }
    for (const auto& Entry : Number)
    {
        const FString Name = Entry.Key;
        Data.Number.Add(Name, TAttribute<float>::CreateLambda([Weak, Name]()
        {
            float Value = 0.0f;
            const auto* Owner = Weak.Get();
            if (Owner != nullptr) { Owner->TryGetNumber(Name, Value); }
            return Value;
        }));
        Data.NumberChanged.Add(Name, FCkUiOnNumberChanged::CreateLambda([Weak, Name](float Proposed)
        {
            auto* Owner = Weak.Get();
            if (Owner != nullptr && Owner->CanDispatchEvents() && FMath::IsFinite(Proposed))
            { Owner->OnNumberChanged.Broadcast(Name, Proposed); }
        }));
        Data.NumberInteraction.Add(Name, FCkUiOnNumberInteraction::CreateLambda([Weak, Name](const FCkUiNumberInteraction& Interaction)
        {
            auto* Owner = Weak.Get();
            if (Owner != nullptr && Owner->CanDispatchEvents() && Interaction.Phase == ECkUiInteractionPhase::Commit
                && FMath::IsFinite(Interaction.Value))
            { Owner->OnNumberChanged.Broadcast(Name, Interaction.Value); }
        }));
    }
    for (const auto& Entry : Bool)
    {
        const FString Name = Entry.Key;
        Data.Visibility.Add(Name, TAttribute<bool>::CreateLambda([Weak, Name]()
        {
            bool Value = false;
            const auto* Owner = Weak.Get();
            if (Owner != nullptr) { Owner->TryGetBool(Name, Value); }
            return Value;
        }));
        Data.BoolChanged.Add(Name, FCkUiOnBoolChanged::CreateLambda([Weak, Name](bool Proposed)
        {
            auto* Owner = Weak.Get();
            if (Owner != nullptr && Owner->CanDispatchEvents()) { Owner->OnBoolChanged.Broadcast(Name, Proposed); }
        }));
    }
    auto Actions = FCkUiView::FActions{};
    for (const FString& Name : InConfig.Get_Actions())
    {
        Actions.Add(Name, FSimpleDelegate::CreateLambda([Weak, Name]()
        {
            auto* Owner = Weak.Get();
            if (Owner != nullptr && Owner->CanDispatchEvents()) { Owner->OnAction.Broadcast(Name); }
        }));
    }
    const TSharedRef<FCkUiView> Candidate = FCkUiView::Create({}, MoveTemp(Actions), {},
        FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 10), MoveTemp(Data), Registry.CreateSnapshot(), Fonts.CreateSnapshot());
    // Native document admission requires the complete retained mount set before the first reload.
    for (const FString& Name : InConfig.Get_Regions())
    { Candidate->GetRegion(Name); }
    if (_Released)
    {
        Candidate->ReleaseOwnerInteractions();
        return Fail(OutErrors, TEXT("Authored session was released during initialization."));
    }
    if (InteractionOwner != nullptr && (!IsValid(InteractionOwner) || !IsValid(InputPlayer) || !IsValid(InputWorld)
        || InteractionOwner->GetOwningLocalPlayer() != InputPlayer || InteractionOwner->GetWorld() != InputWorld
        || InputPlayer->GetSlateUser() != SlateUser))
    {
        Candidate->ReleaseOwnerInteractions();
        return Fail(OutErrors, TEXT("Authored interaction context changed during initialization."));
    }
    _Images = MoveTemp(Images);
    _ImageTextures = MoveTemp(ImageTextures);
    _Text = MoveTemp(Text);
    _Number = MoveTemp(Number);
    _Bool = MoveTemp(Bool);
    _Collections = MoveTemp(Collections);
    _View = Candidate;
    _InputOwnerBound = InteractionOwner != nullptr;
    _InteractionOwner = InteractionOwner;
    _InputPlayer = InputPlayer;
    _InputWorld = InputWorld;
    _SlateUser = SlateUser;
    _SlateUserIndex = SlateUser.IsValid() ? SlateUser->GetUserIndex() : INDEX_NONE;
    _Initialized = true;
    return true;
}

auto
    UCk_UI_AuthoredSession_UE::
    TryReload(
        const FString& InMarkup,
        const FString& InStylesheet,
        TArray<FString>& OutErrors)
    -> bool
{
    using namespace ck_ui_authored_session;
    OutErrors.Reset();
    if (!Get_IsUsable()) { return Fail(OutErrors, TEXT("Authored session reload requires a usable game-thread session.")); }
    if (_OperationActive) { return Fail(OutErrors, TEXT("Authored session reload cannot reenter another operation.")); }
    TGuardValue<bool> OperationGuard(_OperationActive, true);
    const TSharedPtr<FCkUiView> View = _View;
    const FCkUiLoadResult Result = View->TryReload(InMarkup, InStylesheet, TEXT("AuthoredSession"));
    OutErrors = Result.Errors;
    if (!Result.Succeeded) { return false; }
    if (!Get_IsUsable() || _View != View)
    { return Fail(OutErrors, TEXT("Authored session was released during reload.")); }
    return true;
}

auto
    UCk_UI_AuthoredSession_UE::
    TryReloadFiles(
        const FString& InMarkupPath,
        const FString& InStylesheetPath,
        TArray<FString>& OutErrors)
    -> bool
{
    using namespace ck_ui_authored_session;
    OutErrors.Reset();
    if (!Get_IsUsable()) { return Fail(OutErrors, TEXT("Authored file reload requires a usable game-thread session.")); }
    if (_OperationActive) { return Fail(OutErrors, TEXT("Authored file reload cannot reenter another operation.")); }
    if (InMarkupPath.IsEmpty() || InStylesheetPath.IsEmpty())
    { return Fail(OutErrors, TEXT("Authored file reload requires markup and stylesheet paths.")); }
    TGuardValue<bool> OperationGuard(_OperationActive, true);
    const TSharedPtr<FCkUiView> View = _View;
    const FCkUiLoadResult Result = View->ReloadFiles(InMarkupPath, InStylesheetPath);
    OutErrors = Result.Errors;
    if (!Result.Succeeded) { return false; }
    if (!Get_IsUsable() || _View != View)
    { return Fail(OutErrors, TEXT("Authored session was released during file reload.")); }
    return true;
}

auto
    UCk_UI_AuthoredSession_UE::
    TryMount(
        UCk_UI_AuthoredSurface_UE* InSurface,
        const FString& InRegionName,
        TArray<FString>& OutErrors)
    -> bool
{
    using namespace ck_ui_authored_session;
    OutErrors.Reset();
    if (!Get_IsUsable()) { return Fail(OutErrors, TEXT("Authored mounting requires a usable game-thread session.")); }
    if (_OperationActive) { return Fail(OutErrors, TEXT("Authored mounting cannot reenter another operation.")); }
    if (!IsValid(InSurface)) { return Fail(OutErrors, TEXT("Authored mounting requires a valid surface.")); }
    if (_InputOwnerBound && (!HasOwnedInputContext() || InSurface->GetTypedOuter<UUserWidget>() != _InteractionOwner.Get()))
    { return Fail(OutErrors, TEXT("An owned authored surface must belong to the admitted interaction owner.")); }
    if (_HasMountedSurface && _MountedSurface.Get() != InSurface)
    { return Fail(OutErrors, TEXT("An authored session may mount on only one surface during its lifetime.")); }
    const TSharedPtr<FCkUiView> View = _View;
    if (InRegionName.IsEmpty() || View->GetRevision() == 0 || !View->HasRegion(InRegionName))
    { return Fail(OutErrors, TEXT("Authored mounting requires a region in a committed document.")); }
    TGuardValue<bool> OperationGuard(_OperationActive, true);
    if (!InSurface->TryConfigure(View, InRegionName))
    { return Fail(OutErrors, TEXT("The authored surface rejected configuration.")); }
    if (!Get_IsUsable() || _View != View || !IsValid(InSurface) || !InSurface->IsConfiguredWith(View)
        || (_InputOwnerBound && !HasOwnedInputContext()))
    { return Fail(OutErrors, TEXT("Authored session or surface was released during mounting.")); }
    _MountedSurface = InSurface;
    _HasMountedSurface = true;
    return true;
}

auto
    UCk_UI_AuthoredSession_UE::
    TrySetText(
        const FString& InName,
        const FText& InValue)
    -> bool
{
    if (!Get_IsUsable()) { return false; }
    FText* Value = _Text.Find(InName);
    if (Value == nullptr) { return false; }
    *Value = InValue;
    return true;
}

auto
    UCk_UI_AuthoredSession_UE::
    TrySetNumber(
        const FString& InName,
        float InValue)
    -> bool
{
    if (!Get_IsUsable() || !FMath::IsFinite(InValue)) { return false; }
    float* Value = _Number.Find(InName);
    if (Value == nullptr) { return false; }
    *Value = InValue;
    return true;
}

auto
    UCk_UI_AuthoredSession_UE::
    TrySetBool(
        const FString& InName,
        bool InValue)
    -> bool
{
    if (!Get_IsUsable()) { return false; }
    bool* Value = _Bool.Find(InName);
    if (Value == nullptr) { return false; }
    *Value = InValue;
    return true;
}

auto
    UCk_UI_AuthoredSession_UE::
    TrySetImage(
        const FString& InName,
        UTexture2D* InTexture)
    -> bool
{
    if (!Get_IsUsable()) { return false; }
    const bool TextureValid = IsValid(InTexture);
    CK_ENSURE_IF_NOT(TextureValid, TEXT("Authored image replacement requires a valid texture.")) {}
    if (!TextureValid) { return false; }
    const bool DimensionsValid = InTexture->GetSizeX() > 0 && InTexture->GetSizeY() > 0;
    CK_ENSURE_IF_NOT(DimensionsValid, TEXT("Authored image replacement requires positive texture dimensions.")) {}
    if (!DimensionsValid) { return false; }
    const TSharedPtr<FCkUiAuthoredImageStorage> Storage = _Images.FindRef(InName);
    const bool Declared = Storage.IsValid();
    CK_ENSURE_IF_NOT(Declared, TEXT("Authored image replacement requires a declared image.")) {}
    if (!Declared) { return false; }
    Storage->SetTexture(InTexture);
    _ImageTextures.FindChecked(InName) = InTexture;
    return true;
}

auto
    UCk_UI_AuthoredSession_UE::
    TryGetText(
        const FString& InName,
        FText& OutValue) const
    -> bool
{
    if (!Get_IsUsable()) { return false; }
    const FText* Value = _Text.Find(InName);
    if (Value == nullptr) { return false; }
    OutValue = *Value;
    return true;
}

auto
    UCk_UI_AuthoredSession_UE::
    TryGetNumber(
        const FString& InName,
        float& OutValue) const
    -> bool
{
    if (!Get_IsUsable()) { return false; }
    const float* Value = _Number.Find(InName);
    if (Value == nullptr) { return false; }
    OutValue = *Value;
    return true;
}

auto
    UCk_UI_AuthoredSession_UE::
    TryGetBool(
        const FString& InName,
        bool& OutValue) const
    -> bool
{
    if (!Get_IsUsable()) { return false; }
    const bool* Value = _Bool.Find(InName);
    if (Value == nullptr) { return false; }
    OutValue = *Value;
    return true;
}

auto
    UCk_UI_AuthoredSession_UE::
    TrySetRecords(
        const FString& InName,
        const TArray<FCk_UI_AuthoredRecord>& InRecords,
        TArray<FString>& OutErrors)
    -> bool
{
    using namespace ck_ui_authored_session;
    OutErrors.Reset();
    if (!Get_IsUsable()) { return Fail(OutErrors, TEXT("Authored record replacement requires a usable game-thread session.")); }
    if (_OperationActive) { return Fail(OutErrors, TEXT("Authored record replacement cannot reenter another operation.")); }
    const FString Name = InName;
    const TSharedPtr<FCkUiCollection> Collection = _Collections.FindRef(Name);
    if (!Collection.IsValid()) { return Fail(OutErrors, TEXT("Authored record replacement requires a declared collection.")); }
    TGuardValue<bool> OperationGuard(_OperationActive, true);
    TArray<FCkUiRecordData> Records;
    if (!ck_ui_authored_collection::TryConvertRecords(InRecords, Records, OutErrors)) { return false; }
    const FCkUiLoadResult Result = Collection->TrySetRecords(MoveTemp(Records));
    OutErrors = Result.Errors;
    if (!Result.Succeeded) { return false; }
    if (!Get_IsUsable() || _Collections.FindRef(Name) != Collection)
    { return Fail(OutErrors, TEXT("Authored session was retired during accepted record publication.")); }
    return true;
}

auto
    UCk_UI_AuthoredSession_UE::
    TryGetRecords(
        const FString& InName,
        TArray<FCk_UI_AuthoredRecord>& OutRecords) const
    -> bool
{
    if (!Get_IsUsable()) { return false; }
    const TSharedPtr<FCkUiCollection> Collection = _Collections.FindRef(InName);
    return Collection.IsValid() && ck_ui_authored_collection::TryReadRecords(*Collection, OutRecords);
}

auto
    UCk_UI_AuthoredSession_UE::
    TryGetCollectionRevision(
        const FString& InName,
        int64& OutRevision) const
    -> bool
{
    if (!Get_IsUsable()) { return false; }
    const TSharedPtr<FCkUiCollection> Collection = _Collections.FindRef(InName);
    if (!Collection.IsValid()) { return false; }
    OutRevision = Collection->GetRevision();
    return true;
}

auto
    UCk_UI_AuthoredSession_UE::
    HasOwnedInputContext() const
    -> bool
{
    if (!IsInGameThread() || !FSlateApplication::IsInitialized() || !_InputOwnerBound) { return false; }
    const auto* Owner = _InteractionOwner.Get();
    const auto* Player = _InputPlayer.Get();
    const auto* World = _InputWorld.Get();
    if (Owner == nullptr || Player == nullptr || World == nullptr || World->bIsTearingDown) { return false; }
    if (Owner->GetOwningLocalPlayer() != Player || Owner->GetWorld() != World || Player->GetWorld() != World) { return false; }
    const auto* Controller = Owner->GetOwningPlayer();
    if (!IsValid(Controller) || !Controller->IsLocalController() || Controller->GetLocalPlayer() != Player
        || Controller->GetWorld() != World) { return false; }
    const TSharedPtr<FSlateUser> User = _SlateUser.Pin();
    return User.IsValid() && User->GetUserIndex() == _SlateUserIndex
        && FSlateApplication::Get().GetUser(_SlateUserIndex) == User && Player->GetSlateUser() == User;
}

auto
    UCk_UI_AuthoredSession_UE::
    Get_IsInputActive() const
    -> bool
{
    if (!Get_IsUsable() || !_InputActive || !_InputReleaseGuard.IsValid() || _InputReleaseGuard->IsDraining()
        || !HasOwnedInputContext()) { return false; }
    const auto* Owner = _InteractionOwner.Get();
    const auto* Surface = _MountedSurface.Get();
    return Owner->IsActivated() && Surface != nullptr && Surface->IsConfiguredWith(_View)
        && Surface->GetTypedOuter<UUserWidget>() == Owner;
}

auto
    UCk_UI_AuthoredSession_UE::
    Get_SlateUserIndex() const
    -> int32
{
    return Get_IsUsable() && HasOwnedInputContext() ? _SlateUserIndex : INDEX_NONE;
}

auto
    UCk_UI_AuthoredSession_UE::
    Get_IsButtonFocused(
        const FString& InId) const
    -> bool
{
    if (!Get_IsInputActive()) { return false; }
    const TSharedPtr<SButton> Button = _View->GetButton(InId);
    return Button.IsValid() && FSlateApplication::Get().GetUserFocusedWidget(_SlateUserIndex) == Button;
}

auto
    UCk_UI_AuthoredSession_UE::
    TryBeginInput()
    -> bool
{
    if (!Get_IsUsable() || _OperationActive || _InputOperationActive || !HasOwnedInputContext()) { return false; }
    const auto* Owner = _InteractionOwner.Get();
    const auto* Surface = _MountedSurface.Get();
    if (!Owner->IsActivated() || Surface == nullptr || !Surface->IsConfiguredWith(_View)
        || Surface->GetTypedOuter<UUserWidget>() != Owner) { return false; }
    if (Get_IsInputActive()) { return true; }
    TGuardValue<bool> InputGuard(_InputOperationActive, true);
    Request_EndInput();
    // Ending a prior admission can run focus callbacks; never publish into replaced state.
    if (!Get_IsUsable() || !HasOwnedInputContext() || _InteractionOwner.Get() != Owner || !Owner->IsActivated()
        || _MountedSurface.Get() != Surface || !Surface->IsConfiguredWith(_View)) { return false; }
    const uint64 Generation = ++_InputGeneration;
    _PreviousInputFocus = FSlateApplication::Get().GetUserFocusedWidget(_SlateUserIndex);
    _InputReleaseGuard = MakeShared<FCkUiAuthoredInputReleaseGuard>(this, _InputWorld.Get(), _SlateUserIndex);
    _InputActive = true;
    const bool Registered = FSlateApplication::Get().RegisterInputPreProcessor(_InputReleaseGuard, 0);
    if (!Registered || _InputGeneration != Generation || !Get_IsInputActive())
    {
        if (_InputGeneration == Generation) { Request_EndInput(); }
        return false;
    }
    return true;
}

auto
    UCk_UI_AuthoredSession_UE::
    Request_EndInput(
        bool InRestoreFocus)
    -> void
{
    if (!IsInGameThread()) { return; }
    if (!_InputActive && !_InputReleaseGuard.IsValid() && !_PreviousInputFocus.IsValid()) { return; }
    TGuardValue<bool> InputGuard(_InputOperationActive, true);
    ++_InputGeneration;
    _InputActive = false;
    const TSharedPtr<FCkUiAuthoredInputReleaseGuard> ReleaseGuard = MoveTemp(_InputReleaseGuard);
    if (ReleaseGuard.IsValid()) { ReleaseGuard->BeginDrain(); }
    const TSharedPtr<SWidget> PreviousFocus = _PreviousInputFocus.Pin();
    _PreviousInputFocus.Reset();
    if (!InRestoreFocus) { return; }
    if (!FSlateApplication::IsInitialized() || !PreviousFocus.IsValid() || !HasOwnedInputContext()) { return; }
    const auto* Owner = _InteractionOwner.Get();
    const auto* Surface = _MountedSurface.Get();
    if (Surface == nullptr || !Surface->IsConfiguredWith(_View)) { return; }
    const TSharedPtr<SWidget> OwnerRoot = Owner->GetCachedWidget();
    if (!OwnerRoot.IsValid()) { return; }
    auto& Slate = FSlateApplication::Get();
    const TSharedPtr<SWidget> CurrentFocus = Slate.GetUserFocusedWidget(_SlateUserIndex);
    if (CurrentFocus == OwnerRoot || Slate.HasUserFocusedDescendants(OwnerRoot.ToSharedRef(), _SlateUserIndex))
    { Slate.SetUserFocus(_SlateUserIndex, PreviousFocus, EFocusCause::SetDirectly); }
}

auto
    UCk_UI_AuthoredSession_UE::
    FindFocusableButton(const FString& InId)
    -> TSharedPtr<SWidget>
{
    if (!Get_IsInputActive() || _OperationActive || InId.IsEmpty()) { return {}; }
    const TSharedPtr<FCkUiView> View = _View;
    const int64 Revision = View->GetRevision();
    const uint64 Generation = _InputGeneration;
    const TSharedPtr<SButton> Button = View->GetButton(InId);
    if (!Button.IsValid()) { return {}; }
    if (Button->HasRegisteredSlateAttribute() && Button->IsAttributesUpdatesEnabled())
    {
        FSlateAttributeMetaData::UpdateAllAttributes(*Button,
            FSlateAttributeMetaData::EInvalidationPermission::AllowInvalidationIfConstructed);
    }
    if (!Get_IsInputActive() || _View != View || View->GetRevision() != Revision || _InputGeneration != Generation
        || View->GetButton(InId) != Button || !Button->IsEnabled() || !Button->GetVisibility().IsVisible()
        || !Button->SupportsKeyboardFocus()) { return {}; }
    return Button;
}

auto
    UCk_UI_AuthoredSession_UE::
    DoFocusButton(const FString& InId, const TSharedPtr<SWidget>& InButton)
    -> bool
{
    const TSharedPtr<SWidget> Button = InButton;
    if (!Get_IsInputActive() || !Button.IsValid() || _View->GetButton(InId) != Button) { return false; }
    const TSharedPtr<FCkUiView> View = _View;
    const int64 Revision = View->GetRevision();
    const uint64 Generation = _InputGeneration;
    FSlateApplication::Get().SetUserFocus(_SlateUserIndex, Button, EFocusCause::Navigation);
    return Get_IsInputActive() && _View == View && _InputGeneration == Generation && View->GetRevision() == Revision
        && View->GetButton(InId) == Button && FSlateApplication::Get().GetUserFocusedWidget(_SlateUserIndex) == Button;
}

auto
    UCk_UI_AuthoredSession_UE::
    TryFocusButton(const FString& InId)
    -> bool
{
    if (_InputOperationActive || !Get_IsInputActive() || _OperationActive) { return false; }
    // Focus/attribute callbacks may mutate caller storage; keep a stable request across them.
    const FString Id = InId;
    TGuardValue<bool> InputGuard(_InputOperationActive, true);
    return DoFocusButton(Id, FindFocusableButton(Id));
}

auto
    UCk_UI_AuthoredSession_UE::
    TryNavigateButtons(const TArray<FString>& InIds, int32 InDirection)
    -> bool
{
    if (_InputOperationActive || !Get_IsInputActive() || _OperationActive || InIds.IsEmpty()
        || (InDirection != -1 && InDirection != 1)) { return false; }
    const TArray<FString> Ids = InIds;
    TGuardValue<bool> InputGuard(_InputOperationActive, true);
    const TSharedPtr<FCkUiView> View = _View;
    const int64 Revision = View->GetRevision();
    const uint64 Generation = _InputGeneration;
    TSet<FString> Names;
    for (const FString& Id : Ids)
    {
        if (Id.IsEmpty() || Names.Contains(Id) || !View->GetButton(Id).IsValid()) { return false; }
        Names.Add(Id);
    }
    const int32 Count = Ids.Num();
    int32 Current = InDirection > 0 ? Count - 1 : 0;
    const TSharedPtr<SWidget> Focused = FSlateApplication::Get().GetUserFocusedWidget(_SlateUserIndex);
    for (int32 Index = 0; Index < Count; ++Index)
    { if (View->GetButton(Ids[Index]) == Focused) { Current = Index; break; } }
    for (int32 Offset = 0; Offset < Count; ++Offset)
    {
        if (!Get_IsInputActive() || _View != View || View->GetRevision() != Revision || _InputGeneration != Generation) { return false; }
        Current = InDirection > 0 ? (Current == Count - 1 ? 0 : Current + 1) : (Current == 0 ? Count - 1 : Current - 1);
        const TSharedPtr<SWidget> Candidate = FindFocusableButton(Ids[Current]);
        // A focus callback can establish another owner's focus. Skip only ineligible candidates;
        // after attempting focus, yield to that callback instead of trying another button.
        if (Candidate.IsValid()) { return DoFocusButton(Ids[Current], Candidate); }
    }
    return false;
}

auto
    UCk_UI_AuthoredSession_UE::
    Request_Release()
    -> void
{
    if (!IsInGameThread() || _Released) { return; }
    _Released = true;
    Request_EndInput();
    const TSharedPtr<FCkUiView> View = _View;
    if (View.IsValid()) { View->ReleaseOwnerInteractions(); }
    _View.Reset();
    _MountedSurface.Reset();
    _Text.Reset();
    _Number.Reset();
    _Bool.Reset();
    _Collections.Reset();
    _Images.Reset();
    _ImageTextures.Reset();
    _InteractionOwner.Reset();
    _InputPlayer.Reset();
    _InputWorld.Reset();
    _SlateUser.Reset();
}

auto
    UCk_UI_AuthoredSession_UE::
    Get_IsUsable() const
    -> bool
{
    return IsInGameThread() && FSlateApplication::IsInitialized() && !_Released && _Initialized
        && _View.IsValid() && !_View->IsOwnerReleased();
}

auto
    UCk_UI_AuthoredSession_UE::
    Get_Revision() const
    -> int64
{
    return Get_IsUsable() ? _View->GetRevision() : 0;
}

auto
    UCk_UI_AuthoredSession_UE::
    Get_View() const
    -> TSharedPtr<FCkUiView>
{
    return Get_IsUsable() ? _View : TSharedPtr<FCkUiView>{};
}

auto
    UCk_UI_AuthoredSession_UE::
    Get_Collection(
        const FString& InName) const
    -> TSharedPtr<const FCkUiCollection>
{
    return Get_IsUsable() ? _Collections.FindRef(InName) : TSharedPtr<const FCkUiCollection>{};
}

auto
    UCk_UI_AuthoredSession_UE::
    CanDispatchEvents() const
    -> bool
{
    return Get_IsUsable() && !_OperationActive && (!_InputOwnerBound || Get_IsInputActive());
}

auto
    UCk_UI_AuthoredSession_UE::
    BeginDestroy()
    -> void
{
    Request_Release();
    Super::BeginDestroy();
}
