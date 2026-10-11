#include "CkPixelSpriteEditor/CkPixelSpriteEditor_ViewerBridge.h"
#include "Modules/ModuleManager.h"

namespace ck::pixel_sprite_editor
{
    auto
        Get_OpenViewer()
        -> FOpenViewer&
    {
        static auto Delegate = FOpenViewer{};
        return Delegate;
    }
}

IMPLEMENT_MODULE(FDefaultModuleImpl, CkPixelSpriteEditor)
