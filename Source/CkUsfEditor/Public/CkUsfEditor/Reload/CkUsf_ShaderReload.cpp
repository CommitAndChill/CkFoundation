#include "CkUsfEditor/Reload/CkUsf_ShaderReload.h"

#include "CkUsfEditor_Log.h"
#include "CkUsfEditor/Generator/CkUsf_Generator.h"
#include "CkCore/Macros/CkMacros.h"

#include "Containers/Ticker.h"
#include "DirectoryWatcherModule.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "IDirectoryWatcher.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"
#include "MaterialShared.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Modules/ModuleManager.h"
#include "ShaderCore.h"
#include "ShaderCompilerCore.h"
#if __has_include("ShaderSourceDependencies.h")
#include "ShaderSourceDependencies.h"
#endif
#include "UObject/UObjectIterator.h"

#if defined(UE_SHADER_SOURCE_DEPENDENCIES_API_VERSION) && UE_SHADER_SOURCE_DEPENDENCIES_API_VERSION >= 1
#define CK_USF_HAS_SHADER_SOURCE_DEPENDENCIES 1
#else
#define CK_USF_HAS_SHADER_SOURCE_DEPENDENCIES 0
#endif

namespace ck::usf_editor
{
    class FShaderReload::FState
    {
    public:
        auto
            Startup()
            -> void
        {
            _AutoReloadVariable = MakeUnique<FAutoConsoleVariableRef>(
                TEXT("Ck.Usf.AutoReload"), _AutoReload,
                TEXT("Reload affected loaded CkUsf material looks after shader saves (1=on, 0=off). Global shaders always require an explicit command."));
            _ReloadCommand = MakeUnique<FAutoConsoleCommand>(
                TEXT("Ck_Usf_ReloadShaders"),
                TEXT("Recompile all loaded CkUsf look masters in place, including live transient masters; no generation or asset saves."),
                FConsoleCommandDelegate::CreateRaw(this, &FState::RequestMaterialReload));
            _GlobalCommand = MakeUnique<FAutoConsoleCommand>(
                TEXT("Ck_Usf_ReloadGlobalShaders"),
                TEXT("Explicit CkUsfRenderer entry-point recompilation with development-mode retries. For shared lighting includes use Ck_Usf_ReloadShadingModels."),
                FConsoleCommandDelegate::CreateRaw(this, &FState::RequestGlobalReload));
            _ShadingModelCommand = MakeUnique<FAutoConsoleCommand>(
                TEXT("Ck_Usf_ReloadShadingModels"),
                TEXT("Enable shader development-mode retries and recompile changed global/material shader dependencies, including CkUsf lighting HLSL. Explicit only; repair errors before choosing Retry."),
                FConsoleCommandDelegate::CreateRaw(this, &FState::RequestShadingModelReload));
            _Ticker = FTSTicker::GetCoreTicker().AddTicker(
                FTickerDelegate::CreateRaw(this, &FState::Tick));
        }

        auto
            Shutdown()
            -> void
        {
            FTSTicker::GetCoreTicker().RemoveTicker(_Ticker);
            _Ticker.Reset();
            _GlobalCommand.Reset();
            _ShadingModelCommand.Reset();
            _ReloadCommand.Reset();
            _AutoReloadVariable.Reset();
            if (FModuleManager::Get().IsModuleLoaded(TEXT("DirectoryWatcher")))
            {
                auto& WatcherModule = FModuleManager::GetModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
                if (WatcherModule.Get() != nullptr)
                {
                    for (const auto& Watch : _Watches)
                    {
                        WatcherModule.Get()->UnregisterDirectoryChangedCallback_Handle(Watch.Key, Watch.Value);
                    }
                }
            }
            _Watches.Reset();
            _ActiveMaterials.Reset();
            _ActiveHashes.Reset();
            _ActiveRoots.Reset();
            _ActivePendingFiles.Reset();
            _PendingFiles.Reset();
            _RejectedFiles.Reset();
            _RejectedRoots.Reset();
            _DependencyEdges.Reset();
            _FileHashes.Reset();
        }

