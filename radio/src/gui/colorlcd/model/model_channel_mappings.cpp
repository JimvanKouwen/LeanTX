#include "model_channel_mappings.h"
#include "edgetx.h"
#include "sourcechoice.h"
#include "static.h"

void ModelChannelMappingsPage::build(Window* window)
{
  window->setFlexLayout(LV_FLEX_FLOW_COLUMN, PAD_SMALL);
  for (unsigned channel = 0; channel < MAX_OUTPUT_CHANNELS; ++channel) {
    auto row = new Window(window, rect_t{0, 0, lv_pct(100), LV_SIZE_CONTENT});
    row->setFlexLayout(LV_FLEX_FLOW_ROW, PAD_SMALL);
    new StaticText(row, rect_t{}, getSourceString(MIXSRC_FIRST_CH + channel));
    auto choice = new SourceChoice(row, rect_t{}, MIXSRC_NONE, MIXSRC_LAST_SWITCH,
      [=]() { return physicalInputToSource(g_model.channelMappings[channel].source); },
      [=](int16_t source) {
        g_model.channelMappings[channel].source = physicalInputFromSource(source);
        storageDirty(EE_MODEL);
      });
    choice->setAvailableHandler(isPhysicalSourceAvailable);
  }
}
