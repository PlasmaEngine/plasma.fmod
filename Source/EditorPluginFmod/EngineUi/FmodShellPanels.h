#pragma once

/// \brief The FMOD Sound Bank and Sound Event windows in the engine shell.
///
/// Each window shows one panel that stacks the asset's status banner, its body and, for a bank, the Sub-assets list:
/// the banner and the property grid (a bank) with the Sub-assets list below them, or the banner (an event) above the
/// event's empty state.
namespace plFmodShellPanels
{
  void Register();
  void Unregister();
} // namespace plFmodShellPanels