    private:
        static auto
            Validate_ReloadSupport()
            -> bool
        {
            if (FShaderReload::Get_IsSupported())
            { return true; }
            Warning(TEXT("CkUsf shader reload rejected: this engine lacks ShaderSourceDependencies API v1. Update the engine to reload safely; no shader reload was started."));
            return false;
        }

        auto
            RequestMaterialReload()
            -> void
        {
            if (NOT Validate_ReloadSupport())
            { return; }
            _ManualReload = true;
        }

        auto
            RequestGlobalReload()
            -> void
        {
            if (NOT Validate_ReloadSupport())
            { return; }
            _GlobalReload = true;
        }

        auto
            RequestShadingModelReload()
            -> void
        {
            if (NOT Validate_ReloadSupport())
            { return; }
            _ShadingModelReload = true;
            _ManualReload = true;
        }

        auto
            ReloadGlobalShaders(const TCHAR* InCommand)
            -> void
        {
            if (GEngine == nullptr || IConsoleManager::Get().FindConsoleVariable(TEXT("r.ShaderDevelopmentMode")) == nullptr)
            {
                Warning(TEXT("CkUsf global reload rejected: engine or shader development mode is unavailable."));
                return;
            }
            IConsoleManager::Get().FindConsoleVariable(TEXT("r.ShaderDevelopmentMode"))->Set(1, ECVF_SetByConsole);
            if (IConsoleManager::Get().FindConsoleVariable(TEXT("r.ShaderDevelopmentMode"))->GetInt() == 0)
            {
                Warning(TEXT("CkUsf global reload rejected: shader development-mode retries could not be enabled."));
                return;
            }
            Log(TEXT("CkUsf explicit global reload [{}], development-mode retries enabled. Repair shader errors on disk before choosing Retry; cancelling a failed global compile can terminate the editor."), InCommand);
            const auto Started = FPlatformTime::Seconds();
            const auto Dispatched = GEngine->Exec(nullptr, InCommand);
            if (NOT Dispatched)
            {
                Warning(TEXT("CkUsf global reload command was not handled [{}]."), InCommand);
                return;
            }
            if (GEditor != nullptr)
            { GEditor->RedrawAllViewports(); }
            Log(TEXT("CkUsf shading-model reload complete: [{}] seconds; pending material shader compilation may continue asynchronously."),
                FPlatformTime::Seconds() - Started);
        }

        auto
            RegisterWatches()
            -> void
        {
            if (NOT FShaderReload::Get_IsSupported())
            {
                Log(TEXT("CkUsf shader watching unavailable: this engine lacks ShaderSourceDependencies API v1. Reload commands will explicitly reject requests."));
                _WatchesAttempted = true;
                return;
            }
            auto& WatcherModule = FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
            if (WatcherModule.Get() == nullptr)
            {
                Warning(TEXT("CkUsf shader watching unavailable on this platform; use Ck_Usf_ReloadShaders."));
                _WatchesAttempted = true;
                return;
            }
            for (const auto& VirtualRoot : {FString{TEXT("/CkUsf")}, FString{TEXT("/CkUsfRenderer")}})
            {
                if (NOT AllShaderSourceDirectoryMappings().Contains(VirtualRoot))
                {
                    Warning(TEXT("CkUsf shader watcher: mapping [{}] is unavailable."), VirtualRoot);
                    continue;
                }
                auto Directory = FPaths::ConvertRelativePathToFull(AllShaderSourceDirectoryMappings().FindChecked(VirtualRoot));
                FPaths::NormalizeDirectoryName(Directory);
                FDelegateHandle Handle;
                const auto Registered = WatcherModule.Get()->RegisterDirectoryChangedCallback_Handle(
                    Directory, IDirectoryWatcher::FDirectoryChanged::CreateLambda(
                        [this, VirtualRoot, Directory](const TArray<FFileChangeData>& InChanges)
                        {
                            OnFilesChanged(InChanges, VirtualRoot, Directory);
                        }), Handle);
                if (Registered)
                {
                    _Watches.Add(Directory, Handle);
                    Log(TEXT("CkUsf watching [{}] at [{}]; global shader edits remain manual."), VirtualRoot, Directory);
                }
                else
                {
                    Warning(TEXT("CkUsf failed to watch [{}]; use the manual reload commands."), Directory);
                }
            }
            _WatchesAttempted = true;
        }

