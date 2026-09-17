// LeanTX telemetry navigation regression tests. GPL-2.0-or-later.
#include "gtests.h"
#include "edgetx.h"

#if defined(PCBTARANIS)
TEST(TelemetryNavigation, TeleKeyLeavesMainViewUnchanged)
{
  MODEL_RESET();
  auto previousHandler = menuHandlers[0];
  auto previousLevel = menuLevel;
  menuLevel = 0;
  menuHandlers[0] = menuMainView;
  const auto modelView = g_model.view;
  const auto radioView = g_eeGeneral.view;

  for (event_t event : {EVT_KEY_FIRST(KEY_TELE), EVT_KEY_BREAK(KEY_TELE),
                        EVT_KEY_LONG(KEY_TELE)}) {
    menuMainView(event);
    EXPECT_EQ(menuMainView, menuHandlers[0]);
    EXPECT_EQ(0, menuLevel);
    EXPECT_EQ(modelView, g_model.view);
    EXPECT_EQ(radioView, g_eeGeneral.view);
  }

  menuHandlers[0] = previousHandler;
  menuLevel = previousLevel;
}
#endif
