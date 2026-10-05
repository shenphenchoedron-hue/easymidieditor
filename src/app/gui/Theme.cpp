#include "gui/Theme.h"
#include "BinaryData.h"

namespace mc::gui::theme {

juce::Font uiFont(float size, bool semibold)
{
    juce::Font f{juce::FontOptions(size)};
    if (semibold) f = f.boldened();
    return f;
}

juce::Font monoFont(float size)
{
    return juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), size, juce::Font::bold));
}

std::unique_ptr<juce::Drawable> createLogo()
{
    return juce::Drawable::createFromImageData(BinaryData::logo_svg, BinaryData::logo_svgSize);
}

juce::Image logoImage(int size)
{
    juce::Image img(juce::Image::ARGB, size, size, true);
    if (auto d = createLogo()) {
        juce::Graphics g(img);
        d->drawWithin(g, juce::Rectangle<float>(0, 0, (float)size, (float)size), juce::RectanglePlacement::centred, 1.0f);
    }
    return img;
}

LookAndFeel::LookAndFeel()
{
    using namespace col;
    auto scheme = getLightColourScheme();
    scheme.setUIColour(ColourScheme::windowBackground, appBg);
    scheme.setUIColour(ColourScheme::widgetBackground, field);
    scheme.setUIColour(ColourScheme::menuBackground, field);
    scheme.setUIColour(ColourScheme::outline, border);
    scheme.setUIColour(ColourScheme::defaultText, text);
    scheme.setUIColour(ColourScheme::defaultFill, accent);
    scheme.setUIColour(ColourScheme::highlightedText, text);
    scheme.setUIColour(ColourScheme::highlightedFill, accentSoft);
    scheme.setUIColour(ColourScheme::menuText, text);
    setColourScheme(scheme);

    setColour(juce::ResizableWindow::backgroundColourId, appBg);
    setColour(juce::DocumentWindow::backgroundColourId, appBg);
    setColour(juce::TextButton::buttonColourId, field);
    setColour(juce::TextButton::buttonOnColourId, accent);
    setColour(juce::TextButton::textColourOffId, text);
    setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    setColour(juce::ComboBox::backgroundColourId, field);
    setColour(juce::ComboBox::outlineColourId, border);
    setColour(juce::ComboBox::textColourId, text);
    setColour(juce::ComboBox::arrowColourId, textDim);
    setColour(juce::Label::textColourId, text);
    setColour(juce::Label::textWhenEditingColourId, text);
    setColour(juce::Label::outlineWhenEditingColourId, accent);
    setColour(juce::Label::backgroundWhenEditingColourId, field);
    setColour(juce::TextEditor::backgroundColourId, field);
    setColour(juce::TextEditor::textColourId, text);
    setColour(juce::TextEditor::outlineColourId, border);
    setColour(juce::TextEditor::focusedOutlineColourId, accent);
    setColour(juce::TextEditor::highlightColourId, accentSoft);
    setColour(juce::CaretComponent::caretColourId, text);
    setColour(juce::ToggleButton::textColourId, text);
    setColour(juce::ToggleButton::tickColourId, juce::Colours::white);
    setColour(juce::ToggleButton::tickDisabledColourId, border);
    setColour(juce::Slider::textBoxTextColourId, text);
    setColour(juce::Slider::textBoxBackgroundColourId, field);
    setColour(juce::Slider::textBoxOutlineColourId, border);
    setColour(juce::Slider::trackColourId, accent);
    setColour(juce::Slider::backgroundColourId, borderSoft);
    setColour(juce::Slider::thumbColourId, field);
    setColour(juce::ScrollBar::thumbColourId, juce::Colour(0xff9aa5b1));
    setColour(juce::PopupMenu::backgroundColourId, field);
    setColour(juce::PopupMenu::textColourId, text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, accentSoft);
    setColour(juce::PopupMenu::highlightedTextColourId, text);
    setColour(juce::TooltipWindow::backgroundColourId, display);
    setColour(juce::TooltipWindow::textColourId, juce::Colours::white);
    setColour(juce::TooltipWindow::outlineColourId, display);
    setColour(juce::ListBox::backgroundColourId, field);
    setColour(juce::ListBox::outlineColourId, border);
    setColour(juce::TableHeaderComponent::backgroundColourId, panel);
    setColour(juce::TableHeaderComponent::textColourId, textDim);
    setColour(juce::TableHeaderComponent::outlineColourId, border);
    setColour(juce::AlertWindow::backgroundColourId, panel);
    setColour(juce::AlertWindow::textColourId, text);
    setColour(juce::AlertWindow::outlineColourId, border);
    setColour(juce::ProgressBar::foregroundColourId, accent);
    setColour(juce::ProgressBar::backgroundColourId, borderSoft);
}

