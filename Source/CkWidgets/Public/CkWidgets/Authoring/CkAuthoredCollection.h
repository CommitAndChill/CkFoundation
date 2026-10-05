#pragma once

#include "CkCore/Macros/CkMacros.h"
#include "CoreMinimal.h"

#include "CkAuthoredCollection.generated.h"

/** Supported flat collection values; admission rejects unknown enum values. */
UENUM(BlueprintType)
enum class ECk_UI_AuthoredFieldKind : uint8
{
    Text,
    Number,
    Bool,
    Integer,
    Color
};

USTRUCT(BlueprintType)
struct CKWIDGETS_API FCk_UI_AuthoredFieldSchema
{
    GENERATED_BODY()
public:
    CK_GENERATED_BODY(FCk_UI_AuthoredFieldSchema);
private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ck|UI|Authoring", meta = (AllowPrivateAccess = true))
    FString _Name;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ck|UI|Authoring", meta = (AllowPrivateAccess = true))
    ECk_UI_AuthoredFieldKind _Kind = ECk_UI_AuthoredFieldKind::Text;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ck|UI|Authoring", meta = (AllowPrivateAccess = true))
    bool _Required = true;
public:
    CK_PROPERTY(_Name);
    CK_PROPERTY(_Kind);
    CK_PROPERTY(_Required);
    CK_DEFINE_CONSTRUCTORS(FCk_UI_AuthoredFieldSchema, _Name, _Kind);
};

USTRUCT(BlueprintType)
struct CKWIDGETS_API FCk_UI_AuthoredFieldValue
{
    GENERATED_BODY()
public:
    CK_GENERATED_BODY(FCk_UI_AuthoredFieldValue);
private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ck|UI|Authoring", meta = (AllowPrivateAccess = true))
    FString _Name;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ck|UI|Authoring", meta = (AllowPrivateAccess = true))
    ECk_UI_AuthoredFieldKind _Kind = ECk_UI_AuthoredFieldKind::Text;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ck|UI|Authoring", meta = (AllowPrivateAccess = true))
    FText _Text;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ck|UI|Authoring", meta = (AllowPrivateAccess = true))
    float _Number = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ck|UI|Authoring", meta = (AllowPrivateAccess = true))
    bool _Bool = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ck|UI|Authoring", meta = (AllowPrivateAccess = true))
    int32 _Integer = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ck|UI|Authoring", meta = (AllowPrivateAccess = true))
    FLinearColor _Color = FLinearColor::White;
public:
    CK_PROPERTY(_Name);
    CK_PROPERTY(_Kind);
    CK_PROPERTY(_Text);
    CK_PROPERTY(_Number);
    CK_PROPERTY(_Bool);
    CK_PROPERTY(_Integer);
    CK_PROPERTY(_Color);
    CK_DEFINE_CONSTRUCTORS(FCk_UI_AuthoredFieldValue, _Name, _Kind);
};

USTRUCT(BlueprintType)
struct CKWIDGETS_API FCk_UI_AuthoredRecord
{
    GENERATED_BODY()
public:
    CK_GENERATED_BODY(FCk_UI_AuthoredRecord);
private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ck|UI|Authoring", meta = (AllowPrivateAccess = true))
    FString _Key;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ck|UI|Authoring", meta = (AllowPrivateAccess = true))
    TArray<FCk_UI_AuthoredFieldValue> _Fields;
public:
    CK_PROPERTY(_Key);
    CK_PROPERTY(_Fields);
    CK_DEFINE_CONSTRUCTORS(FCk_UI_AuthoredRecord, _Key);
};

USTRUCT(BlueprintType)
struct CKWIDGETS_API FCk_UI_AuthoredCollection
{
    GENERATED_BODY()
public:
    CK_GENERATED_BODY(FCk_UI_AuthoredCollection);
private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ck|UI|Authoring", meta = (AllowPrivateAccess = true))
    FString _Name;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ck|UI|Authoring", meta = (AllowPrivateAccess = true))
    TArray<FCk_UI_AuthoredFieldSchema> _Schema;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ck|UI|Authoring", meta = (AllowPrivateAccess = true))
    TArray<FCk_UI_AuthoredRecord> _Records;
public:
    CK_PROPERTY(_Name);
    CK_PROPERTY(_Schema);
    CK_PROPERTY(_Records);
    CK_DEFINE_CONSTRUCTORS(FCk_UI_AuthoredCollection, _Name);
};
