// #pl-version 1

using PlasmaBuild.Core.Configuration;
using PlasmaBuild.Core.Rules;

public class PlasmaFmodWorkspace : WorkspaceRules
{
    public PlasmaFmodWorkspace(BuildContext context) : base(context)
    {
        TargetNames.Add("FmodPlugin");
        TargetNames.Add("EditorPluginFmod");
    }
}
