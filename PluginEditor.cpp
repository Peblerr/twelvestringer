#include "PluginEditor.h"

namespace Palette
{
    const juce::Colour bg        { 0xff1b1815 };
    const juce::Colour panel     { 0xff25211d };
    const juce::Colour border    { 0xff3a332c };
    const juce::Colour text      { 0xfff2e8dc };
    const juce::Colour dim       { 0xff9d8f80 };
    const juce::Colour track     { 0xff3d362f };
    const juce::Colour octave    { 0xffe8a54b };   // amber
    const juce::Colour unison    { 0xff6cc5b0 };   // teal
    const juce::Colour feel      { 0xffd98a7a };   // clay
    const juce::Colour steel     { 0xffd9d2c8 };
}

static juce::Font uiFont (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

//==============================================================================
TwelveLookAndFeel::TwelveLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, Palette::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, Palette::text);
    setColour (juce::ComboBox::backgroundColourId, Palette::panel);
    setColour (juce::ComboBox::outlineColourId, Palette::border);
    setColour (juce::ComboBox::textColourId, Palette::text);
    setColour (juce::ComboBox::arrowColourId, Palette::octave);
    setColour (juce::PopupMenu::backgroundColourId, Palette::panel);
    setColour (juce::PopupMenu::textColourId, Palette::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Palette::octave.withAlpha (0.25f));
    setColour (juce::PopupMenu::highlightedTextColourId, Palette::text);
    setColour (juce::TextEditor::backgroundColourId, Palette::bg);
    setColour (juce::TextEditor::textColourId, Palette::text);
    setColour (juce::TextEditor::highlightColourId, Palette::octave.withAlpha (0.4f));
    setColour (juce::CaretComponent::caretColourId, Palette::text);
}

void TwelveLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                          float startAngle, float endAngle, juce::Slider& s)
{
    const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float arcR = radius - 3.f;

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.f, startAngle, endAngle, true);
    g.setColour (Palette::track);
    g.strokePath (track, juce::PathStrokeType (4.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (pos > 0.001f)
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.f, startAngle, angle, true);
        g.setColour (s.isEnabled() ? accent : accent.withSaturation (0.1f));
        g.strokePath (value, juce::PathStrokeType (4.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // knob cap
    const float capR = arcR - 8.f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3b342d), centre.x, centre.y - capR,
                                             juce::Colour (0xff221e1a), centre.x, centre.y + capR, false));
    g.fillEllipse (centre.x - capR, centre.y - capR, capR * 2.f, capR * 2.f);
    g.setColour (Palette::border.brighter (0.2f));
    g.drawEllipse (centre.x - capR, centre.y - capR, capR * 2.f, capR * 2.f, 1.f);

    // pointer
    juce::Path pointer;
    pointer.addRoundedRectangle (-1.5f, -capR + 4.f, 3.f, capR * 0.45f, 1.5f);
    g.setColour (Palette::text);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre));
}

void TwelveLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.f);
    const bool on = b.getToggleState();
    const auto accent = Palette::feel;

    g.setColour (on ? accent.withAlpha (0.22f) : Palette::panel);
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour (on ? accent : (highlighted ? Palette::dim : Palette::border));
    g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.2f);

    const float dot = 7.f;
    g.setColour (on ? accent : Palette::dim.withAlpha (0.5f));
    g.fillEllipse (r.getX() + 10.f, r.getCentreY() - dot * 0.5f, dot, dot);

    g.setColour (on ? Palette::text : Palette::dim);
    g.setFont (uiFont (12.f, true));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (22.f), juce::Justification::centred);
}

juce::Label* TwelveLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (uiFont (12.5f));
    l->setColour (juce::Label::textColourId, Palette::text.withAlpha (0.85f));
    l->setColour (juce::Label::outlineWhenEditingColourId, Palette::border);
    return l;
}

juce::Font TwelveLookAndFeel::getComboBoxFont (juce::ComboBox&) { return uiFont (13.5f); }
juce::Font TwelveLookAndFeel::getPopupMenuFont()                 { return uiFont (14.f); }

