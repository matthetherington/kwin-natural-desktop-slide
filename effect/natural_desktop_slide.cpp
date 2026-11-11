/*
    SPDX-FileCopyrightText: 2007 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2008 Lucas Murray <lmurray@undefinedfire.com>
    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2026 Matt Hetherington
    SPDX-License-Identifier: GPL-2.0-or-later

    Adapted from KWin v6.8.2 Slide on 2026-11-11: unique plugin/classes,
    JSON panel policy, frozen per-transition decisions, read-only D-Bus
    diagnostics, fixed wallpaper/gap defaults, RAII screen ownership, and
    selective dock translation/elevation. See docs/UPSTREAM.md.
*/
#include "natural_desktop_slide.h"
#include "compat/runtime_version.h"
#include "window_adapter.h"
#include "overscroll.h"
#include "core/output.h"
#include "core/renderviewport.h"
#include "core/rendertarget.h"
#include "opengl/glutils.h"
#include "effect/effecthandler.h"
#include "window.h"
#include <QDBusConnection>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QScopedValueRollback>
#include <algorithm>
#include <array>
#include <cmath>

namespace KWin
{
namespace
{
constexpr auto dbusPath = "/org/kde/KWin/Effects/NaturalDesktopSlide";
NaturalSwipe::PanelInfo panelInfo(const EffectWindow *window)
{
    if (!window->screen() || !window->window()) return {};
    const auto frame = window->frameGeometry();
    const auto output = window->screen()->geometry();
    return NaturalSwipe::panelFromWindow(window->isDock(), window->window()->resourceClass(),
        window->screen()->name(), QRectF(frame.x(), frame.y(), frame.width(), frame.height()),
        QRectF(output.x(), output.y(), output.width(), output.height()));
}
QJsonArray rectangle(const QRectF &rect)
{
    return {rect.x(), rect.y(), rect.width(), rect.height()};
}
}

NaturalSlideEffect::NaturalSlideEffect()
{
    m_configPath = NaturalSwipe::panelConfigPath();
    m_gesturePath = NaturalSwipe::gestureConfigPath();
    reconfigure(ReconfigureAll);
    connect(effects, &EffectsHandler::desktopChanged, this, &NaturalSlideEffect::desktopChanged);
    connect(effects, &EffectsHandler::desktopChanging, this, &NaturalSlideEffect::desktopChanging);
    connect(effects, QOverload<>::of(&EffectsHandler::desktopChangingCancelled), this, [this]() {
        if (effects->activeFullScreenEffect() == this) {
            for (auto &[output, screen] : m_screens) screen->desktopChangingCancelled();
            m_releaseFlick = false;
        }
    });
    connect(effects, &EffectsHandler::windowAdded, this, [this](EffectWindow *window) {
        if (auto screen = screenState(window->screen())) screen->windowAdded(window);
        if (isActive()) {
            window->setData(WindowForceBlurRole, true);
            window->setData(WindowForceBackgroundContrastRole, true);
        }
    });
    connect(effects, &EffectsHandler::windowDeleted, this, [this](EffectWindow *window) {
        for (auto &[output, screen] : m_screens) screen->windowDeleted(window);
    });
    connect(effects, &EffectsHandler::desktopAdded, this, &NaturalSlideEffect::finishedSwitching);
    connect(effects, &EffectsHandler::desktopRemoved, this, &NaturalSlideEffect::finishedSwitching);
    const auto watchOutput = [this](LogicalOutput *output) {
        connect(output, &LogicalOutput::aboutToChange, this, [this]() { finishedSwitching(); });
        connect(output, &LogicalOutput::geometryChanged, this, &NaturalSlideEffect::finishedSwitching);
        connect(output, &LogicalOutput::scaleChanged, this, &NaturalSlideEffect::finishedSwitching);
    };
    for (auto output : effects->screens()) watchOutput(output);
    connect(effects, &EffectsHandler::screenAdded, this, [this, watchOutput](LogicalOutput *output) {
        finishedSwitching();
        watchOutput(output);
    });
    connect(effects, &EffectsHandler::screenRemoved, this, [this](LogicalOutput *) { finishedSwitching(); });
    connect(effects, &EffectsHandler::currentActivityAboutToChange, this, [this]() { m_switchingActivity = true; });
    connect(effects, &EffectsHandler::currentActivityChanged, this, [this]() { m_switchingActivity = false; });
    m_dbusRegistered = QDBusConnection::sessionBus().registerObject(
        QString::fromLatin1(dbusPath), this, QDBusConnection::ExportScriptableSlots);
    if (!m_dbusRegistered) qWarning() << "Natural Desktop Slide: could not register diagnostics D-Bus object";
}

NaturalSlideEffect::~NaturalSlideEffect()
{
    finishedSwitching();
    if (m_dbusRegistered) QDBusConnection::sessionBus().unregisterObject(QString::fromLatin1(dbusPath));
}
bool NaturalSlideEffect::supported()
{
    // The configurable solid gap uses GL shaders/VBOs. Refuse software/QPainter
    // compositors instead of advertising support and dereferencing a GL context.
    return NaturalSwipe::runtimeMatchesBuild() && effects->animationsSupported() && effects->isOpenGLCompositing();
}

bool NaturalSlideEffect::overscrollEnabled() const
{
    return overscrollSupported() && gestureSettings().overscrollEnabled;
}

bool NaturalSlideEffect::overscrollSupported() const
{
    return true;
}

QString NaturalSlideEffect::overscrollMode() const
{
    return m_overscrollMode.isEmpty() ? gestureSettings().overscrollBackground : m_overscrollMode;
}

double NaturalSlideEffect::settlingSpeed(bool returning) const
{
    const auto &settings = gestureSettings();
    return returning ? settings.returnSpeed : m_releaseFlick ? settings.flickSettleSpeed : settings.settleSpeed;
}

void NaturalSlideEffect::loadGestureSettings()
{
    if (!m_gestureSettings.loadFile(m_gesturePath, &m_gestureError)) {
        qWarning().noquote() << "Natural Desktop Slide: retaining previous gesture settings:" << m_gestureError;
    } else {
        m_overscrollMode.clear();
    }
    m_gestureReloadPending = false;
}

bool NaturalSlideEffect::setOverscrollMode(const QString &mode)
{
    // Idle-only comparison control; normal changes use the saved settings UI.
    if (!overscrollEnabled() || isActive() || (mode != "black" && mode != "wallpaper")) return false;
    m_overscrollMode = mode;
    effects->addRepaintFull();
    return true;
}

void NaturalSlideEffect::reconfigure(ReconfigureFlags)
{
    // Decisions and elevation remain frozen until the current transition ends.
    if (!m_config.loadFile(m_configPath, &m_configError)) {
        qWarning().noquote() << "Natural Desktop Slide: retaining previous rules:" << m_configError;
    }
    if (isActive()) m_gestureReloadPending = true;
    else loadGestureSettings();
}
bool NaturalSlideEffect::isActive() const
{
    return std::ranges::any_of(m_screens, [](const auto &entry) { return entry.second->isActive(); });
}
bool NaturalSlideEffect::shouldSlidePanel(const EffectWindow *window) const
{
    return window->isOnAllDesktops()
        && NaturalSwipe::selectPanel(panelInfo(window), m_config.rules()).action == NaturalSwipe::PanelAction::Slide;
}
QString NaturalSlideEffect::configurationStatus() const
{
    QJsonArray outputs;
    for (const auto &[output, screen] : m_screens) {
        auto state = screen->status();
        state["output"] = output->name();
        outputs.append(state);
    }
    return QString::fromUtf8(QJsonDocument(QJsonObject{
        {"path", m_configPath}, {"error", m_configError},
        {"ruleCount", static_cast<qint64>(m_config.rules().size())},
        {"defaultAction", "fixed"}, {"overscrollSupported", overscrollSupported()},
        {"overscrollEnabled", overscrollEnabled()},
        {"overscrollMode", overscrollEnabled() ? overscrollMode() : QStringLiteral("disabled")},
        {"gesturePath", m_gesturePath}, {"gestureError", m_gestureError},
        {"settingsDeferred", m_gestureReloadPending},
        {"continuousGestureHandover", true},
        {"multiOutputGestures", true},
        {"outputs", outputs},
        {"gestureSettings", NaturalSwipe::gestureSettingsJson(gestureSettings())}}).toJson());
}
QString NaturalSlideEffect::panelSnapshot() const
{
    QJsonArray entries;
    for (const auto window : effects->stackingOrder()) {
        if (!window->isDock() || !window->screen() || !window->window()) continue;
        const auto panel = panelInfo(window);
        const auto decision = NaturalSwipe::selectPanel(panel, m_config.rules());
        QJsonObject entry{
            {"label", window->internalId().toString()},
            {"isPlasmaPanel", panel.isPlasmaPanel}, {"output", panel.output},
            {"geometry", rectangle(panel.geometry)}, {"outputGeometry", rectangle(panel.outputGeometry)},
            {"resourceClass", window->window()->resourceClass()},
            {"onAllDesktops", window->isOnAllDesktops()}, {"hidden", window->isHidden()},
            {"action", shouldSlidePanel(window) ? "slide" : "fixed"}};
        if (decision.ruleIndex) {
            entry["ruleIndex"] = static_cast<qint64>(*decision.ruleIndex);
            entry["ruleName"] = m_config.rules()[*decision.ruleIndex].name;
        }
        entries.append(entry);
    }
    return QString::fromUtf8(QJsonDocument(entries).toJson());
}
NaturalSlideScreen *NaturalSlideEffect::screenState(LogicalOutput *output)
{
    const auto it = m_screens.find(output);
    return it == m_screens.end() ? nullptr : it->second.get();
}
NaturalSlideScreen &NaturalSlideEffect::ensureScreen(LogicalOutput *output)
{
    auto &screen = m_screens[output];
    if (!screen) screen = std::make_unique<NaturalSlideScreen>(this, output);
    return *screen;
}
void NaturalSlideEffect::desktopChanged(VirtualDesktop *old, VirtualDesktop *current, EffectWindow *with, LogicalOutput *output)
{
    if (!output || m_switchingActivity
        || (effects->hasActiveFullScreenEffect() && effects->activeFullScreenEffect() != this)) return;
    if (!isActive()) {
        for (const auto window : effects->stackingOrder()) {
            window->setData(WindowForceBlurRole, true);
            window->setData(WindowForceBackgroundContrastRole, true);
        }
    }
    ensureScreen(output).desktopChanged(old, current, with);
    effects->setActiveFullScreenEffect(this);
}
void NaturalSlideEffect::desktopChanging(VirtualDesktop *old, QPointF offset, EffectWindow *with, LogicalOutput *output)
{
    if (!output || m_switchingActivity
        || (effects->hasActiveFullScreenEffect() && effects->activeFullScreenEffect() != this)) return;
    if (!isActive()) {
        for (const auto window : effects->stackingOrder()) {
            window->setData(WindowForceBlurRole, true);
            window->setData(WindowForceBackgroundContrastRole, true);
        }
    }
    ensureScreen(output).desktopChanging(old, offset, with);
    effects->setActiveFullScreenEffect(this);
}
void NaturalSlideEffect::prePaintScreen(ScreenPrePaintData &data)
{
    if (auto screen = screenState(data.screen)) screen->prePaintScreen(data);
    effects->prePaintScreen(data);
}
void NaturalSlideEffect::prePaintWindow(RenderView *view, EffectWindow *window, WindowPrePaintData &data)
{
    data.setTransformed();
    effects->prePaintWindow(view, window, data);
}
NaturalSlidePaintResult NaturalSlideEffect::paintWindow(const RenderTarget &target, const RenderViewport &viewport, EffectWindow *window,
                                     int mask, const Region &region, WindowPaintData &data)
{
    return NaturalSwipe::paintResult<NaturalSlidePaintResult>([&]() {
        if (auto screen = screenState(m_paintOutput); screen && screen->isActive())
            return screen->paintWindow(target, viewport, window, mask, region, data);
        // References held for another output must not reveal inactive desktops.
        if (m_paintOutput && !window->isOnCurrentDesktop()) return true;
        return NaturalSwipe::paintSucceeded([&]() { return effects->paintWindow(target, viewport, window, mask, region, data); });
    });
}
NaturalSlidePaintResult NaturalSlideEffect::paintScreen(const RenderTarget &target, const RenderViewport &viewport,
                                    int mask, const Region &region, LogicalOutput *output)
{
    const QScopedValueRollback<LogicalOutput *> scopedOutput(m_paintOutput, output);
    return NaturalSwipe::paintResult<NaturalSlidePaintResult>([&]() { return effects->paintScreen(target, viewport, mask, region, output); });
}
void NaturalSlideEffect::cancelGestureForOutput(const QString &outputName)
{
    if (effects->activeFullScreenEffect() != this) return;
    for (auto &[output, screen] : m_screens) {
        if (output->name() == outputName) screen->desktopChangingCancelled();
    }
}
void NaturalSlideEffect::postPaintScreen()
{
    for (auto &[output, screen] : m_screens) screen->postPaintScreen();
    if (!isActive()) {
        finishedSwitching();
        if (m_gestureReloadPending) loadGestureSettings();
    }
    effects->addRepaintFull();
    effects->postPaintScreen();
}
void NaturalSlideEffect::finishedSwitching()
{
    m_screens.clear();
    m_releaseFlick = false;
    // Never clear another fullscreen effect's state when unloaded while idle.
    if (effects->activeFullScreenEffect() == this) {
        for (const auto window : effects->stackingOrder()) {
            window->setData(WindowForceBlurRole, QVariant());
            window->setData(WindowForceBackgroundContrastRole, QVariant());
        }
        effects->setActiveFullScreenEffect(nullptr);
    }
}

NaturalSlideScreen::NaturalSlideScreen(NaturalSlideEffect *parent, LogicalOutput *screen)
    : m_motionX(300.0 / effects->animationTimeFactor(), 1.1)
    , m_motionY(300.0 / effects->animationTimeFactor(), 1.1)
    , m_parent(parent)
    , m_screen(screen)
{
}
NaturalSlideScreen::~NaturalSlideScreen() { finishedSwitching(); }
bool NaturalSlideScreen::isActive() const { return m_state != State::Inactive; }
QJsonObject NaturalSlideScreen::status() const
{
    const auto position = m_state == State::ActiveAnimation ? animationPosition() : m_gesturePos;
    return {{"active", isActive()}, {"gesture", m_state == State::ActiveGesture},
            {"position", QJsonArray{position.x(), position.y()}}, {"windowPaints", qint64(m_windowPaints)}};
}
void NaturalSlideScreen::prePaintScreen(ScreenPrePaintData &data)
{
    if (!isActive()) return;
    if (m_state == State::ActiveAnimation) {
        const auto delta = m_animationSpeed.advance(m_clock.tick(data.view));
        m_motionX.advance(delta);
        m_motionY.advance(delta);
        const QSize size = effects->virtualScreenSize();
        m_position = {m_motionX.position() / size.width(), m_motionY.position() / size.height()};
    } else {
        m_position = m_gesturePos;
    }
    const int width = effects->desktopGridWidth(), height = effects->desktopGridHeight();
    if (m_parent->overscrollEnabled() && !effects->optionRollOverDesktops()) {
        m_position.setX(std::clamp(m_position.x(), -m_settings.overscrollLimit,
                                  width - 1 + m_settings.overscrollLimit));
    }
    m_visibleDesktops.clear();
    // Reset the inclusion tests for every desktop (unlike the upstream loop).
    for (const auto desktop : effects->desktops()) {
        const QPoint coords = effects->desktopGridCoords(desktop);
        const bool x = coords.x() % width == int(m_position.x()) % width
            || coords.x() % width == (int(m_position.x()) + 1) % width;
        const bool y = coords.y() % height == int(m_position.y()) % height
            || coords.y() % height == (int(m_position.y()) + 1) % height;
        if (x && y) m_visibleDesktops.append(desktop);
    }
    data.mask |= Effect::PAINT_SCREEN_TRANSFORMED;
}
bool NaturalSlideScreen::slidesPanel(const EffectWindow *window) const
{
    const auto it = m_windows.constFind(window);
    return it != m_windows.cend() && it->slidePanel;
}
bool NaturalSlideScreen::isTranslated(const EffectWindow *window) const
{
    if (!isActive()) return false;
    if (window->isOnAllDesktops()) return window->isDesktop() || slidesPanel(window);
    return window != m_movingWindow;
}
bool NaturalSlideScreen::willBePainted(const EffectWindow *window) const
{
    if (!isActive() || window->isOnAllDesktops() || window == m_movingWindow) return true;
    return std::ranges::any_of(m_visibleDesktops, [window](auto desktop) { return window->isOnDesktop(desktop); });
}
bool NaturalSlideScreen::paintWindow(const RenderTarget &target, const RenderViewport &viewport, EffectWindow *window,
                                    int mask, const Region &region, WindowPaintData &data)
{
    // Never translate another output's wallpaper/panel into this viewport.
    if (!window->frameGeometry().intersects(m_screen->geometry())) return true;
    if (!willBePainted(window)) return true;
    if (!isTranslated(window)) return NaturalSwipe::paintSucceeded([&]() { return effects->paintWindow(target, viewport, window, mask, region, data); });
    ++m_windowPaints;
    const bool wrap = effects->optionRollOverDesktops();
    QPointF position = m_parent->overscrollEnabled() && !wrap ? m_position : forcePositivePosition(m_position);
    if (wrap) position = constrainToDrawableRange(position);
    const int width = effects->desktopGridWidth(), height = effects->desktopGridHeight();
    const bool wrapX = (wrap || !m_parent->overscrollEnabled()) && position.x() > width - 1;
    const bool wrapY = (wrap || !m_parent->overscrollEnabled()) && position.y() > height - 1;
    if (!wrap && m_parent->overscrollEnabled() && window->isDesktop()) {
        const Rect area = m_screen->geometry();
        const QRectF gap = NaturalSwipe::overscrollGap(position.x(), width,
            QRectF(area.x(), area.y(), area.width(), area.height()), m_settings.overscrollLimit);
        if (!gap.isEmpty()) {
            // Move a wallpaper-only adjacent tile with the viewport. At the
            // left edge this reveals its rightmost pixels first (mirrored at
            // the right edge). No windows/panels are copied into that tile.
            const RectF logicalGap(gap.x(), gap.y(), gap.width(), gap.height());
            const auto clipped = region.intersected(viewport.mapToDeviceCoordinatesAligned(logicalGap));
            if (m_parent->wallpaperOverscroll()) {
                WindowPaintData adjacent(data);
                adjacent += QPointF(NaturalSwipe::overscrollWallpaperOffset(position.x(), width, area.width(), m_settings.overscrollLimit), 0);
                if (!NaturalSwipe::paintSucceeded([&]() { return effects->paintWindow(target, viewport, window, mask, clipped, adjacent); })) return false;
            } else {
                // Paint at wallpaper depth, so fixed panels and windows remain
                // above the gap. Use the transition's frozen settings snapshot.
                ShaderBinder binder(ShaderTrait::UniformColor | ShaderTrait::TransformColorspace);
                binder.shader()->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, viewport.projectionMatrix());
                binder.shader()->setUniform(GLShader::ColorUniform::Color, QColor(m_settings.overscrollColor));
                binder.shader()->setColorspaceUniforms(ColorDescription::sRGB, target.colorDescription(), RenderingIntent::Perceptual);
                auto vbo = GLVertexBuffer::streamingBuffer();
                for (const auto &rect : clipped.rects()) {
                    const auto r = viewport.mapFromDeviceCoordinates(RectF(rect)).scaled(viewport.scale());
                    const std::array<QVector2D, 4> vertices{{
                        {float(r.left()), float(r.top())}, {float(r.right()), float(r.top())},
                        {float(r.left()), float(r.bottom())}, {float(r.right()), float(r.bottom())}}};
                    vbo->setVertices(vertices);
                    vbo->render(GL_TRIANGLE_STRIP);
                }
            }
        }
    }
    for (const auto desktop : std::as_const(m_visibleDesktops)) {
        if (!window->isOnDesktop(desktop)) continue;
        const QPoint coords = effects->desktopGridCoords(desktop);
        QPointF translation = QPointF(coords) - position;
        if (coords.x() == 0 && wrapX) translation.rx() += width;
        if (coords.y() == 0 && wrapY) translation.ry() += height;
        {
            const Rect area = m_screen->geometry();
            const QPoint offset(translation.x() * area.width(), translation.y() * area.height());
            WindowPaintData copy(data);
            copy += offset;
            const Rect damage = area.translated(offset).intersected(area);
            if (!NaturalSwipe::paintSucceeded([&]() { return effects->paintWindow(target, viewport, window, mask,
                region.intersected(viewport.mapToDeviceCoordinatesAligned(damage)), copy); })) return false;
        }
    }
    return true;
}
void NaturalSlideScreen::postPaintScreen()
{
    if (m_state == State::ActiveAnimation && !m_motionX.isMoving() && !m_motionY.isMoving()) finishedSwitching();
}
void NaturalSlideScreen::windowAdded(EffectWindow *window)
{
    if (!isActive() || !window->frameGeometry().intersects(m_screen->geometry())) return;
    const bool slide = window->screen() == m_screen && m_parent->shouldSlidePanel(window);
    m_windows.insert(window, {EffectWindowVisibleRef(window, EffectWindow::PAINT_DISABLED_BY_DESKTOP), slide});
    if (window->isDock() && window->screen() == m_screen && !slide) {
        effects->setElevatedWindow(window, true);
        m_elevated.append(window);
    }
}
void NaturalSlideScreen::windowDeleted(EffectWindow *window)
{
    if (window == m_movingWindow) m_movingWindow = nullptr;
    m_elevated.removeAll(window);
    m_windows.remove(window);
}
void NaturalSlideScreen::prepareSwitching()
{
    m_settings = m_parent->gestureSettings();
    m_state = State::ActiveAnimation;
    for (const auto window : effects->stackingOrder()) windowAdded(window);
}
void NaturalSlideScreen::finishedSwitching()
{
    for (const auto window : std::as_const(m_elevated)) effects->setElevatedWindow(window, false);
    m_elevated.clear();
    m_windows.clear();
    m_movingWindow = nullptr;
    m_handover.reset();
    m_state = State::Inactive;
}
void NaturalSlideScreen::startAnimation(QPointF oldPosition, VirtualDesktop *current, EffectWindow *moving, bool returning)
{
    if (!isActive()) prepareSwitching();
    if (m_state == State::ActiveGesture) {
        // Once fingers own the position, momentum from an earlier interrupted
        // spring is stale. Do not reintroduce it when this gesture releases.
        m_motionX.setVelocity(0);
        m_motionY.setVelocity(0);
    }
    m_handover.reset();
    m_state = State::ActiveAnimation;
    m_movingWindow = moving;
    m_startPos = oldPosition;
    m_endPos = effects->desktopGridCoords(current);
    if (effects->optionRollOverDesktops()) optimizePath();
    const QSize size = effects->virtualScreenSize();
    m_motionX.setAnchor(m_endPos.x() * size.width());
    m_motionX.setPosition(m_startPos.x() * size.width());
    m_motionY.setAnchor(m_endPos.y() * size.height());
    m_motionY.setPosition(m_startPos.y() * size.height());
    m_clock.reset();
    m_animationSpeed.reset(m_parent->settlingSpeed(returning));
    effects->addRepaint(m_screen->geometry());
}
void NaturalSlideScreen::desktopChanged(VirtualDesktop *old, VirtualDesktop *current, EffectWindow *with)
{
    QPointF previous;
    if (m_state == State::Inactive) previous = effects->desktopGridCoords(old);
    else if (m_state == State::ActiveGesture) previous = m_gesturePos;
    else previous = animationPosition();
    startAnimation(previous, current, with);
}
void NaturalSlideScreen::desktopChanging(VirtualDesktop *old, QPointF offset, EffectWindow *with)
{
    // Capture the rendered spring position BEFORE replacing its state. Also
    // works when release and takeover arrive before another paint frame.
    const auto inherited = m_state == State::ActiveAnimation
        ? std::optional<QPointF>(animationPosition()) : std::nullopt;
    if (!isActive()) prepareSwitching();
    m_state = State::ActiveGesture;
    m_movingWindow = with;
    m_gesturePos = constrainGesturePosition(m_handover.update(
        QPointF(effects->desktopGridCoords(old)), offset, inherited));
    effects->addRepaint(m_screen->geometry());
}
QPointF NaturalSlideScreen::animationPosition() const
{
    const QSize size = effects->virtualScreenSize();
    if (size.isEmpty()) return m_position;
    return constrainGesturePosition({m_motionX.position() / size.width(), m_motionY.position() / size.height()});
}
QPointF NaturalSlideScreen::constrainGesturePosition(QPointF position) const
{
    if (effects->optionRollOverDesktops()) {
        return forcePositivePosition(position);
    } else if (m_parent->overscrollEnabled()) {
        return {std::clamp(position.x(), -m_settings.overscrollLimit,
                                      effects->desktopGridWidth() - 1 + m_settings.overscrollLimit),
                std::clamp(position.y(), 0.0, double(effects->desktopGridHeight() - 1))};
    } else {
        return moveInsideDesktopGrid(position);
    }
}
void NaturalSlideScreen::desktopChangingCancelled()
{
    if (!isActive()) return;
    const auto position = m_state == State::ActiveGesture ? m_gesturePos : animationPosition();
    startAnimation(position, effects->currentDesktop(m_screen), nullptr, true);
}
QPointF NaturalSlideScreen::forcePositivePosition(QPointF position) const
{
    if (position.x() < 0) position.rx() += std::ceil(-position.x() / effects->desktopGridWidth()) * effects->desktopGridWidth();
    if (position.y() < 0) position.ry() += std::ceil(-position.y() / effects->desktopGridHeight()) * effects->desktopGridHeight();
    return position;
}
QPointF NaturalSlideScreen::constrainToDrawableRange(QPointF position) const
{
    return {std::fmod(position.x(), effects->desktopGridWidth()), std::fmod(position.y(), effects->desktopGridHeight())};
}
QPointF NaturalSlideScreen::moveInsideDesktopGrid(QPointF position) const
{
    return {std::clamp(position.x(), 0.0, double(effects->desktopGridWidth() - 1)),
            std::clamp(position.y(), 0.0, double(effects->desktopGridHeight() - 1))};
}
void NaturalSlideScreen::optimizePath()
{
    const int width = effects->desktopGridWidth(), height = effects->desktopGridHeight();
    if (m_startPos.x() >= width && m_endPos.x() >= width) {
        m_startPos.setX(std::fmod(m_startPos.x(), width));
        m_endPos.setX(std::fmod(m_endPos.x(), width));
    }
    if (m_startPos.y() >= height && m_endPos.y() >= height) {
        m_startPos.setY(std::fmod(m_startPos.y(), height));
        m_endPos.setY(std::fmod(m_endPos.y(), height));
    }
    if (std::abs(m_startPos.x() - m_endPos.x()) > width / 2.0) {
        if (m_startPos.x() < m_endPos.x()) {
            while (m_startPos.x() < m_endPos.x()) m_startPos.rx() += width;
        } else {
            while (m_endPos.x() < m_startPos.x()) m_endPos.rx() += width;
        }
        if (m_startPos.x() >= width && m_endPos.x() >= width) {
            m_startPos.setX(std::fmod(m_startPos.x(), width));
            m_endPos.setX(std::fmod(m_endPos.x(), width));
        }
    }
    if (std::abs(m_startPos.y() - m_endPos.y()) > height / 2.0) {
        if (m_startPos.y() < m_endPos.y()) {
            while (m_startPos.y() < m_endPos.y()) m_startPos.ry() += height;
        } else {
            while (m_endPos.y() < m_startPos.y()) m_endPos.ry() += height;
        }
        if (m_startPos.y() >= height && m_endPos.y() >= height) {
            m_startPos.setY(std::fmod(m_startPos.y(), height));
            m_endPos.setY(std::fmod(m_endPos.y(), height));
        }
    }
}
}
