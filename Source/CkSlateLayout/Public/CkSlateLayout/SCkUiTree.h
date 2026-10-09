#pragma once

#include "CkSlateLayout/CkUiTreeCollection.h"
#include "CkSlateLayout/CkUiContextMenu.h"
#include "CkSlateLayout/CkUiWidgetRegistry.h"
#include "Fonts/SlateFontInfo.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"

class FCkUiView;

DECLARE_DELEGATE_TwoParams(FOnCkUiTreeSelectionChanged, TOptional<FString>, ESelectInfo::Type);

enum class ECkUiTreeSelectionRequestState : uint8
{
    None,
    Pending,
    Applied,
    Rejected
};

/** Virtualized native tree whose read-only authored row uses FCkUiView. */
class CKSLATELAYOUT_API SCkUiTree final : public SCompoundWidget
{
public:
    using FNode = TSharedPtr<const FCkUiTreeNode>;
    using FCellFactory = TFunction<TSharedPtr<FCkUiView>(const FCkUiNode&, TWeakPtr<const FCkUiTreeNode>, FString&)>;

    SLATE_BEGIN_ARGS(SCkUiTree) {}
        SLATE_ARGUMENT(TSharedPtr<FCkUiTreeCollection>, Collection)
        SLATE_ARGUMENT(FSlateFontInfo, BaseFont)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    virtual ~SCkUiTree() override;
    virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

    /** Prepare detached row views; Commit publishes only after the outer document succeeds. */
    auto Prepare(const FCkUiNode& InDefinition, FCellFactory InFactory, TAttribute<FText> InFilter,
        FOnCkUiTreeSelectionChanged InSelectionChanged, FString& OutFailure,
        FOnCkUiContextMenuOpening InAuthoredContextMenu = {}) -> TUniquePtr<ICkUiPreparedWidgetUpdate>;
    /** Optional native host key hook. Authored Shift+F10 context menus retain precedence; an unhandled reply falls through to STreeView. */
    void SetHostKeyDownHandler(FOnKeyDown InHandler);
    void ReleaseContextMenu();
    /** Permanently retire owner input and deferred work, including on externally retained widgets. */
    void ReleaseOwnerInteraction();
    auto TrySelectKey(TOptional<FString> InKey, bool InNotify = false) -> bool;
    /** Queue selection after native projection refresh. True means admitted, not applied. Latest admitted request wins.
     *  Optional scrolling runs only after a changed key is accepted; unchanged selection preserves user scroll. */
    auto TryRequestSelectKey(TOptional<FString> InKey, bool InNotify = false,
        bool InScrollIntoView = false) -> bool;
    void CancelSelectionRequest();
    auto GetSelectionRequestState() const -> ECkUiTreeSelectionRequestState;
    const FString& GetSelectionRequestError() const;
    auto TrySetExpanded(const FString& InKey, bool bInExpanded) -> bool;
    auto GetSelectedKey() const -> TOptional<FString>;
    auto GetExpandedKeys() const -> TSet<FString>;
    auto GetVisibleNodeCount() const -> int32;
    auto GetLiveRowCount() const -> int32;
    auto GetTree() const -> TSharedPtr<STreeView<FNode>>;
    const FString& GetLastCellError() const;

private:
    struct FImpl;
    TSharedPtr<FImpl> _Impl;
};
