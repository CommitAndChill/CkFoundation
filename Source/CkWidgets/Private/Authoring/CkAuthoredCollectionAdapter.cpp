#include "CkAuthoredCollectionAdapter.h"
#include "CkCore/Ensure/CkEnsure.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"
#include "UObject/StrongObjectPtr.h"

auto FCk_UI_AuthoredFieldValue::Get_Texture() const -> UTexture2D*
{
    return _Texture;
}

auto FCk_UI_AuthoredFieldValue::Set_Texture(UTexture2D* InValue) -> ThisType&
{
    _Texture = InValue;
    return *this;
}

namespace ck_ui_authored_collection
{
    namespace
    {
        /** Native records own the brush; the brush keeps its texture alive even after session release. */
        class FCkAuthoredRecordImageBrush final : public FSlateBrush
        {
        public:
            explicit FCkAuthoredRecordImageBrush(UTexture2D* InTexture) : Texture(InTexture)
            {
                DrawAs = ESlateBrushDrawType::Image;
                SetResourceObject(InTexture);
                ImageSize = FVector2D(InTexture->GetSizeX(), InTexture->GetSizeY());
            }

        private:
            TStrongObjectPtr<UTexture2D> Texture;
        };

        auto ValidTexture(UTexture2D* InTexture) -> bool
        {
            const bool TextureValid = IsValid(InTexture);
            CK_ENSURE_IF_NOT(TextureValid, TEXT("Authored collection image requires a valid texture.")) {}
            if (!TextureValid) { return false; }
            const bool DimensionsValid = InTexture->GetSizeX() > 0 && InTexture->GetSizeY() > 0;
            CK_ENSURE_IF_NOT(DimensionsValid, TEXT("Authored collection image requires positive texture dimensions.")) {}
            return DimensionsValid;
        }

        auto TryNativeKind(ECk_UI_AuthoredFieldKind InKind, ECkUiFieldKind& OutKind) -> bool
        {
            switch (InKind)
            {
            case ECk_UI_AuthoredFieldKind::Text: OutKind = ECkUiFieldKind::Text; return true;
            case ECk_UI_AuthoredFieldKind::Number: OutKind = ECkUiFieldKind::Number; return true;
            case ECk_UI_AuthoredFieldKind::Bool: OutKind = ECkUiFieldKind::Bool; return true;
            case ECk_UI_AuthoredFieldKind::Integer: OutKind = ECkUiFieldKind::Integer; return true;
            case ECk_UI_AuthoredFieldKind::Color: OutKind = ECkUiFieldKind::Color; return true;
            case ECk_UI_AuthoredFieldKind::Image: OutKind = ECkUiFieldKind::Image; return true;
            default: return false;
            }
        }

        auto TryReflectedValue(const FString& InName, const FCkUiFieldValue& InValue,
            FCk_UI_AuthoredFieldValue& OutValue) -> bool
        {
            ECk_UI_AuthoredFieldKind Kind;
            switch (InValue.Kind)
            {
            case ECkUiFieldKind::Text: Kind = ECk_UI_AuthoredFieldKind::Text; break;
            case ECkUiFieldKind::Number: Kind = ECk_UI_AuthoredFieldKind::Number; break;
            case ECkUiFieldKind::Bool: Kind = ECk_UI_AuthoredFieldKind::Bool; break;
            case ECkUiFieldKind::Integer: Kind = ECk_UI_AuthoredFieldKind::Integer; break;
            case ECkUiFieldKind::Color: Kind = ECk_UI_AuthoredFieldKind::Color; break;
            case ECkUiFieldKind::Image: Kind = ECk_UI_AuthoredFieldKind::Image; break;
            default: return false;
            }
            FCk_UI_AuthoredFieldValue Candidate{InName, Kind};
            switch (Kind)
            {
            case ECk_UI_AuthoredFieldKind::Text: Candidate.Set_Text(InValue.Text); break;
            case ECk_UI_AuthoredFieldKind::Number: Candidate.Set_Number(InValue.Number); break;
            case ECk_UI_AuthoredFieldKind::Bool: Candidate.Set_Bool(InValue.Bool); break;
            case ECk_UI_AuthoredFieldKind::Integer: Candidate.Set_Integer(InValue.Integer); break;
            case ECk_UI_AuthoredFieldKind::Color: Candidate.Set_Color(InValue.Color); break;
            case ECk_UI_AuthoredFieldKind::Image:
            {
                UTexture2D* Texture = InValue.Image.IsValid()
                    ? Cast<UTexture2D>(InValue.Image->GetResourceObject()) : nullptr;
                if (!ValidTexture(Texture)) { return false; }
                Candidate.Set_Texture(Texture);
                break;
            }
            default: return false;
            }
            OutValue = MoveTemp(Candidate);
            return true;
        }
    }

