// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#include "swipe_session.h"
#include "settling_swipe.h"
#include "output_context.h"
#include "api_compat.h"
#include "compat/runtime_version.h"
#include "core/inputdevice.h"
#include "core/output.h"
#include "effect/effect.h"
#include "effect/effecthandler.h"
#include "input.h"
#include "input_event.h"
#include "virtualdesktops.h"
#include "workspace.h"
#include <QDBusConnection>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QPointer>
#include <QRegularExpression>
#include <QTimer>
#if NATURAL_DESKTOP_SLIDE_TEST_DRIVER
#include "core/outputconfiguration.h"
#endif

namespace KWin
{
using NaturalSwipe::SwipeFinish;
using NaturalSwipe::SwipeFrame;

namespace
{
bool isPreviewRenderer(const QString &name)
{
    static const QRegularExpression pattern(QStringLiteral("^kwin_natural_desktop_slide(?:_dev[a-zA-Z0-9]+)?$"));
    return pattern.match(name).hasMatch();
}

QStringList loadedRenderers()
{
    QStringList names;
    for (const auto &name : effects->loadedEffects()) {
        if (name == "slide" || isPreviewRenderer(name)) names.append(name);
    }
    return names;
}

QVector<NaturalSwipe::GestureOutput> outputLayout()
{
    QVector<NaturalSwipe::GestureOutput> result;
    for (auto output : effects->screens()) {
        const auto r = output->geometry();
        result.append({reinterpret_cast<quintptr>(output), QRectF(r.x(), r.y(), r.width(), r.height()),
                       output->scale(), int(output->transform().kind())});
    }
    return result;
}
}

class SwipeController;

// Composition avoids Effect::pointerAxis(void) conflicting with the input
// filter's consuming pointerAxis(bool) callback.
class SwipeControllerFilter : public InputEventFilter
{
public:
    explicit SwipeControllerFilter(SwipeController *controller);
    bool swipeGestureBegin(PointerSwipeGestureBeginEvent *event) override;
    bool swipeGestureUpdate(PointerSwipeGestureUpdateEvent *event) override;
    bool swipeGestureEnd(PointerSwipeGestureEndEvent *event) override;
    bool swipeGestureCancelled(PointerSwipeGestureCancelEvent *event) override;
    bool pointerAxis(PointerAxisEvent *event) override;

private:
    SwipeController *m_controller;
};

// Experimental input owner, deliberately separate from the animation plugin.
// KWin 6.8.2's VirtualDesktopManager notifications feed Workspace and then the
// existing EffectsHandler desktopChanging/Changed/Cancelled renderer callbacks.
class SwipeController : public Effect
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KWin.NaturalDesktopSlideController")
public:
    SwipeController() : m_filter(this)
    {
        input()->installInputEventFilter(&m_filter);
        m_watchdog.setInterval(100);
        connect(&m_watchdog, &QTimer::timeout, this, [this]() {
            if (owned() && (!contextValid() || (m_scroll.owned() && !m_scrollDevice))) interrupt();
        });
        m_edgeTimer.setSingleShot(true);
        m_edgeTimer.setTimerType(Qt::PreciseTimer);
        m_edgeTimer.setInterval(NaturalSwipe::SettlingSwipe::edgeGrace);
        connect(&m_edgeTimer, &QTimer::timeout, this, [this]() {
            if (m_scroll.edgeWindow() && !m_scroll.owned()) closeFollowups();
        });
        connect(effects, &EffectsHandler::screenLockingChanged, this, [this](bool locked) {
            if (locked) interrupt();
        });
        connect(effects, &EffectsHandler::activeFullScreenEffectChanged, this, [this]() {
            if (effects->activeFullScreenEffect() != m_settlingOwner) {
                closeFollowups();
                m_settlingOwner = nullptr;
            }
            if (owned() && m_previewOwner
                && effects->activeFullScreenEffect() != m_previewOwner) interrupt();
        });
        const auto manager = VirtualDesktopManager::self();
        connect(manager, &VirtualDesktopManager::currentChanged, this, [this](VirtualDesktop *, VirtualDesktop *, LogicalOutput *output) {
            if (!m_independent || m_targets.contains(output)) interrupt();
        });
        connect(manager, &VirtualDesktopManager::layoutChanged, this, [this]() { interrupt(); });
        connect(manager, &VirtualDesktopManager::desktopRemoved, this, [this]() { interrupt(); });
        connect(manager, &VirtualDesktopManager::navigationWrappingAroundChanged, this, [this]() { interrupt(); });
        connect(manager, &VirtualDesktopManager::perOutputVirtualDesktopsChanged, this, [this]() { interrupt(); });
        connect(effects, &EffectsHandler::screenRemoved, this, [this]() { interrupt(); });
        connect(effects, &EffectsHandler::screenAdded, this, [this]() { interrupt(); });
        reconfigure(ReconfigureAll);
        m_registered = QDBusConnection::sessionBus().registerObject(
            QString::fromLatin1(path), this, QDBusConnection::ExportScriptableSlots);
        if (!m_registered) qWarning() << "Natural Desktop Slide controller: could not register diagnostic object";
    }

