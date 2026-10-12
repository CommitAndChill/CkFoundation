#include "CkDebugDraw_Subsystem.h"

#include "CkCore/Algorithms/CkAlgorithms.h"
#include "CkCore/Diagnostics/CkDiagnosticVisibility.h"
#include "CkCore/Debug/CkDebugDraw_Utils.h"
#include "CkCore/TypeTraits/CkTypeTraits.h"
#include "CkCore/Validation/CkIsValid.h"

#include <DrawDebugHelpers.h>
#include <Engine/Canvas.h>
#include <Engine/Engine.h>
#include <GameFramework/HUD.h>
#include <GameFramework/PlayerController.h>

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_DebugDraw_Subsystem_UE::
    Initialize(
        FSubsystemCollectionBase& InCollection)
    -> void
{
    Super::Initialize(InCollection);

    if (GetWorld()->IsNetMode(NM_DedicatedServer))
    { return; }

    _PostRenderHUD_DelegateHandle = AHUD::OnHUDPostRender.AddWeakLambda(this, [this](AHUD* InHUD, UCanvas* InCanvas)
    {
        if (ck::Is_NOT_Valid(this))
        { return; }

        this->DoOnPostRenderHUD(InHUD, InCanvas);
    });
}

auto
    UCk_DebugDraw_Subsystem_UE::
    Deinitialize()
    -> void
{
    if (_PostRenderHUD_DelegateHandle.IsValid())
    {
        AHUD::OnHUDPostRender.Remove(_PostRenderHUD_DelegateHandle);
        _PostRenderHUD_DelegateHandle.Reset();
    }

    _PendingDrawRequests.Reset();
    _FrameDrawRequests.Reset();
    _RenderedHUDsThisFrame.Reset();
    _DrawFrame = MAX_uint64;
    _bSuppressedThisFrame = false;

    Super::Deinitialize();
}

auto
    UCk_DebugDraw_Subsystem_UE::
    Request_DrawRect_OnScreen(
        const FCk_Request_DebugDrawOnScreen_Rect& InRequest)
    -> void
{
    _PendingDrawRequests.Emplace(InRequest);
}

auto
    UCk_DebugDraw_Subsystem_UE::
    Request_DrawLine_OnScreen(
        const FCk_Request_DebugDrawOnScreen_Line& InRequest)
    -> void
{
    _PendingDrawRequests.Emplace(InRequest);
}

void
    UCk_DebugDraw_Subsystem_UE::
    Request_DrawText_OnScreen(
        const FCk_Request_DebugDrawOnScreen_Text& InRequest)
{
    _PendingDrawRequests.Emplace(InRequest);
}

auto
    UCk_DebugDraw_Subsystem_UE::
    DoOnPostRenderHUD(
        AHUD* InHUD,
        UCanvas* InCanvas)
    -> void
{
    const auto* World = GetWorld();
    const auto* Owner = ck::IsValid(InHUD) ? InHUD->GetOwningPlayerController() : nullptr;
    const bool bValidOwner =
        ck::IsValid(World) &&
        ck::IsValid(InHUD) &&
        InHUD->GetWorld() == World &&
        ck::IsValid(Owner) &&
        Owner->GetWorld() == World &&
        Owner->IsLocalController() &&
        ck::IsValid(InCanvas) &&
        InCanvas->Canvas != nullptr;
    if (!bValidOwner)
    { return; }

    if (_DrawFrame != GFrameCounter)
    {
        _DrawFrame = GFrameCounter;
        _RenderedHUDsThisFrame.Reset();
        _bSuppressedThisFrame = false;
        Swap(_PendingDrawRequests, _FrameDrawRequests);
        _PendingDrawRequests.Reset();
    }

    if (ck::diagnostic_visibility::Is_HiddenForStreamerMode())
    {
        if (!_bSuppressedThisFrame)
        {
            FlushPersistentDebugLines(GetWorld());
            FlushDebugStrings(GetWorld());
        }
        _bSuppressedThisFrame = true;
        _PendingDrawRequests.Reset();
        _FrameDrawRequests.Reset();
        return;
    }

    if (_bSuppressedThisFrame || _RenderedHUDsThisFrame.Contains(InHUD))
    { return; }
    _RenderedHUDsThisFrame.Add(InHUD);

    ck::algo::ForEachRequest(_FrameDrawRequests, ck::Visitor([&](const auto& RequestVariant)
    {
        DoHandleRequest(InHUD, InCanvas, RequestVariant);
    }), ck::policy::DontResetContainer{});
}

auto
    UCk_DebugDraw_Subsystem_UE::
    DoHandleRequest(
        AHUD* InHUD,
        UCanvas* InCanvas,
        const FCk_Request_DebugDrawOnScreen_Rect& InRequest)
    -> void
{
    const auto& Rect = InRequest.Get_Rect();
    const auto& RectColor = InRequest.Get_RectColor();

    InHUD->DrawRect(RectColor, Rect.Min.X, Rect.Min.Y, Rect.GetExtent().X * 2.0f, Rect.GetExtent().Y * 2.0f);
}

auto
    UCk_DebugDraw_Subsystem_UE::
    DoHandleRequest(
        AHUD* InHUD,
        UCanvas* InCanvas,
        const FCk_Request_DebugDrawOnScreen_Line& InRequest)
    -> void
{
    const auto& LineStart = InRequest.Get_LineStart();
    const auto& LineEnd = InRequest.Get_LineEnd();
    const auto& LineColor = InRequest.Get_LineColor();
    const auto& LineThickness = InRequest.Get_LineThickness();

    InHUD->DrawLine(LineStart.X, LineStart.Y, LineEnd.X, LineEnd.Y, LineColor, LineThickness);
}

auto
    UCk_DebugDraw_Subsystem_UE::
    DoHandleRequest(
        AHUD* InHUD,
        UCanvas* InCanvas,
        const FCk_Request_DebugDrawOnScreen_Text& InRequest)
        -> void
{
    const bool bValidCanvas = ck::IsValid(InCanvas) && InCanvas->Canvas != nullptr;
    CK_ENSURE_IF_NOT(bValidCanvas, TEXT("Canvas is not valid"))
    {}
    if (!bValidCanvas)
    { return; }

    // Set text rendering properties
    const auto TextColor = InRequest.Get_TextColor().ToFColor(true);
    const auto TextScale = FVector2D(InRequest.Get_TextScale(), InRequest.Get_TextScale());

    // Use default font (you could make this configurable via the request if needed)
    const bool bValidEngine = ck::IsValid(GEngine);
    CK_ENSURE_IF_NOT(bValidEngine, TEXT("Engine is not valid"))
    {}
    if (!bValidEngine)
    { return; }

    auto* Font = GEngine->GetMediumFont();
    if (!ck::IsValid(Font))
    { Font = GEngine->GetSmallFont(); }

    const bool bValidFont = ck::IsValid(Font);
    CK_ENSURE_IF_NOT(bValidFont, TEXT("No valid font available for text rendering"))
    {}
    if (!bValidFont)
    { return; }

    // Draw the text
    InCanvas->K2_DrawText(
        Font,
        InRequest.Get_Text(),
        InRequest.Get_TextPosition(),
        TextScale,
        TextColor,
        0.0f, // Kerning
        FLinearColor::Black, // Shadow color
        FVector2D(1.0f, 1.0f), // Shadow offset
        false, // Centered X
        false, // Centered Y
        true,  // Outlined
        FLinearColor::Black // Outline color
    );
}

// --------------------------------------------------------------------------------------------------------------------
