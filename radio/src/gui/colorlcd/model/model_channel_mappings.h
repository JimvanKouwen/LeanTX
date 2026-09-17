#pragma once
#include "pagegroup.h"
class ModelChannelMappingsPage : public PageGroupItem {
 public:
  ModelChannelMappingsPage(const PageDef& pageDef) : PageGroupItem(pageDef) {}
  void build(Window* window) override;
};
