#pragma once
#include <cstdint>
#if defined(ARDUINO)
#include <Arduino.h>
#endif
#include <vector>
#include <utility>
struct RuntimeStyleProperty {
  String key;
  String value;
};

struct RuntimeStyleRule {
  String target;
  std::vector<RuntimeStyleProperty> properties;
  bool hasVisible = false;
  bool visible = true;
  String className;

#ifdef CM_RUNTIME_META_TEST_INSTRUMENTATION
  RuntimeStyleRule() = default;

  RuntimeStyleRule(const RuntimeStyleRule& other)
      : target(other.target), properties(other.properties), hasVisible(other.hasVisible), visible(other.visible), className(other.className) {
    ++copyCount;
  }

  RuntimeStyleRule& operator=(const RuntimeStyleRule& other) {
    if (this != &other) {
      target = other.target;
      properties = other.properties;
      hasVisible = other.hasVisible;
      visible = other.visible;
      className = other.className;
    }
    ++copyCount;
    return *this;
  }

  RuntimeStyleRule(RuntimeStyleRule&&) noexcept = default;
  RuntimeStyleRule& operator=(RuntimeStyleRule&&) noexcept = default;

  static void resetCopyCount() {
    copyCount = 0;
  }

  static size_t getCopyCount() {
    return copyCount;
  }

private:
  inline static size_t copyCount = 0;

public:
#endif

  // Cppcheck rationale: Keep allocation-free, early-exit loops on the embedded target.
  // cppcheck-suppress-begin useStlAlgorithm
  RuntimeStyleRule& set(const char* property, const char* value) {
    if (!property || !property[0]) {
      return *this;
    }
    for (auto& prop : properties) {
      if (prop.key == property) {
        prop.value = value ? value : "";
        return *this;
      }
    }
    properties.push_back(RuntimeStyleProperty{String(property), value ? String(value) : String()});
    return *this;
  }
  // Cppcheck rationale: End the local embedded-loop exception.
  // cppcheck-suppress-end useStlAlgorithm

  RuntimeStyleRule& setVisible(bool value) {
    hasVisible = true;
    visible = value;
    return *this;
  }

  RuntimeStyleRule& addCSSClass(const char* cssClass) {
    if (!cssClass || !cssClass[0]) {
      return *this;
    }
    if (!className.isEmpty()) {
      className += " ";
    }
    className += cssClass;
    return *this;
  }
};

struct RuntimeFieldStyle {
  std::vector<RuntimeStyleRule> rules;

  // Cppcheck rationale: Keep allocation-free, early-exit loops on the embedded target.
  // cppcheck-suppress-begin useStlAlgorithm
  RuntimeStyleRule& rule(const char* targetName) {
    String target = targetName ? String(targetName) : String();
    for (auto& styleRule : rules) {
      if (styleRule.target == target) {
        return styleRule;
      }
    }
    RuntimeStyleRule created;
    created.target = target;
    rules.push_back(std::move(created));
    return rules.back();
  }
  // Cppcheck rationale: End the local embedded-loop exception.
  // cppcheck-suppress-end useStlAlgorithm

  bool empty() const {
    return rules.empty();
  }
};

// Runtime field metadata structure
struct RuntimeFieldMeta {
  String group;
  String sourceGroup;
  String page;
  String key;
  String label;
  String unit;
  String onLabel;
  String offLabel;
  int precision = 2;
  int order = 100;
  bool isBool = false;
  bool isString = false;
  bool isDivider = false;
  bool isButton = false;
  bool isCheckbox = false;
  bool isStateButton = false;
  bool isMomentaryButton = false;
  bool isIntSlider = false;
  bool isFloatSlider = false;
  bool isIntInput = false;
  bool isFloatInput = false;
  bool hasAlarm = false;
  bool alarmWhenTrue = false;
  bool boolAlarmValue = false;
  float alarmMin = 0.0f;
  float alarmMax = 0.0f;
  float warnMin = 0.0f;
  float warnMax = 0.0f;
  // Slider specific properties
  int intMin = 0;
  int intMax = 100;
  int intInit = 0;
  float floatMin = 0.0f;
  float floatMax = 1.0f;
  float floatInit = 0.0f;
  int floatPrecision = 2;
  bool initialState = false;
  String staticValue;
  String card;
  RuntimeFieldStyle style;
};

// An opaque handle stays valid across compact-pool growth and legacy promotion.
struct RuntimeFieldId {
  uint32_t value = 0;
  explicit operator bool() const { return value != 0; }
  bool operator==(RuntimeFieldId other) const { return value == other.value; }
  bool operator!=(RuntimeFieldId other) const { return value != other.value; }
};
enum class RuntimeFieldKind : uint8_t {
  Invalid, Float, Int, Bool, Text, Divider, Button, Checkbox, StateButton,
  MomentaryButton, IntSlider, IntInput, FloatSlider, FloatInput
};
struct RuntimeFieldBase {
  String key, label, group, sourceGroup, page, card;
  int order = 100;
};
struct RuntimeFloatMeta { RuntimeFieldBase base; String unit; int precision = 2; };
struct RuntimeIntMeta { RuntimeFieldBase base; String unit; };
struct RuntimeBoolMeta { RuntimeFieldBase base; };
struct RuntimeTextMeta { RuntimeFieldBase base; };
struct RuntimeDividerMeta { RuntimeFieldBase base; };
struct RuntimeButtonMeta { RuntimeFieldBase base; };
struct RuntimeCheckboxMeta { RuntimeFieldBase base; };
struct RuntimeStateButtonMeta { RuntimeFieldBase base; bool initialState = false; };
struct RuntimeMomentaryButtonMeta { RuntimeFieldBase base; };
struct RuntimeIntSliderMeta { RuntimeFieldBase base; String unit; int intMin = 0, intMax = 100, intInit = 0; };
struct RuntimeIntInputMeta { RuntimeFieldBase base; String unit; int intMin = 0, intMax = 100, intInit = 0; };
struct RuntimeFloatSliderMeta { RuntimeFieldBase base; String unit; float floatMin = 0, floatMax = 1, floatInit = 0; int precision = 2; };
struct RuntimeFloatInputMeta { RuntimeFieldBase base; String unit; float floatMin = 0, floatMax = 1, floatInit = 0; int precision = 2; };
struct RuntimeNumericAlarm { bool hasAlarm = false; float alarmMin = 0, alarmMax = 0, warnMin = 0, warnMax = 0; };
struct RuntimeBoolAlarm { bool hasAlarm = false, alarmWhenTrue = false, boolAlarmValue = false; };
struct RuntimeStateLabels { String onLabel, offLabel; };
