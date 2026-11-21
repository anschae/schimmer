#include "Displays.h"
#include "PluginProcessor.h"
#include "dsp/ReverbModel.h"

namespace schimmer
{
using namespace theme;

dsp::Settings currentSettings (SchimmerProcessor& p)
{
    std::array<float, kNumParams> v {};
    for (int k = 0; k < kNumParams; ++k) v[(size_t) k] = p.value ((P) k);
    return SchimmerProcessor::makeSettings (v.data(), p.hostBpm.load(), 1.0);
}

namespace
{
void drawHandle (juce::Graphics& g, juce::Point<float> c, juce::Colour col, bool hot)
{
    const float r = hot ? 6.5f : 5.0f;
    g.setColour (col.withAlpha (hot ? 0.35f : 0.2f));
    g.fillEllipse (c.x - r - 4, c.y - r - 4, 2 * r + 8, 2 * r + 8);
    g.setColour (col);
    g.fillEllipse (c.x - r, c.y - r, 2 * r, 2 * r);
    g.setColour (bg);
    g.drawEllipse (c.x - r, c.y - r, 2 * r, 2 * r, 1.2f);
}

float niceStep (float range, int targetLines)
{
    const float raw = range / (float) targetLines;
    const float mag = std::pow (10.0f, std::floor (std::log10 (raw)));
    for (float m : { 1.0f, 2.0f, 5.0f, 10.0f })
        if (raw <= m * mag) return m * mag;
    return 10.0f * mag;
}

void beginEnd (SchimmerProcessor& proc, P p, bool begin)
{
    if (auto* prm = proc.getApvts().getParameter (pid (p)))
    {
        if (begin) prm->beginChangeGesture();
        else prm->endChangeGesture();
    }
}
} // namespace

// ============================================================================ MeterHub
MeterHub::MeterHub (SchimmerProcessor& p) : proc (p)
{
    dsp::MeterFrame scratch[256];
    while (proc.getEngine().readMeterFrames (scratch, 256) > 0) {}
    trace.reserve (8000);
    startTimerHz (30);
}

MeterHub::~MeterHub() { stopTimer(); }

void MeterHub::timerCallback()
{
    dsp::MeterFrame f[256];
    const int n = proc.getEngine().readMeterFrames (f, 256);
    for (int i = 0; i < n; ++i)
    {
        const auto& m = f[i];
        // onset: the input jumps well above its recent minimum
        minIn = std::min (m.inDb, minIn + 0.35f);
        if (m.inDb > -50.0f && m.inDb - minIn > 15.0f && sinceOnset > 40)
        {
            trace.clear();
            traceT = 0.0f;
            sinceOnset = 0;
            minIn = m.inDb;
        }
        ++sinceOnset;
        if (trace.size() < 7500)
            trace.push_back ({ traceT, m.wetDb, m.grDb });
        traceT += 0.01f;
        latest = m;
        corrSmoothed += 0.08f * (m.corr - corrSmoothed);
        auto ball = [] (float& v, float x) { v = x > v ? x : v + 0.12f * (x - v); };
        ball (lowDb, m.lowDb);
        ball (midDb, m.midDb);
        ball (highDb, m.highDb);
        ball (wetDb, m.wetDb);
    }
    if (n > 0 && onUpdate)
        onUpdate();
}

// ============================================================================ TailView
TailView::TailView (SchimmerProcessor& p, MeterHub& h) : proc (p), hub (h)
{
    setTooltip ("Tail View: timeline from the input. Shaded = pre-delay, stems = delay repeats (blue L, pink R), "
                "curves = expected decay of the low (orange), mid (blue) and high (lilac) band, white = measured wet "
                "level since the last note, green = ducking. Drag the handles to change pre-delay, decay and delay time.");
}

juce::Rectangle<float> TailView::plotArea() const { return getLocalBounds().toFloat().reduced (10.0f, 8.0f).withTrimmedTop (18.0f).withTrimmedBottom (16.0f).withTrimmedLeft (30.0f); }
float TailView::timeToX (float t) const { auto r = plotArea(); return r.getX() + t / tMax * r.getWidth(); }
float TailView::xToTime (float x) const { auto r = plotArea(); return (x - r.getX()) / r.getWidth() * tMax; }
float TailView::dbToY (float db) const
{
    auto r = plotArea();
    return r.getY() + juce::jlimit (0.0f, 1.0f, (kDbTop - db) / (kDbTop - kDbBottom)) * r.getHeight();
}

juce::Point<float> TailView::handlePos (Handle h) const
{
    const auto s = currentSettings (proc);
    const float top = dbToY (-6.0f);
    switch (h)
    {
        case Handle::PreDelay: return { timeToX (s.preDelayMs * 0.001f), top };
        case Handle::Decay:
        {
            // on the mid curve, where it has fallen by ~30 dB
            const float t = s.preDelayMs * 0.001f + s.decaySec * 0.5f;
            const double db = s.freeze ? -6.0 : dsp::model::envelopeDb (s, t, dsp::model::t60At (s, 1000.0)) + anchorDb;
            return { timeToX (t), dbToY ((float) std::max (db, (double) kDbBottom)) };
        }
        case Handle::DelayTime:
            if (! s.echo.on) return { -100.0f, -100.0f };
            return { timeToX (s.echo.timeMs * 0.001f), dbToY (-2.0f) };
        default: return {};
    }
}

TailView::Handle TailView::hitTest (juce::Point<float> pt) const
{
    for (auto h : { Handle::DelayTime, Handle::PreDelay, Handle::Decay })
        if (handlePos (h).getDistanceFrom (pt) < 11.0f)
            return h;
    return Handle::None;
}

void TailView::setParam (P p, float plain) { proc.setValueNotifying (p, juce::jlimit (info (p).min, info (p).max, plain)); }

void TailView::mouseMove (const juce::MouseEvent& e)
{
    const auto h = hitTest (e.position);
    if (h != hover) { hover = h; repaint(); }
    setMouseCursor (h == Handle::None ? juce::MouseCursor::NormalCursor : juce::MouseCursor::LeftRightResizeCursor);
}

void TailView::mouseDown (const juce::MouseEvent& e)
{
    dragging = hitTest (e.position);
    if (dragging == Handle::PreDelay) beginEnd (proc, P::preDelay, true);
    if (dragging == Handle::Decay) beginEnd (proc, P::decay, true);
    if (dragging == Handle::DelayTime) beginEnd (proc, P::delayTime, true);
}

void TailView::mouseDrag (const juce::MouseEvent& e)
{
    const float t = std::max (0.0f, xToTime (e.position.x));
    switch (dragging)
    {
        case Handle::PreDelay:
            if (proc.value (P::preDelaySync) < 0.5f) setParam (P::preDelay, t * 1000.0f);
            break;
        case Handle::Decay:
            setParam (P::decay, std::max (0.3f, 2.0f * (t - proc.value (P::preDelay) * 0.001f)));
            break;
        case Handle::DelayTime:
            if (proc.value (P::delaySync) < 0.5f) setParam (P::delayTime, std::max (1.0f, t * 1000.0f));
            break;
        default: break;
    }
    repaint();
}

void TailView::mouseUp (const juce::MouseEvent&)
{
    if (dragging == Handle::PreDelay) beginEnd (proc, P::preDelay, false);
    if (dragging == Handle::Decay) beginEnd (proc, P::decay, false);
    if (dragging == Handle::DelayTime) beginEnd (proc, P::delayTime, false);
    dragging = Handle::None;
}

void TailView::mouseDoubleClick (const juce::MouseEvent&)
{
    tMax = 0.0f;   // re-fit the time axis on the next paint
    repaint();
}

void TailView::paint (juce::Graphics& g)
{
    const auto s = currentSettings (proc);
    auto bounds = getLocalBounds().toFloat();
    g.setColour (plot);
    g.fillRoundedRectangle (bounds, 8.0f);
    g.setColour (panelEdge);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.0f);
    drawCaption (g, "TAIL VIEW", getLocalBounds().reduced (12, 4).removeFromTop (18));

