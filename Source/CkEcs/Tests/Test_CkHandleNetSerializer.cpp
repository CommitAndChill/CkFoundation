#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Handle/CkHandle.h"
#include "CkEcs/World/CkEcsWorld.h"

#include "Misc/AutomationTest.h"
#include "Misc/EngineVersionComparison.h"

// This fixture uses the UE 5.8 private reference type; keep UE 5.7 builds independent of it.
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR && !UE_VERSION_OLDER_THAN(5, 8, 0)

#include "Iris/ReplicationSystem/ObjectReferenceCache.h"
#include "Iris/Serialization/InternalNetSerializationContext.h"
#include "Iris/Serialization/NetBitStreamReader.h"
#include "Iris/Serialization/NetBitStreamUtil.h"
#include "Iris/Serialization/NetBitStreamWriter.h"
#include "Iris/Serialization/NetSerializationContext.h"
#include "Iris/Serialization/ObjectNetSerializer.h"
#include "Iris/Serialization/QuantizedObjectReference.h"
#include "StructUtils/InstancedStruct.h"

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_HandleNetSerializer_NullClearsReusedTarget,
    "Ck.Ecs.HandleNetSerializer.NullClearsReusedTarget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkTest_HandleNetSerializer_NullClearsReusedTarget::RunTest(
        const FString&)
    -> bool
{
    using namespace UE::Net;

    auto World = ck::FEcsWorld{};
    auto& Registry = World.Get_Registry();
    const auto PreviousHandle = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(Registry);
    auto Target = FInstancedStruct::Make<FCk_Handle>(PreviousHandle);
    TestTrue(TEXT("The reused target starts with a live entity"), ck::IsValid(Target.Get<FCk_Handle>()));

    auto InternalContext = Private::FInternalNetSerializationContext{};
    auto Context = FNetSerializationContext{};
    Context.SetInternalContext(&InternalContext);

    // Default-state quantization exercises the engine's real null representation.
    Context.SetIsInitializingDefaultState(true);
    auto NullObject = TWeakObjectPtr<UObject>{};
    auto NullReference = FObjectNetSerializerQuantizedReferenceStorage{};
    const auto& ObjectSerializer = UE_NET_GET_SERIALIZER(FWeakObjectNetSerializer);
    auto QuantizeArgs = FNetQuantizeArgs{};
    QuantizeArgs.NetSerializerConfig = ObjectSerializer.DefaultConfig;
    QuantizeArgs.Source = reinterpret_cast<NetSerializerValuePointer>(&NullObject);
    QuantizeArgs.Target = reinterpret_cast<NetSerializerValuePointer>(&NullReference);
    ObjectSerializer.Quantize(Context, QuantizeArgs);
    Context.SetIsInitializingDefaultState(false);

    const auto& HandleSerializer = UE_NET_GET_SERIALIZER(FCk_HandleNetSerializer);
    auto DequantizeArgs = FNetDequantizeArgs{};
    DequantizeArgs.NetSerializerConfig = HandleSerializer.DefaultConfig;
    DequantizeArgs.Source = reinterpret_cast<NetSerializerValuePointer>(&NullReference);
    DequantizeArgs.Target = reinterpret_cast<NetSerializerValuePointer>(Target.GetMutableMemory());
    HandleSerializer.Dequantize(Context, DequantizeArgs);

    TestTrue(TEXT("Explicit null replaces the reused handle with the default handle"),
        Target.Get<FCk_Handle>() == FCk_Handle{});
    TestFalse(TEXT("The cleared target no longer identifies a live entity"), ck::IsValid(Target.Get<FCk_Handle>()));
    TestTrue(TEXT("Clearing the reference does not destroy the previously referenced entity"), ck::IsValid(PreviousHandle));

    HandleSerializer.Dequantize(Context, DequantizeArgs);
    TestTrue(TEXT("Repeated null remains cleared"), Target.Get<FCk_Handle>() == FCk_Handle{});
    TestFalse(TEXT("Null dequantization reports no serialization error"), Context.HasError());
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTest_HandleNetSerializer_UnresolvedPreservesReusedTarget,
    "Ck.Ecs.HandleNetSerializer.UnresolvedPreservesReusedTarget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkTest_HandleNetSerializer_UnresolvedPreservesReusedTarget::RunTest(
        const FString&)
    -> bool
{
    using namespace UE::Net;

    auto World = ck::FEcsWorld{};
    auto& Registry = World.Get_Registry();
    const auto PreviousHandle = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(Registry);
    auto Target = FInstancedStruct::Make<FCk_Handle>(PreviousHandle);

    // Read a valid dynamic reference through Iris rather than fabricating its bit layout.
    uint32 Buffer[8]{};
    auto Writer = FNetBitStreamWriter{};
    Writer.InitBytes(Buffer, sizeof(Buffer));
    Writer.WriteBool(true);
    WritePackedUint64(&Writer, 2U);
    Writer.CommitWrites();

    auto Reader = FNetBitStreamReader{};
    Reader.InitBits(Buffer, Writer.GetPosBits());
    auto Context = FNetSerializationContext{&Reader};
    const auto NetHandle = ReadNetRefHandle(Context);
    TestTrue(TEXT("The incoming reference is non-null"), NetHandle.IsValid());
    TestFalse(TEXT("The reference bitstream is valid"), Context.HasErrorOrOverflow());

    auto Reference = FQuantizedObjectReference{};
    Reference.NetReference = Private::FObjectReferenceCache::MakeNetObjectReference(NetHandle);
    auto InternalContext = Private::FInternalNetSerializationContext{};
    Context.SetInternalContext(&InternalContext);

    auto ResolvedObject = TWeakObjectPtr<UObject>{};
    const auto& ObjectSerializer = UE_NET_GET_SERIALIZER(FWeakObjectNetSerializer);
    auto DequantizeArgs = FNetDequantizeArgs{};
    DequantizeArgs.NetSerializerConfig = ObjectSerializer.DefaultConfig;
    DequantizeArgs.Source = reinterpret_cast<NetSerializerValuePointer>(&Reference);
    DequantizeArgs.Target = reinterpret_cast<NetSerializerValuePointer>(&ResolvedObject);
    ObjectSerializer.Dequantize(Context, DequantizeArgs);
    TestFalse(TEXT("The non-null reference cannot resolve without a reference cache"), ResolvedObject.IsValid());

    const auto& HandleSerializer = UE_NET_GET_SERIALIZER(FCk_HandleNetSerializer);
    DequantizeArgs.NetSerializerConfig = HandleSerializer.DefaultConfig;
    DequantizeArgs.Target = reinterpret_cast<NetSerializerValuePointer>(Target.GetMutableMemory());
    HandleSerializer.Dequantize(Context, DequantizeArgs);

    TestTrue(TEXT("An unresolved non-null reference preserves the previous target"),
        Target.Get<FCk_Handle>() == PreviousHandle);
    TestTrue(TEXT("The preserved target remains valid"), ck::IsValid(Target.Get<FCk_Handle>()));
    TestFalse(TEXT("Pending dequantization reports no serialization error"), Context.HasError());
    return true;
}

#endif
