#include "gtests.h"
#include "hal/adc_driver.h"
#include "physical_input.h"

class PhysicalControlTest : public EdgeTxTest {};

TEST_F(PhysicalControlTest, EveryNativeValueReachesEveryChannelUnchanged)
{
  MODEL_RESET();
#if defined(STICK_DEAD_ZONE)
  g_eeGeneral.stickDeadZone = 0;
#endif
  for (auto& mapping : g_model.channelMappings) mapping.source = physicalStick(0);
  for (int value = -RESX; value <= RESX; ++value) {
    anaSetFiltered(inputMappingConvertMode(0), value);
    updateChannelOutputs();
    for (int channel = 0; channel < MAX_OUTPUT_CHANNELS; ++channel)
      ASSERT_EQ(value, channelOutputs[channel]) << channel;
  }
  memset(g_model.channelMappings, 0, sizeof(g_model.channelMappings));
  updateChannelOutputs();
  for (auto value : channelOutputs) EXPECT_EQ(0, value);
}

TEST_F(PhysicalControlTest, ReplacementAndInvalidSourcesCannotAccumulateOrLatch)
{
  MODEL_RESET();
#if defined(STICK_DEAD_ZONE)
  g_eeGeneral.stickDeadZone = 0;
#endif
  anaSetFiltered(inputMappingConvertMode(0), 777);
  anaSetFiltered(inputMappingConvertMode(1), -333);
  auto& source = g_model.channelMappings[31].source;
  source = physicalStick(0);
  updateChannelOutputs();
  EXPECT_EQ(777, channelOutputs[31]);
  source = physicalStick(1);
  updateChannelOutputs();
  EXPECT_EQ(-333, channelOutputs[31]);
  for (int id : {0, 32, 64, 96, 97, 255}) {
    source = PhysicalInputId(id);
    ASSERT_FALSE(isPhysicalInputAvailable(source));
    updateChannelOutputs();
    EXPECT_EQ(0, channelOutputs[31]);
  }
}

TEST_F(PhysicalControlTest, SwitchPositionsReachChannelsImmediately)
{
  MODEL_RESET();
  int sw = findHwSwitch(SWITCH_3POS);
  ASSERT_GE(sw, 0);
  g_model.channelMappings[0].source = physicalSwitch(sw);
  g_model.channelMappings[31].source = physicalSwitch(sw);
  for (int position : {-1, 1, 0, -1}) {
    simuSetSwitch(sw, position);
    updateChannelOutputs();
    EXPECT_EQ(position * RESX, channelOutputs[0]);
    EXPECT_EQ(position * RESX, channelOutputs[31]);
  }
}

TEST_F(PhysicalControlTest, FlexInputsKeepNativeResolution)
{
#if defined(STICK_DEAD_ZONE)
  g_eeGeneral.stickDeadZone = 0;
#endif
  for (unsigned input = 0; input < adcGetMaxInputs(ADC_INPUT_FLEX); ++input) {
    auto source = physicalFlex(input);
    if (!isPhysicalInputAvailable(source)) continue;
    g_model.channelMappings[0].source = source;
    for (int value : {-RESX, -777, -1, 0, 1, 777, RESX}) {
      anaSetFiltered(adcGetInputOffset(ADC_INPUT_FLEX) + input, value);
      updateChannelOutputs();
      EXPECT_EQ(value, channelOutputs[0]);
    }
  }
}

TEST_F(PhysicalControlTest, UISelectorUsesPhysicalIds)
{
  EXPECT_TRUE(isChannelMappingInputAvailable(0));
  for (int id = 1; id <= 255; ++id)
    EXPECT_EQ(isPhysicalInputAvailable(PhysicalInputId(id)),
              isChannelMappingInputAvailable(id)) << id;
  for (int id : {-256, -1, 256, 257, 321})
    EXPECT_FALSE(isChannelMappingInputAvailable(id));
}

TEST_F(PhysicalControlTest, PhysicalInputLabelsPreserveControlNames)
{
  char label[32];
  getPhysicalInputLabel(label, PhysicalInputId::None);
  EXPECT_STREQ(STR_EMPTY, label);
  for (int id = 1; id <= 96; ++id) {
    auto source = PhysicalInputId(id);
    if (!isPhysicalInputAvailable(source)) continue;
    getPhysicalInputLabel(label, source);
    int legacySource = id < 33 ? MIXSRC_FIRST_STICK + id - 1 :
                       id < 65 ? MIXSRC_FIRST_POT + id - 33 :
                                 MIXSRC_FIRST_SWITCH + id - 65;
    EXPECT_STREQ(getSourceString(legacySource), label) << id;
  }
}

