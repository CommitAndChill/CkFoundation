#pragma once

#include "CoreMinimal.h"

#include <Components/Widget.h>

#include "CkAuthoredSurface_Widget.generated.h"

class FCkUiView;
class SBox;
class SWidget;

/** Mounts one accepted CkSlateLayout region inside a UMG widget tree. */
UCLASS(BlueprintType, meta = (DisplayName = "Ck Authored Surface"))
class CKWIDGETS_API UCk_UI_AuthoredSurface_UE : public UWidget
{
    GENERATED_BODY()

public:
    /**
     * The caller owns document construction and reload. The view must have a committed document
     * containing InRegionName and must remain usable. Entry rejection leaves this widget unchanged;
     * callbacks during replacement may establish newer state that an interrupted configuration yields to.
     * Configuration and release run on the game thread.
     */
    auto TryConfigure(const TSharedPtr<FCkUiView>& InView, const FString& InRegionName) -> bool;

    UFUNCTION(BlueprintCallable, Category = "Ck|UI|Authoring")
    void ReleaseView();

    UFUNCTION(BlueprintPure, Category = "Ck|UI|Authoring")
    bool HasConfiguredView() const;

    /** Native owner identity check; never changes or realizes the surface. */
    auto IsConfiguredWith(const TSharedPtr<FCkUiView>& InView) const -> bool;

    auto ReleaseSlateResources(bool bReleaseChildren) -> void override;

protected:
    auto RebuildWidget() -> TSharedRef<SWidget> override;

private:
    bool _ReleasingSlateResources = false;
    uint64 _MutationGeneration = 0;
    TSharedPtr<FCkUiView> _View;
    TSharedPtr<SWidget> _RegionMount;
    TSharedPtr<SBox> _Host;
};
