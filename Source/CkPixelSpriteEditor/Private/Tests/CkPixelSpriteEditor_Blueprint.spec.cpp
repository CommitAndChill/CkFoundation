#include "CkPixelSprite/CkPixelSprite_Asset.h"
#include "CkPixelSprite/CkPixelSprite_Player.h"

#include "CkCore/Macros/CkMacros.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Interfaces/IPluginManager.h"
#include "K2Node_CallFunction.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ck_pixel_sprite_blueprint_tests
{
    template <typename TNode>
    auto
        FindNode(TObjectPtr<UEdGraph> InGraph)
        -> TObjectPtr<TNode>
    {
        for (const auto& Node : InGraph->Nodes)
        {
            const auto Candidate = TObjectPtr<TNode>{Cast<TNode>(Node.Get())};
            if (Candidate)
            {
                return Candidate;
            }
        }
        return {};
    }

    auto
        AddCall(TObjectPtr<UEdGraph> InGraph, TObjectPtr<UFunction> InFunction)
        -> TObjectPtr<UK2Node_CallFunction>
    {
        FGraphNodeCreator<UK2Node_CallFunction> Creator{*InGraph};
        auto Node = TObjectPtr<UK2Node_CallFunction>{Creator.CreateNode()};
        Node->SetFromFunction(InFunction.Get());
        Creator.Finalize();
        return Node;
    }

    auto
        Connect(UEdGraphPin& InFrom, UEdGraphPin& InTo)
        -> bool
    {
        return GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(&InFrom, &InTo);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkTest_PixelSprite_BlueprintVM,
    "Ck.PixelSprite.Editor.BlueprintVM", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkTest_PixelSprite_BlueprintVM::
    RunTest(const FString&)
    -> bool
{
    const auto Plugin = IPluginManager::Get().FindPlugin(TEXT("CkFoundation"));
    if (NOT TestTrue(TEXT("Foundation plugin is available"), Plugin.IsValid()))
    {
        return false;
    }
    auto Manifest = FCk_PixelSprite_Manifest{};
    auto Diagnostic = FCk_PixelSprite_Diagnostic{};
    const auto Fixture = Plugin->GetBaseDir() / TEXT("Tools/CkPixel/Fixtures/Manifest/valid-loop.json");
    if (NOT TestTrue(TEXT("Authored fixture loads"),
        ck::pixel_sprite::TryLoad_Manifest(Fixture, Manifest, Diagnostic)))
    {
        AddError(Diagnostic.Get_Text()); return false;
    }
    auto Asset = TStrongObjectPtr<UCk_PixelSpriteAsset_UE>{NewObject<UCk_PixelSpriteAsset_UE>()};
    if (NOT TestTrue(TEXT("Authored fixture becomes a runtime asset"),
        Asset->Request_ReplaceManifest(Manifest, Diagnostic)))
    {
        AddError(Diagnostic.Get_Text()); return false;
    }

    auto Blueprint = TStrongObjectPtr<UBlueprint>{FKismetEditorUtilities::CreateBlueprint(
        UCk_PixelSpritePlayer_UE::StaticClass(), GetTransientPackage(),
        MakeUniqueObjectName(GetTransientPackage(), UBlueprint::StaticClass(), TEXT("PixelSpriteVM")),
        BPTYPE_Normal)};
    if (NOT TestTrue(TEXT("Transient player Blueprint created"), Blueprint.IsValid()))
    {
        return false;
    }
    const auto Graph = TObjectPtr<UEdGraph>{FBlueprintEditorUtils::CreateNewGraph(
        Blueprint.Get(), TEXT("ExercisePlayer"), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass())};
    const auto SetSpeedFunction = TObjectPtr<UFunction>{UCk_PixelSpritePlayer_UE::StaticClass()->
        FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UCk_PixelSpritePlayer_UE, Request_SetSpeed))};
    if (NOT TestTrue(TEXT("Function graph and reflected signature exist"),
        Graph && SetSpeedFunction))
    {
        return false;
    }
    FBlueprintEditorUtils::AddFunctionGraph(Blueprint.Get(), Graph.Get(), true, SetSpeedFunction.Get());
    const auto Entry = ck_pixel_sprite_blueprint_tests::FindNode<UK2Node_FunctionEntry>(Graph);
    const auto Result = ck_pixel_sprite_blueprint_tests::FindNode<UK2Node_FunctionResult>(Graph);
    if (NOT TestTrue(TEXT("Generated graph has executable entry and bool return"), Entry && Result))
    {
        return false;
    }

    const auto SelectFunction = TObjectPtr<UFunction>{UCk_PixelSpritePlayer_UE::StaticClass()->
        FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UCk_PixelSpritePlayer_UE, Request_SelectAnimation))};
    const auto DirectionFunction = TObjectPtr<UFunction>{UCk_PixelSpritePlayer_UE::StaticClass()->
        FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UCk_PixelSpritePlayer_UE, Request_SetDirection))};
    const auto PlayFunction = TObjectPtr<UFunction>{UCk_PixelSpritePlayer_UE::StaticClass()->
        FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UCk_PixelSpritePlayer_UE, Request_Play))};
    const auto AdvanceFunction = TObjectPtr<UFunction>{UCk_PixelSpritePlayer_UE::StaticClass()->
        FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UCk_PixelSpritePlayer_UE, Request_AdvanceMilliseconds))};
    if (NOT TestTrue(TEXT("All runtime calls are reflected"),
        SelectFunction && DirectionFunction && PlayFunction && AdvanceFunction))
    {
        return false;
    }
    const auto Select = ck_pixel_sprite_blueprint_tests::AddCall(Graph, SelectFunction);
    const auto Direction = ck_pixel_sprite_blueprint_tests::AddCall(Graph, DirectionFunction);
    const auto Speed = ck_pixel_sprite_blueprint_tests::AddCall(Graph, SetSpeedFunction);
    const auto Play = ck_pixel_sprite_blueprint_tests::AddCall(Graph, PlayFunction);
    const auto Advance = ck_pixel_sprite_blueprint_tests::AddCall(Graph, AdvanceFunction);
    if (NOT TestTrue(TEXT("All Blueprint call pins exist"),
        Entry->FindPin(UEdGraphSchema_K2::PN_Then) && Entry->FindPin(TEXT("InSpeed")) &&
        Select->FindPin(UEdGraphSchema_K2::PN_Execute) && Select->FindPin(UEdGraphSchema_K2::PN_Then) &&
        Select->FindPin(TEXT("InName")) && Direction->FindPin(TEXT("InDirection")) &&
        Direction->FindPin(UEdGraphSchema_K2::PN_Execute) &&
        Direction->FindPin(UEdGraphSchema_K2::PN_Then) &&
        Speed->FindPin(TEXT("InSpeed")) && Speed->FindPin(UEdGraphSchema_K2::PN_Execute) &&
        Speed->FindPin(UEdGraphSchema_K2::PN_Then) &&
        Play->FindPin(UEdGraphSchema_K2::PN_Execute) && Play->FindPin(UEdGraphSchema_K2::PN_Then) &&
        Advance->FindPin(TEXT("InDeltaMilliseconds")) &&
        Advance->FindPin(UEdGraphSchema_K2::PN_Execute) &&
        Advance->FindPin(UEdGraphSchema_K2::PN_Then) &&
        Advance->FindPin(UEdGraphSchema_K2::PN_ReturnValue) &&
        Result->FindPin(UEdGraphSchema_K2::PN_Execute) &&
        Result->FindPin(UEdGraphSchema_K2::PN_ReturnValue)))
    {
        return false;
    }
    GetDefault<UEdGraphSchema_K2>()->TrySetDefaultValue(*Select->FindPin(TEXT("InName")), TEXT("walk"));
    GetDefault<UEdGraphSchema_K2>()->TrySetDefaultValue(*Direction->FindPin(TEXT("InDirection")), TEXT("1"));
    GetDefault<UEdGraphSchema_K2>()->TrySetDefaultValue(
        *Advance->FindPin(TEXT("InDeltaMilliseconds")), TEXT("62.5"));
    if (NOT TestTrue(TEXT("Execution and return pins wire into a real Blueprint function"),
        ck_pixel_sprite_blueprint_tests::Connect(*Entry->FindPin(UEdGraphSchema_K2::PN_Then),
            *Select->FindPin(UEdGraphSchema_K2::PN_Execute)) &&
        ck_pixel_sprite_blueprint_tests::Connect(*Select->FindPin(UEdGraphSchema_K2::PN_Then),
            *Direction->FindPin(UEdGraphSchema_K2::PN_Execute)) &&
        ck_pixel_sprite_blueprint_tests::Connect(*Direction->FindPin(UEdGraphSchema_K2::PN_Then),
            *Speed->FindPin(UEdGraphSchema_K2::PN_Execute)) &&
        ck_pixel_sprite_blueprint_tests::Connect(*Speed->FindPin(UEdGraphSchema_K2::PN_Then),
            *Play->FindPin(UEdGraphSchema_K2::PN_Execute)) &&
        ck_pixel_sprite_blueprint_tests::Connect(*Play->FindPin(UEdGraphSchema_K2::PN_Then),
            *Advance->FindPin(UEdGraphSchema_K2::PN_Execute)) &&
        ck_pixel_sprite_blueprint_tests::Connect(*Advance->FindPin(UEdGraphSchema_K2::PN_Then),
            *Result->FindPin(UEdGraphSchema_K2::PN_Execute)) &&
        ck_pixel_sprite_blueprint_tests::Connect(*Entry->FindPin(TEXT("InSpeed")),
            *Speed->FindPin(TEXT("InSpeed"))) &&
        ck_pixel_sprite_blueprint_tests::Connect(*Advance->FindPin(UEdGraphSchema_K2::PN_ReturnValue),
            *Result->FindPin(UEdGraphSchema_K2::PN_ReturnValue))))
    {
        return false;
    }

    auto CompileLog = FCompilerResultsLog{};
    FKismetEditorUtilities::CompileBlueprint(Blueprint.Get(), EBlueprintCompileOptions::None, &CompileLog);
    if (NOT TestEqual(TEXT("Blueprint compiler errors"), CompileLog.NumErrors, 0) ||
        NOT TestTrue(TEXT("Blueprint generated runnable class"),
            Blueprint->GeneratedClass && Blueprint->IsUpToDate()))
    {
        return false;
    }
    auto Player = TStrongObjectPtr<UCk_PixelSpritePlayer_UE>{NewObject<UCk_PixelSpritePlayer_UE>(
        GetTransientPackage(), Blueprint->GeneratedClass)};
    if (NOT TestTrue(TEXT("Compiled Blueprint player instance created"), Player.IsValid()) ||
        NOT TestTrue(TEXT("Runtime asset selected before VM execution"),
            Player->TrySelectAsset(Asset.Get(), Diagnostic)))
    {
        AddError(Diagnostic.Get_Text()); return false;
    }
    const auto Function = TObjectPtr<UFunction>{Player->FindFunction(TEXT("ExercisePlayer"))};
    if (NOT TestTrue(TEXT("Compiled graph exposes its own Blueprint function"), IsValid(Function.Get())))
    {
        return false;
    }
    if (NOT TestTrue(TEXT("Compiled function has speed input and bool result"),
        FindFProperty<FDoubleProperty>(Function.Get(), TEXT("InSpeed")) &&
        FindFProperty<FBoolProperty>(Function.Get(), TEXT("ReturnValue"))))
    {
        return false;
    }
    auto Parameters = TArray<uint8>{};
    Parameters.SetNumZeroed(Function->ParmsSize);
    FindFProperty<FDoubleProperty>(Function.Get(), TEXT("InSpeed"))->
        SetPropertyValue_InContainer(Parameters.GetData(), 2.0);
    Player->ProcessEvent(Function.Get(), Parameters.GetData());
    TestTrue(TEXT("Blueprint VM returned native advance success"),
        FindFProperty<FBoolProperty>(Function.Get(), TEXT("ReturnValue"))->
            GetPropertyValue_InContainer(Parameters.GetData()));
    TestEqual(TEXT("Blueprint VM selected, played and advanced authored time"),
        Player->Get_ElapsedMilliseconds(), 125.0);
    TestEqual(TEXT("Blueprint VM reached second authored frame"), Player->Get_CurrentFrame(), 1);
    return true;
}

#endif