    ~SwipeController() override
    {
        m_watchdog.stop();
        if (input()) input()->uninstallInputEventFilter(&m_filter);
        interrupt();
        if (m_registered) QDBusConnection::sessionBus().unregisterObject(QString::fromLatin1(path));
    }

    bool isActive() const override { return false; } // This plugin never paints.
    static bool supported() { return NaturalSwipe::runtimeMatchesBuild() && input() && VirtualDesktopManager::self(); }

    void reconfigure(ReconfigureFlags) override
    {
        if (owned() || effects->hasActiveFullScreenEffect()) {
            m_settingsDeferred = true;
            return;
        }
        if (!m_settings.loadFile(NaturalSwipe::gestureConfigPath(), &m_settingsError))
            qWarning().noquote() << "Natural Desktop Slide controller: retaining gesture settings:" << m_settingsError;
        m_settingsDeferred = false;
    }

    bool swipeGestureBegin(PointerSwipeGestureBeginEvent *event)
    {
        if (m_scroll.owned()) {
            // libinput changed gesture type: finish the old scroll stream before
            // admitting a fresh native swipe. Never keep both models owned.
            apply(m_scroll.cancel(event->time), false);
            m_scrollDevice = nullptr;
        }
        ++m_beginCount;
        m_lastFingers = event->fingerCount;
        m_lastBeginTime = event->time.count();
        m_lastBeginDuringSettling = m_settlingOwner
            && effects->activeFullScreenEffect() == m_settlingOwner;
        if (m_session.owned()) {
            m_lastAdmission = "drain-overlapping-begin";
            return apply(m_session.begin(event->fingerCount, event->time, false, false, false));
        }
        if (event->fingerCount != 3) {
            m_lastAdmission = "pass-through-finger-count";
            return false;
        }
        closeFollowups();
        m_previewOwner = nullptr;
        m_previewSent = false;
        const QString blocked = blockedReason();
        if (!blocked.isEmpty()) {
            m_lastError = blocked;
            // Foreign fullscreen effects must not hand a fresh stream to
            // KWin's opposite-direction native desktop recognizer. Our own
            // renderer's settling animation is allowed by blockedReason().
            if (blockedReason(false).isEmpty()) {
                m_lastAdmission = "drain-foreign-transition";
                return apply(m_session.begin(event->fingerCount, event->time, true, false, false, false));
            }
            m_lastAdmission = "pass-through-blocked";
            return false;
        }
        if (!prepareContext()) { m_lastAdmission = "pass-through-no-output-or-desktop"; return false; }
        m_lastStartDesktop = m_start->x11DesktopNumber();
        const auto frame = m_session.begin(event->fingerCount, event->time, true,
            m_left && m_left != m_start, m_right && m_right != m_start, true, m_overscroll, m_contextSettings);
        if (frame.consume) {
            m_lastAdmission = m_session.draining() ? "drain-no-neighbours" : "track";
            m_lastError.clear();
            m_lastInterruptReason.clear();
            m_watchdog.start();
        }
        return apply(frame);
    }

    bool swipeGestureUpdate(PointerSwipeGestureUpdateEvent *event)
    {
        ++m_updateCount;
        return apply(m_session.update(event->delta, event->time, contextValid()));
    }

    bool swipeGestureEnd(PointerSwipeGestureEndEvent *event)
    {
        ++m_endCount;
        return finishGesture(event->time, false);
    }

    bool swipeGestureCancelled(PointerSwipeGestureCancelEvent *event)
    {
        ++m_cancelCount;
        return finishGesture(event->time, true);
    }

