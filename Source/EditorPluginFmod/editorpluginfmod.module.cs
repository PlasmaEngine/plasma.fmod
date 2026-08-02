using PlasmaBuild.Core.Configuration;
using PlasmaBuild.Core.Rules;

public class EditorPluginFmodModule : ModuleRules
{
    public EditorPluginFmodModule(BuildContext context) : base(context)
    {
        PlasmaPackageSdk.ConfigurePackageModule(this, context, "Source/EditorPluginFmod", "EditorPluginFmodPCH.h",
            "BUILDSYSTEM_BUILDING_EDITORPLUGINFMOD_LIB");

        PlasmaPackageSdk.ConfigureEditorSdkConsumer(this, context);
        PlasmaPackageFmod.ConfigureConsumer(this, context);

        PlasmaPackageSdk.AddPackagePluginImport(this, context, "Source/FmodPlugin", "plFmodPlugin");

        PlasmaPackageQt.ConfigureQtModule(this, context, "EditorPluginFmodModule", "Source/EditorPluginFmod",
            "Core", "Gui", "Widgets");
    }
}
