// Custom-switch lists carry their physical identity in `sw`. Stage by file
// position, then resolve to native switches after the complete record is read.
#if defined(FUNCTION_SWITCHES)
constexpr unsigned FunctionSlots = 16;
static int8_t functionSlots[FunctionSlots] = {0, 1, 2,  3,  4,  5,  6,  7,
                                              8, 9, 10, 11, 12, 13, 14, 15};
static int8_t pendingFunctionSlots[FunctionSlots];
static customSwitch pendingFunctions[FunctionSlots];
static uint16_t functionFields[FunctionSlots];
static bool functionRecords[FunctionSlots];
static bool functionMarkers[FunctionSlots];
int functionIndex(ModelData& model, unsigned slot)
{
  if (slot >= FunctionSlots) return -1;
  bool staged = &model == &candidate ||
                (&model == &defaults && saveContext.model == &candidate);
  return (staged ? pendingFunctionSlots : functionSlots)[slot];
}
bool functionAccess(ModelData& model, unsigned slot, const char* path, Field& f,
                    const char* text)
{
  const char* leaf = strchr(path + 15, '/');
  if (!leaf || slot >= FunctionSlots) return false;
  ++leaf;
  bool loading = &model == &candidate;
  int index = functionIndex(model, slot);
  f.available = index >= 0 && index < NUM_FUNCTIONS_SWITCHES;
  if (loading && !functionMarkers[slot]) f.available = true;
  if (!strcmp(leaf, "sw")) {
    if (loading) f.available = true;
    if (f.metadataOnly || !f.available) return true;
    if (text) {
      char name[32];
      size_t length;
      if (!string(text, name, sizeof(name) - 1, length)) return false;
      name[length] = 0;
      int physical = switchLookupIdx(name, length);
      pendingFunctionSlots[slot] =
          physical >= 0 && switchIsCustomSwitch(physical)
              ? switchGetCustomSwitchIdx(physical)
              : -1;
      functionRecords[slot] = true;
      functionMarkers[slot] = true;
      return true;
    }
    const char* name =
        switchGetDefaultName(switchGetSwitchFromCustomIdx(index));
    return name && quoted(f.value, sizeof(f.value), name, strlen(name));
  }
  if (!f.available) return true;
  auto& value = loading ? pendingFunctions[slot] : model.customSwitches[index];
  unsigned field = 0;
  const char* names[] = {"name",
                         "type",
                         "group",
                         "start",
                         "state",
                         "onColorLuaOverride",
                         "offColorLuaOverride",
                         "onColor/r",
                         "onColor/g",
                         "onColor/b",
                         "offColor/r",
                         "offColor/g",
                         "offColor/b"};
  while (field < 13 && strcmp(leaf, names[field])) ++field;
  if (field == 13) {
    f.available = false;
    return true;
  }
#if !defined(FUNCTION_SWITCHES_RGB_LEDS)
  if (field >= 5) {
    f.available = false;
    return true;
  }
#endif
  if (f.metadataOnly) return true;
  if (text) {
    functionRecords[slot] = true;
    functionFields[slot] |= 1u << field;
  }
  int64_t n;
#define FUNCTION_NUMBER(id, member, limit)           \
  case id:                                           \
    if (text) {                                      \
      if (!integer(text, 0, limit, n)) return false; \
      value.member = n;                              \
      return true;                                   \
    }                                                \
    return formatInteger(f.value, sizeof(f.value), value.member);
#define FUNCTION_ENUM(id, member, choices, limit)                             \
  case id:                                                                    \
    if (text) {                                                               \
      if (!enumeration(choices, text, n) || n < 0 || n > limit) return false; \
      value.member = n;                                                       \
      return true;                                                            \
    }                                                                         \
    return enumOutput(choices, value.member, f);
  switch (field) {
    case 0:
      return textValue(value.name, f, text);
      FUNCTION_ENUM(1, type, FunctionType, 7)
      FUNCTION_NUMBER(2, group, NUM_FUNCTIONS_GROUPS)
      FUNCTION_ENUM(3, start, FunctionStart, 2)
      FUNCTION_NUMBER(4, state, 1)
#if defined(FUNCTION_SWITCHES_RGB_LEDS)
      FUNCTION_ENUM(5, onColorLuaOverride, BooleanMode, 1)
      FUNCTION_ENUM(6, offColorLuaOverride, BooleanMode, 1)
      FUNCTION_NUMBER(7, onColor.r, 255)
      FUNCTION_NUMBER(8, onColor.g, 255)
      FUNCTION_NUMBER(9, onColor.b, 255)
      FUNCTION_NUMBER(10, offColor.r, 255)
      FUNCTION_NUMBER(11, offColor.g, 255)
      FUNCTION_NUMBER(12, offColor.b, 255)
#endif
  }
#undef FUNCTION_NUMBER
#undef FUNCTION_ENUM
  return false;
}
void beginFunctions()
{
  for (unsigned i = 0; i < FunctionSlots; ++i) pendingFunctionSlots[i] = i;
  memset(pendingFunctions, 0, sizeof(pendingFunctions));
  memset(functionRecords, 0, sizeof(functionRecords));
  memset(functionMarkers, 0, sizeof(functionMarkers));
  memset(functionFields, 0, sizeof(functionFields));
}
const char* resolveFunctions()
{
  uint16_t assigned = 0;
  for (unsigned slot = 0; slot < FunctionSlots; ++slot) {
    int index = pendingFunctionSlots[slot];
    if (!functionRecords[slot]) {
      pendingFunctionSlots[slot] = -1;
      continue;
    }
    if (index < 0 || index >= NUM_FUNCTIONS_SWITCHES) continue;
    if (assigned & (1u << index)) return "duplicate model function switch";
    assigned |= 1u << index;
    auto& target = candidate.customSwitches[index];
    auto& value = pendingFunctions[slot];
    unsigned fields = functionFields[slot];
    if (fields & 1) memcpy(target.name, value.name, LEN_SWITCH_NAME);
#define COPY_FUNCTION(id, member) \
  if (fields & (1u << id)) target.member = value.member;
    COPY_FUNCTION(1, type)
    COPY_FUNCTION(2, group)
    COPY_FUNCTION(3, start) COPY_FUNCTION(4, state)
#if defined(FUNCTION_SWITCHES_RGB_LEDS)
        COPY_FUNCTION(5, onColorLuaOverride)
            COPY_FUNCTION(6, offColorLuaOverride) COPY_FUNCTION(7, onColor.r)
                COPY_FUNCTION(8, onColor.g) COPY_FUNCTION(9, onColor.b)
                    COPY_FUNCTION(10, offColor.r) COPY_FUNCTION(11, offColor.g)
                        COPY_FUNCTION(12, offColor.b)
#endif
#undef COPY_FUNCTION
  }
  for (unsigned index = 0; index < NUM_FUNCTIONS_SWITCHES; ++index)
    if (!(assigned & (1u << index))) {
      unsigned slot = 0;
      while (slot < FunctionSlots && functionRecords[slot]) ++slot;
      if (slot == FunctionSlots)
        return "model function switch capacity exceeded";
      pendingFunctionSlots[slot] = index;
      functionRecords[slot] = true;
    }
  return nullptr;
}
#define FUNCTION_ACCESS(path)                \
  if (!strncmp(path, "customSwitches/", 15)) \
    return functionAccess(m, n, path, f, t);
#else
#define FUNCTION_ACCESS(path)
#endif
