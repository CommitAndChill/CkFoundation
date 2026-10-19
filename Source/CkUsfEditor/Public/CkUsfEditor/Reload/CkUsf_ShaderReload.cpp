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
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Modules/ModuleManager.h"
#include "ShaderCore.h"
#include "UObject/UObjectIterator.h"

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
            _PendingFiles.Reset();
        }

    private:
        auto
            RequestMaterialReload()
            -> void
        {
            _ManualReload = true;
        }

        auto
            RequestGlobalReload()
            -> void
        {
            _GlobalReload = true;
        }

        auto
            RequestShadingModelReload()
            -> void
        {
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
                _PendingFiles.Add(VirtualPath, PhysicalPath);
                _LastSaveTime = FPlatformTime::Seconds();
                _PendingRemoval |= Change.Action == FFileChangeData::FCA_Removed;
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

        auto
            StartMaterialBatch()
            -> void
        {
            const auto Manual = _ManualReload;
            const auto Removal = _PendingRemoval;
            _ManualReload = false;
            _PendingRemoval = false;
            const auto PendingFiles = MoveTemp(_PendingFiles);
            _PendingFiles.Reset();
            auto ChangedFiles = TSet<FString>{};
            auto MissingSource = false;
            for (const auto& Pending : PendingFiles)
            {
                auto Contents = TArray<uint8>{};
                if (NOT FFileHelper::LoadFileToArray(Contents, *Pending.Value))
                {
                    _FileHashes.Remove(Pending.Key);
                    Warning(TEXT("CkUsf reload rejected: shader source is absent or unreadable [{}]. Restore it and save again, or use Ck_Usf_ReloadShaders."), Pending.Key);
                    MissingSource = true;
                    ChangedFiles.Add(Pending.Key);
                    continue;
                }
                const auto Hash = FSHA1::HashBuffer(Contents.GetData(), Contents.Num());
                if (NOT _FileHashes.Contains(Pending.Key) || _FileHashes.FindChecked(Pending.Key) != Hash)
                {
                    _FileHashes.Add(Pending.Key, Hash);
                    ChangedFiles.Add(Pending.Key);
                }
            }
            if (NOT Manual && ChangedFiles.IsEmpty())
            { return; }
            if (MissingSource)
            { return; }

            FlushShaderFileCache();
            auto Candidates = TArray<TWeakObjectPtr<UMaterial>>{};
            for (TObjectIterator<UMaterial> It{RF_ClassDefaultObject, true, EInternalObjectFlags::Garbage}; It; ++It)
            {
                if (It->HasAnyFlags(RF_ClassDefaultObject | RF_BeginDestroyed | RF_FinishDestroyed))
                { continue; }
                const auto Includes = GetLookIncludes(**It);
                if (Includes.IsEmpty())
                { continue; }
                auto Affected = Manual || Removal;
                auto SourcesValid = true;
                for (const auto& Include : Includes)
                {
                    auto Error = FString{};
                    if (TryGetShaderFileHash(*Include, GMaxRHIShaderPlatform, &Error) == nullptr)
                    {
                        Warning(TEXT("CkUsf reload rejected material [{}], include [{}]: {}"), It->GetPathName(), Include, Error);
                        SourcesValid = false;
                        break;
                    }
                    if (NOT Affected)
                    {
                        auto Dependencies = TArray<FString>{};
                        GetShaderIncludes(*Include, *Include, Dependencies, GMaxRHIShaderPlatform);
                        Dependencies.Add(Include);
                        for (const auto& Dependency : Dependencies)
                        {
                            if (ChangedFiles.Contains(Dependency))
                            { Affected = true; break; }
                        }
                    }
                }
                if (SourcesValid && Affected)
                { Candidates.Add(TWeakObjectPtr<UMaterial>{*It}); }
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
                for (const auto& Error : ContractErrors)
                { Warning(TEXT("CkUsf reload rejected before material changes: {}"), Error); }
                return;
            }
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
                    _ActiveMaterials.Add(Material);
                }
            }
            _BatchStarted = FPlatformTime::Seconds();
            _TimeoutReported = false;
            Log(TEXT("CkUsf reload started: [{}] loaded master(s), [{}] changed source(s). Existing material and MID identities are preserved."),
                _ActiveMaterials.Num(), ChangedFiles.Num());
        }

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
                        || (NOT TimedOut && Resource.GetCompileErrors().IsEmpty() && Resource.GetGameThreadShaderMap() == nullptr))
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
                    || NOT Resource.GetCompileErrors().IsEmpty() || Resource.GetGameThreadShaderMap() == nullptr)
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
            _ActiveMaterials.Reset();
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
            if (_ManualReload || (_AutoReload != 0 && NOT _PendingFiles.IsEmpty()
                && FPlatformTime::Seconds() - _LastSaveTime >= 0.25))
            { StartMaterialBatch(); }
            return true;
        }

        int32 _AutoReload = 1;
        bool _WatchesAttempted = false;
        bool _ManualReload = false;
        bool _GlobalReload = false;
        bool _ShadingModelReload = false;
        bool _PendingRemoval = false;
        bool _TimeoutReported = false;
        double _LastSaveTime = 0.0;
        double _BatchStarted = 0.0;
        FTSTicker::FDelegateHandle _Ticker;
        TMap<FString, FDelegateHandle> _Watches;
        TMap<FString, FString> _PendingFiles;
        TMap<FString, FSHAHash> _FileHashes;
        TArray<TWeakObjectPtr<UMaterial>> _ActiveMaterials;
        TUniquePtr<FAutoConsoleVariableRef> _AutoReloadVariable;
        TUniquePtr<FAutoConsoleCommand> _ReloadCommand;
        TUniquePtr<FAutoConsoleCommand> _GlobalCommand;
        TUniquePtr<FAutoConsoleCommand> _ShadingModelCommand;
    };

    FShaderReload::FShaderReload() = default;
    FShaderReload::~FShaderReload() { Shutdown(); }

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