//==============================================================================
TwelveStringerEditor::TwelveStringerEditor (TwelveStringerProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lnf);

    setupKnob (octave,  ParamID::octave,  "Mix",     Palette::octave);
    setupKnob (focus,   ParamID::focus,   "Range",   Palette::octave);
    setupKnob (sparkle, ParamID::sparkle, "Sparkle", Palette::octave);
    setupKnob (unison,  ParamID::unison,  "Mix",     Palette::unison);
    setupKnob (detune,  ParamID::detune,  "Detune",  Palette::unison);
    setupKnob (drift,   ParamID::drift,   "Drift",   Palette::unison);
    setupKnob (strum,   ParamID::strum,   "Strum",   Palette::feel);
    setupKnob (width,   ParamID::width,   "Width",   Palette::feel);
    setupKnob (dry,     ParamID::dry,     "Dry",     Palette::feel);
    setupKnob (output,  ParamID::output,  "Output",  Palette::feel);

    sections[0] = { "OCTAVE STRINGS", "low 4 courses, +12 st", Palette::octave, { &octave, &focus, &sparkle }, {} };
    sections[1] = { "UNISON STRINGS", "high 2 courses, detuned", Palette::unison, { &unison, &detune, &drift }, {} };
    sections[2] = { "FEEL & OUTPUT", "pick timing, stereo, level", Palette::feel, { &strum, &width, &dry, &output }, {} };

    for (int i = 0; i < p.getNumPrograms(); ++i)
        presetBox.addItem (p.getProgramName (i), i + 1);
    presetBox.setSelectedId (p.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedId() - 1;
        if (idx >= 0) processor.setCurrentProgram (idx);
    };
    addAndMakeVisible (presetBox);

    liveButton.setTooltip ("Live mode: zero latency for monitoring while you record. "
                           "Turn off for mixing - HQ mode has a cleaner octave and FL Studio compensates its delay.");
    addAndMakeVisible (liveButton);
    liveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, ParamID::live, liveButton);

    setSize (920, 318);
    startTimerHz (30);
}

TwelveStringerEditor::~TwelveStringerEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void TwelveStringerEditor::setupKnob (Knob& k, const char* paramID, const juce::String& name, juce::Colour accent)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    k.slider.setColour (juce::Slider::textBoxTextColourId, Palette::text.withAlpha (0.85f));
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 18);
    k.slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                  juce::MathConstants<float>::pi * 2.75f, true);
    addAndMakeVisible (k.slider);

    k.label.setText (name.toUpperCase(), juce::dontSendNotification);
    k.label.setFont (uiFont (11.5f, true));
    k.label.setColour (juce::Label::textColourId, Palette::dim);
    k.label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, paramID, k.slider);

    if (auto* param = processor.apvts.getParameter (paramID))
        k.slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
}

//==============================================================================
void TwelveStringerEditor::timerCallback()
{
    phase += 0.12f;
    if (phase > 1000.f) phase = 0.f;
    repaint (stringsArea);

    if (presetBox.getSelectedId() != processor.getCurrentProgram() + 1 && ! presetBox.isPopupActive())
        presetBox.setSelectedId (processor.getCurrentProgram() + 1, juce::dontSendNotification);

    const juce::String dot (juce::CharPointer_UTF8 ("  \xc2\xb7  "));
    const int lat = processor.getLatencySamples();
    const double sr = processor.getSampleRate() > 0 ? processor.getSampleRate() : 44100.0;
    const juce::String s = lat == 0
        ? "LIVE MODE" + dot + "zero latency for monitoring, lighter octave quality"
        : "HQ MODE" + dot + juce::String (1000.0 * lat / sr, 1)
              + " ms latency, reported to FL Studio for automatic delay compensation";

    if (s != statusText)
    {
        statusText = s;
        repaint (statusArea);
    }
}

void TwelveStringerEditor::paintStrings (juce::Graphics& g, juce::Rectangle<float> area) const
{
    const float octMix = (float) octave.slider.getValue() * 0.01f;
    const float uniMix = (float) unison.slider.getValue() * 0.01f;
    const float driftAmt = (float) drift.slider.getValue() * 0.01f;

    const float spacing = area.getHeight() / 6.f;
    const float x0 = area.getX(), x1 = area.getRight();

    for (int c = 0; c < 6; ++c)                 // c = 0 is the low E course (drawn at the bottom)
    {
        const float y = area.getBottom() - (c + 0.5f) * spacing;
        const bool octaveCourse = c < 4;
        const float mix = octaveCourse ? octMix : uniMix;
        const auto accent = octaveCourse ? Palette::octave : Palette::unison;

        // partner string (the one this plugin adds)
        if (mix > 0.01f)
        {
            juce::Path partner;
            const float yp = y - 3.2f;
            const float amp = 0.4f + 1.4f * driftAmt * mix;
            for (int i = 0; i <= 60; ++i)
            {
                const float t = (float) i / 60.f;
                const float env = std::sin (t * juce::MathConstants<float>::pi);
                const float wob = amp * env * std::sin (phase * (1.f + 0.17f * c) + t * 9.f);
                const juce::Point<float> pt (x0 + t * (x1 - x0), yp + wob);
                if (i == 0) partner.startNewSubPath (pt); else partner.lineTo (pt);
            }
            g.setColour (accent.withAlpha (0.25f + 0.75f * mix));
            g.strokePath (partner, juce::PathStrokeType (octaveCourse ? 0.9f : 0.6f + 0.25f * (6 - c)));
        }

        // the guitar's own string
        g.setColour (Palette::steel.withAlpha (0.9f));
        g.fillRect (juce::Rectangle<float> (x0, y - (0.5f + 0.32f * (5 - c)) * 0.5f, x1 - x0, 0.5f + 0.32f * (5 - c)));
    }

    // nut
    g.setColour (Palette::dim.withAlpha (0.6f));
    g.fillRoundedRectangle (x0 - 6.f, area.getY() - 2.f, 4.f, area.getHeight() + 4.f, 1.5f);
}