        auto OnFilesChanged(const TArray<FFileChangeData>& InChanges, const FString& InVirtualRoot,
            const FString& InDirectory) -> void
        {
            for (const auto& Change : InChanges)
            {
                if (Change.Action == FFileChangeData::FCA_RescanRequired)
                {
                    if (InVirtualRoot == TEXT("/CkUsfRenderer"))
                    { Warning(TEXT("CkUsf global shader watcher needs a rescan; use Ck_Usf_ReloadGlobalShaders explicitly.")); }
                    else if (_AutoReload != 0)
                    {
                        Warning(TEXT("CkUsf shader watcher needs a rescan; reloading all loaded look masters."));
                        _FileHashes.Reset();
                        _ManualReload = true;
                    }
                    continue;
                }
                const auto Extension = FPaths::GetExtension(Change.Filename);
                if (NOT Extension.Equals(TEXT("ush"), ESearchCase::IgnoreCase)
                    && NOT Extension.Equals(TEXT("usf"), ESearchCase::IgnoreCase))
                { continue; }

                auto PhysicalPath = FPaths::ConvertRelativePathToFull(Change.Filename);
                FPaths::NormalizeFilename(PhysicalPath);
                auto RelativePath = PhysicalPath;
                if (NOT FPaths::MakePathRelativeTo(RelativePath, *(InDirectory + TEXT("/"))))
                { continue; }
                const auto VirtualPath = InVirtualRoot / RelativePath;
                if (InVirtualRoot == TEXT("/CkUsfRenderer"))
                {
                    Log(TEXT("CkUsf global shader changed [{}]. Run [{}] explicitly to apply it."), VirtualPath,
                        RelativePath.StartsWith(TEXT("ShadingModels/")) ? TEXT("Ck_Usf_ReloadShadingModels") : TEXT("Ck_Usf_ReloadGlobalShaders"));
                    continue;
                }
                if (_AutoReload == 0)
                { continue; }
                _PendingFiles.Add(VirtualPath);
                _LastSaveTime = FPlatformTime::Seconds();
            }
        }