    // ---- time range (follows the settings; never jumps while dragging)
    const double tMid = dsp::model::t60At (s, 1000.0);
    float want = (float) (s.preDelayMs * 0.001 + (s.freeze ? 8.0 : std::min (tMid, 70.0) * 1.05) + s.bloomSec);
    if (s.echo.on) want = std::max (want, s.echo.timeMs * 0.001f * 4.0f);
    want = juce::jlimit (1.0f, 80.0f, want);
    if (dragging == Handle::None && (tMax <= 0.0f || want > tMax * 1.02f || want < tMax * 0.6f))
        tMax = want;

    auto r = plotArea();
    // grid
    g.setFont (font (10.5f));
    const float step = niceStep (tMax, 8);
    for (float t = 0.0f; t <= tMax + 1e-3f; t += step)
    {
        const float x = timeToX (t);
        g.setColour (gridLine);
        g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
        g.setColour (textFaint);
        g.drawText (step < 1.0f ? juce::String (t, 1) + " s" : juce::String ((int) std::round (t)) + " s",
                    juce::Rectangle<float> (x - 30, r.getBottom() + 1, 60, 14), juce::Justification::centred);
    }
    for (float db = 0.0f; db >= kDbBottom; db -= 12.0f)
    {
        const float y = dbToY (db);
        g.setColour (gridLine.withAlpha (0.7f));
        g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
        g.setColour (textFaint);
        g.drawText (juce::String ((int) db), juce::Rectangle<float> (r.getX() - 30, y - 7, 26, 14), juce::Justification::centredRight);
    }

