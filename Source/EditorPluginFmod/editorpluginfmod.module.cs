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

        // The sound windows' lists and the project settings content are engine-drawn: EditorUi and the retained UI runtime.
        PlasmaPackageSdk.AddImportLibrary(this, context, "plEditorUi");
        PlasmaPackageSdk.AddImportLibrary(this, context, "plGameUiPlugin");
    }
}
