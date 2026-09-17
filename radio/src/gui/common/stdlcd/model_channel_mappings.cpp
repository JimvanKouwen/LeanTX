#include "edgetx.h"
#include "physical_input.h"

void menuModelChannelMappings(event_t event)
{
  SIMPLE_MENU(STR_CHANNEL_MAPPING, menuTabModel, MENU_MODEL_CHANNEL_MAPPING,
              HEADER_LINE + MAX_OUTPUT_CHANNELS);
  int selected = menuVerticalPosition - HEADER_LINE;
  for (unsigned row = 0; row < NUM_BODY_LINES; ++row) {
    unsigned channel = menuVerticalOffset + row;
    if (channel >= MAX_OUTPUT_CHANNELS) break;
    coord_t y = MENU_HEADER_HEIGHT + 1 + row * FH;
    LcdFlags flags = selected == int(channel) ? (s_editMode ? BLINK | INVERS : INVERS) : 0;
    putsChn(0, y, channel + 1, 0);
    auto& mapping = g_model.channelMappings[channel];
    int source = physicalInputToSource(mapping.source);
    drawSource(6 * FW, y, source, flags);
    if (selected == int(channel) && s_editMode > 0) {
      source = checkIncDec(event, source, MIXSRC_NONE, MIXSRC_LAST_SWITCH,
                          EE_MODEL | NO_INCDEC_MARKS, isPhysicalSourceAvailable);
      if (checkIncDec_Ret) mapping.source = physicalInputFromSource(source);
    }
  }
}