    // sync grid (beats) when the delay or the pre-delay is synced
    if ((s.echo.on && proc.value (P::delaySync) > 0.5f) || proc.value (P::preDelaySync) > 0.5f)
    {
        const float beat = 60.0f / (float) std::max (20.0, proc.hostBpm.load());
        g.setColour (hostBar.withAlpha (0.13f));
        for (float t = beat; t < tMax; t += beat)
            g.drawVerticalLine ((int) timeToX (t), r.getY(), r.getBottom());
    }

    // pre-delay
    const float pre = s.preDelayMs * 0.001f;
    g.setColour (reverb.withAlpha (0.07f));
    g.fillRect (juce::Rectangle<float> (r.getX(), r.getY(), timeToX (pre) - r.getX(), r.getHeight()));

    // ---- expected envelopes per band
    const double tL = dsp::model::t60At (s, 125.0), tH = dsp::model::t60At (s, 6000.0);
    // anchor: measured peak of the trace if there is one, else -12 dB
    float anchor = -12.0f;
    {
        float pk = -200.0f;
        for (const auto& pnt : hub.getTrace()) pk = std::max (pk, pnt.wetDb);
        if (pk > -70.0f) anchor = pk;
    }
    anchorDb = anchor;
    auto curve = [&] (double t60, juce::Colour c, float width, bool fill) {
        juce::Path p;
        bool started = false;
        const int N = 300;
        for (int i = 0; i <= N; ++i)
        {
            const double t = tMax * (double) i / N;
            double db = s.freeze && t > pre + 0.3 ? dsp::model::envelopeDb (s, pre + 0.3, 1.0e9) : dsp::model::envelopeDb (s, t, t60);
            db += anchor;
            const float x = timeToX ((float) t), y = dbToY ((float) std::max (db, (double) kDbBottom - 2.0));
            if (! started) { p.startNewSubPath (x, y); started = true; }
            else p.lineTo (x, y);
        }
        if (fill)
        {
            juce::Path f (p);
            f.lineTo (r.getRight(), r.getBottom());
            f.lineTo (r.getX(), r.getBottom());
            f.closeSubPath();
            g.setGradientFill (juce::ColourGradient (c.withAlpha (0.28f), r.getX(), r.getY(), c.withAlpha (0.02f), r.getX(), r.getBottom(), false));
            g.fillPath (f);
        }
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };
    curve (tMid, bandMid, 2.2f, true);
    curve (tL, bandLow.withAlpha (0.85f), 1.4f, false);
    curve (tH, bandHigh.withAlpha (0.85f), 1.4f, false);
    if (s.freeze)
    {
        g.setColour (shimmer);
        g.setFont (font (12.0f, true));
        g.drawText ("FREEZE", r.reduced (8).removeFromTop (16), juce::Justification::topRight);
    }

