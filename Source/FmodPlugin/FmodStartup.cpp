#include <FmodPlugin/FmodPluginPCH.h>

#include <AudioSystem/Core/AudioSystem.h>
#include <FmodPlugin/FmodIncludes.h>
#include <FmodPlugin/FmodMiddleware.h>
#include <FmodPlugin/FmodSingleton.h>
#include <FmodPlugin/Resources/FmodSoundBankResource.h>
#include <FmodPlugin/Resources/FmodSoundEventResource.h>
#include <Foundation/Configuration/Startup.h>
#include <Foundation/IO/FileSystem/FileReader.h>
#include <Foundation/IO/FileSystem/FileWriter.h>
#include <Foundation/IO/OpenDdlReader.h>
#include <Foundation/IO/OpenDdlUtils.h>
#include <Foundation/IO/OpenDdlWriter.h>
static plFmodSoundBankResourceLoader s_SoundBankResourceLoader;
static plFmodSoundEventResourceLoader s_SoundEventResourceLoader;
static plFmodMiddleware* s_pFmodMiddleware = nullptr;

// clang-format off
PL_BEGIN_SUBSYSTEM_DECLARATION(FMOD, FmodPlugin)

  BEGIN_SUBSYSTEM_DEPENDENCIES
    "Foundation",
    "Core",
    "AudioSystem"
  END_SUBSYSTEM_DEPENDENCIES

  ON_CORESYSTEMS_STARTUP
  {
  }

  ON_CORESYSTEMS_SHUTDOWN
  {
  }

  ON_HIGHLEVELSYSTEMS_STARTUP
  {
    plResourceManager::SetResourceTypeLoader<plFmodSoundBankResource>(&s_SoundBankResourceLoader);
    plResourceManager::SetResourceTypeLoader<plFmodSoundEventResource>(&s_SoundEventResourceLoader);

    plResourceManager::RegisterResourceForAssetType("Sound Bank", plGetStaticRTTI<plFmodSoundBankResource>());
    plResourceManager::RegisterResourceForAssetType("Sound Event", plGetStaticRTTI<plFmodSoundEventResource>());

    {
      plFmodSoundEventResourceDescriptor desc;
      plFmodSoundEventResourceHandle hResource = plResourceManager::CreateResource<plFmodSoundEventResource>("FmodEventMissing", std::move(desc), "Fallback for missing sound event");
      plResourceManager::SetResourceTypeMissingFallback<plFmodSoundEventResource>(hResource);
    }

    {
      plFmodSoundBankResourceDescriptor desc;
      plFmodSoundBankResourceHandle hResource = plResourceManager::CreateResource<plFmodSoundBankResource>("FmodBankMissing", std::move(desc), "Fallback for missing sound bank");
      plResourceManager::SetResourceTypeMissingFallback<plFmodSoundBankResource>(hResource);
    }
    s_pFmodMiddleware = PL_DEFAULT_NEW(plFmodMiddleware);
    plAudioSystem::GetSingleton()->Startup();
  }

  ON_HIGHLEVELSYSTEMS_SHUTDOWN
  {
    plAudioSystem::GetSingleton()->Shutdown();
    PL_DEFAULT_DELETE(s_pFmodMiddleware);
    s_pFmodMiddleware = nullptr;
    plResourceManager::SetResourceTypeLoader<plFmodSoundBankResource>(nullptr);
    plResourceManager::SetResourceTypeLoader<plFmodSoundEventResource>(nullptr);

    plFmodSoundEventResource::CleanupDynamicPluginReferences();
    plFmodSoundBankResource::CleanupDynamicPluginReferences();
  }

PL_END_SUBSYSTEM_DECLARATION;
// clang-format on