    auto TryConvertRecords(const TArray<FCk_UI_AuthoredRecord>& InRecords, TArray<FCkUiRecordData>& OutRecords,
        TArray<FString>& OutErrors, const FCkUiCollection* InPrevious) -> bool
    {
        TArray<FCkUiRecordData> Candidate;
        Candidate.Reserve(InRecords.Num());
        for (const FCk_UI_AuthoredRecord& Record : InRecords)
        {
            FCkUiRecordData NativeRecord;
            NativeRecord.Key = Record.Get_Key();
            for (const FCk_UI_AuthoredFieldValue& Field : Record.Get_Fields())
            {
                if (NativeRecord.Fields.Contains(Field.Get_Name()))
                {
                    OutErrors.Add(TEXT("Authored collection record contains a duplicate field name."));
                    return false;
                }
                FCkUiFieldValue Value;
                if (!TryNativeKind(Field.Get_Kind(), Value.Kind))
                {
                    OutErrors.Add(TEXT("Authored collection record contains an unsupported field kind."));
                    return false;
                }
                switch (Field.Get_Kind())
                {
                case ECk_UI_AuthoredFieldKind::Text: Value.Text = Field.Get_Text(); break;
                case ECk_UI_AuthoredFieldKind::Number: Value.Number = Field.Get_Number(); break;
                case ECk_UI_AuthoredFieldKind::Bool: Value.Bool = Field.Get_Bool(); break;
                case ECk_UI_AuthoredFieldKind::Integer: Value.Integer = Field.Get_Integer(); break;
                case ECk_UI_AuthoredFieldKind::Color: Value.Color = Field.Get_Color(); break;
                case ECk_UI_AuthoredFieldKind::Image:
                {
                    UTexture2D* Texture = Field.Get_Texture();
                    if (!ValidTexture(Texture))
                    {
                        OutErrors.Add(FString::Printf(TEXT("Authored collection record '%s' has an invalid image field '%s'."),
                            *Record.Get_Key(), *Field.Get_Name()));
                        return false;
                    }
                    const TSharedPtr<const FCkUiRecord> PreviousRecord = InPrevious != nullptr
                        ? InPrevious->FindRecord(Record.Get_Key()) : nullptr;
                    const FCkUiFieldValue* PreviousField = PreviousRecord.IsValid()
                        ? PreviousRecord->FindField(Field.Get_Name()) : nullptr;
                    if (PreviousField != nullptr && PreviousField->Kind == ECkUiFieldKind::Image
                        && PreviousField->Image.IsValid()
                        && PreviousField->Image->GetResourceObject() == Texture)
                    { Value.Image = PreviousField->Image; }
                    else
                    { Value.Image = MakeShared<FCkAuthoredRecordImageBrush>(Texture); }
                    break;
                }
                default: return false;
                }
                NativeRecord.Fields.Add(Field.Get_Name(), MoveTemp(Value));
            }
            Candidate.Add(MoveTemp(NativeRecord));
        }
        OutRecords = MoveTemp(Candidate);
        return true;
    }

    auto TryCreate(const FCk_UI_AuthoredCollection& InDeclaration, TSharedPtr<FCkUiCollection>& OutCollection,
        TArray<FString>& OutErrors) -> bool
    {
        TArray<FCkUiFieldSchema> Schema;
        for (const FCk_UI_AuthoredFieldSchema& Field : InDeclaration.Get_Schema())
        {
            ECkUiFieldKind Kind;
            if (!TryNativeKind(Field.Get_Kind(), Kind))
            {
                OutErrors.Add(TEXT("Authored collection schema contains an unsupported field kind."));
                return false;
            }
            Schema.Add({Field.Get_Name(), Kind, Field.Get_Required()});
        }
        TSharedPtr<FCkUiCollection> Candidate;
        const FCkUiLoadResult Created = FCkUiCollection::TryCreate(MoveTemp(Schema), Candidate);
        if (!Created.Succeeded) { OutErrors.Append(Created.Errors); return false; }
        TArray<FCkUiRecordData> Records;
        if (!TryConvertRecords(InDeclaration.Get_Records(), Records, OutErrors)) { return false; }
        const FCkUiLoadResult Seeded = Candidate->TrySetRecords(MoveTemp(Records));
        if (!Seeded.Succeeded) { OutErrors.Append(Seeded.Errors); return false; }
        OutCollection = MoveTemp(Candidate);
        return true;
    }

    auto TryReadRecords(const FCkUiCollection& InCollection, TArray<FCk_UI_AuthoredRecord>& OutRecords) -> bool
    {
        TArray<FCk_UI_AuthoredRecord> Candidate;
        Candidate.Reserve(InCollection.GetRecords().Num());
        for (const TSharedPtr<const FCkUiRecord>& Record : InCollection.GetRecords())
        {
            if (!Record.IsValid()) { return false; }
            FCk_UI_AuthoredRecord Reflected{Record->GetKey()};
            TArray<FCk_UI_AuthoredFieldValue> Fields;
            for (const FCkUiFieldSchema& Schema : InCollection.GetSchema())
            {
                const FCkUiFieldValue* Value = Record->FindField(Schema.Name);
                if (Value == nullptr) { continue; }
                FCk_UI_AuthoredFieldValue Field;
                if (!TryReflectedValue(Schema.Name, *Value, Field)) { return false; }
                Fields.Add(MoveTemp(Field));
            }
            Reflected.Set_Fields(MoveTemp(Fields));
            Candidate.Add(MoveTemp(Reflected));
        }
        OutRecords = MoveTemp(Candidate);
        return true;
    }
}
