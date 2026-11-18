#include "Modules/ModuleManager.h"

auto CkOnlineLobby_ReleaseOrphans() -> void;

class FCkOnlineLobbyModule : public IModuleInterface
{
public:
    auto SupportsDynamicReloading() -> bool override { return false; }

    auto ShutdownModule() -> void override
    {
        CkOnlineLobby_ReleaseOrphans();
    }
};

IMPLEMENT_MODULE(FCkOnlineLobbyModule, CkOnlineLobby)
