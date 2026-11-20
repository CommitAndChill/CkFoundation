#include "CkHexGrid_Spec.h"

#include "CkGrid/HexGridSystem/Grid/CkHexGrid_Utils.h"
#include "CkGrid/HexGridSystem/Geometry/CkHexGrid_Geometry_Utils.h"
#include <Misc/SecureHash.h>

#if WITH_EDITOR
auto
    UCk_HexGrid_AuthoringSpec::
    EditorOnly_TryReplaceSpec(
        const FCk_HexGrid_Spec& InSpec,
        ECk_HexBoardFailure& OutFailure)
    -> bool
{
    if (!UCk_Utils_HexGrid_UE::TryValidateSpec(InSpec, OutFailure)) { return false; }
    _Spec = InSpec;
    return true;
}
#endif

namespace ck_hex_recipe
{
    auto CoordinateLess(const FCk_HexCoord& A, const FCk_HexCoord& B) -> bool
    { return A.Get_R() == B.Get_R() ? A.Get_Q() < B.Get_Q() : A.Get_R() < B.Get_R(); }

    struct FEncoder
    {
        TArray<uint8> Bytes;

        void Integer(uint32 Value)
        {
            for (int32 Shift = 0; Shift < 32; Shift += 8) { Bytes.Add(static_cast<uint8>(Value >> Shift)); }
        }
        void Number(double Value)
        {
            uint64 Bits = 0;
            if (Value != 0.0) { FMemory::Memcpy(&Bits, &Value, sizeof(Bits)); }
            Integer(static_cast<uint32>(Bits));
            Integer(static_cast<uint32>(Bits >> 32));
        }
        void String(const FString& Value)
        {
            const FTCHARToUTF8 Utf8{*Value};
            Integer(Utf8.Length());
            Bytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
        }
        void Coord(const FCk_HexCoord& Value)
        { Integer(Value.Get_Q()); Integer(Value.Get_R()); }
        void Coords(TArray<FCk_HexCoord> Values)
        {
            Values.Sort(CoordinateLess);
            Integer(Values.Num());
            for (const auto& Value : Values) { Coord(Value); }
        }
        void Tags(const FGameplayTagContainer& Values)
        {
            TArray<FString> Names;
            for (const auto& Tag : Values) { Names.Add(Tag.ToString()); }
            Names.Sort([](const FString& A, const FString& B) { return A.Compare(B, ESearchCase::CaseSensitive) < 0; });
            Integer(Names.Num());
            for (const auto& Name : Names) { String(Name); }
        }
    };
}

bool ck::hex_grid::TryEncodeRecipe(const FCk_HexGrid_Spec& InSpec, TArray<uint8>& OutBytes, ECk_HexBoardFailure& OutFailure)
{
    OutBytes.Reset();
    if (!UCk_Utils_HexGrid_UE::TryValidateSpec(InSpec, OutFailure)) { return false; }
    TArray<FCk_HexCoord> Domain;
    auto GeometryFailure = ECk_HexGeometryFailure::None;
    if (!UCk_Utils_HexGeometry_UE::TryResolve_Domain(InSpec.Get_Domain(), Domain, GeometryFailure))
    { OutFailure = ECk_HexBoardFailure::InvalidSpec; return false; }

    ck_hex_recipe::FEncoder Out;
    Out.Bytes = {'C','K','H','E','X', 1, static_cast<uint8>(InSpec.Get_Layout().Get_Orientation()), 1};
    Out.Number(InSpec.Get_Layout().Get_OuterRadius());
    const auto& Pivot = InSpec.Get_Pivot();
    const auto Pos = Pivot.GetTranslation();
    auto Rot = Pivot.GetRotation().GetNormalized();
    // A quaternion and its negation encode the same orientation.
    if (Rot.W < 0 || (Rot.W == 0 && (Rot.X < 0 || (Rot.X == 0 && (Rot.Y < 0 || (Rot.Y == 0 && Rot.Z < 0))))))
    { Rot = Rot * -1.0; }
    const auto Scale = Pivot.GetScale3D();
    for (double Component : {Pos.X, Pos.Y, Pos.Z, Rot.X, Rot.Y, Rot.Z, Rot.W, Scale.X, Scale.Y, Scale.Z})
    { Out.Number(Component); }
    Out.Coords(MoveTemp(Domain));
    Out.Tags(InSpec.Get_DefaultCellTags());
    Out.Coords(InSpec.Get_InitialDisabledCells());
    auto Overrides = InSpec.Get_PerCellTags();
    Overrides.Sort([](const auto& A, const auto& B)
    { return ck_hex_recipe::CoordinateLess(A.Get_Coordinate(), B.Get_Coordinate()); });
    Out.Integer(Overrides.Num());
    for (const auto& Entry : Overrides) { Out.Coord(Entry.Get_Coordinate()); Out.Tags(Entry.Get_Tags()); }

    TArray<TArray<uint8>> Declarations;
    for (const auto& Blocker : InSpec.Get_Blockers())
    {
        ck_hex_recipe::FEncoder Declaration;
        Declaration.String(Blocker.Get_Name().IsValid() ? Blocker.Get_Name().ToString() : FString{});
        Declaration.Coords(Blocker.Get_Cells());
        Declaration.Bytes.Add(Blocker.Get_InitiallyActive() ? 1 : 0);
        Declarations.Add(MoveTemp(Declaration.Bytes));
    }
    Declarations.Sort([](const auto& A, const auto& B)
    {
        const auto Order = FMemory::Memcmp(A.GetData(), B.GetData(), FMath::Min(A.Num(), B.Num()));
        return Order == 0 ? A.Num() < B.Num() : Order < 0;
    });
    Out.Integer(Declarations.Num());
    for (const auto& Declaration : Declarations) { Out.Bytes.Append(Declaration); }
    OutBytes = MoveTemp(Out.Bytes);
    return true;
}

auto
    UCk_HexGrid_AuthoringSpec::
    TryResolve(
        FCk_HexGrid_Spec& OutSpec,
        ECk_HexBoardFailure& OutFailure) const
    -> bool
{
    auto Identity = FCk_HexGridRecipeIdentity{};
    return TryResolveWithIdentity(OutSpec, Identity, OutFailure);
}

bool UCk_HexGrid_AuthoringSpec::TryResolveWithIdentity(FCk_HexGrid_Spec& OutSpec,
    FCk_HexGridRecipeIdentity& OutIdentity, ECk_HexBoardFailure& OutFailure) const
{
    OutSpec = {};
    OutIdentity = {};
    OutFailure = ECk_HexBoardFailure::InvalidSpec;
    TArray<uint8> Bytes;
    if (_RecipeRevision < 1 || !ck::hex_grid::TryEncodeRecipe(_Spec, Bytes, OutFailure)) { return false; }
    OutIdentity.Set_AssetPath(IsAsset() ? GetPathName() : FString{}).Set_RecipeRevision(_RecipeRevision)
        .Set_Fingerprint(FSHA1::HashBuffer(Bytes.GetData(), Bytes.Num()).ToString().ToLower());
    OutSpec = _Spec;
    return true;
}