TEST_F(PhysicalControlTest, SharedSystemAndTimerAPIsRemainReadable)
{
  g_vbat100mV = 83;
  timersStates[0].val = 123;
  EXPECT_EQ(83, getValue(MIXSRC_TX_VOLTAGE));
  EXPECT_EQ(-123, getValue(-MIXSRC_FIRST_TIMER));
}

TEST_F(PhysicalControlTest, PhysicalTrimButtonsDoNotOffsetSticks)
{
  // Even nonzero bytes left at old trim storage positions must have no effect.
  g_model.reservedThrTrim = 1;
  g_model.reservedExtendedTrims = 1;
  g_model.reservedTrimInc = 2;

  for (int mode = 0; mode < 4; ++mode) {
    g_eeGeneral.stickMode = mode;
    for (int axis = 0; axis < 4; ++axis)
      anaSetFiltered(axis, -600 + axis * 350);
    updateChannelOutputs();
    int16_t baseline[MAX_OUTPUT_CHANNELS];
    memcpy(baseline, channelOutputs, sizeof(baseline));
    for (int button = 0; button < keysGetMaxTrims() * 2; ++button) {
      simuSetTrim(button, true);
      for (int tick = 0; tick < 30; ++tick) {
        updateChannelOutputs();
        for (int ch = 0; ch < MAX_OUTPUT_CHANNELS; ++ch)
          ASSERT_EQ(baseline[ch], channelOutputs[ch]) << "button=" << button;
      }
      simuSetTrim(button, false);
    }

  }
}

TEST_F(PhysicalControlTest, RemovedTrimValueSourcesAreUnavailable)
{
  for (int source = MIXSRC_FIRST_SWITCH - MAX_TRIMS; source < MIXSRC_FIRST_SWITCH; ++source) {
    EXPECT_FALSE(isSourceAvailable(source));
    bool valid = true;
    EXPECT_EQ(0, getValue(source, &valid));
    EXPECT_FALSE(valid);
  }
}

TEST_F(PhysicalControlTest, DefaultQuadUsesPhysicalAETRControls)
{
  generalDefault();
  setModelDefaults();
  const int sticks[] = {3, 1, 2, 0}; // Ail, Ele, Thr, Rud
#if defined(STICK_DEAD_ZONE)
  g_eeGeneral.stickDeadZone = 0;
#endif
  for (int mode = 0; mode < 4; ++mode) {
    g_eeGeneral.stickMode = mode;
    for (int ch = 0; ch < 4; ++ch) {
      EXPECT_EQ(physicalStick(sticks[ch]), g_model.channelMappings[ch].source);
      anaSetFiltered(inputMappingConvertMode(sticks[ch]), (ch - 2) * 256);
    }
    updateChannelOutputs();
    for (int ch = 0; ch < 4; ++ch)
      EXPECT_EQ((ch - 2) * 256, channelOutputs[ch]);
  }
}


#if !defined(COLORLCD)
TEST_F(PhysicalControlTest, ManualSelectionIgnoresHardwareMovement)
{
  int sw = findHwSwitch(SWITCH_3POS);
  ASSERT_GE(sw, 0);
  s_editMode = EDIT_MODIFY_FIELD;
  int selected = SWSRC_FIRST_SWITCH + sw * 3;
  for (int position : {-1, 1, 0}) {
    simuSetSwitch(sw, position);
    EXPECT_EQ(selected, checkIncDec(0, selected, SWSRC_NONE, SWSRC_LAST_SWITCH,
                                   INCDEC_SWITCH, nullptr));
    EXPECT_EQ(MIXSRC_FIRST_STICK,
              checkIncDec(0, MIXSRC_FIRST_STICK, MIXSRC_FIRST_STICK,
                          MIXSRC_LAST_SWITCH, INCDEC_SOURCE, isSourceAvailable));
  }
  // The manual category menu still selects sources and switches.
  onSwitchLongEnterPress(STR_MENU_TRIMS);
  EXPECT_EQ(SWSRC_FIRST_TRIM,
            checkIncDec(0, selected, SWSRC_NONE, SWSRC_LAST_TRIM,
                        INCDEC_SWITCH, nullptr));
  onSourceLongEnterPress(STR_MENU_SWITCHES);
  EXPECT_EQ(MIXSRC_FIRST_SWITCH,
            checkIncDec(0, MIXSRC_FIRST_STICK, MIXSRC_NONE, MIXSRC_LAST_SWITCH,
                        INCDEC_SOURCE, isSourceAvailable));
}
#endif