    // ---- delay repeats as stems
    if (s.echo.on)
    {
        const float dt = s.echo.timeMs * 0.001f;
        const float fb = std::max (0.0f, s.echo.feedback);
        for (int k = 1; k < 200; ++k)
        {
            const float lvl = 20.0f * std::log10 (std::max (1.0e-6f, std::pow (fb, (float) (k - 1))));
            if (lvl < kDbBottom || dt * (float) k > tMax) break;
            auto stem = [&] (float t, juce::Colour c) {
                const float x = timeToX (t);
                g.setColour (c.withAlpha (0.85f));
                g.drawLine (x, r.getBottom(), x, dbToY (lvl + anchor * 0.0f - 2.0f), 2.0f);
                g.fillEllipse (x - 3, dbToY (lvl - 2.0f) - 3, 6, 6);
            };
            if (s.echo.pingPong)
                stem (dt * (float) k + (k % 2 == 0 ? s.echo.offsetMs * 0.001f : 0.0f), k % 2 ? delayL : delayR);
            else
            {
                stem (dt * (float) k, delayL);
                if (std::abs (s.echo.offsetMs) > 0.5f) stem ((dt + s.echo.offsetMs * 0.001f) * (float) k, delayR);
            }
        }
    }

    // ---- live: measured wet level and ducking since the last onset
    const auto& tr = hub.getTrace();
    if (tr.size() > 1)
    {
        juce::Path w, d;
        bool dStarted = false;
        for (size_t i = 0; i < tr.size(); ++i)
        {
            if (tr[i].t > tMax) break;
            const float x = timeToX (tr[i].t);
            if (i == 0) w.startNewSubPath (x, dbToY (std::max (tr[i].wetDb, kDbBottom)));
            else w.lineTo (x, dbToY (std::max (tr[i].wetDb, kDbBottom)));
            if (proc.value (P::duckOn) > 0.5f)
            {
                const float y = dbToY (std::max (tr[i].grDb, kDbBottom));
                if (! dStarted) { d.startNewSubPath (x, y); dStarted = true; }
                else d.lineTo (x, y);
            }
        }
        g.setColour (text.withAlpha (0.75f));
        g.strokePath (w, juce::PathStrokeType (1.3f));
        if (dStarted)
        {
            g.setColour (duck);
            g.strokePath (d, juce::PathStrokeType (1.6f));
        }
    }

    // ---- handles
    drawHandle (g, handlePos (Handle::PreDelay), reverb, hover == Handle::PreDelay || dragging == Handle::PreDelay);
    drawHandle (g, handlePos (Handle::Decay), bandMid, hover == Handle::Decay || dragging == Handle::Decay);
    if (s.echo.on)
        drawHandle (g, handlePos (Handle::DelayTime), delayL, hover == Handle::DelayTime || dragging == Handle::DelayTime);

    // readout
    g.setFont (font (11.5f));
    g.setColour (textDim);
    juce::String info = "pre " + formatValue (P::preDelay, s.preDelayMs) + "   RT60 low " + juce::String (std::min (tL, 999.0), 1)
                        + " s  mid " + juce::String (std::min (tMid, 999.0), 1) + " s  high " + juce::String (std::min (tH, 999.0), 1) + " s";
    if (s.freeze) info = "pre " + formatValue (P::preDelay, s.preDelayMs) + "   infinite (freeze)";
    g.drawText (info, getLocalBounds().reduced (12, 4).removeFromTop (18), juce::Justification::centredRight);
}

// ============================================================================ DecayCurveView
DecayCurveView::DecayCurveView (SchimmerProcessor& p) : proc (p)
{
    setTooltip ("Decay time per frequency (from Decay, Low/High Decay, crossovers, Low Cut, High Cut, Color). "
                "Drag the dots: left = low band, middle = Decay, right = high band; the dashed lines are the "
                "crossovers, the triangles at the bottom Low Cut and High Cut.");
}

