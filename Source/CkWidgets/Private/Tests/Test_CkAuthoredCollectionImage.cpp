#include "Authoring/CkAuthoredCollectionAdapter.h"

#include "Engine/Texture2D.h"
#include "Misc/AutomationTest.h"
#include "Styling/SlateBrush.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ck_ui_authored_collection_image_test
{
    auto Record(const FString& InKey, UTexture2D* InTexture) -> FCk_UI_AuthoredRecord
    {
        FCk_UI_AuthoredFieldValue Image{TEXT("portrait"), ECk_UI_AuthoredFieldKind::Image};
        Image.Set_Texture(InTexture);
        FCk_UI_AuthoredRecord Result{InKey};
        Result.Set_Fields({Image});
        return Result;
    }

    auto Declaration(UTexture2D* InTexture) -> FCk_UI_AuthoredCollection
    {
        FCk_UI_AuthoredCollection Result{TEXT("items")};
        Result.Set_Schema({FCk_UI_AuthoredFieldSchema{TEXT("portrait"), ECk_UI_AuthoredFieldKind::Image}});
        Result.Set_Records({Record(TEXT("first"), InTexture)});
        return Result;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkAuthoredCollectionImage,
    "Ck.UiAuthoring.Widgets.CollectionImageRoundTripAndAtomicRejection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkAuthoredCollectionImage::RunTest(const FString&) -> bool
{
    using namespace ck_ui_authored_collection_image_test;
    TStrongObjectPtr<UTexture2D> First{UTexture2D::CreateTransient(8, 8)};
    TStrongObjectPtr<UTexture2D> Second{UTexture2D::CreateTransient(16, 4)};
    TStrongObjectPtr<UTexture2D> Default{NewObject<UTexture2D>()};
    if (!TestTrue(TEXT("image fixtures exist"), IsValid(First.Get()) && IsValid(Second.Get())
        && IsValid(Default.Get()))) { return false; }

    TSharedPtr<FCkUiCollection> Collection;
    TArray<FString> Errors;
    if (!TestTrue(TEXT("image schema and seed record admit"),
        ck_ui_authored_collection::TryCreate(Declaration(First.Get()), Collection, Errors)))
    { return false; }
    const TSharedPtr<const FCkUiRecord> Original = Collection->FindRecord(TEXT("first"));
    const FCkUiFieldValue* OriginalImage = Original.IsValid() ? Original->FindField(TEXT("portrait")) : nullptr;
    if (!TestTrue(TEXT("native record owns image brush"), OriginalImage != nullptr
        && OriginalImage->Kind == ECkUiFieldKind::Image && OriginalImage->Image.IsValid()
        && OriginalImage->Image->GetResourceObject() == First.Get())) { return false; }

    TArray<FCk_UI_AuthoredRecord> Reflected;
    if (!TestTrue(TEXT("native image reads back through reflected field"),
        ck_ui_authored_collection::TryReadRecords(*Collection, Reflected))) { return false; }
    TestTrue(TEXT("round trip retains image kind and texture"), Reflected.Num() == 1
        && Reflected[0].Get_Fields().Num() == 1
        && Reflected[0].Get_Fields()[0].Get_Kind() == ECk_UI_AuthoredFieldKind::Image
        && Reflected[0].Get_Fields()[0].Get_Texture() == First.Get());

    const int64 OriginalRevision = Collection->GetRevision();
    TArray<FCkUiRecordData> SameRecords;
    Errors.Reset();
    if (!TestTrue(TEXT("unchanged image converts with its accepted brush"),
        ck_ui_authored_collection::TryConvertRecords({Record(TEXT("first"), First.Get())},
            SameRecords, Errors, Collection.Get()))) { return false; }
    if (!TestTrue(TEXT("unchanged image does not revise collection"),
        Collection->TrySetRecords(MoveTemp(SameRecords)).Succeeded
        && Collection->GetRevision() == OriginalRevision)) { return false; }

    AddExpectedError(TEXT("Authored collection image requires a valid texture."),
        EAutomationExpectedErrorFlags::Contains, -1);
    TArray<FCkUiRecordData> RejectedRecords;
    Errors.Reset();
    TestFalse(TEXT("null image rejects candidate"),
        ck_ui_authored_collection::TryConvertRecords({Record(TEXT("first"), Second.Get()),
            Record(TEXT("second"), nullptr)}, RejectedRecords, Errors, Collection.Get()));
    TestTrue(TEXT("null rejection returns no partial candidate"), RejectedRecords.IsEmpty() && !Errors.IsEmpty());

    AddExpectedError(TEXT("Authored collection image requires positive texture dimensions."),
        EAutomationExpectedErrorFlags::Contains, -1);
    Errors.Reset();
    TestFalse(TEXT("default zero-size texture rejects candidate"),
        ck_ui_authored_collection::TryConvertRecords({Record(TEXT("first"), Second.Get()),
            Record(TEXT("second"), Default.Get())}, RejectedRecords, Errors, Collection.Get()));
    TestTrue(TEXT("dimension rejection returns no partial candidate"), RejectedRecords.IsEmpty() && !Errors.IsEmpty());

    TestTrue(TEXT("rejections preserve published record, brush and revision"),
        Collection->GetRevision() == OriginalRevision && Collection->GetRecords().Num() == 1
        && Collection->FindRecord(TEXT("first")) == Original
        && Collection->FindRecord(TEXT("first"))->FindField(TEXT("portrait"))->Image == OriginalImage->Image);
    return true;
}

#endif
