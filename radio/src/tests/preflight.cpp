#include "gtests.h"
#include "hal/adc_driver.h"
#include "tasks/mixer_task.h"

extern bool isThrottleWarningAlertNeeded();

class PreflightTest : public EdgeTxTest {};

TEST_F(PreflightTest, ThrottleModesAndCustomPosition)
{
  mixerTaskStart();
  g_model.disableThrottleWarning = false;
  for (uint8_t mode = 0; mode < 4; ++mode) {
    g_eeGeneral.stickMode = mode;
    // The other vertical stick must not affect the warning.
    for (uint8_t stick = 0; stick < MAX_STICKS; ++stick)
      anaSetFiltered(stick, RESX);
    auto throttle = inputMappingConvertMode(inputMappingGetThrottle());
    g_model.enableCustomThrottleWarning = false;
    anaSetFiltered(throttle, -RESX + THRCHK_DEADBAND);
    EXPECT_FALSE(isThrottleWarningAlertNeeded()) << int(mode);
    anaSetFiltered(throttle, -RESX + THRCHK_DEADBAND + 1);
    EXPECT_TRUE(isThrottleWarningAlertNeeded()) << int(mode);
    g_model.enableCustomThrottleWarning = true;
    g_model.customThrottleWarningPosition = 25;
    for (int delta : {-17, -16, 0, 16, 17}) {
      anaSetFiltered(throttle, RESX / 4 + delta);
      EXPECT_EQ(abs(delta) > THRCHK_DEADBAND, isThrottleWarningAlertNeeded());
    }
  }
  g_model.disableThrottleWarning = true;
  EXPECT_FALSE(isThrottleWarningAlertNeeded());
  mixerTaskStop();
}

TEST_F(PreflightTest, SwitchCaptureReadsHardwareWithoutMovementPolling)
{
  int sw = findHwSwitch(SWITCH_3POS);
  ASSERT_GE(sw, 0);
  g_model.switchWarning = 0;
  g_model.setSwitchWarning(sw, 1);
  for (int position : {1, 0, -1}) {
    simuSetSwitch(sw, position);
    EXPECT_EQ(position + 2, g_model.getSwitchStateForWarning(sw));
    setAllPreflightSwitchStates();
    EXPECT_EQ(position + 2, g_model.getSwitchWarning(sw));
    uint16_t badPots = 0;
    EXPECT_FALSE(isSwitchWarningRequired(badPots));
    simuSetSwitch(sw, position == 1 ? -1 : 1);
    EXPECT_TRUE(isSwitchWarningRequired(badPots));
  }
  g_model.setSwitchWarning(sw, 0);
  setAllPreflightSwitchStates();
  EXPECT_EQ(0, g_model.getSwitchWarning(sw));
}

TEST_F(PreflightTest, WarningCaptureDoesNotConsumeSwitchMovement)
{
  int sw = findHwSwitch(SWITCH_3POS);
  ASSERT_GE(sw, 0);
  simuSetSwitch(sw, -1);
  getMovedSwitch();
  g_model.setSwitchWarning(sw, 1);
  simuSetSwitch(sw, 1);
  setAllPreflightSwitchStates();
  EXPECT_EQ(SWSRC_FIRST_SWITCH + 3 * sw + 2, getMovedSwitch());
}

TEST_F(PreflightTest, TwoPositionWarningHasNoMiddleState)
{
  int sw = findHwSwitch(SWITCH_3POS);
  ASSERT_GE(sw, 0);
  g_model.setSwitchType(sw, SWITCH_2POS);
  for (int position : {-1, 0, 1}) {
    simuSetSwitch(sw, position);
    EXPECT_EQ(position == -1 ? 1 : 3, g_model.getSwitchStateForWarning(sw));
  }
}

TEST_F(PreflightTest, PotCaptureAndToleranceUseCalibratedInput)
{
  mixerTaskStart();
  g_model.switchWarning = 0;
  g_model.potsWarnMode = POTS_WARN_MANUAL;
  for (uint8_t pot = 0; pot < adcGetMaxInputs(ADC_INPUT_FLEX); ++pot) {
    if (!IS_POT_SLIDER_AVAILABLE(pot)) continue;
    g_model.potsWarnEnabled = 1 << pot;
    auto input = adcGetInputOffset(ADC_INPUT_FLEX) + pot;
    anaSetFiltered(input, 320);
    evalAnalogControls(false);
    SAVE_POT_POSITION(pot);
    EXPECT_EQ(20, g_model.potsWarnPosition[pot]);
    for (int delta : {-32, -16, 0, 16, 32}) {
      anaSetFiltered(input, 320 + delta);
      uint16_t badPots = 0;
      EXPECT_EQ(abs(delta) > 16, isSwitchWarningRequired(badPots));
      EXPECT_EQ(abs(delta) > 16 ? 1 << pot : 0, badPots);
    }
  }
  mixerTaskStop();
}

#if defined(FUNCTION_SWITCHES)
TEST_F(PreflightTest, FunctionSwitchCaptureUsesFunctionState)
{
  g_model.switchWarning = 0;
  for (uint8_t sw = 0; sw < switchGetMaxSwitches(); ++sw) {
    if (!switchIsCustomSwitch(sw)) continue;
    g_model.setSwitchType(sw, SWITCH_2POS);
    g_model.setSwitchWarning(sw, 1);
    for (bool active : {true, false}) {
      g_model.cfsSetState(sw, active);
      setAllPreflightSwitchStates();
      EXPECT_EQ(active ? 3 : 1, g_model.getSwitchWarning(sw));
    }
  }
}
#endif
