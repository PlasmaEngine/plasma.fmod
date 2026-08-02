#pragma once

#include <Core/Messages/EventMessage.h>
#include <Core/ResourceManager/Resource.h>
#include <FmodPlugin/Components/FmodComponent.h>

struct plFmodParameterId
{
public:
  PL_ALWAYS_INLINE void Invalidate() { m_uiValue = -1; }
  PL_ALWAYS_INLINE bool IsInvalidated() const { return m_uiValue == -1; }

private:
  plUInt64 m_uiValue = -1;
};

class plPhysicsWorldModuleInterface;
struct plMsgSetFloatParameter;

class plFmodEventComponentManager : public plComponentManager<class plFmodEventComponent, plBlockStorageType::FreeList>
{
public:
  plFmodEventComponentManager(plWorld* pWorld);

  virtual void Initialize() override;
  virtual void Deinitialize() override;

private:
  friend class plFmodEventComponent;

  struct OcclusionState
  {
    plFmodEventComponent* m_pComponent = nullptr;
    plFmodParameterId m_OcclusionParamId;
    plUInt32 m_uiRaycastHits = 0;
    plUInt8 m_uiNextRayIndex = 0;
    plUInt8 m_uiNumUsedRays = 0;
    float m_fRadius = 0.0f;
    float m_fLastOcclusionValue = -1.0f;

    float GetOcclusionValue(float fThreshold) const { return plMath::Clamp((m_fLastOcclusionValue - fThreshold) / plMath::Max(1.0f - fThreshold, 0.0001f), 0.0f, 1.0f); }
  };

  plUInt32 m_uiFirstComponentIndex = 0;
  plDynamicArray<OcclusionState> m_OcclusionStates;

  plUInt32 AddOcclusionState(plFmodEventComponent* pComponent, plFmodParameterId occlusionParamId, float fRadius);
  void RemoveOcclusionState(plUInt32 uiIndex);
  const OcclusionState& GetOcclusionState(plUInt32 uiIndex) const { return m_OcclusionStates[uiIndex]; }

  void ShootOcclusionRays(
    OcclusionState& state, plVec3 listenerPos, plUInt32 uiNumRays, const plPhysicsWorldModuleInterface* pPhysicsWorldModule, plTime deltaTime);
  void UpdateOcclusion(const plWorldModule::UpdateContext& context);
  void UpdateEvents(const plWorldModule::UpdateContext& context);

  void ResourceEventHandler(const plResourceEvent& e);
};

using plFmodSoundEventResourceHandle = plTypedResourceHandle<class plFmodSoundEventResource>;

struct plResourceEvent;

//////////////////////////////////////////////////////////////////////////

/// \brief Sent when a plFmodEventComponent finishes playing a sound. Not sent for one-shot sound events.
struct PL_FMODPLUGIN_DLL plMsgFmodSoundFinished : public plEventMessage
{
  PL_DECLARE_MESSAGE_TYPE(plMsgFmodSoundFinished, plEventMessage);
};


//////////////////////////////////////////////////////////////////////////

/// \brief Represents a sound (called an 'event') in the FMOD sound system.
///
/// Provides functions to start, pause, stop a sound, set parameters, change volume, pitch etc.
class PL_FMODPLUGIN_DLL plFmodEventComponent : public plFmodComponent
{
  PL_DECLARE_COMPONENT_TYPE(plFmodEventComponent, plFmodComponent, plFmodEventComponentManager);

  //////////////////////////////////////////////////////////////////////////
  // plComponent

public:
  virtual void SerializeComponent(plWorldWriter& inout_stream) const override;
  virtual void DeserializeComponent(plWorldReader& inout_stream) override;

protected:
  virtual void OnSimulationStarted() override;
  virtual void OnDeactivated() override;


  //////////////////////////////////////////////////////////////////////////
  // plFmodComponent

private:
  virtual void plFmodComponentIsAbstract() override {}
  friend class plComponentManagerSimple<class plFmodEventComponent, plComponentUpdateType::WhenSimulating>;


  //////////////////////////////////////////////////////////////////////////
  // plFmodEventComponent

public:
  plFmodEventComponent();
  ~plFmodEventComponent();

  void SetPaused(bool b);                                                               // [ property ]
  bool GetPaused() const { return m_bPaused; }                                          // [ property ]

  void SetUseOcclusion(bool b);                                                         // [ property ]
  bool GetUseOcclusion() const { return m_bUseOcclusion; }                              // [ property ]

