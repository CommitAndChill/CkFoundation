namespace utils_physics
{
    FGameplayTagContainer Get_PhysicalMaterialTagsFromHitResult(FHitResult InHitResult)
    {
        auto Obj = Cast<UCk_PhysicalMaterialWithTags>(InHitResult.PhysMaterial);

        if (ck::Is_NOT_Valid(Obj))
        { return FGameplayTagContainer(); }

        return Obj.Get_Tags();
    }

    // Empty for a null phys mat or one that is not a UCk_PhysicalMaterialWithTags.
    FGameplayTagContainer Get_PhysicalMaterialTags(UPhysicalMaterial InPhysicalMaterial)
    {
        auto Obj = Cast<UCk_PhysicalMaterialWithTags>(InPhysicalMaterial);

        if (ck::Is_NOT_Valid(Obj))
        { return FGameplayTagContainer(); }

        return Obj.Get_Tags();
    }
}