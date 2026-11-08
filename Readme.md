# plasma.fmod

FMOD Studio audio for PlasmaEngine — runtime playback plus editor support for sound banks and sound
events. Moved out of the engine tree to be built and distributed as a package.

Published as [`plasma.fmod`](https://plasmaengine.github.io/PlasmaPackages/index.plPackageIndex) in
the Plasma package registry.

## FMOD is not included

This package ships Plasma's own plugin binaries and nothing else. FMOD is licensed middleware and
the package registry is public, so `fmod.dll` and `fmodstudio.dll` are not redistributed here.

To use the plugin:

1. Download the FMOD Studio API for Windows from [fmod.com](https://www.fmod.com/download).
2. Put `fmod.dll` and `fmodstudio.dll` next to the editor executable, or anywhere else on the
   library search path.

`plFmodPlugin.dll` imports from both, so without them it will not load. Note that FMOD's own
licensing terms govern shipping it in a game — check them for your project.

## Layout

```
Package.plPackage        the manifest - identity, compatibility, components
Fmod.plPluginBundle      legacy descriptor, kept until the engine reads manifests directly
Source/
  FmodPlugin/            runtime -> plFmodPlugin
  EditorPluginFmod/      editor  -> plEditorPluginFmod  (asset editors, settings dialog)
Configs/                 build rules for building against an installed engine SDK
Branding/                icon and banner
```

There is no editor-engine-process plugin: the editor drives FMOD through the runtime plugin
directly.

## Dependencies

None resolvable through the registry. The FMOD runtime is an external prerequisite, as above.

`Fmod.plPluginBundle` declares `%ExclusiveFeatures{"Sound"}`, so enabling this plugin excludes the
other audio plugins — a project plays sound through one backend.

## Building

Requires an engine SDK and an FMOD SDK:

```bash
PLASMA_SDK_ROOT=/path/to/engine FMOD_SDK_PATH=/path/to/FMOD_Studio_API_Windows plasmabuild build EditorPluginFmod
```

`FMOD_SDK_PATH` names the directory containing `api/` — for a default install, the
`FMOD Studio API Windows` folder. If it is unset, the build falls back to an FMOD vendored inside
the engine SDK, which is what makes the in-engine workflow keep working unchanged.

Output lands in `Bin/Windows_x64_Development/`. Only Plasma's DLLs are produced; the build links
FMOD's import libraries but deliberately copies no FMOD binaries.

## Publishing a new version

Staging and publishing are driven from the engine and registry repositories:

```bash
# engine repo - stage and validate, emitting machine-readable output
python Utilities/PackageInspector.py <staged-package-dir> --json <out>.json

# registry repo - upload blobs and record the release
python tools/publish_package.py <out>.json
```

Versions are immutable. A mistake is fixed by publishing a new patch version, never by replacing an
existing one — every lock file that already trusts a version must keep resolving to the same bytes.