void plFmodConfiguration::Save(plOpenDdlWriter& ref_ddl) const
{
  plOpenDdlUtils::StoreString(ref_ddl, m_sMasterSoundBank, "MasterBank");
  plOpenDdlUtils::StoreUInt16(ref_ddl, m_uiVirtualChannels, "VirtualChannels");
  plOpenDdlUtils::StoreUInt32(ref_ddl, m_uiSamplerRate, "SamplerRate");

  switch (m_SpeakerMode)
  {
    case plFmodSpeakerMode::ModeStereo:
      plOpenDdlUtils::StoreString(ref_ddl, "Stereo", "Mode");
      break;
    case plFmodSpeakerMode::Mode5Point1:
      plOpenDdlUtils::StoreString(ref_ddl, "5.1", "Mode");
      break;
    case plFmodSpeakerMode::Mode7Point1:
      plOpenDdlUtils::StoreString(ref_ddl, "7.1", "Mode");
      break;
  }
}

void plFmodConfiguration::Load(const plOpenDdlReaderElement& ddl)
{
  if (const plOpenDdlReaderElement* pElement = ddl.FindChildOfType(plOpenDdlPrimitiveType::String, "MasterBank"))
  {
    m_sMasterSoundBank = pElement->GetPrimitivesString()[0];
  }

  if (const plOpenDdlReaderElement* pElement = ddl.FindChildOfType(plOpenDdlPrimitiveType::UInt16, "VirtualChannels"))
  {
    m_uiVirtualChannels = pElement->GetPrimitivesUInt16()[0];
  }

  if (const plOpenDdlReaderElement* pElement = ddl.FindChildOfType(plOpenDdlPrimitiveType::UInt32, "SamplerRate"))
  {
    m_uiSamplerRate = pElement->GetPrimitivesUInt32()[0];
  }

  if (const plOpenDdlReaderElement* pElement = ddl.FindChildOfType(plOpenDdlPrimitiveType::String, "Mode"))
  {
    auto mode = pElement->GetPrimitivesString()[0];

    if (mode == "Stereo")
      m_SpeakerMode = plFmodSpeakerMode::ModeStereo;
    else if (mode == "7.1")
      m_SpeakerMode = plFmodSpeakerMode::Mode7Point1;
    else
      m_SpeakerMode = plFmodSpeakerMode::Mode5Point1;
  }
}

bool plFmodConfiguration::operator==(const plFmodConfiguration& rhs) const
{
  if (m_sMasterSoundBank != rhs.m_sMasterSoundBank)
    return false;
  if (m_uiVirtualChannels != rhs.m_uiVirtualChannels)
    return false;
  if (m_uiSamplerRate != rhs.m_uiSamplerRate)
    return false;
  if (m_SpeakerMode != rhs.m_SpeakerMode)
    return false;

  return true;
}

plResult plFmodAssetProfiles::Save(plStringView sFile) const
{
  plFileWriter file;
  PL_SUCCEED_OR_RETURN(file.Open(sFile));

  plOpenDdlWriter ddl;
  ddl.SetOutputStream(&file);

  for (auto it = m_AssetProfiles.GetIterator(); it.IsValid(); ++it)
  {
    if (!it.Key().IsEmpty())
    {
      ddl.BeginObject("Platform", it.Key());

      it.Value().Save(ddl);

      ddl.EndObject();
    }
  }

  return PL_SUCCESS;
}

plResult plFmodAssetProfiles::Load(plStringView sFile)
{
  m_AssetProfiles.Clear();

  plFileReader file;
  PL_SUCCEED_OR_RETURN(file.Open(sFile));

  plOpenDdlReader ddl;
  PL_SUCCEED_OR_RETURN(ddl.ParseDocument(file));

  const plOpenDdlReaderElement* pRoot = ddl.GetRootElement();
  const plOpenDdlReaderElement* pChild = pRoot->GetFirstChild();

  while (pChild)
  {
    if (pChild->IsCustomType("Platform") && pChild->HasName())
    {
      auto& cfg = m_AssetProfiles[pChild->GetName()];

      cfg.Load(*pChild);
    }

    pChild = pChild->GetSibling();
  }

  return PL_SUCCESS;
}

PL_STATICLINK_FILE(FmodPlugin, FmodPlugin_FmodStartup);