  void SetOcclusionCollisionLayer(plUInt8 uiCollisionLayer);                            // [ property ]
  plUInt8 GetOcclusionCollisionLayer() const { return m_uiOcclusionCollisionLayer; }    // [ property ]

  void SetOcclusionThreshold(float fThreshold);                                         // [ property ]
  float GetOcclusionThreshold() const;                                                  // [ property ]

  void SetPitch(float f);                                                               // [ property ]
  float GetPitch() const { return m_fPitch; }                                           // [ property ]

  void SetVolume(float f);                                                              // [ property ]
  float GetVolume() const { return m_fVolume; }                                         // [ property ]

  void SetSoundEvent(const plFmodSoundEventResourceHandle& hSoundEvent);                // [ property ]
  const plFmodSoundEventResourceHandle& GetSoundEvent() const { return m_hSoundEvent; } // [ property ]

  plEnum<plOnComponentFinishedAction> m_OnFinishedAction;                               // [ property ]

  void SetShowDebugInfo(bool bShow);                                                    // [ property ]
  bool GetShowDebugInfo() const;                                                        // [ property ]

  /// \brief If set, the global game speed does not affect the pitch of this event.
  ///
  /// This is important for global sounds, such as music or UI effects, so that they always play at their regular speed,
  /// even when the game is in slow motion.
  void SetNoGlobalPitch(bool bEnable); // [ property ]
  bool GetNoGlobalPitch() const;       // [ property ]

  /// \brief Makes the sound play.
  ///
  /// If it was not yet playing, it starts playing a new sound.
  /// If it was already playing, but paused, playback is resumed.
  /// If it was already playing, there is no change.
  void Play(); // [ scriptable ]

  /// \brief If a sound is playing, it pauses at the current play position.
  ///
  /// Call Play() to resume playing.
  void Pause(); // [ scriptable ]

  /// \brief Interrupts the sound playback abruptly.
  void Stop(); // [ scriptable ]

  /// \brief Stops the sound, by fading it out over a short period.
  void FadeOut(); // [ scriptable ]

  /// \brief Plays a completely new sound at the location of this component and with all its current properties.
  ///
  /// Pitch, volume, position, direction and velocity are copied to the new sound instance.
  /// The new sound event then plays to the end and cannot be controlled through this component any further.
  /// If the referenced FMOD sound event is not a "one shot" event, this function is ignored.
  /// The event that is controlled through this component is unaffected by this.
  void StartOneShot(); // [ scriptable ]

  /// \brief Triggers an FMOD sound cue. Whatever that is useful for.
  void SoundCue(); // [ scriptable ]

  /// \brief Tries to find the FMOD event parameter by name. Returns the parameter id or -1, if no such parameter exists.
  plFmodParameterId FindParameter(const char* szName) const;

  /// \brief Sets an FMOD event parameter value. See FindParameter() for the index.
  void SetParameter(plFmodParameterId paramId, float fValue);

  /// \brief Gets an FMOD event parameter value. See FindParameter() for the index. Returns 0, if the index is invalid.
  float GetParameter(plFmodParameterId paramId) const;

  /// \brief Sets an event parameter via name lookup, so this is less efficient than SetParameter()
  void SetEventParameter(const char* szParamName, float fValue); // [ scriptable ]

  /// \brief Allows one to set event parameters through the generic plMsgSetFloatParameter message.
  ///
  /// Requires event parameter lookup via a name, so this is less efficient than SetParameter().
  void OnMsgSetFloatParameter(plMsgSetFloatParameter& ref_msg); // [ msg handler ]

protected:
  void OnMsgDeleteGameObject(plMsgDeleteGameObject& msg);       // [ msg handler ]

  void Update();
  void UpdateParameters(FMOD::Studio::EventInstance* pInstance);
  void UpdateOcclusion();

  /// Called when the event resource has been unloaded (for a reload)
  void InvalidateResource(bool bTryToRestore);


  bool m_bPaused;
  bool m_bUseOcclusion;
  plUInt8 m_uiOcclusionThreshold;
  plUInt8 m_uiOcclusionCollisionLayer;
  float m_fPitch;
  float m_fVolume;
  plInt32 m_iTimelinePosition = -1; // used to restore a sound after reloading the resource
  plUInt32 m_uiOcclusionStateIndex = plInvalidIndex;
  plFmodSoundEventResourceHandle m_hSoundEvent;

  FMOD::Studio::EventDescription* m_pEventDesc;
  FMOD::Studio::EventInstance* m_pEventInstance;

  plEventMessageSender<plMsgFmodSoundFinished> m_SoundFinishedEventSender; // [ event ]
};