void TwelveStringerEditor::paint (juce::Graphics& g)
{
    g.fillAll (Palette::bg);

    // ---- header ----
    g.setColour (Palette::text);
    g.setFont (uiFont (26.f, true));
    g.drawText ("TwelveStringer", 24, 18, 260, 32, juce::Justification::left);
    g.setColour (Palette::dim);
    g.setFont (uiFont (12.5f));
    g.drawText (juce::String (juce::CharPointer_UTF8 ("6-string in  \xe2\x86\x92  12-string out")),
                25, 50, 260, 18, juce::Justification::left);

    paintStrings (g, stringsArea.toFloat());

    // ---- sections ----
    for (const auto& s : sections)
    {
        const auto r = s.bounds.toFloat();
        g.setColour (Palette::panel);
        g.fillRoundedRectangle (r, 10.f);
        g.setColour (Palette::border);
        g.drawRoundedRectangle (r.reduced (0.5f), 10.f, 1.f);

        g.setColour (s.accent);
        g.fillRoundedRectangle (r.getX() + 16.f, r.getY() + 17.f, 3.f, 26.f, 1.5f);

        g.setColour (Palette::text);
        g.setFont (uiFont (13.f, true));
        g.drawText (s.title, (int) r.getX() + 26, (int) r.getY() + 13, 220, 18, juce::Justification::left);
        g.setColour (Palette::dim);
        g.setFont (uiFont (11.5f));
        g.drawText (s.caption, (int) r.getX() + 26, (int) r.getY() + 30, 220, 16, juce::Justification::left);
    }

    // ---- status ----
    g.setColour (Palette::dim);
    g.setFont (uiFont (11.5f));
    g.drawText (statusText, statusArea, juce::Justification::centredLeft);
}

void TwelveStringerEditor::resized()
{
    auto area = getLocalBounds().reduced (20, 0);

    auto header = area.removeFromTop (84);
    auto right = header.removeFromRight (190).withSizeKeepingCentre (190, 64);
    presetBox.setBounds (right.removeFromTop (28));
    right.removeFromTop (8);
    liveButton.setBounds (right.removeFromTop (28).removeFromRight (96));

    header.removeFromLeft (290);
    stringsArea = header.reduced (36, 18);

    statusArea = area.removeFromBottom (30).reduced (4, 0);

    // sections: widths proportional to knob count
    const int gap = 12;
    const int totalKnobs = 10;
    const int available = area.getWidth() - gap * 2;
    auto row = area.withTrimmedBottom (4);

    for (size_t i = 0; i < sections.size(); ++i)
    {
        auto& s = sections[i];
        const int w = i == sections.size() - 1 ? row.getWidth()
                                               : available * (int) s.knobs.size() / totalKnobs;
        s.bounds = row.removeFromLeft (w);
        row.removeFromLeft (gap);

        auto inner = s.bounds.reduced (8, 0).withTrimmedTop (58).withTrimmedBottom (12);
        const int kw = inner.getWidth() / (int) s.knobs.size();
        for (auto* k : s.knobs)
        {
            auto cell = inner.removeFromLeft (kw);
            k->label.setBounds (cell.removeFromTop (18));
            const int knob = juce::jmin (kw - 4, cell.getHeight() - 20, 80);
            k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, kw, 18);
            k->slider.setBounds (cell.withSizeKeepingCentre (kw, knob + 20).withY (cell.getY() + 2));
        }
    }
}