juce::Rectangle<float> DecayCurveView::plotArea() const { return getLocalBounds().toFloat().reduced (10.0f, 8.0f).withTrimmedTop (18.0f).withTrimmedBottom (14.0f).withTrimmedLeft (30.0f); }
float DecayCurveView::fToX (float f) const { auto r = plotArea(); return r.getX() + std::log (f / 20.0f) / std::log (1000.0f) * r.getWidth(); }
float DecayCurveView::xToF (float x) const { auto r = plotArea(); return 20.0f * std::pow (1000.0f, (x - r.getX()) / r.getWidth()); }
float DecayCurveView::tToY (float t) const
{
    auto r = plotArea();
    const float lo = std::log (0.1f), hi = std::log (150.0f);
    return r.getBottom() - (std::log (juce::jlimit (0.1f, 150.0f, t)) - lo) / (hi - lo) * r.getHeight();
}
float DecayCurveView::yToT (float y) const
{
    auto r = plotArea();
    const float lo = std::log (0.1f), hi = std::log (150.0f);
    return std::exp (lo + (r.getBottom() - y) / r.getHeight() * (hi - lo));
}

juce::Point<float> DecayCurveView::handlePos (Handle h) const
{
    const auto s = currentSettings (proc);
    auto r = plotArea();
    auto at = [&] (float f) { return juce::Point<float> (fToX (f), tToY ((float) dsp::model::t60At (s, f))); };
    switch (h)
    {
        case Handle::Low: return at (std::min (60.0f, s.lowXHz * 0.4f));
        case Handle::Mid: return at (std::sqrt (s.lowXHz * s.highXHz));
        case Handle::High: return at (std::min (15000.0f, s.highXHz * 2.5f));
        case Handle::LowX: return { fToX (s.lowXHz), r.getY() + 10.0f };
        case Handle::HighX: return { fToX (s.highXHz), r.getY() + 10.0f };
        case Handle::LowCut: return { fToX (std::max (20.0f, s.lowCutHz)), r.getBottom() - 6.0f };
        case Handle::HighCut: return { fToX (std::min (19999.0f, s.highCutHz)), r.getBottom() - 6.0f };
        default: return {};
    }
}

DecayCurveView::Handle DecayCurveView::hitTest (juce::Point<float> pt) const
{
    for (auto h : { Handle::Low, Handle::Mid, Handle::High, Handle::LowCut, Handle::HighCut, Handle::LowX, Handle::HighX })
        if (handlePos (h).getDistanceFrom (pt) < 10.0f) return h;
    return Handle::None;
}

void DecayCurveView::setParam (P p, float plain) { proc.setValueNotifying (p, juce::jlimit (info (p).min, info (p).max, plain)); }

static P paramOf (int h)
{
    static const P map[] = { P::mode, P::lowMult, P::decay, P::highMult, P::lowXover, P::highXover, P::lowCut, P::highCut };
    return map[h];
}

void DecayCurveView::mouseMove (const juce::MouseEvent& e)
{
    const auto h = hitTest (e.position);
    if (h != hover) { hover = h; repaint(); }
    const bool horiz = h == Handle::LowX || h == Handle::HighX || h == Handle::LowCut || h == Handle::HighCut;
    setMouseCursor (h == Handle::None ? juce::MouseCursor::NormalCursor
                                      : horiz ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::UpDownResizeCursor);
}

void DecayCurveView::mouseDown (const juce::MouseEvent& e)
{
    dragging = hitTest (e.position);
    if (dragging != Handle::None) beginEnd (proc, paramOf ((int) dragging), true);
}

void DecayCurveView::mouseDrag (const juce::MouseEvent& e)
{
    const auto s = currentSettings (proc);
    const float t = yToT (e.position.y), f = xToF (e.position.x);
    switch (dragging)
    {
        case Handle::Mid: setParam (P::decay, t); break;
        case Handle::Low: setParam (P::lowMult, t / std::max (0.1f, s.decaySec)); break;
        case Handle::High: setParam (P::highMult, t / std::max (0.1f, s.decaySec)); break;
        case Handle::LowX: setParam (P::lowXover, f); break;
        case Handle::HighX: setParam (P::highXover, f); break;
        case Handle::LowCut: setParam (P::lowCut, f < 22.0f ? 10.0f : f); break;
        case Handle::HighCut: setParam (P::highCut, f > 18000.0f ? 20000.0f : f); break;
        default: break;
    }
    repaint();
}

