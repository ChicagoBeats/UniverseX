#include "Randomizer.h"

namespace ux
{
Randomizer::Randomizer(juce::AudioProcessorValueTreeState& state) : apvts(state) {}

Section Randomizer::sectionForLazy(int buttonIndex)
{
    return static_cast<Section>(juce::jlimit(0, 8, buttonIndex));
}

void Randomizer::lazy(int buttonIndex)
{
    if (buttonIndex >= numLazyButtons - 1)
        randomizeAll();
    else
        randomizeSection(sectionForLazy(buttonIndex));
}

void Randomizer::randomizeSection(Section s)
{
    pushUndo();
    randomizeSectionInternal(s);
}

void Randomizer::randomizeAll()
{
    pushUndo();

    for (int i = 0; i < (int) Section::Global; ++i)
    {
        const auto s = static_cast<Section>(i);
        if (! isSectionLocked(s))
            randomizeSectionInternal(s);
    }
}

void Randomizer::randomizeSectionInternal(Section s)
{
    for (const auto& d : getParamDefs())
        if (d.section == s && d.randomizable)
            randomizeParam(d);

    if (s == Section::Oscillators)
        keepSoundAudible();
}

void Randomizer::randomizeParam(const ParamDef& d)
{
    auto* p = apvts.getParameter(d.id);
    if (p == nullptr)
        return;

    const float current = p->getValue();
    float target;

    if (d.isChoice())
    {
        // Choices cannot be blended: change them with probability = amount.
        if (rng.nextFloat() > amount)
            return;

        target = p->convertTo0to1((float) rng.nextInt(d.choices.size()));
        p->beginChangeGesture();
        p->setValueNotifyingHost(target);
        p->endChangeGesture();
        return;
    }

    target = d.randLo + rng.nextFloat() * (d.randHi - d.randLo);
    const float newValue = juce::jlimit(0.0f, 1.0f, current + (target - current) * amount);

    p->beginChangeGesture();
    p->setValueNotifyingHost(newValue);
    p->endChangeGesture();
}

// Avoid generating a silent patch: at least one sound source must be audible.
void Randomizer::keepSoundAudible()
{
    auto get = [this](const char* id)
    {
        auto* p = apvts.getParameter(id);
        return p != nullptr ? p->getValue() : 0.0f;
    };

    if (get("osc1_level") < 0.1f && get("osc2_level") < 0.1f && get("osc3_level") < 0.1f
        && get("sub_level") < 0.1f && get("noise_level") < 0.1f)
    {
        setNormalised("osc1_level", 0.7f);
    }
}

void Randomizer::setNormalised(const juce::String& id, float v)
{
    if (auto* p = apvts.getParameter(id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost(v);
        p->endChangeGesture();
    }
}

// ---------------- undo / redo ----------------

std::vector<float> Randomizer::snapshot() const
{
    std::vector<float> s;
    s.reserve(getParamDefs().size());

    for (const auto& d : getParamDefs())
    {
        auto* p = apvts.getParameter(d.id);
        s.push_back(p != nullptr ? p->getValue() : 0.0f);
    }
    return s;
}

void Randomizer::apply(const std::vector<float>& values)
{
    const auto& defs = getParamDefs();

    for (size_t i = 0; i < defs.size() && i < values.size(); ++i)
        setNormalised(defs[i].id, values[i]);
}

void Randomizer::pushUndo()
{
    undoStack.push_back(snapshot());

    if (undoStack.size() > maxHistory)
        undoStack.erase(undoStack.begin());

    redoStack.clear();
}

bool Randomizer::undo()
{
    if (undoStack.empty())
        return false;

    redoStack.push_back(snapshot());
    apply(undoStack.back());
    undoStack.pop_back();
    return true;
}

bool Randomizer::redo()
{
    if (redoStack.empty())
        return false;

    undoStack.push_back(snapshot());
    apply(redoStack.back());
    redoStack.pop_back();
    return true;
}
}