        static auto
            GetLookIncludes(const UMaterial& InMaterial)
            -> TSet<FString>
        {
            auto Includes = TSet<FString>{};
            auto IsLook = false;
            for (const auto& Expression : InMaterial.GetExpressions())
            {
                const auto Custom = TWeakObjectPtr<UMaterialExpressionCustom>{Cast<UMaterialExpressionCustom>(Expression.Get())};
                if (NOT Custom.IsValid())
                { continue; }
                for (const auto& Include : Custom->IncludeFilePaths)
                {
                    IsLook |= Include.StartsWith(TEXT("/CkUsf/"));
                    Includes.Add(Include);
                }
            }
            return IsLook ? Includes : TSet<FString>{};
        }

#if CK_USF_HAS_SHADER_SOURCE_DEPENDENCIES
        auto
            StartMaterialBatch()
            -> void
        {
            const auto Manual = _ManualReload;
            _ManualReload = false;
            auto PendingFiles = MoveTemp(_PendingFiles);
            _PendingFiles.Reset();
            auto RetriedFiles = TSet<FString>{};
            for (const auto& Rejected : _RejectedFiles)
            {
                PendingFiles.Add(Rejected);
                RetriedFiles.Add(Rejected);
            }
            _RejectedFiles.Reset();

            // Refresh source-local edges before attributing a file event. A missing parent keeps
            // its previous edges; a readable parent replaces its edges even if a sibling is absent.
            FlushShaderFileCache();
            auto MaterialRoots = TMap<TWeakObjectPtr<UMaterial>, TSet<FString>>{};
            auto Roots = TSet<FString>{};
            for (TObjectIterator<UMaterial> It{RF_ClassDefaultObject, true, EInternalObjectFlags::Garbage}; It; ++It)
            {
                if (It->HasAnyFlags(RF_ClassDefaultObject | RF_BeginDestroyed | RF_FinishDestroyed))
                { continue; }
                auto Includes = GetLookIncludes(**It);
                if (Includes.IsEmpty())
                { continue; }
                Roots.Append(Includes);
                MaterialRoots.Add(TWeakObjectPtr<UMaterial>{*It}, MoveTemp(Includes));
            }
            auto UnavailableSources = TSet<FString>{};
            auto IncompleteSources = TSet<FString>{};
            auto CompleteSources = TSet<FString>{};
            auto InvalidRoots = TSet<FString>{};
            auto RootErrors = TMap<FString, FString>{};
            for (const auto& Root : Roots)
            {
                auto Dependencies = FShaderSourceDependencies{};
                auto Error = FString{};
                const auto Complete = TryGetShaderSourceDependencies(*Root, Dependencies, GMaxRHIShaderPlatform, &Error);
                if (NOT Dependencies.RootVirtualFilePath.IsEmpty() && Dependencies.RootVirtualFilePath != Root)
                { _DependencyEdges.Add(Root, TArray<FString>{Dependencies.RootVirtualFilePath}); }
                for (const auto& Source : Dependencies.DirectIncludes)
                {
                    if (Dependencies.IncompleteSources.Contains(Source.Key))
                    {
                        if (NOT CompleteSources.Contains(Source.Key))
                        {
                            auto& Edges = _DependencyEdges.FindOrAdd(Source.Key);
                            for (const auto& Child : Source.Value)
                            { Edges.AddUnique(Child); }
                        }
                    }
                    else
                    {
                        _DependencyEdges.Add(Source.Key, Source.Value);
                        CompleteSources.Add(Source.Key);
                    }
                }
                UnavailableSources.Append(Dependencies.UnavailableSources);
                IncompleteSources.Append(Dependencies.IncompleteSources);
                if (NOT Complete && Dependencies.UnavailableSources.IsEmpty() && Dependencies.IncompleteSources.IsEmpty())
                {
                    RootErrors.Add(Root, MoveTemp(Error));
                    InvalidRoots.Add(Root);
                }
            }
            // A shared parent may hit a limit through a deep root and be fully scanned through
            // a shallower root. Its complete current edges win independently of root iteration order.
            for (const auto& Source : CompleteSources)
            {
                IncompleteSources.Remove(Source);
                UnavailableSources.Remove(Source);
            }
            const auto CollectDependencies = [this](const TSet<FString>& InRoots)
            {
                auto Result = TSet<FString>{};
                auto Pending = InRoots.Array();
                while (NOT Pending.IsEmpty())
                {
                    const auto Source = Pending.Pop(EAllowShrinking::No);
                    if (Result.Contains(Source))
                    { continue; }
                    Result.Add(Source);
                    if (const auto Children = _DependencyEdges.Find(Source))
                    { Pending.Append(*Children); }
                }
                return Result;
            };
            const auto OwnedSources = CollectDependencies(Roots);
            const auto InvalidSources = CollectDependencies(InvalidRoots);
            for (auto It = _RejectedRoots.CreateIterator(); It; ++It)
            {
                if (NOT Roots.Contains(*It))
                { It.RemoveCurrent(); }
            }
            const auto RejectedSources = CollectDependencies(_RejectedRoots);
            for (auto It = _DependencyEdges.CreateIterator(); It; ++It)
            {
                if (NOT OwnedSources.Contains(It.Key()))
                { It.RemoveCurrent(); }
            }
            for (auto It = _FileHashes.CreateIterator(); It; ++It)
            {
                if (NOT OwnedSources.Contains(It.Key()))
                { It.RemoveCurrent(); }
            }
            auto ChangedFiles = TSet<FString>{};
            auto ProposedHashes = TMap<FString, FSHAHash>{};
            auto MissingChangedSources = TSet<FString>{};
            auto OwnedPendingFiles = TSet<FString>{};
            auto SourcesToHash = PendingFiles;
            if (Manual)
            { SourcesToHash.Append(OwnedSources); }
            for (const auto& Pending : SourcesToHash)
            {
                if (NOT OwnedSources.Contains(Pending))
                { continue; }
                if (PendingFiles.Contains(Pending))
                { OwnedPendingFiles.Add(Pending); }
                if (InvalidSources.Contains(Pending))
                {
                    // The scan may have rejected a platform/path prerequisite needed by the loader.
                    ChangedFiles.Add(Pending);
                    continue;
                }
                // Deduplication must describe the same cached source snapshot used by dependency
                // preflight and compilation, rather than a later disk save racing this batch.
                auto Contents = FString{};
                auto Errors = TArray<FShaderCompilerError>{};
                if (NOT LoadShaderSourceFile(*Pending, GMaxRHIShaderPlatform, &Contents, &Errors))
                {
                    Warning(TEXT("CkUsf reload rejected: shader source is absent or unreadable [{}]. Restore it and save again, or use Ck_Usf_ReloadShaders."), Pending);
                    MissingChangedSources.Add(Pending);
                    ChangedFiles.Add(Pending);
                    continue;
                }
                const auto Hash = FSHA1::HashBuffer(*Contents, Contents.Len() * sizeof(TCHAR));
                if (NOT _FileHashes.Contains(Pending) || _FileHashes.FindChecked(Pending) != Hash)
                {
                    ProposedHashes.Add(Pending, Hash);
                    ChangedFiles.Add(Pending);
                }
                else if (RetriedFiles.Contains(Pending) || RejectedSources.Contains(Pending))
                {
                    // Retried rejected work must dispatch even when a restored file has its old bytes.
                    // The dependency graph can also have changed without this file's bytes changing.
                    ChangedFiles.Add(Pending);
                }
            }
            if (NOT Manual && ChangedFiles.IsEmpty())
            { return; }
            auto Candidates = TArray<TWeakObjectPtr<UMaterial>>{};
            auto CandidateRoots = TSet<FString>{};
            auto SourceErrors = TArray<FString>{};
            for (const auto& Material : MaterialRoots)
            {
                const auto Dependencies = CollectDependencies(Material.Value);
                auto Affected = Manual;
                for (const auto& Changed : ChangedFiles)
                { Affected |= Dependencies.Contains(Changed); }
                if (NOT Affected)
                { continue; }
                CandidateRoots.Append(Material.Value);
                for (const auto& Root : Material.Value)
                {
                    if (const auto Error = RootErrors.Find(Root))
                    { SourceErrors.AddUnique(*Error); }
                }
                for (const auto& Dependency : Dependencies)
                {
                    if (UnavailableSources.Contains(Dependency) && NOT MissingChangedSources.Contains(Dependency))
                    {
                        SourceErrors.AddUnique(FString::Printf(TEXT("shader source is absent or unreadable [%s]"), *Dependency));
                    }
                    if (IncompleteSources.Contains(Dependency))
                    { SourceErrors.AddUnique(FString::Printf(TEXT("dependency scan is incomplete [%s]"), *Dependency)); }
                }
                Candidates.Add(Material.Key);
            }
            if (NOT MissingChangedSources.IsEmpty() || NOT SourceErrors.IsEmpty())
            {
                _RejectedFiles = MoveTemp(OwnedPendingFiles);
                _RejectedRoots.Append(CandidateRoots);
                for (const auto& Error : SourceErrors)
                { Warning(TEXT("CkUsf reload rejected before material changes: {}. Restore the source and save again, or use Ck_Usf_ReloadShaders."), Error); }
                return;
            }

            if (Candidates.IsEmpty())
            {
                Log(TEXT("CkUsf reload: no affected loaded look masters."));
                return;
            }
            auto Preparations = TArray<FPreparedLookShadingModelReload>{};
            auto ContractErrors = TArray<FString>{};
            for (const auto& Material : Candidates)
            {
                auto Prepared = FPreparedLookShadingModelReload{};
                if (Material.IsValid())
                { Prepare_LookShadingModelReload(*Material, Prepared, ContractErrors); }
                else
                { ContractErrors.Add(TEXT("A reload candidate was destroyed before preflight.")); }
                Preparations.Add(MoveTemp(Prepared));
            }
            if (NOT ContractErrors.IsEmpty())
            {
                _RejectedFiles = MoveTemp(OwnedPendingFiles);
                _RejectedRoots.Append(CandidateRoots);
                for (const auto& Error : ContractErrors)
                { Warning(TEXT("CkUsf reload rejected before material changes: {}"), Error); }
                return;
            }
            _ActiveHashes = MoveTemp(ProposedHashes);
            _ActiveRoots = MoveTemp(CandidateRoots);
            _ActivePendingFiles = MoveTemp(OwnedPendingFiles);
            {
                FMaterialUpdateContext UpdateContext;
                for (auto Index = 0; Index < Candidates.Num(); ++Index)
                {
                    const auto& Material = Candidates[Index];
                    if (NOT Material.IsValid())
                    { continue; }
                    UpdateContext.AddMaterial(Material.Get());
                    Material->PreEditChange(nullptr);
                    Apply_LookShadingModelReload(*Material, Preparations[Index]);
                    Material->PostEditChange();
                    Material->CacheShaders(EMaterialShaderPrecompileMode::Default);
                    _ActiveMaterials.Add(Material);
                }
            }
            _BatchStarted = FPlatformTime::Seconds();
            _TimeoutReported = false;
            Log(TEXT("CkUsf reload started: [{}] loaded master(s), [{}] changed source(s). Existing material and MID identities are preserved."),
                _ActiveMaterials.Num(), ChangedFiles.Num());
        }

#endif