    bool handlePointerAxis(PointerAxisEvent *event)
    {
        if (!settlingUndoEnabled || !event->device || !event->device->isTouchpad()
            || event->source != PointerAxisSource::Finger || NaturalSwipe::isAutoscroll(*event)
            || event->orientation != Qt::Horizontal) return false;
        const bool unmodified = event->buttons == Qt::NoButton && event->modifiers == Qt::NoModifier;
        if (!m_scroll.owned()) {
            if (m_session.owned() || event->delta == 0) return false;
            if (!m_scroll.windowOpen(event->timestamp)) {
                closeFollowups();
                return false;
            }
            if (!unmodified || !followupWindowValid()) {
                closeFollowups();
                return false;
            }
            if (!prepareContext(true)) {
                closeFollowups();
                return false;
            }
            m_previewOwner = nullptr;
            m_previewSent = false;
            m_scrollDevice = event->device;
            m_lastScrollStartDesktop = m_start->x11DesktopNumber();
        } else if (event->device != m_scrollDevice) {
            return false;
        }
        const bool wasOwned = m_scroll.owned();
        const bool wasDraining = m_scroll.draining();
        const bool valid = unmodified && contextValid();
        const auto frame = m_scroll.axis(event->delta, event->inverted, true, true,
            event->timestamp, valid, m_left && m_left != m_start, m_right && m_right != m_start, m_overscroll, m_contextSettings);
        if (frame.consume) ++m_scrollAxisCount;
        if (!wasOwned && m_scroll.owned()) {
            m_edgeTimer.stop();
            ++m_scrollBeginCount;
            m_watchdog.start();
        }
        const bool ended = wasOwned && !m_scroll.owned();
        if (ended) {
            ++m_scrollEndCount;
            m_lastScrollFinish = frame.finish == SwipeFinish::Left ? "left"
                : frame.finish == SwipeFinish::Right ? "right"
                : frame.finish == SwipeFinish::Return ? "return" : "none";
            m_lastScrollProgress = m_scroll.tracker().progress();
            m_lastScrollVelocity = m_scroll.tracker().velocity();
            m_scrollDevice = nullptr;
        }
        const bool consumed = apply(frame, valid, m_scroll.tracker().flickRelease());
        // Below-acquisition streams may have no finish frame. Preserve the
        // prior settling window only if it genuinely still exists.
        if (ended && frame.finish == SwipeFinish::None && valid && !wasDraining) reopenFollowups();
        return consumed;
    }

public Q_SLOTS:
#if NATURAL_DESKTOP_SLIDE_TEST_DRIVER
    // Deliberately compiled out of normal builds. The installer refuses builds
    // with these hooks, and the separate-bus test harness must opt in at runtime.
    Q_SCRIPTABLE bool testConfigure(bool independent)
    {
        if (qEnvironmentVariable("NDS_ISOLATED_TEST") != "1") return false;
        const auto manager = VirtualDesktopManager::self();
        manager->setCount(3);
        manager->setRows(1);
        manager->setPerOutputVirtualDesktops(independent);
        for (auto output : effects->screens()) manager->setCurrent(1, output);
        return true;
    }
    Q_SCRIPTABLE bool testGesture(int outputIndex, int phase, double dx, qlonglong timeUs)
    {
        if (qEnvironmentVariable("NDS_ISOLATED_TEST") != "1" || timeUs < 0 || !std::isfinite(dx)
            || outputIndex < 0 || outputIndex >= effects->screens().size()) return false;
        const auto time = std::chrono::microseconds(timeUs);
        if (phase == 0) {
            workspace()->setActiveOutput(effects->screens()[outputIndex]);
            PointerSwipeGestureBeginEvent event{3, time};
            return swipeGestureBegin(&event);
        }
        if (phase == 1) {
            PointerSwipeGestureUpdateEvent event{{dx, 0}, time};
            return swipeGestureUpdate(&event);
        }
        if (phase == 2) return finishGesture(time, false);
        if (phase == 3) return finishGesture(time, true);
        return false;
    }
    Q_SCRIPTABLE QString testOutputs() const
    {
        if (qEnvironmentVariable("NDS_ISOLATED_TEST") != "1") return "[]";
        QJsonArray result;
        for (auto output : effects->screens()) {
            const auto r = output->geometry();
            result.append(QJsonObject{{"name", output->name()}, {"desktop", int(VirtualDesktopManager::self()->current(output))},
                {"geometry", QJsonArray{r.x(), r.y(), r.width(), r.height()}}, {"scale", output->scale()}});
        }
        return QString::fromUtf8(QJsonDocument(result).toJson());
    }
    Q_SCRIPTABLE bool testOutputLayout(int index, int x, int y, double scale)
    {
        if (qEnvironmentVariable("NDS_ISOLATED_TEST") != "1" || index < 0
            || index >= effects->screens().size() || scale < 0.5 || scale > 4) return false;
        OutputConfiguration config;
        auto change = config.changeSet(effects->screens()[index]->backendOutput());
        change->pos = QPoint(x, y);
        change->scale = scale;
        change->scaleSetting = scale;
        return NaturalSwipe::outputConfigurationSucceeded(workspace()->applyOutputConfiguration(config));
    }
#endif
    Q_SCRIPTABLE QString controllerStatus() const
    {
        const auto &tracker = m_session.tracker();
        return QString::fromUtf8(QJsonDocument(QJsonObject{
            {"mode", "experimental-control"}, {"owned", owned()},
            {"draining", m_session.draining() || m_scroll.draining()}, {"blockedReason", blockedReason()},
            {"lastError", m_lastError}, {"state", NaturalSwipe::swipeStateName(tracker.state())},
            {"progress", tracker.progress()}, {"velocity", tracker.velocity()},
            {"beginCount", m_beginCount}, {"updateCount", m_updateCount},
            {"endCount", m_endCount}, {"cancelCount", m_cancelCount},
            {"lastFingers", m_lastFingers}, {"lastAdmission", m_lastAdmission},
            {"lastBeginTimeUs", m_lastBeginTime}, {"lastEndTimeUs", m_lastEndTime},
            {"lastBeginDuringSettling", m_lastBeginDuringSettling},
            {"lastStartDesktop", m_lastStartDesktop}, {"lastTargetDesktop", m_lastTargetDesktop},
            {"lastFinish", m_lastFinish}, {"lastEndReason", m_lastEndReason},
            {"lastReleaseProgress", m_lastReleaseProgress},
            {"lastReleaseVelocity", m_lastReleaseVelocity},
            {"lastInterruptReason", m_lastInterruptReason},
            {"settlingUndoEnabled", settlingUndoEnabled},
            {"followupWindow", m_scroll.windowOpen()}, {"twoFingerOwned", m_scroll.owned()},
            {"edgeWindow", m_scroll.edgeWindow()}, {"edgeWindowCount", m_edgeWindowCount},
            {"twoFingerProgress", m_scroll.tracker().progress()},
            {"twoFingerVelocity", m_scroll.tracker().velocity()},
            {"twoFingerState", NaturalSwipe::swipeStateName(m_scroll.tracker().state())},
            {"scrollBeginCount", m_scrollBeginCount}, {"scrollEndCount", m_scrollEndCount},
            {"scrollAxisCount", m_scrollAxisCount}, {"lastScrollFinish", m_lastScrollFinish},
            {"lastScrollStartDesktop", m_lastScrollStartDesktop},
            {"lastScrollProgress", m_lastScrollProgress}, {"lastScrollVelocity", m_lastScrollVelocity},
            {"renderer", m_renderer}, {"overscroll", m_overscroll},
            {"outputCount", effects->screens().size()}, {"independentDesktops", m_independent},
            {"targetOutputCount", m_targets.size()}, {"output", m_output ? m_output->name() : QString()},
            {"gesturePath", NaturalSwipe::gestureConfigPath()}, {"gestureError", m_settingsError},
            {"settingsDeferred", m_settingsDeferred},
            {"configuredSettings", NaturalSwipe::gestureSettingsJson(m_settings.settings())},
            {"contextSettings", NaturalSwipe::gestureSettingsJson(m_contextSettings)}}).toJson());
    }

private:
    bool owned() const { return m_session.owned() || m_scroll.owned(); }

