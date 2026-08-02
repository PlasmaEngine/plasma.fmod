// #pl-version 1
//
// Locates an FMOD SDK to build against.
//
// This package deliberately ships no FMOD binaries. FMOD is licensed middleware and the package
// registry is public, so redistributing fmod.dll and fmodstudio.dll through it is not something the
// engine's vendoring arrangement covers. The package carries our plugin code only; the FMOD runtime
// comes from the developer's own installation, the same arrangement the Amplitude plugin already
// uses with AM_SDK_PATH.
//
// Consequence at runtime: plFmodPlugin.dll imports from fmod.dll, so it will not load unless FMOD's
// libraries are reachable. That is stated in the package description rather than left to be
// discovered as a load failure.

using System;
using System.IO;
using PlasmaBuild.Core.Configuration;
using PlasmaBuild.Core.Rules;

public static class PlasmaPackageFmod
{
    /// \brief Finds the directory that holds FMOD's "api" folder.
    ///
    /// FMOD_SDK_PATH wins so a developer can build against the version they actually ship with. An
    /// SDK that still vendors FMOD is used as a fallback, which keeps the in-engine workflow working
    /// unchanged while the package itself stays clean of FMOD binaries.
    public static bool TryGetSdk(out string apiRoot, out string reason)
    {
        apiRoot = string.Empty;
        reason = string.Empty;

        var fromEnv = Environment.GetEnvironmentVariable("FMOD_SDK_PATH");
        if (!string.IsNullOrWhiteSpace(fromEnv))
        {
            fromEnv = Path.GetFullPath(fromEnv.Trim());

            if (HasSdkFiles(fromEnv))
            {
                apiRoot = fromEnv;
                return true;
            }

            reason = $"FMOD_SDK_PATH is '{fromEnv}', but no FMOD SDK was found there. It should name the " +
                     "directory containing 'api' - for a default install, the 'FMOD Studio API Windows' folder.";
            return false;
        }

        var vendored = Path.Combine(PlasmaPackageSdk.Root, "Code", "ThirdParty", "fmod", "windows");
        if (HasSdkFiles(vendored))
        {
            apiRoot = vendored;
            return true;
        }

        reason = "No FMOD SDK found. Set FMOD_SDK_PATH to the directory containing 'api' (for a default " +
                 "install, 'FMOD Studio API Windows'). This package ships no FMOD binaries by design.";
        return false;
    }

    /// \brief Include paths and import libraries. Deliberately copies no runtime DLLs.
    public static void ConfigureConsumer(ModuleRules rules, BuildContext context)
    {
        if (context.Platform != TargetPlatform.Windows)
        {
            throw new PlatformNotSupportedException("The FMOD plugin is only supported on Windows desktop.");
        }

        if (!TryGetSdk(out var apiRoot, out var reason))
        {
            throw new DirectoryNotFoundException(reason);
        }

        rules.PublicIncludePaths.Add(Path.Combine(apiRoot, "api", "core", "inc"));
        rules.PublicIncludePaths.Add(Path.Combine(apiRoot, "api", "studio", "inc"));
        rules.PublicIncludePaths.Add(Path.Combine(apiRoot, "api", "fsbank", "inc"));

        // Import libraries only. The matching DLLs are the developer's to provide - adding them to
        // the package output is exactly what this arrangement exists to avoid.
        rules.PublicLibraries.Add(Path.Combine(apiRoot, "api", "core", "lib", "x64", "fmod_vc.lib"));
        rules.PublicLibraries.Add(Path.Combine(apiRoot, "api", "studio", "lib", "x64", "fmodstudio_vc.lib"));
        rules.PublicLibraries.Add(Path.Combine(apiRoot, "api", "fsbank", "lib", "x64", "fsbank_vc.lib"));
    }

    private static bool HasSdkFiles(string apiRoot)
    {
        return File.Exists(Path.Combine(apiRoot, "api", "core", "inc", "fmod.h")) &&
               File.Exists(Path.Combine(apiRoot, "api", "core", "lib", "x64", "fmod_vc.lib")) &&
               File.Exists(Path.Combine(apiRoot, "api", "studio", "lib", "x64", "fmodstudio_vc.lib")) &&
               File.Exists(Path.Combine(apiRoot, "api", "fsbank", "lib", "x64", "fsbank_vc.lib"));
    }
}
