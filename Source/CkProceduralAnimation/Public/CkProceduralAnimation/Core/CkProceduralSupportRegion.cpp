#include "CkProceduralAnimation/Core/CkProceduralSupportRegion.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck::procedural_support::detail
{
    constexpr auto MaximumContacts = 64;
    constexpr auto GeometryToleranceCm = 1.0e-4;

    auto
        IsFinite(
            const FVector& InValue)
        -> bool
    {
        return FMath::IsFinite(InValue.X) && FMath::IsFinite(InValue.Y) && FMath::IsFinite(InValue.Z);
    }

    auto
        IsUnit(
            const FVector& InValue)
        -> bool
    {
        return IsFinite(InValue) && FMath::IsNearlyEqual(InValue.SizeSquared(), 1.0, 1.0e-4);
    }

    auto
        Cross(
            const FVector2D& InA,
            const FVector2D& InB)
        -> double
    {
        return InA.X * InB.Y - InA.Y * InB.X;
    }
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck::procedural_support
{
    auto
        ValidatePolicy(
            const FCk_ProceduralLimbControl_Support& InPolicy)
        -> bool
    {
        if (InPolicy.Get_Mode() == ECk_ProceduralLimbControl_SupportMode::Disabled)
        { return true; }

        return (InPolicy.Get_Mode() == ECk_ProceduralLimbControl_SupportMode::CountOnly
                || InPolicy.Get_Mode() == ECk_ProceduralLimbControl_SupportMode::ProjectedSupportRegion)
            && InPolicy.Get_MinimumTrustedContacts() > 0
            && InPolicy.Get_MinimumTrustedContacts() <= detail::MaximumContacts
            && FMath::IsFinite(InPolicy.Get_ProjectedRegionInsetCm())
            && InPolicy.Get_ProjectedRegionInsetCm() >= 0.0f
            && FMath::IsFinite(InPolicy.Get_MaxContactNormalSpreadDegrees())
            && InPolicy.Get_MaxContactNormalSpreadDegrees() >= 0.0f
            && InPolicy.Get_MaxContactNormalSpreadDegrees() < 90.0f
            && FMath::IsFinite(InPolicy.Get_SupportHoldTimeout().Get_Seconds())
            && InPolicy.Get_SupportHoldTimeout() > FCk_Time{};
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Evaluate(
            const FCk_ProceduralLimbControl_Support& InPolicy,
            TArrayView<const FContact> InContacts,
            const FVector& InRootProxy,
            const FVector& InSupportUp,
            TArray<FVector>* OutProjectedRegion)
        -> FObservation
    {
        if (OutProjectedRegion != nullptr) { OutProjectedRegion->Reset(); }
        if (NOT ValidatePolicy(InPolicy))
        { return FObservation{EResult::InvalidPolicy, 0, 0.0}; }
        if (InPolicy.Get_Mode() == ECk_ProceduralLimbControl_SupportMode::Disabled)
        { return FObservation{EResult::Disabled, 0, 0.0}; }
        if (NOT detail::IsFinite(InRootProxy) || NOT detail::IsUnit(InSupportUp))
        { return FObservation{EResult::InvalidFrame, 0, 0.0}; }
        if (InContacts.Num() > detail::MaximumContacts)
        { return FObservation{EResult::InvalidContact, 0, 0.0}; }

        auto Contacts = TArray<const FContact*, TInlineAllocator<detail::MaximumContacts>>{};
        for (const auto& Contact : InContacts)
        {
            if (NOT Contact.Get_Trusted())
            { continue; }
            if (NOT detail::IsFinite(Contact.Get_Position()) || NOT detail::IsUnit(Contact.Get_Normal()))
            { return FObservation{EResult::InvalidContact, Contacts.Num(), 0.0}; }
            Contacts.Add(&Contact);
        }
        const auto Count = Contacts.Num();
        if (Count < InPolicy.Get_MinimumTrustedContacts())
        { return FObservation{EResult::InsufficientContacts, Count, 0.0}; }

        const auto MinimumDot = FMath::Cos(FMath::DegreesToRadians(
            static_cast<double>(InPolicy.Get_MaxContactNormalSpreadDegrees())));
        for (auto Index = 0; Index < Count; ++Index)
        {
            if (FVector::DotProduct(Contacts[Index]->Get_Normal(), InSupportUp)
                + UE_DOUBLE_SMALL_NUMBER < MinimumDot)
            { return FObservation{EResult::IncoherentNormals, Count, 0.0}; }
            for (auto Other = 0; Other < Index; ++Other)
            {
                if (FVector::DotProduct(Contacts[Index]->Get_Normal(), Contacts[Other]->Get_Normal())
                    + UE_DOUBLE_SMALL_NUMBER < MinimumDot)
                { return FObservation{EResult::IncoherentNormals, Count, 0.0}; }
            }
        }
        if (InPolicy.Get_Mode() == ECk_ProceduralLimbControl_SupportMode::CountOnly)
        { return FObservation{EResult::Accepted, Count, 0.0}; }

        auto AxisX = FVector::ZeroVector;
        auto AxisY = FVector::ZeroVector;
        InSupportUp.FindBestAxisVectors(AxisX, AxisY);
        auto Points = TArray<FVector2D, TInlineAllocator<detail::MaximumContacts>>{};
        for (const auto* Contact : Contacts)
        {
            const auto Relative = Contact->Get_Position() - InRootProxy;
            const auto Point = FVector2D{FVector::DotProduct(Relative, AxisX), FVector::DotProduct(Relative, AxisY)};
            if (NOT FMath::IsFinite(Point.X) || NOT FMath::IsFinite(Point.Y))
            { return FObservation{EResult::InvalidContact, Count, 0.0}; }
            Points.Add(Point);
        }
        Points.Sort([](const FVector2D& InA, const FVector2D& InB)
        { return InA.X < InB.X || (InA.X == InB.X && InA.Y < InB.Y); });

        auto Unique = TArray<FVector2D, TInlineAllocator<detail::MaximumContacts>>{};
        for (const auto& Point : Points)
        {
            if (Unique.IsEmpty() || NOT Point.Equals(Unique.Last(), detail::GeometryToleranceCm))
            { Unique.Add(Point); }
        }
        if (Unique.Num() < 3)
        { return FObservation{EResult::DegenerateRegion, Count, 0.0}; }

        // Andrew's monotone chain; at most 64 contacts, with no per-call heap allocation.
        auto Hull = TArray<FVector2D, TInlineAllocator<detail::MaximumContacts * 2>>{};
        for (const auto& Point : Unique)
        {
            while (Hull.Num() >= 2 && detail::Cross(Hull.Last() - Hull[Hull.Num() - 2], Point - Hull.Last()) <= 0.0)
            { Hull.Pop(EAllowShrinking::No); }
            Hull.Add(Point);
        }
        const auto LowerCount = Hull.Num();
        for (auto Index = Unique.Num() - 2; Index >= 0; --Index)
        {
            const auto& Point = Unique[Index];
            while (Hull.Num() > LowerCount && detail::Cross(Hull.Last() - Hull[Hull.Num() - 2], Point - Hull.Last()) <= 0.0)
            { Hull.Pop(EAllowShrinking::No); }
            Hull.Add(Point);
        }
        Hull.Pop(EAllowShrinking::No);
        if (Hull.Num() < 3)
        { return FObservation{EResult::DegenerateRegion, Count, 0.0}; }

        auto MinimumDistance = TNumericLimits<double>::Max();
        for (auto Index = 0; Index < Hull.Num(); ++Index)
        {
            const auto& Start = Hull[Index];
            const auto Edge = Hull[(Index + 1) % Hull.Num()] - Start;
            const auto Length = Edge.Size();
            if (NOT FMath::IsFinite(Length) || Length <= detail::GeometryToleranceCm)
            { return FObservation{EResult::DegenerateRegion, Count, 0.0}; }
            const auto Distance = detail::Cross(Edge, -Start) / Length;
            if (NOT FMath::IsFinite(Distance))
            { return FObservation{EResult::DegenerateRegion, Count, 0.0}; }
            MinimumDistance = FMath::Min(MinimumDistance, Distance);
        }
        if (OutProjectedRegion != nullptr)
        {
            auto Vertices = TArray<FVector, TInlineAllocator<detail::MaximumContacts>>{};
            for (const auto& Point : Hull)
            {
                const auto Vertex = InRootProxy + AxisX * Point.X + AxisY * Point.Y;
                if (Vertex.ContainsNaN()) { return FObservation{EResult::InvalidContact, Count, 0.0}; }
                Vertices.Add(Vertex);
            }
            OutProjectedRegion->Append(Vertices);
        }
        return FObservation{
            MinimumDistance + detail::GeometryToleranceCm >= InPolicy.Get_ProjectedRegionInsetCm()
                ? EResult::Accepted : EResult::OutsideRegion,
            Count, MinimumDistance};
    }
}

// --------------------------------------------------------------------------------------------------------------------
