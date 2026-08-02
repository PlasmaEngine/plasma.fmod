using PlasmaBuild.Core.Configuration;
using PlasmaBuild.Core.Rules;

public class FmodPluginTarget : TargetRules
{
    public FmodPluginTarget(BuildContext context) : base(context)
    {
        Type = TargetType.SharedLibrary;
        OutputName = "plFmodPlugin";
        OutputDirectory = PlasmaPackageSdk.PackageBinaryDirectory(context);

        UsePCHFiles = true;
        UseUnityBuild = true;
        UseAdaptiveUnityBuild = true;
        UseIncrementalLinking = true;

        ExtraModules.Add("FmodPluginModule");
    }
}
