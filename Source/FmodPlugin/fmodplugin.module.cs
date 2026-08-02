using PlasmaBuild.Core.Configuration;
using PlasmaBuild.Core.Rules;

public class FmodPluginModule : ModuleRules
{
    public FmodPluginModule(BuildContext context) : base(context)
    {
        PlasmaPackageSdk.ConfigurePackageModule(this, context, "Source/FmodPlugin", "FmodPluginPCH.h",
            "BUILDSYSTEM_BUILDING_FMODPLUGIN_LIB");

        // Headers and import libraries from the developer's own FMOD install. No FMOD binaries
        // are copied into this package - see Configs/PlasmaPackageFmod.pb.cs.
        PlasmaPackageFmod.ConfigureConsumer(this, context);
    }
}
