#pragma once

#include <EditorFramework/DragDrop/ComponentDragDropHandler.h>

class plSoundEventComponentDragDropHandler : public plComponentDragDropHandler
{
  PL_ADD_DYNAMIC_REFLECTION(plSoundEventComponentDragDropHandler, plComponentDragDropHandler);

public:
  float CanHandle(const plDragDropInfo* pInfo) const override;

  virtual void OnDragBegin(const plDragDropInfo* pInfo) override;
};
