#pragma once

#include <Interfaces/IPluginManager.h>
#include <Misc/Paths.h>
#include <UObject/Class.h>
#include <UObject/Package.h>

#if WITH_ANGELSCRIPT_CK
#include "ClassGenerator/ASClass.h"
#endif

// Construct once per synchronous bucketing pass. The next pass sees newly enabled plugins.
class FCkAngelscriptGenerator_PluginLookup
{
    struct FPluginDirectory
    {
        TSharedRef<IPlugin> Plugin;
        FString Directory;
    };

public:
    FCkAngelscriptGenerator_PluginLookup()
    {
        for (const auto& Plugin : IPluginManager::Get().GetEnabledPlugins())
        {
            auto Directory = FPaths::ConvertRelativePathToFull(Plugin->GetBaseDir());
            FPaths::NormalizeDirectoryName(Directory);
            Directories.Add(FPluginDirectory{Plugin, MoveTemp(Directory)});

            for (const auto& Module : Plugin->GetDescriptor().Modules)
            {
                ModuleToPlugin.Add(Module.Name.ToString(), Plugin);
            }
        }
    }

    auto Find_PluginForClass(const UClass* InClass) const -> const IPlugin*
    {
        // AS-defined classes all live under /Script/Angelscript, so this only matches C++ classes.
        const auto PackageName = InClass->GetOutermost()->GetName();
        constexpr auto ScriptPrefix = TEXT("/Script/");
        if (PackageName.StartsWith(ScriptPrefix))
        {
            const auto ModuleName = PackageName.RightChop(FString(ScriptPrefix).Len());
            if (const auto* Found = ModuleToPlugin.Find(ModuleName))
            { return &Found->Get(); }
        }

#if WITH_ANGELSCRIPT_CK
        // GetFirstASClass walks the hierarchy; a plain Cast<UASClass> misses derived classes.
        if (auto* ASClass = UASClass::GetFirstASClass(const_cast<UClass*>(InClass)))
        {
            if (const auto* Plugin = Find_PluginByPathPrefix(ASClass->GetSourceFilePath()))
            { return Plugin; }
        }
#endif

        return nullptr;
    }

private:
    auto Find_PluginByPathPrefix(const FString& InPath) const -> const IPlugin*
    {
        if (InPath.IsEmpty())
        { return nullptr; }

        auto NormalizedPath = FPaths::ConvertRelativePathToFull(InPath);
        FPaths::NormalizeFilename(NormalizedPath);

        const IPlugin* BestMatch = nullptr;
        auto BestMatchLen = int32{0};
        for (const auto& Entry : Directories)
        {
            if (NormalizedPath.StartsWith(Entry.Directory, ESearchCase::IgnoreCase)
                && Entry.Directory.Len() > BestMatchLen)
            {
                BestMatch = &Entry.Plugin.Get();
                BestMatchLen = Entry.Directory.Len();
            }
        }
        return BestMatch;
    }

    TArray<FPluginDirectory> Directories;
    TMap<FString, TSharedRef<IPlugin>> ModuleToPlugin;
};
