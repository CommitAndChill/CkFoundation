#include "Widgets/SCkProceduralAction_Channels.h"

#include "CkProceduralAnimationEditor/CkProceduralActionAuthoringCommands.h"
#include "CkProceduralAnimationEditor/CkProceduralActionAuthoringSession.h"
#include "CkProceduralAnimationEditor/CkProceduralActionEditorModel.h"
#include "CkProceduralAnimation/Gait/CkProceduralGait_Fragment_Data.h"
#include "CkProceduralAnimation/Rig/CkProceduralRig_Fragment_Data.h"
#include "CkEditorTools/Style/CkStyle.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "CkProceduralActionChannels"

namespace ck_procedural_action_channels
{
    auto
        TagName(
            FName InChannel)
        -> FString
    {
        return InChannel == ck::procedural_action_editor::Get_BodyChannelName() ? FString{TEXT("Body")} : InChannel.ToString();
    }

    auto
        SubjectLimbs(
            const UCk_ProceduralActionPreview_Config& InConfig)
        -> TArray<FName>
    {
        auto Limbs = TArray<FName>{};
        if (InConfig.Get_SubjectMode() == ECk_ProceduralActionPreview_SubjectMode::Walking)
        {
            if (const auto* Rig = InConfig.Get_WalkingRig().Get())
            {
                for (const auto& Leg : Rig->Get_Legs())
                { Limbs.Add(Leg.Get_Id()); }
            }
            return Limbs;
        }
        for (const auto& Limb : InConfig.Get_Layout().Get_Limbs())
        { Limbs.Add(Limb.Get_Id()); }
        return Limbs;
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkProceduralAction_Channels::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _Session = InArgs._Session;
    _Model = InArgs._Model;
    _OnResult = InArgs._OnResult;
    ChildSlot
    [
        SAssignNew(_Tree, STreeView<FItemPtr>)
        .Tag(FName{TEXT("CkActionEditor.Channels")})
        .TreeItemsSource(&_Items)
        .SelectionMode(ESelectionMode::Single)
        .OnGenerateRow(this, &SCkProceduralAction_Channels::MakeRow)
        .OnGetChildren_Lambda([](FItemPtr, TArray<FItemPtr>&) {})
        .OnSelectionChanged(this, &SCkProceduralAction_Channels::OnSelectionChanged)
        .OnContextMenuOpening(this, &SCkProceduralAction_Channels::MakeContextMenu)
    ];
    Refresh();
}

auto
    SCkProceduralAction_Channels::
    Get_BoundLimb(
        FName InSlot) const
    -> FName
{
    const auto Session = _Session.Pin();
    const auto* Config = Session.IsValid() ? Session->Get_PreviewConfig() : nullptr;
    const auto* Binding = Config == nullptr ? nullptr : Config->Get_SlotBindings().FindByPredicate(
        [InSlot](const FCk_ProceduralAction_SlotBinding& InBinding) { return InBinding.Get_Slot() == InSlot; });
    return Binding == nullptr ? NAME_None : Binding->Get_LimbId();
}

auto
    SCkProceduralAction_Channels::
    Refresh()
    -> void
{
    using EKind = FItem::EKind;
    const auto Session = _Session.Pin();
    const auto* Clip = Session.IsValid() ? Session->Get_Clip() : nullptr;
    const auto* Config = Session.IsValid() ? Session->Get_PreviewConfig() : nullptr;
    auto Wanted = TArray<FItemPtr>{};
    const auto Add = [this, &Wanted](EKind InKind, FName InId, const FText& InLabel)
    {
        const auto Key = FString::Printf(TEXT("%d:%s"), static_cast<int32>(InKind), *InId.ToString());
        auto& Item = _ItemsByKey.FindOrAdd(Key);
        if (NOT Item.IsValid())
        { Item = MakeShared<FItem>(); }
        Item->Kind = InKind;
        Item->Id = InId;
        Item->Label = InLabel;
        Wanted.Add(Item);
    };
    if (Clip != nullptr)
    {
        if (Clip->Get_BodyChannel().Get_Enabled())
        { Add(EKind::Body, ck::procedural_action_editor::Get_BodyChannelName(), LOCTEXT("Body", "Body")); }
        auto Bound = TArray<FName>{};
        for (const auto& Slot : Clip->Get_Slots())
        {
            Add(EKind::Slot, Slot.Get_Id(), FText::FromName(Slot.Get_Id()));
            Bound.Add(Get_BoundLimb(Slot.Get_Id()));
        }
        if (Config != nullptr)
        {
            const auto Walking = Config->Get_SubjectMode() == ECk_ProceduralActionPreview_SubjectMode::Walking;
            for (const auto Limb : ck_procedural_action_channels::SubjectLimbs(*Config))
            {
                if (NOT Bound.Contains(Limb))
                {
                    Add(EKind::Limb, Limb, FText::Format(Walking ? LOCTEXT("UnderGait", "{0}   under the gait") : LOCTEXT("Unbound", "{0}   unbound"),
                        FText::FromName(Limb)));
                }
            }
            const auto& Subject = ck::procedural_action_editor::Get_Subject(*Clip);
            Add(EKind::Summary, NAME_None, FText::Format(LOCTEXT("Summary", "Rig: {0}   Gait: {1}   Floor: {2}"),
                FText::FromString(Subject.Get_WalkingRig().IsNull() ? FString{TEXT("dedicated layout")} : Subject.Get_WalkingRig().GetAssetName()),
                FText::FromString(Subject.Get_WalkingGait().IsNull() ? FString{TEXT("none")} : Subject.Get_WalkingGait().GetAssetName()),
                Config->Get_HasStaticFloor() ? LOCTEXT("Yes", "yes") : LOCTEXT("No", "no")));
        }
    }
    if (Wanted != _Items)
    {
        _Items = MoveTemp(Wanted);
        for (auto It = _ItemsByKey.CreateIterator(); It; ++It)
        {
            if (NOT _Items.Contains(It.Value()))
            { It.RemoveCurrent(); }
        }
        _Tree->RequestTreeRefresh();
    }
    _Tree->RebuildList();
    SyncSelection();
}

auto
    SCkProceduralAction_Channels::
    SyncSelection()
    -> void
{
    const auto Model = _Model.Pin();
    const auto Channel = Model.IsValid() ? Model->Get_SelectedChannel() : NAME_None;
    const auto* Selected = _Items.FindByPredicate([Channel](const FItemPtr& InItem)
        { return InItem->Kind != FItem::EKind::Summary && InItem->Kind != FItem::EKind::Limb && InItem->Id == Channel; });
    if (Selected == nullptr)
    {
        _Tree->ClearSelection();
        return;
    }
    if (NOT _Tree->IsItemSelected(*Selected))
    { _Tree->SetSelection(*Selected, ESelectInfo::Direct); }
}

auto
    SCkProceduralAction_Channels::
    OnSelectionChanged(
        FItemPtr InItem,
        ESelectInfo::Type InInfo)
    -> void
{
    if (InInfo == ESelectInfo::Direct || NOT InItem.IsValid()
        || InItem->Kind == FItem::EKind::Summary || InItem->Kind == FItem::EKind::Limb)
    { return; }
    const auto Model = _Model.Pin();
    const auto Session = _Session.Pin();
    if (NOT Model.IsValid() || NOT Session.IsValid())
    { return; }
    auto Key = FCk_ProceduralActionKeyTime{};
    if (Session->ElapsedToKey(Model->Get_Time(), Key))
    { Model->SelectKey(Key.Get_PhaseId(), InItem->Id, Key.Get_KeyTime()); }
    else
    { Model->SelectChannel(InItem->Id); }
}

auto
    SCkProceduralAction_Channels::
    MakeRow(
        FItemPtr InItem,
        const TSharedRef<STableViewBase>& InOwner)
    -> TSharedRef<ITableRow>
{
    using EKind = FItem::EKind;
    const auto WeakModel = _Model;
    const auto Channel = InItem->Id;
    const auto TagSuffix = ck_procedural_action_channels::TagName(Channel);
    const auto IsChannel = InItem->Kind == EKind::Body || InItem->Kind == EKind::Slot;
    const auto Row = SNew(SHorizontalBox);
    if (IsChannel)
    {
        Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(2.0f, 0.0f)
        [
            SNew(SCheckBox)
            .Tag(FName{*(TEXT("CkActionEditor.Record.") + TagSuffix)})
            .ToolTipText(LOCTEXT("RecordTip", "Record (S) keys this channel."))
            .IsChecked_Lambda([WeakModel, Channel]() -> ECheckBoxState
            {
                const auto Model = WeakModel.Pin();
                return Model.IsValid() && Model->Get_IsRecorded(Channel) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
            })
            .OnCheckStateChanged_Lambda([WeakModel, Channel](ECheckBoxState InState)
            {
                if (const auto Model = WeakModel.Pin())
                { Model->SetRecorded(Channel, InState == ECheckBoxState::Checked); }
            })
        ];
    }
    Row->AddSlot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(4.0f, 2.0f)
    [
        SNew(STextBlock)
        .Text(InItem->Label)
        .ColorAndOpacity(IsChannel ? CkStyle::Text() : CkStyle::TextMute())
    ];
    if (InItem->Kind == EKind::Slot)
    {
        const auto WeakChannels = TWeakPtr<SCkProceduralAction_Channels>{SharedThis(this)};
        Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(2.0f, 0.0f)
        [
            SNew(SComboButton)
            .Tag(FName{*(TEXT("CkActionEditor.Bind.") + TagSuffix)})
            .ToolTipText(LOCTEXT("BindTip", "The subject limb this slot drives."))
            .OnGetMenuContent_Lambda([WeakChannels, Channel]() -> TSharedRef<SWidget>
            {
                const auto Channels = WeakChannels.Pin();
                return Channels.IsValid() ? Channels->MakeBindingMenu(Channel) : SNullWidget::NullWidget;
            })
            .ButtonContent()
            [
                SNew(STextBlock).Text_Lambda([WeakChannels, Channel]() -> FText
                {
                    const auto Channels = WeakChannels.Pin();
                    const auto Limb = Channels.IsValid() ? Channels->Get_BoundLimb(Channel) : NAME_None;
                    return Limb.IsNone() ? LOCTEXT("NotBound", "bind a limb") : FText::FromName(Limb);
                })
            ]
        ];
    }
    return SNew(STableRow<FItemPtr>, InOwner)
        .Tag(FName{*(TEXT("CkActionEditor.Channel.") + TagSuffix)})
        .Style(&FAppStyle::Get().GetWidgetStyle<FTableRowStyle>("TableView.Row"))
        [Row];
}

auto
    SCkProceduralAction_Channels::
    MakeBindingMenu(
        FName InSlot)
    -> TSharedRef<SWidget>
{
    auto Menu = FMenuBuilder{true, nullptr};
    const auto Session = _Session.Pin();
    const auto* Config = Session.IsValid() ? Session->Get_PreviewConfig() : nullptr;
    if (Config == nullptr)
    { return Menu.MakeWidget(); }
    const auto WeakChannels = TWeakPtr<SCkProceduralAction_Channels>{SharedThis(this)};
    for (const auto Limb : ck_procedural_action_channels::SubjectLimbs(*Config))
    {
        Menu.AddMenuEntry(FText::FromName(Limb), FText::GetEmpty(), FSlateIcon{}, FUIAction{FExecuteAction::CreateLambda([WeakChannels, InSlot, Limb]()
        {
            const auto Channels = WeakChannels.Pin();
            const auto Session = Channels.IsValid() ? Channels->_Session.Pin() : nullptr;
            const auto Model = Channels.IsValid() ? Channels->_Model.Pin() : nullptr;
            if (NOT Session.IsValid() || NOT Model.IsValid())
            { return; }
            auto Error = FString{};
            const auto Bound = FCk_ProceduralActionAuthoringCommands{Session->Get_Clip()}.RetargetSlot(Session->Get_PreviewConfig(), InSlot, Limb, Error);
            if (Channels->_OnResult)
            { Channels->_OnResult(Bound, Bound ? FString::Printf(TEXT("Bound %s to %s."), *InSlot.ToString(), *Limb.ToString()) : Error); }
        })});
    }
    return Menu.MakeWidget();
}

auto
    SCkProceduralAction_Channels::
    MakeContextMenu()
    -> TSharedPtr<SWidget>
{
    const auto Selected = _Tree->GetSelectedItems();
    const auto Session = _Session.Pin();
    const auto* Clip = Session.IsValid() ? Session->Get_Clip() : nullptr;
    if (Selected.IsEmpty() || Selected[0]->Kind != FItem::EKind::Slot || Clip == nullptr)
    { return nullptr; }
    const auto Source = Selected[0]->Id;
    const auto WeakChannels = TWeakPtr<SCkProceduralAction_Channels>{SharedThis(this)};
    auto Menu = FMenuBuilder{true, nullptr};
    Menu.BeginSection(NAME_None, LOCTEXT("MirrorSection", "Mirror onto"));
    for (const auto& Slot : Clip->Get_Slots())
    {
        const auto Destination = Slot.Get_Id();
        if (Destination == Source)
        { continue; }
        for (const auto Normal : {EAxis::Y, EAxis::X, EAxis::Z})
        {
            const auto Plane = Normal == EAxis::Y ? TEXT("left/right") : Normal == EAxis::X ? TEXT("front/back") : TEXT("up/down");
            Menu.AddMenuEntry(FText::FromString(FString::Printf(TEXT("%s (%s)"), *Destination.ToString(), Plane)), FText::GetEmpty(), FSlateIcon{},
                FUIAction{FExecuteAction::CreateLambda([WeakChannels, Source, Destination, Normal]()
            {
                const auto Channels = WeakChannels.Pin();
                const auto Session = Channels.IsValid() ? Channels->_Session.Pin() : nullptr;
                const auto Model = Channels.IsValid() ? Channels->_Model.Pin() : nullptr;
                if (NOT Session.IsValid() || NOT Model.IsValid())
                { return; }
                auto Error = FString{};
                const auto Mirrored = FCk_ProceduralActionAuthoringCommands{Session->Get_Clip()}.MirrorSlot(Source, Destination, Normal, Error);
                if (Channels->_OnResult)
                { Channels->_OnResult(Mirrored, Mirrored ? FString::Printf(TEXT("Mirrored %s onto %s."), *Source.ToString(), *Destination.ToString()) : Error); }
            })});
        }
    }
    Menu.EndSection();
    Menu.BeginSection(NAME_None, LOCTEXT("SpaceSection", "Track space"));
    for (const auto Space : {ECk_ProceduralAction_TrackSpace::BodyLocal, ECk_ProceduralAction_TrackSpace::RootAtStart,
        ECk_ProceduralAction_TrackSpace::LimbRestNormalized})
    {
        const auto* Slot = Clip->Get_Slots().FindByPredicate([Source](const FCk_ProceduralAction_Slot& InSlot) { return InSlot.Get_Id() == Source; });
        const auto Current = Slot != nullptr && Slot->Get_Space() == Space;
        Menu.AddMenuEntry(StaticEnum<ECk_ProceduralAction_TrackSpace>()->GetDisplayNameTextByValue(static_cast<int64>(Space)), FText::GetEmpty(),
            FSlateIcon{}, FUIAction{FExecuteAction::CreateLambda([WeakChannels, Source, Space]()
            {
                if (const auto Channels = WeakChannels.Pin())
                {
                    Channels->EditSlot([Source, Space](FCk_ProceduralActionAuthoringCommands& InCommands, FString& OutError)
                        { return InCommands.SetSlotSpace(Source, Space, OutError); },
                        FString::Printf(TEXT("%s keys its track in %s."), *Source.ToString(), *UEnum::GetValueAsString(Space)));
                }
            }), FCanExecuteAction{}, FIsActionChecked::CreateLambda([Current]() { return Current; })}, NAME_None,
            EUserInterfaceActionType::RadioButton);
    }
    Menu.EndSection();
    Menu.AddMenuEntry(LOCTEXT("DeleteSlot", "Delete slot"), FText::GetEmpty(), FSlateIcon{}, FUIAction{FExecuteAction::CreateLambda([WeakChannels, Source]()
    {
        if (const auto Channels = WeakChannels.Pin())
        {
            Channels->EditSlot([Source](FCk_ProceduralActionAuthoringCommands& InCommands, FString& OutError)
                { return InCommands.RemoveSlot(Source, OutError); }, FString::Printf(TEXT("Deleted %s."), *Source.ToString()));
        }
    })});
    return Menu.MakeWidget();
}

auto
    SCkProceduralAction_Channels::
    OnKeyDown(
        const FGeometry& InGeometry,
        const FKeyEvent& InKeyEvent)
    -> FReply
{
    const auto Slot = Get_SelectedSlot();
    if (InKeyEvent.GetKey() != EKeys::Delete || Slot.IsNone())
    { return SCompoundWidget::OnKeyDown(InGeometry, InKeyEvent); }
    EditSlot([Slot](FCk_ProceduralActionAuthoringCommands& InCommands, FString& OutError)
        { return InCommands.RemoveSlot(Slot, OutError); }, FString::Printf(TEXT("Deleted %s."), *Slot.ToString()));
    return FReply::Handled();
}

auto
    SCkProceduralAction_Channels::
    Get_SelectedSlot() const
    -> FName
{
    const auto Selected = _Tree->GetSelectedItems();
    return Selected.IsEmpty() || Selected[0]->Kind != FItem::EKind::Slot ? NAME_None : Selected[0]->Id;
}

auto
    SCkProceduralAction_Channels::
    EditSlot(
        TFunctionRef<bool(FCk_ProceduralActionAuthoringCommands&, FString&)> InEdit,
        const FString& InSuccess)
    -> bool
{
    const auto Session = _Session.Pin();
    if (NOT Session.IsValid() || Session->Get_Clip() == nullptr)
    { return false; }
    auto Commands = FCk_ProceduralActionAuthoringCommands{Session->Get_Clip()};
    auto Error = FString{};
    const auto Done = InEdit(Commands, Error);
    if (_OnResult)
    { _OnResult(Done, Done ? InSuccess : Error); }
    return Done;
}

#undef LOCTEXT_NAMESPACE
