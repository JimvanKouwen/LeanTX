#include "model_channel_mappings.h"
#include "edgetx.h"
#include "choice.h"
#include "static.h"

void ModelChannelMappingsPage::build(Window* window)
{
  window->setFlexLayout(LV_FLEX_FLOW_COLUMN, PAD_SMALL);
  for (unsigned channel = 0; channel < MAX_OUTPUT_CHANNELS; ++channel) {
    auto row = new Window(window, rect_t{0, 0, lv_pct(100), LV_SIZE_CONTENT});
    row->setFlexLayout(LV_FLEX_FLOW_ROW, PAD_SMALL);
    char label[32];
    strAppendStringWithIndex(label, STR_CH, channel + 1);
    new StaticText(row, rect_t{}, label);
    auto choice = new Choice(row, rect_t{}, int(PhysicalInputId::None), int(physicalSwitch(31)),
      [=]() { return int(g_model.channelMappings[channel].source); },
      [=](int source) {
        g_model.channelMappings[channel].source = PhysicalInputId(source);
        storageDirty(EE_MODEL);
      }, STR_SOURCE);
    choice->setAvailableHandler(isChannelMappingInputAvailable);
    choice->setTextHandler([](int source) {
      char label[32];
      getPhysicalInputLabel(label, PhysicalInputId(source));
      return std::string(label);
    });
  }
}
