#include "gtests.h"
#include "hal/adc_driver.h"
#include "inactivity_timer.h"

class InactivityTest : public EdgeTxTest {};

TEST_F(InactivityTest, SwitchMovementDoesNotResetCounterUntilRequested)
{
  int sw = findHwSwitch(SWITCH_3POS);
  ASSERT_GE(sw, 0);
  ASSERT_LT(sw, getSwitchCount());
  simuSetSwitch(sw, -1);
  inactivityCheckInputs();
  EXPECT_FALSE(inactivityCheckInputs());
  inactivity.counter = 123;
  for (int position : {0, 1, -1}) {
    simuSetSwitch(sw, position);
    EXPECT_TRUE(inactivityCheckInputs());
    EXPECT_FALSE(inactivityCheckInputs());
    EXPECT_EQ(123, inactivity.counter);
  }
  inactivityTimerReset(ActivitySource::MainControls);
  EXPECT_EQ(0, inactivity.counter);
}

TEST_F(InactivityTest, SubThresholdMovementAccumulatesAgainstLastActivity)
{
  const auto original = getAnalogValue(0);
  setAnalogValue(0, 0);
  inactivityCheckInputs();
  const auto baseline = inactivity.sum;
  setAnalogValue(0, 127);
  EXPECT_FALSE(inactivityCheckInputs());
  setAnalogValue(0, 255);
  EXPECT_FALSE(inactivityCheckInputs());
  EXPECT_EQ(baseline, inactivity.sum);
  setAnalogValue(0, 256);
  EXPECT_TRUE(inactivityCheckInputs());
  EXPECT_EQ(uint8_t(baseline + 2), inactivity.sum);
  EXPECT_FALSE(inactivityCheckInputs());
  setAnalogValue(0, original);
}