// ------------------------------------------------------------------ buttons
void LookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hi, bool down)
{
    using namespace col;
    const auto style = b.getProperties()[styleKey].toString();
    const bool on = b.getToggleState();
    auto r = b.getLocalBounds().toFloat().reduced(0.5f);

    juce::Colour fill = field, edge = border;
    if (on) {
        juce::Colour a = accent;
        if (style == stylePlay) a = play;
        else if (style == styleRecord) a = record;
        else if (style == styleWarm) a = warm;
        else if (b.findColour(juce::TextButton::buttonOnColourId) != accent) a = b.findColour(juce::TextButton::buttonOnColourId);
        fill = a;
        edge = a.darker(0.15f);
    } else {
        if (style == styleDanger) { fill = record.withAlpha(0.10f); edge = record.withAlpha(0.7f); }
        else if (style == stylePlay || style == styleRecord || style == styleTransport) {
            edge = border.darker(0.08f);
        }
        if (hi) { fill = fill.overlaidWith(accent.withAlpha(0.07f)); edge = accent.withAlpha(0.65f); }
    }
    if (down) fill = fill.darker(0.06f);
    if (!b.isEnabled()) { fill = fill.withMultipliedAlpha(0.6f); edge = edge.withMultipliedAlpha(0.6f); }

    g.setColour(fill);
    g.fillRoundedRectangle(r, radius);
    g.setColour(edge);
    g.drawRoundedRectangle(r, radius, 1.0f);
}

juce::Font LookAndFeel::getTextButtonFont(juce::TextButton& b, int h)
{
    const auto style = b.getProperties()[styleKey].toString();
    const bool strong = style == stylePlay || style == styleRecord || style == styleTransport;
    return uiFont(juce::jmin(14.0f, (float)h * 0.55f), strong);
}

void LookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    using namespace col;
    const auto style = b.getProperties()[styleKey].toString();
    juce::Colour c = text;
    if (b.getToggleState()) c = juce::Colours::white;
    else if (style == stylePlay) c = play.darker(0.25f);
    else if (style == styleRecord || style == styleDanger) c = record.darker(0.15f);
    if (!b.isEnabled()) c = textDim.withAlpha(0.55f);
    g.setFont(getTextButtonFont(b, b.getHeight()));
    g.setColour(c);
    if (style == styleSelector || style == styleDanger) { // dropdown-like selector
        const float cx = (float)b.getWidth() - 12.0f, cy = (float)b.getHeight() * 0.5f;
        juce::Path p;
        p.startNewSubPath(cx - 4, cy - 2); p.lineTo(cx, cy + 2); p.lineTo(cx + 4, cy - 2);
        g.strokePath(p, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.drawFittedText(b.getButtonText(), b.getLocalBounds().reduced(8, 2).withTrimmedRight(16), juce::Justification::centredLeft, 1, 0.9f);
        return;
    }
    g.drawFittedText(b.getButtonText(), b.getLocalBounds().reduced(6, 2), juce::Justification::centred, 1, 0.8f);
}

