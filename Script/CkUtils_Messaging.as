namespace utils_messaging
{
    FCk_Handle
    TryGet_MessengerEntity(FCk_Handle InHandle, UScriptStruct MessagePayloadTemplate)
    {
        if (!ck::IsValid(InHandle) || !ck::IsValid(MessagePayloadTemplate))
        { return FCk_Handle(); }

        auto MessagePayloadStructName = MessagePayloadTemplate.GetName();
        // Bind/Broadcast register this name. Lookup must not create a missing gameplay tag.
        auto MessageName = FGameplayTag::RequestGameplayTag(
            FName(f"Message.{MessagePayloadStructName}"), false);
        return utils_messaging::TryGet_MessengerEntity(InHandle, MessageName);
    }

    int
    Get_NumBoundTo_OnBroadcast(
        FCk_Handle InHandle, UScriptStruct MessagePayloadTemplate,
        ECk_Signal_PostFireBehavior InPostFireBehavior)
    {
        if (!ck::IsValid(InHandle) || !ck::IsValid(MessagePayloadTemplate)) { return 0; }
        auto MessageName = FGameplayTag::RequestGameplayTag(
            FName(f"Message.{MessagePayloadTemplate.GetName()}"), false);
        return utils_messaging::Get_NumBoundTo_OnBroadcast(InHandle, MessageName, InPostFireBehavior);
    }

    int
    Get_NumUnconditionalBindingsTo_OnBroadcast(
        FCk_Handle InHandle, UScriptStruct MessagePayloadTemplate,
        const FCk_Delegate_Messaging_OnBroadcast &in InDelegate,
        ECk_Signal_PostFireBehavior InPostFireBehavior)
    {
        if (!ck::IsValid(InHandle) || !ck::IsValid(MessagePayloadTemplate)) { return 0; }
        auto MessageName = FGameplayTag::RequestGameplayTag(
            FName(f"Message.{MessagePayloadTemplate.GetName()}"), false);
        return utils_messaging::Get_NumUnconditionalBindingsTo_OnBroadcast(
            InHandle, MessageName, InDelegate, InPostFireBehavior);
    }

    void
    Broadcast(FCk_Handle InHandle, FAngelscriptAnyStructParameter InPayload)
    {
        auto MessagePayloadStructName = InPayload.InstancedStruct.ScriptStruct.GetName();
        auto MessageName = GameplayTags::ResolveGameplayTag(FName(f"Message.{MessagePayloadStructName}"), "Generated from a Message payload definition");
        utils_messaging::Broadcast(InHandle, MessageName, InPayload.InstancedStruct);
    }

    void
    UnbindFrom_OnBroadcast(FCk_Handle InHandle, UScriptStruct MessagePayloadTemplate, const FCk_Delegate_Messaging_OnBroadcast &in InDelegate)
    {
        auto MessagePayloadStructName = MessagePayloadTemplate.GetName();
        auto MessageName = GameplayTags::ResolveGameplayTag(FName(f"Message.{MessagePayloadStructName}"), "Generated from a Message payload definition");
        utils_messaging::UnbindFrom_OnBroadcast(InHandle, MessageName, InDelegate);
    }

    void
    BindTo_OnBroadcast(
        FCk_Handle InHandle,
        UScriptStruct MessagePayloadTemplate,
        const FCk_Delegate_Messaging_OnBroadcast &in InDelegate,
        ECk_Signal_BindingPolicy InBindingPolicy = ECk_Signal_BindingPolicy::FireIfPayloadInFlightThisFrame,
        ECk_Signal_PostFireBehavior InPostFireBehavior = ECk_Signal_PostFireBehavior::DoNothing)
    {
        auto MessagePayloadStructName = MessagePayloadTemplate.GetName();
        auto MessageName = GameplayTags::ResolveGameplayTag(FName(f"Message.{MessagePayloadStructName}"), "Generated from a Message payload definition");
        utils_messaging::BindTo_OnBroadcast(InHandle, MessageName, InDelegate, InBindingPolicy, InPostFireBehavior);
    }
}