void DecayCurveView::mouseUp (const juce::MouseEvent&)
{
    if (dragging != Handle::None) beginEnd (proc, paramOf ((int) dragging), false);
    dragging = Handle::None;
}

void DecayCurveView::paint (juce::Graphics& g)
{
    const auto s = currentSettings (proc);
    auto bounds = getLocalBounds().toFloat();
    g.setColour (plot);
    g.fillRoundedRectangle (bounds, 8.0f);
    g.setColour (panelEdge);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.0f);
    drawCaption (g, "DECAY / FREQUENCY", getLocalBounds().reduced (12, 4).removeFromTop (18));
    auto r = plotArea();
    g.setFont (font (10.0f));
    for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const float x = fToX (f);
        g.setColour (gridLine);
        g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
        g.setColour (textFaint);
        g.drawText (f >= 1000.0f ? juce::String ((int) (f / 1000)) + "k" : juce::String ((int) f), juce::Rectangle<float> (x - 20, r.getBottom() + 1, 40, 12),
                    juce::Justification::centred);
    }
    for (float t : { 0.3f, 1.0f, 3.0f, 10.0f, 30.0f, 100.0f })
    {
        const float y = tToY (t);
        g.setColour (gridLine);
        g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
        g.setColour (textFaint);
        g.drawText (t < 1.0f ? juce::String (t, 1) + "s" : juce::String ((int) t) + "s", juce::Rectangle<float> (r.getX() - 32, y - 6, 28, 12),
                    juce::Justification::centredRight);
    }
    // crossovers
    const float dash[] = { 4.0f, 4.0f };
    g.setColour (tone.withAlpha (0.45f));
    for (float f : { s.lowXHz, s.highXHz })
        g.drawDashedLine ({ fToX (f), r.getY(), fToX (f), r.getBottom() }, dash, 2, 1.0f);
    // curve
    juce::Path p;
    for (int i = 0; i <= 200; ++i)
    {
        const float f = 20.0f * std::pow (1000.0f, (float) i / 200.0f);
        const float y = tToY ((float) dsp::model::t60At (s, f));
        if (i == 0) p.startNewSubPath (fToX (f), y);
        else p.lineTo (fToX (f), y);
    }
    juce::Path fill (p);
    fill.lineTo (r.getRight(), r.getBottom());
    fill.lineTo (r.getX(), r.getBottom());
    fill.closeSubPath();
    g.setGradientFill (juce::ColourGradient (tone.withAlpha (0.25f), r.getX(), r.getY(), tone.withAlpha (0.02f), r.getX(), r.getBottom(), false));
    g.fillPath (fill);
    g.setColour (tone);
    g.strokePath (p, juce::PathStrokeType (2.0f));
    if (s.freeze)
    {
        g.setColour (shimmer);
        g.setFont (font (12.0f, true));
        g.drawText (juce::CharPointer_UTF8 ("\xe2\x88\x9e  FREEZE"), r.reduced (6), juce::Justification::topRight);
    }
    // low / high cut markers
    auto tri = [&] (juce::Point<float> c, bool hot, bool active) {
        juce::Path t;
        t.addTriangle (c.x, c.y - 6, c.x - 6, c.y + 5, c.x + 6, c.y + 5);
        g.setColour ((active ? warn : textFaint).withAlpha (hot ? 1.0f : 0.8f));
        g.fillPath (t);
    };
    tri (handlePos (Handle::LowCut), hover == Handle::LowCut, s.lowCutHz > 10.5f);
    tri (handlePos (Handle::HighCut), hover == Handle::HighCut, s.highCutHz < 19900.0f);
    for (auto h : { Handle::LowX, Handle::HighX })
    {
        auto c = handlePos (h);
        g.setColour (tone.withAlpha (hover == h ? 1.0f : 0.6f));
        g.fillRect (c.x - 3, c.y - 6, 6.0f, 12.0f);
    }
    drawHandle (g, handlePos (Handle::Low), bandLow, hover == Handle::Low || dragging == Handle::Low);
    drawHandle (g, handlePos (Handle::Mid), bandMid, hover == Handle::Mid || dragging == Handle::Mid);
    drawHandle (g, handlePos (Handle::High), bandHigh, hover == Handle::High || dragging == Handle::High);
}