        auto
            PollMaterialBatch()
            -> void
        {
            const auto TimedOut = FPlatformTime::Seconds() - _BatchStarted >= 120.0;
            if (TimedOut && NOT _TimeoutReported)
            {
                _TimeoutReported = true;
                Warning(TEXT("CkUsf reload exceeded 120 seconds; unfinished compiler jobs remain pending and later saves stay queued."));
            }
            for (const auto& Material : _ActiveMaterials)
            {
                if (NOT Material.IsValid())
                { continue; }
                if (Material->IsCompiling())
                { return; }
                if (Material->GetMaterialResource(GMaxRHIShaderPlatform) != nullptr)
                {
                    const auto& Resource = *Material->GetMaterialResource(GMaxRHIShaderPlatform);
                    if (NOT Resource.IsCompilationFinished()
                        || (NOT TimedOut && Resource.GetCompileErrors().IsEmpty()
                            && (Resource.GetGameThreadShaderMap() == nullptr || NOT Resource.IsGameThreadShaderMapComplete())))
                    { return; }
                }
            }
            auto Failed = 0;
            for (const auto& Material : _ActiveMaterials)
            {
                if (NOT Material.IsValid())
                {
                    ++Failed;
                    Warning(TEXT("CkUsf reload: a material was destroyed before completion."));
                    continue;
                }
                if (Material->GetMaterialResource(GMaxRHIShaderPlatform) == nullptr)
                {
                    ++Failed;
                    Warning(TEXT("CkUsf reload failed [{}]: no material resource for the active platform."), Material->GetPathName());
                    continue;
                }
                const auto& Resource = *Material->GetMaterialResource(GMaxRHIShaderPlatform);
                if (TimedOut || Material->IsCompiling() || NOT Resource.IsCompilationFinished()
                    || NOT Resource.GetCompileErrors().IsEmpty() || Resource.GetGameThreadShaderMap() == nullptr
                    || NOT Resource.IsGameThreadShaderMapComplete())
                {
                    ++Failed;
                    Warning(TEXT("CkUsf reload failed [{}]: [{}] compile error(s), applied shader map [{}], timed out [{}]."),
                        Material->GetPathName(), Resource.GetCompileErrors().Num(), Resource.GetGameThreadShaderMap() != nullptr, TimedOut);
                    for (const auto& Error : Resource.GetCompileErrors())
                    { Warning(TEXT("CkUsf [{}]: {}"), Material->GetPathName(), Error); }
                }
            }
            Log(TEXT("CkUsf reload complete: [{}] master(s), [{}] failed, [{}] seconds. No assets saved."),
                _ActiveMaterials.Num(), Failed, FPlatformTime::Seconds() - _BatchStarted);
            if (Failed == 0)
            {
                for (const auto& Proposed : _ActiveHashes)
                { _FileHashes.Add(Proposed.Key, Proposed.Value); }
                for (const auto& Root : _ActiveRoots)
                { _RejectedRoots.Remove(Root); }
            }
            else
            {
                _RejectedFiles.Append(_ActivePendingFiles);
                _RejectedRoots.Append(_ActiveRoots);
            }
            _ActiveMaterials.Reset();
            _ActiveHashes.Reset();
            _ActiveRoots.Reset();
            _ActivePendingFiles.Reset();
            if (GEditor != nullptr)
            { GEditor->RedrawAllViewports(); }
        }

