#pragma once
// Visual theme: palette + LookAndFeel. Purely presentational; no behaviour.
#include <juce_gui_extra/juce_gui_extra.h>
#include <array>

namespace mc::gui::theme {

namespace col {
// light chrome
inline const juce::Colour appBg{0xffe9edf2};
inline const juce::Colour panel{0xfff4f6f8};
inline const juce::Colour field{0xffffffff};
inline const juce::Colour fieldHover{0xfff7f9fb};
inline const juce::Colour text{0xff25303b};
inline const juce::Colour textDim{0xff66717d};
inline const juce::Colour border{0xffbcc5cf};
inline const juce::Colour borderSoft{0xffd5dce3};
inline const juce::Colour accent{0xff35a9c8};
inline const juce::Colour accentSoft{0xffe1f2f7};
inline const juce::Colour accent2{0xff5578d1};
inline const juce::Colour warm{0xffe5a742};
inline const juce::Colour warmSoft{0xfffbf1dd};
inline const juce::Colour record{0xffe45b5b};
inline const juce::Colour play{0xff3fae94};
inline const juce::Colour display{0xff1f262e};
inline const juce::Colour displayText{0xff4fd0ec};

// dark piano roll
inline const juce::Colour rollBg{0xff20262e};
inline const juce::Colour rollRowWhite{0xff262d36};
inline const juce::Colour rollRowBlack{0xff1d2229};
inline const juce::Colour rollRowScale{0xff2c3540};
inline const juce::Colour rollRowC{0xff3a4552};       // separator under C rows
inline const juce::Colour gridBar{0xff8994a1};
inline const juce::Colour gridBeat{0xff535e6b};
inline const juce::Colour gridSub{0xff323a45};
inline const juce::Colour note{0xff35a9c8};
inline const juce::Colour noteSel{0xffe5a742};
inline const juce::Colour playhead{0xffe45b5b};
inline const juce::Colour loop{0x1c35a9c8};
inline const juce::Colour timelineBg{0xff2a313a};
inline const juce::Colour velLaneBg{0xff1b2027};

// keyboard
inline const juce::Colour keyWhite{0xfffafbfc};
inline const juce::Colour keyBlack{0xff222a33};
inline const juce::Colour keyLine{0xffcfd6dd};
} // namespace col

// Muted palette for track headers (name, colour)
inline const std::array<std::pair<const char*, juce::Colour>, 10> trackPalette{{
    {"Cyan", juce::Colour(0xff35a9c8)}, {"Blue", juce::Colour(0xff5578d1)}, {"Lavender", juce::Colour(0xff8a7cc8)},
    {"Rose", juce::Colour(0xffc77a98)}, {"Red", juce::Colour(0xffd46a6a)}, {"Orange", juce::Colour(0xffe08f4f)},
    {"Sand", juce::Colour(0xffd6b25e)}, {"Olive", juce::Colour(0xff9aac5a)}, {"Green", juce::Colour(0xff5aae84)},
    {"Slate", juce::Colour(0xff7d8a98)}}};

// spacing
inline constexpr int gapS = 4, gap = 8, gapGroup = 12, gapSection = 16;
inline constexpr float radius = 5.0f;

// Button style hints, set via button.getProperties().set(styleKey, ...)
inline const juce::Identifier styleKey{"mcStyle"};
inline const juce::String stylePlay{"play"}, styleRecord{"record"}, styleTransport{"transport"},
    styleWarm{"warm"}, styleDanger{"danger"}, styleSelector{"selector"};

inline void setStyle(juce::Component& c, const juce::String& s) { c.getProperties().set(styleKey, s); }

juce::Font uiFont(float size, bool semibold = false);
juce::Font monoFont(float size);
std::unique_ptr<juce::Drawable> createLogo();
juce::Image logoImage(int size);

class LookAndFeel final : public juce::LookAndFeel_V4 {
public:
    LookAndFeel();

    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;

    void drawComboBox(juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    void positionComboBoxText(juce::ComboBox&, juce::Label&) override;

    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawTickBox(juce::Graphics&, juce::Component&, float x, float y, float w, float h, bool ticked, bool enabled,
                     bool highlighted, bool down) override;

    void drawLinearSlider(juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                          juce::Slider::SliderStyle, juce::Slider&) override;
    juce::Label* createSliderTextBox(juce::Slider&) override;
    juce::Slider::SliderLayout getSliderLayout(juce::Slider&) override;

    void drawScrollbar(juce::Graphics&, juce::ScrollBar&, int x, int y, int w, int h, bool vertical, int thumbStart,
                       int thumbSize, bool over, bool down) override;
    int getDefaultScrollbarWidth() override { return 8; }

    void drawTextEditorOutline(juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void fillTextEditorBackground(juce::Graphics&, int w, int h, juce::TextEditor&) override;

    void drawPopupMenuBackground(juce::Graphics&, int w, int h) override;
    void drawMenuBarBackground(juce::Graphics&, int w, int h, bool, juce::MenuBarComponent&) override;
    juce::Font getPopupMenuFont() override { return uiFont(14.0f); }
    juce::Font getMenuBarFont(juce::MenuBarComponent&, int, const juce::String&) override { return uiFont(14.0f); }

    juce::Font getLabelFont(juce::Label&) override;
};

} // namespace mc::gui::theme