// ============================================================================ StereoMeter
StereoMeter::StereoMeter (SchimmerProcessor& p, MeterHub& h) : proc (p), hub (h)
{
    setTooltip ("Wet signal: level per band (low / mid / high) and L/R correlation "
                "(0 = fully decorrelated and wide, +1 = mono, negative = out of phase).");
}

void StereoMeter::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (plot);
    g.fillRoundedRectangle (bounds, 8.0f);
    g.setColour (panelEdge);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.0f);
    drawCaption (g, "STEREO / BANDS", getLocalBounds().reduced (12, 4).removeFromTop (18));
    auto r = getLocalBounds().toFloat().reduced (12.0f, 8.0f).withTrimmedTop (20.0f);

    // band meters
    auto bars = r.removeFromLeft (r.getWidth() * 0.5f).reduced (4.0f, 2.0f);
    const float vals[3] = { hub.lowDb, hub.midDb, hub.highDb };
    const juce::Colour cols[3] = { bandLow, bandMid, bandHigh };
    const char* names[3] = { "LOW", "MID", "HIGH" };
    const float bh = bars.getHeight() / 3.0f;
    for (int i = 0; i < 3; ++i)
    {
        auto row = bars.removeFromTop (bh).reduced (0.0f, 3.0f);
        g.setColour (textFaint);
        g.setFont (font (10.0f, true));
        g.drawText (names[i], row.removeFromLeft (36.0f), juce::Justification::centredLeft);
        g.setColour (control);
        g.fillRoundedRectangle (row, 3.0f);
        const float t = juce::jlimit (0.0f, 1.0f, (vals[i] + 72.0f) / 72.0f);
        g.setColour (cols[i]);
        g.fillRoundedRectangle (row.withWidth (row.getWidth() * t), 3.0f);
    }

    // correlation
    auto c = r.reduced (8.0f, 4.0f);
    g.setFont (font (10.0f, true));
    g.setColour (textFaint);
    g.drawText ("CORRELATION", c.removeFromTop (14.0f), juce::Justification::centred);
    auto bar = c.removeFromTop (14.0f);
    g.setColour (control);
    g.fillRoundedRectangle (bar, 3.0f);
    g.setColour (gridLine);
    g.drawVerticalLine ((int) bar.getCentreX(), bar.getY(), bar.getBottom());
    const float cv = juce::jlimit (-1.0f, 1.0f, hub.corrSmoothed);
    const float x = bar.getCentreX() + cv * bar.getWidth() * 0.5f;
    g.setColour (cv < -0.2f ? warn : reverb);
    g.fillRoundedRectangle (juce::Rectangle<float> (std::min (x, bar.getCentreX()), bar.getY() + 2, std::abs (x - bar.getCentreX()) + 2.0f, bar.getHeight() - 4), 2.0f);
    g.setColour (textDim);
    g.setFont (font (10.0f));
    auto lab = c.removeFromTop (14.0f);
    g.drawText ("-1", lab, juce::Justification::centredLeft);
    g.drawText ("0", lab, juce::Justification::centred);
    g.drawText ("+1", lab, juce::Justification::centredRight);
    // width visual: an ellipse (wide = decorrelated)
    auto e = c.reduced (4.0f);
    const float w = e.getWidth() * juce::jlimit (0.15f, 1.0f, 1.0f - 0.85f * std::max (0.0f, cv));
    const float level = juce::jlimit (0.0f, 1.0f, (hub.wetDb + 72.0f) / 72.0f);
    g.setColour (reverb.withAlpha (0.12f + 0.35f * level));
    g.fillEllipse (e.withSizeKeepingCentre (w, e.getHeight() * (0.4f + 0.6f * level)));
    const float lim = proc.getEngine().limiterGain.load();
    if (lim < 0.97f)
    {
        g.setColour (warn);
        g.setFont (font (10.0f, true));
        g.drawText ("LIMIT " + juce::String (juce::Decibels::gainToDecibels (lim), 1) + " dB", e, juce::Justification::bottomRight);
    }
}
} // namespace schimmer