        auto
            Tick(float InDeltaTime)
            -> bool
        {
            if (NOT _WatchesAttempted)
            { RegisterWatches(); }
            if (NOT _ActiveMaterials.IsEmpty())
            { PollMaterialBatch(); }
            if (NOT _ActiveMaterials.IsEmpty())
            { return true; }
            if (_ShadingModelReload)
            {
                _ShadingModelReload = false;
                _GlobalReload = false;
                ReloadGlobalShaders(TEXT("RecompileShaders Changed"));
            }
            else if (_GlobalReload)
            {
                _GlobalReload = false;
                ReloadGlobalShaders(TEXT("RecompileShaders /CkUsfRenderer/.*"));
            }
#if CK_USF_HAS_SHADER_SOURCE_DEPENDENCIES
            if (_ManualReload || (_AutoReload != 0 && NOT _PendingFiles.IsEmpty()
                && FPlatformTime::Seconds() - _LastSaveTime >= 0.25))
            { StartMaterialBatch(); }
#endif
            return true;
        }

        int32 _AutoReload = 1;
        bool _WatchesAttempted = false;
        bool _ManualReload = false;
        bool _GlobalReload = false;
        bool _ShadingModelReload = false;
        bool _TimeoutReported = false;
        double _LastSaveTime = 0.0;
        double _BatchStarted = 0.0;
        FTSTicker::FDelegateHandle _Ticker;
        TMap<FString, FDelegateHandle> _Watches;
        TSet<FString> _PendingFiles;
        TSet<FString> _RejectedFiles;
        TSet<FString> _RejectedRoots;
        TMap<FString, TArray<FString>> _DependencyEdges;
        TMap<FString, FSHAHash> _FileHashes;
        TMap<FString, FSHAHash> _ActiveHashes;
        TSet<FString> _ActiveRoots;
        TSet<FString> _ActivePendingFiles;
        TArray<TWeakObjectPtr<UMaterial>> _ActiveMaterials;
        TUniquePtr<FAutoConsoleVariableRef> _AutoReloadVariable;
        TUniquePtr<FAutoConsoleCommand> _ReloadCommand;
        TUniquePtr<FAutoConsoleCommand> _GlobalCommand;
        TUniquePtr<FAutoConsoleCommand> _ShadingModelCommand;
    };

    FShaderReload::FShaderReload() = default;
    FShaderReload::~FShaderReload() { Shutdown(); }

    auto
        FShaderReload::
        Get_IsSupported()
        -> bool
    {
        return CK_USF_HAS_SHADER_SOURCE_DEPENDENCIES != 0;
    }

    auto
        FShaderReload::Startup()
        -> void
    {
        if (_State.IsValid())
        { return; }
        _State = MakeUnique<FState>();
        _State->Startup();
    }

    auto
        FShaderReload::Shutdown()
        -> void
    {
        if (_State.IsValid())
        {
            _State->Shutdown();
            _State.Reset();
        }
    }
}

#undef CK_USF_HAS_SHADER_SOURCE_DEPENDENCIES