    void closeFollowups()
    {
        m_edgeTimer.stop();
        m_scroll.closeWindow();
        m_followupExpected = nullptr;
    }

    bool followupWindowValid() const
    {
        const bool rendererSettling = m_settlingOwner && effects->activeFullScreenEffect() == m_settlingOwner;
        const bool edgeGrace = m_scroll.edgeWindow() && m_edgeTimer.isActive() && !effects->hasActiveFullScreenEffect();
        return !owned() && blockedReason().isEmpty()
            && (rendererSettling || edgeGrace)
            && m_output && effects->screens().contains(m_output)
            && outputContextValid(m_followupExpected)
            && m_output->geometry() == m_geometry && effects->isEffectLoaded(m_renderer)
            && m_followupExpected && VirtualDesktopManager::self()->currentDesktop(m_output) == m_followupExpected;
    }

    void reopenFollowups()
    {
        if (!settlingUndoEnabled || !m_contextSettings.followups || owned() || !m_output || !m_settlingOwner
            || effects->activeFullScreenEffect() != m_settlingOwner || !blockedReason().isEmpty()) return;
        m_followupExpected = VirtualDesktopManager::self()->currentDesktop(m_output);
        if (m_followupExpected) {
            m_edgeTimer.stop();
            m_scroll.openWindow();
        }
    }

