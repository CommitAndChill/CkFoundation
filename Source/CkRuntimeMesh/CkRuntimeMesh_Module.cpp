#include "CkRuntimeMesh/CkRuntimeMesh_Fragment.h"
#include "CkRuntimeMesh/Display/CkRuntimeMeshDisplay_Processor.h"

#include <Engine/World.h>
#include <Modules/ModuleManager.h>

class FCkRuntimeMeshModule final : public IModuleInterface
{
public:
    auto StartupModule() -> void override
    {
        _WorldCleanup = FWorldDelegates::OnWorldCleanup.AddLambda(
            [](UWorld* InWorld, bool, bool)
            {
                ck::runtimemesh::display::ReleaseWorld(InWorld);
                ck::runtimemesh::CancelWorld(InWorld);
            });
    }

    auto ShutdownModule() -> void override
    {
        FWorldDelegates::OnWorldCleanup.Remove(_WorldCleanup);
    }

private:
    FDelegateHandle _WorldCleanup;
};

IMPLEMENT_MODULE(FCkRuntimeMeshModule, CkRuntimeMesh)
