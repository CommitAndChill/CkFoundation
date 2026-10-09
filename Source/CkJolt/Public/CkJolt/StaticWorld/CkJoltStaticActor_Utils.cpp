#include "CkJoltStaticActor_Utils.h"

#include "CkJolt/StaticWorld/CkJoltStaticActor_Fragment.h"

#include "CkCore/Ensure/CkEnsure.h"

#include <GameFramework/Actor.h>
#include <PhysicalMaterials/PhysicalMaterial.h>

// --------------------------------------------------------------------------------------------------------------------

CK_DEFINE_HAS_CAST_CONV_HANDLE_TYPESAFE(UCk_Utils_JoltStaticActor_UE, FCk_Handle_JoltStaticActor, ck::FFragment_JoltStaticActor);

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_JoltStaticActor_UE::
    Get_SourceActor(
        const FCk_Handle_JoltStaticActor& InJoltStaticActor)
    -> AActor*
{
    return const_cast<AActor*>(InJoltStaticActor.Get<ck::FFragment_JoltStaticActor>().Get_SourceActor().Get());
}

auto
    UCk_Utils_JoltStaticActor_UE::
    Get_SourceActorName(
        const FCk_Handle_JoltStaticActor& InJoltStaticActor)
    -> FName
{
    return InJoltStaticActor.Get<ck::FFragment_JoltStaticActor>().Get_SourceActorName();
}

auto
    UCk_Utils_JoltStaticActor_UE::
    Get_NumBodies(
        const FCk_Handle_JoltStaticActor& InJoltStaticActor)
    -> int32
{
    return InJoltStaticActor.Get<ck::FFragment_JoltStaticActor>().Get_BodyIds().Num();
}

auto
    UCk_Utils_JoltStaticActor_UE::
    Get_BodyPhysicalMaterial(
        const FCk_Handle_JoltStaticActor& InJoltStaticActor,
        uint32 InBodyIndexAndSequence)
    -> UPhysicalMaterial*
{
    const auto& Fragment = InJoltStaticActor.Get<ck::FFragment_JoltStaticActor>();

    const auto ArraysAreParallel = Fragment.Get_BodyIds().Num() == Fragment.Get_BodyPhysicalMaterials().Num();
    CK_ENSURE_IF_NOT(ArraysAreParallel,
        TEXT("JoltStaticActor [{}] holds [{}] body ids but [{}] body phys mats"),
        InJoltStaticActor, Fragment.Get_BodyIds().Num(), Fragment.Get_BodyPhysicalMaterials().Num())
    { return nullptr; }

    const auto BodyIndex = Fragment.Get_BodyIds().Find(InBodyIndexAndSequence);
    CK_ENSURE_IF_NOT(BodyIndex != INDEX_NONE,
        TEXT("Body [{}] is not one of JoltStaticActor [{}]'s baked bodies"), InBodyIndexAndSequence, InJoltStaticActor)
    { return nullptr; }

    return Fragment.Get_BodyPhysicalMaterials()[BodyIndex].Get();
}

// --------------------------------------------------------------------------------------------------------------------