    void openEdgeFollowups(NaturalSwipe::SwipeTracker::Time released)
    {
        // Reuse a real settling animation if one is still active. Otherwise a
        // blocked boundary has no animation to hold the normal window open.
        reopenFollowups();
        if (m_scroll.windowOpen() || !settlingUndoEnabled || !m_contextSettings.followups
            || m_contextSettings.edgeGraceMs == 0 || owned()
            || effects->hasActiveFullScreenEffect() || !contextValid()) return;
        m_followupExpected = m_start;
        const auto grace = std::chrono::milliseconds(m_contextSettings.edgeGraceMs);
        m_scroll.openEdgeWindow(released, grace);
        m_edgeTimer.setInterval(grace);
        m_edgeTimer.start();
        ++m_edgeWindowCount;
    }

    bool prepareContext(bool followup = false)
    {
        if (m_settingsDeferred) reconfigure(ReconfigureAll);
        m_contextSettings = m_settings.settings();
        const auto manager = VirtualDesktopManager::self();
        m_output = followup ? m_output.data() : effects->activeScreen();
        if (!m_output) return false;
        m_start = manager->currentDesktop(m_output);
        if (!m_start) return false;
        m_wrap = manager->isNavigationWrappingAround();
        m_geometry = m_output->geometry();
        m_independent = manager->isPerOutputVirtualDesktops();
        m_outputLayout = outputLayout();
        const auto targets = NaturalSwipe::gestureOutputs(m_outputLayout, reinterpret_cast<quintptr>(m_output.data()), m_independent);
        m_targets.clear();
        for (auto output : effects->screens()) {
            if (targets.contains(reinterpret_cast<quintptr>(output))) m_targets.append(output);
        }
        if (m_targets.isEmpty()) return false;
        m_left = manager->toLeft(m_start, m_wrap);
        m_right = manager->toRight(m_start, m_wrap);
        const auto renderers = loadedRenderers();
        if (renderers.size() != 1) return false;
        m_renderer = renderers.front();
        m_overscroll = isPreviewRenderer(m_renderer) && m_contextSettings.overscrollEnabled;
        return true;
    }

    bool finishGesture(NaturalSwipe::SwipeTracker::Time time, bool cancelled)
    {
        m_lastEndTime = time.count();
        const bool valid = contextValid();
        m_lastEndReason = !m_session.owned() ? "not-owned"
            : cancelled ? "input-cancelled"
            : m_session.draining() ? "draining"
            : !valid ? "invalid-context" : "released";
        const auto frame = m_session.end(time, cancelled, valid);
        m_lastReleaseProgress = m_session.tracker().progress();
        m_lastReleaseVelocity = m_session.tracker().velocity();
        if (frame.finish == SwipeFinish::None) m_lastFinish = "none";
        const bool consumed = apply(frame, valid, m_session.tracker().flickRelease());
        if (valid && !cancelled && frame.consume && m_session.boundaryRelease()) openEdgeFollowups(time);
        return consumed;
    }

    QString blockedReason(bool includeFullScreen = true) const
    {
        if (effects->isScreenLocked()) return "Screen locked";
        if (effects->isEffectLoaded("kwin_gestures")) return "InputActions is loaded; controller will not claim gestures";
        if (effects->screens().isEmpty()) return "No displays available";
        if (loadedRenderers().size() != 1) return "Load exactly one supported Slide renderer";
        if (effects->screens().size() > 1 && VirtualDesktopManager::self()->isPerOutputVirtualDesktops()) {
            auto renderer = effects->findEffect(loadedRenderers().front());
            if (!renderer || renderer->metaObject()->indexOfMethod("cancelGestureForOutput(QString)") < 0)
                return "Independent-display gestures require an updated Natural Desktop Slide renderer";
        }
        if (input()->isSelectingWindow()) return "Window selection is active";
        if (workspace()->moveResizeWindow()) return "Window move/resize is active";
        if (includeFullScreen && effects->hasActiveFullScreenEffect()
            && effects->activeFullScreenEffect() != m_settlingOwner
            && (!owned() || effects->activeFullScreenEffect() != m_previewOwner)) {
            return "Another transition or fullscreen effect is active";
        }
        return {};
    }

