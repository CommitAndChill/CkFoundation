#include "CkWidgets/Authoring/CkAuthoredSurface_Widget.h"

#include "CkSlateLayout/SCkUiSurface.h"

#include <Widgets/Layout/SBox.h>
#include <Widgets/SNullWidget.h>

auto
    UCk_UI_AuthoredSurface_UE::
    TryConfigure(
        const TSharedPtr<FCkUiView>& InView,
        const FString& InRegionName)
    -> bool
{
    if (!IsInGameThread())
    { return false; }
    const TSharedPtr<FCkUiView> CandidateView = InView;
    const FString CandidateRegion = InRegionName;
    const bool IsCandidateValid = !_ReleasingSlateResources && CandidateView.IsValid() && !CandidateRegion.IsEmpty()
        && CandidateView->GetRevision() > 0 && !CandidateView->IsOwnerReleased() && CandidateView->HasRegion(CandidateRegion);
    if (!IsCandidateValid)
    { return false; }

    const TSharedRef<SWidget> CandidateMount = CandidateView->GetRegion(CandidateRegion);
    if (CandidateMount == SNullWidget::NullWidget)
    { return false; }

    if (_View == CandidateView && _RegionMount == CandidateMount)
    { return true; }

    const uint64 Generation = ++_MutationGeneration;
    const TSharedPtr<FCkUiView> PreviousView = _View;
    if (PreviousView.IsValid() && PreviousView != CandidateView)
    { PreviousView->ReleaseOwnerInteractions(); }
    if (_MutationGeneration != Generation)
    { return false; }
    if (CandidateView->IsOwnerReleased() || CandidateView->GetRevision() == 0 || !CandidateView->HasRegion(CandidateRegion)
        || CandidateView->GetRegion(CandidateRegion) != CandidateMount)
    {
        ReleaseView();
        return false;
    }

    _View = CandidateView;
    _RegionMount = CandidateMount;
    if (_Host.IsValid())
    { _Host->SetContent(CandidateMount); }

    if (_MutationGeneration != Generation)
    { return false; }
    if (CandidateView->IsOwnerReleased())
    {
        ReleaseView();
        return false;
    }
    return _View == CandidateView && _RegionMount == CandidateMount;
}

auto
    UCk_UI_AuthoredSurface_UE::
    ReleaseView()
    -> void
{
    const uint64 Generation = ++_MutationGeneration;
    const TSharedPtr<FCkUiView> PreviousView = _View;
    if (PreviousView.IsValid())
    { PreviousView->ReleaseOwnerInteractions(); }
    if (_MutationGeneration != Generation)
    { return; }

    _RegionMount.Reset();
    _View.Reset();
    if (_Host.IsValid())
    { _Host->SetContent(SNullWidget::NullWidget); }
}

auto
    UCk_UI_AuthoredSurface_UE::
    HasConfiguredView() const
    -> bool
{
    return _View.IsValid() && !_View->IsOwnerReleased() && _RegionMount.IsValid();
}

auto
    UCk_UI_AuthoredSurface_UE::
    IsConfiguredWith(
        const TSharedPtr<FCkUiView>& InView) const
    -> bool
{
    return IsInGameThread() && !_ReleasingSlateResources && InView.IsValid() && _View == InView && HasConfiguredView();
}

auto
    UCk_UI_AuthoredSurface_UE::
    ReleaseSlateResources(
        bool bReleaseChildren)
    -> void
{
    TGuardValue<bool> ReleasingGuard(_ReleasingSlateResources, true);
    ReleaseView();
    _Host.Reset();
    MyWidget.Reset();
    Super::ReleaseSlateResources(bReleaseChildren);
}

auto
    UCk_UI_AuthoredSurface_UE::
    RebuildWidget()
    -> TSharedRef<SWidget>
{
    _Host = SNew(SBox)
    [
        HasConfiguredView() ? _RegionMount.ToSharedRef() : SNullWidget::NullWidget
    ];
    return _Host.ToSharedRef();
}
