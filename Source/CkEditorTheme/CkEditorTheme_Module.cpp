#include "CkEditorTheme_Module.h"

#define LOCTEXT_NAMESPACE "FCkEditorThemeModule"

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkEditorThemeModule::
    StartupModule()
    -> void
{
    _Controller.Start();
}

auto
    FCkEditorThemeModule::
    ShutdownModule()
    -> void
{
    _Controller.Stop();
}

// --------------------------------------------------------------------------------------------------------------------

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FCkEditorThemeModule, CkEditorTheme)