    bool contextValid() const
    {
        if (!blockedReason().isEmpty() || !m_output || !m_start) return false;
        const auto manager = VirtualDesktopManager::self();
        return effects->screens().contains(m_output)
            && outputContextValid(m_start)
            && m_output->geometry() == m_geometry
            && manager->currentDesktop(m_output) == m_start
            && manager->isNavigationWrappingAround() == m_wrap
            && manager->toLeft(m_start, m_wrap) == m_left
            && manager->toRight(m_start, m_wrap) == m_right
            && effects->isEffectLoaded(m_renderer);
    }

    bool outputContextValid(const VirtualDesktop *expected) const
    {
        const auto manager = VirtualDesktopManager::self();
        if (manager->isPerOutputVirtualDesktops() != m_independent
            || !NaturalSwipe::sameOutputLayout(m_outputLayout, outputLayout())) return false;
        for (const auto &output : m_targets) {
            if (!output || !effects->screens().contains(output)
                || manager->currentDesktop(output) != expected) return false;
        }
        return !m_targets.isEmpty();
    }

    void interrupt()
    {
        closeFollowups();
        if (owned() && !m_session.draining() && !m_scroll.draining()) {
            m_lastInterruptReason = blockedReason();
            if (m_lastInterruptReason.isEmpty()) m_lastInterruptReason = "desktop/output/layout changed or renderer lost";
        }
        apply(m_session.interrupt(), false);
        apply(m_scroll.interrupt(), false);
        m_watchdog.stop();
    }

    bool apply(const SwipeFrame &frame, bool allowFollowups = true, bool flick = false)
    {
        const auto manager = VirtualDesktopManager::self();
        if (frame.preview && m_start && m_output) {
            m_previewSent = true;
            // Internal KWin bridge, reviewed for 6.7.0–6.7.6 and 6.8.0–6.8.2.
            // We own offsets ourselves;
            // do not invoke native gestureReleasedX or mutate its private state.
            for (const auto &output : m_targets) {
                if (output) Q_EMIT manager->currentChanging(m_start, QPointF(frame.offset, 0), output);
            }
            if (!m_previewOwner) m_previewOwner = effects->activeFullScreenEffect();
            if (!m_previewOwner) {
                m_lastError = "Renderer did not acquire the preview transition";
                interrupt();
            }
        }
        if (frame.finish != SwipeFinish::None) {
            if (m_previewOwner && m_previewOwner->metaObject()->indexOfMethod("setGestureRelease(bool)") >= 0) {
                QMetaObject::invokeMethod(m_previewOwner, "setGestureRelease", Qt::DirectConnection, Q_ARG(bool, flick));
            }
            m_lastFinish = frame.finish == SwipeFinish::Left ? "left"
                : frame.finish == SwipeFinish::Right ? "right" : "return";
            QPointer<VirtualDesktop> target;
            if (frame.finish == SwipeFinish::Left) target = m_left;
            if (frame.finish == SwipeFinish::Right) target = m_right;
            const bool commit = target && target != m_start && m_output;
            m_lastTargetDesktop = commit ? target->x11DesktopNumber() : (m_start ? m_start->x11DesktopNumber() : 0);
            const bool switched = commit && manager->setCurrent(target, m_output);
            if (!switched) {
                if (m_previewSent) {
                    if (m_independent && m_previewOwner && m_output) {
                        QMetaObject::invokeMethod(m_previewOwner, "cancelGestureForOutput", Qt::DirectConnection,
                                                  Q_ARG(QString, m_output->name()));
                    } else {
                        Q_EMIT manager->currentChangingCancelled();
                    }
                }
            }
            if (m_previewOwner && m_previewOwner->metaObject()->indexOfMethod("setGestureRelease(bool)") >= 0)
                QMetaObject::invokeMethod(m_previewOwner, "setGestureRelease", Qt::DirectConnection, Q_ARG(bool, false));
            // setCurrent commits the logical desktop immediately, before the
            // renderer settles. A fresh gesture can therefore start relative
            // to that committed desktop and reach its next neighbour. The
            // renderer separately inherits the visual spring position; that
            // inherited displacement must not affect our release policy. Retain
            // only the exact renderer which painted our preview, never grant
            // takeover permission merely because some Slide effect is loaded.
            if (m_previewSent && m_previewOwner
                && effects->activeFullScreenEffect() == m_previewOwner) {
                m_settlingOwner = m_previewOwner;
            }
            m_previewSent = false;
            m_previewOwner = nullptr;
            if (allowFollowups) reopenFollowups();
        }
        if (!owned() || m_session.draining() || m_scroll.draining()) m_watchdog.stop();
        return frame.consume;
    }