// ------------------------------------------------------------------ combo
void LookAndFeel::drawComboBox(juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    using namespace col;
    auto r = juce::Rectangle<float>(0, 0, (float)w, (float)h).reduced(0.5f);
    const bool hover = box.isMouseOver(true);
    g.setColour(box.isEnabled() ? (hover ? fieldHover : field) : panel);
    g.fillRoundedRectangle(r, radius);
    g.setColour(box.hasKeyboardFocus(true) || hover ? accent.withAlpha(0.7f) : border);
    g.drawRoundedRectangle(r, radius, 1.0f);

    // chevron
    const float cx = (float)w - 12.0f, cy = (float)h * 0.5f;
    juce::Path p;
    p.startNewSubPath(cx - 4, cy - 2);
    p.lineTo(cx, cy + 2);
    p.lineTo(cx + 4, cy - 2);
    g.setColour(textDim);
    g.strokePath(p, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Font LookAndFeel::getComboBoxFont(juce::ComboBox& b) { return uiFont(juce::jmin(14.0f, (float)b.getHeight() * 0.58f)); }

void LookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    label.setBounds(4, 1, box.getWidth() - 24, box.getHeight() - 2);
    label.setFont(getComboBoxFont(box));
}

// ------------------------------------------------------------------ toggle
void LookAndFeel::drawTickBox(juce::Graphics& g, juce::Component&, float x, float y, float w, float h, bool ticked,
                              bool enabled, bool hi, bool)
{
    using namespace col;
    juce::Rectangle<float> r(x, y, w, h);
    r = r.withSizeKeepingCentre(15, 15);
    g.setColour(ticked ? accent : field);
    g.fillRoundedRectangle(r, 4.0f);
    g.setColour(ticked ? accent.darker(0.15f) : (hi ? accent.withAlpha(0.7f) : border));
    g.drawRoundedRectangle(r.reduced(0.5f), 4.0f, 1.0f);
    if (ticked) {
        juce::Path p;
        p.startNewSubPath(r.getX() + 3.5f, r.getCentreY());
        p.lineTo(r.getX() + 6.5f, r.getBottom() - 4.0f);
        p.lineTo(r.getRight() - 3.5f, r.getY() + 4.0f);
        g.setColour(enabled ? juce::Colours::white : border);
        g.strokePath(p, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

// ------------------------------------------------------------------ slider
void LookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                                   juce::Slider::SliderStyle style, juce::Slider& s)
{
    if (style != juce::Slider::LinearHorizontal) {
        LookAndFeel_V4::drawLinearSlider(g, x, y, w, h, pos, minPos, maxPos, style, s);
        return;
    }
    using namespace col;
    const float cy = (float)y + (float)h * 0.5f;
    const float x0 = (float)x + 6, x1 = (float)(x + w) - 6;
    juce::Rectangle<float> track(x0, cy - 2, x1 - x0, 4);
    g.setColour(borderSoft);
    g.fillRoundedRectangle(track, 2.0f);

    // bipolar sliders (pan) fill from the centre
    const bool bipolar = s.getMinimum() < 0 && s.getMaximum() > 0;
    const float from = bipolar ? (float)s.valueToProportionOfLength(0.0) * (x1 - x0) + x0 : x0;
    const float to = juce::jlimit(x0, x1, pos);
    g.setColour(s.isEnabled() ? accent : border);
    g.fillRoundedRectangle(juce::Rectangle<float>(std::min(from, to), cy - 2, std::abs(to - from), 4), 2.0f);

    const float r = 6.0f;
    auto thumb = juce::Rectangle<float>(to - r, cy - r, 2 * r, 2 * r);
    g.setColour(field);
    g.fillEllipse(thumb);
    g.setColour(s.isMouseOverOrDragging() ? accent : border.darker(0.1f));
    g.drawEllipse(thumb.reduced(0.5f), 1.0f);
}

juce::Label* LookAndFeel::createSliderTextBox(juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox(s);
    l->setFont(uiFont(13.0f));
    l->setColour(juce::Label::outlineColourId, col::border);
    l->setColour(juce::Label::backgroundColourId, col::field);
    l->setColour(juce::Label::textColourId, col::text);
    return l;
}

juce::Slider::SliderLayout LookAndFeel::getSliderLayout(juce::Slider& s)
{
    auto layout = LookAndFeel_V4::getSliderLayout(s);
    if (s.getSliderStyle() == juce::Slider::IncDecButtons)
        layout.textBoxBounds = layout.textBoxBounds.withTrimmedRight(2);
    return layout;
}

// ------------------------------------------------------------------ scrollbar
void LookAndFeel::drawScrollbar(juce::Graphics& g, juce::ScrollBar& bar, int x, int y, int w, int h, bool vertical,
                                int thumbStart, int thumbSize, bool over, bool down)
{
    juce::ignoreUnused(bar);
    juce::Rectangle<float> thumb = vertical ? juce::Rectangle<float>((float)x, (float)thumbStart, (float)w, (float)thumbSize)
                                            : juce::Rectangle<float>((float)thumbStart, (float)y, (float)thumbSize, (float)h);
    thumb = thumb.reduced(vertical ? 2.0f : 1.0f, vertical ? 1.0f : 2.0f);
    juce::Colour c = juce::Colour(0xff8f9aa6).withAlpha(0.55f);
    if (over || down) c = col::accent.withAlpha(0.8f);
    g.setColour(c);
    g.fillRoundedRectangle(thumb, std::min(thumb.getWidth(), thumb.getHeight()) * 0.5f);
}

// ------------------------------------------------------------------ text editor
void LookAndFeel::fillTextEditorBackground(juce::Graphics& g, int w, int h, juce::TextEditor& e)
{
    g.setColour(e.findColour(juce::TextEditor::backgroundColourId));
    g.fillRoundedRectangle(juce::Rectangle<float>(0, 0, (float)w, (float)h).reduced(0.5f), radius);
}

void LookAndFeel::drawTextEditorOutline(juce::Graphics& g, int w, int h, juce::TextEditor& e)
{
    g.setColour(e.hasKeyboardFocus(true) ? col::accent : col::border);
    g.drawRoundedRectangle(juce::Rectangle<float>(0, 0, (float)w, (float)h).reduced(0.5f), radius, 1.0f);
}

// ------------------------------------------------------------------ menus
void LookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int w, int h)
{
    g.fillAll(col::field);
    g.setColour(col::border);
    g.drawRect(0, 0, w, h, 1);
}

void LookAndFeel::drawMenuBarBackground(juce::Graphics& g, int w, int h, bool, juce::MenuBarComponent&)
{
    g.fillAll(col::panel);
    g.setColour(col::borderSoft);
    g.drawHorizontalLine(h - 1, 0.0f, (float)w);
}

juce::Font LookAndFeel::getLabelFont(juce::Label& l)
{
    return l.getFont().getHeight() > 0 ? l.getFont() : uiFont(14.0f);
}

} // namespace mc::gui::theme