    static constexpr auto path = "/org/kde/KWin/Effects/NaturalDesktopSlideController";
    static constexpr bool settlingUndoEnabled = NATURAL_DESKTOP_SLIDE_FOLLOWUPS;
    SwipeControllerFilter m_filter;
    NaturalSwipe::SwipeSession m_session;
    NaturalSwipe::SettlingSwipe m_scroll;
    QTimer m_watchdog;
    QTimer m_edgeTimer;
    qint64 m_edgeWindowCount = 0;
    QPointer<LogicalOutput> m_output;
    QList<QPointer<LogicalOutput>> m_targets;
    QVector<NaturalSwipe::GestureOutput> m_outputLayout;
    bool m_independent = false;
    QPointer<VirtualDesktop> m_start, m_left, m_right;
    QPointer<Effect> m_previewOwner;
    QPointer<Effect> m_settlingOwner;
    QPointer<VirtualDesktop> m_followupExpected;
    QPointer<InputDevice> m_scrollDevice;
    qint64 m_scrollBeginCount = 0, m_scrollEndCount = 0, m_scrollAxisCount = 0;
    int m_lastScrollStartDesktop = 0;
    QString m_lastScrollFinish;
    double m_lastScrollProgress = 0, m_lastScrollVelocity = 0;
    Rect m_geometry;
    QString m_renderer;
    QString m_lastError;
    // Latest-state diagnostics only: no per-event log, raw contacts or device IDs.
    qint64 m_beginCount = 0, m_updateCount = 0, m_endCount = 0, m_cancelCount = 0;
    qint64 m_lastBeginTime = 0, m_lastEndTime = 0;
    int m_lastFingers = 0, m_lastStartDesktop = 0, m_lastTargetDesktop = 0;
    QString m_lastAdmission, m_lastFinish, m_lastEndReason, m_lastInterruptReason;
    double m_lastReleaseProgress = 0, m_lastReleaseVelocity = 0;
    bool m_lastBeginDuringSettling = false;
    bool m_wrap = false;
    bool m_previewSent = false;
    bool m_registered = false;
    bool m_overscroll = false;
    NaturalSwipe::GestureSettingsStore m_settings;
    NaturalSwipe::GestureSettings m_contextSettings;
    QString m_settingsError;
    bool m_settingsDeferred = false;
};

SwipeControllerFilter::SwipeControllerFilter(SwipeController *controller)
    : InputEventFilter(InputFilterOrder::TabBox), m_controller(controller)
{
}

bool SwipeControllerFilter::swipeGestureBegin(PointerSwipeGestureBeginEvent *event)
{
    return m_controller->swipeGestureBegin(event);
}

bool SwipeControllerFilter::swipeGestureUpdate(PointerSwipeGestureUpdateEvent *event)
{
    return m_controller->swipeGestureUpdate(event);
}

bool SwipeControllerFilter::swipeGestureEnd(PointerSwipeGestureEndEvent *event)
{
    return m_controller->swipeGestureEnd(event);
}

bool SwipeControllerFilter::swipeGestureCancelled(PointerSwipeGestureCancelEvent *event)
{
    return m_controller->swipeGestureCancelled(event);
}

bool SwipeControllerFilter::pointerAxis(PointerAxisEvent *event)
{
    return m_controller->handlePointerAxis(event);
}

KWIN_EFFECT_FACTORY_SUPPORTED_ENABLED(SwipeController, "natural_desktop_slide_controller_metadata.json",
    return SwipeController::supported();, return false;)

}

#include "swipe_controller.moc"
