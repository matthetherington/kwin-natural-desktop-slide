/*
    SPDX-FileCopyrightText: 2007 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2008 Lucas Murray <lmurray@undefinedfire.com>
    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2026 Matt Hetherington
    SPDX-License-Identifier: GPL-2.0-or-later

    Adapted from KWin v6.8.2 Slide on 2026-11-11. See docs/UPSTREAM.md.
*/
#pragma once
#include "config.h"
#include "gesture_settings.h"
#include "animation_speed.h"
#include "gesture_handover.h"
#include "effect/effect.h"
#include "effect/effectwindow.h"
#include "effect/timeline.h"
#include "spring_motion.h"
#include "api_compat.h"
#include <map>
#include <memory>

namespace KWin
{
using NaturalSlidePaintResult = NaturalSwipe::MemberResult<decltype(&Effect::paintWindow)>;
class NaturalSlideEffect;
class NaturalSlideScreen
{
public:
    NaturalSlideScreen(NaturalSlideEffect *parent, LogicalOutput *screen);
    ~NaturalSlideScreen();
    void prePaintScreen(ScreenPrePaintData &data);
    bool paintWindow(const RenderTarget &, const RenderViewport &, EffectWindow *, int,
                     const Region &, WindowPaintData &);
    void postPaintScreen();
    bool isActive() const;
    QJsonObject status() const;
    void desktopChanged(VirtualDesktop *old, VirtualDesktop *current, EffectWindow *with);
    void desktopChanging(VirtualDesktop *old, QPointF offset, EffectWindow *with);
    void desktopChangingCancelled();
    void windowAdded(EffectWindow *window);
    void windowDeleted(EffectWindow *window);
    void finishedSwitching();
private:
    bool isTranslated(const EffectWindow *) const;
    bool willBePainted(const EffectWindow *) const;
    bool slidesPanel(const EffectWindow *) const;
    void prepareSwitching();
    void startAnimation(QPointF oldPosition, VirtualDesktop *current, EffectWindow *moving = nullptr, bool returning = false);
    QPointF animationPosition() const;
    QPointF constrainGesturePosition(QPointF position) const;
    QPointF forcePositivePosition(QPointF position) const;
    QPointF constrainToDrawableRange(QPointF position) const;
    QPointF moveInsideDesktopGrid(QPointF position) const;
    void optimizePath();
    enum class State { Inactive, ActiveAnimation, ActiveGesture };
    State m_state = State::Inactive;
    NaturalSwipe::SpringMotion m_motionX;
    NaturalSwipe::SpringMotion m_motionY;
    QPointF m_startPos;
    QPointF m_endPos;
    QPointF m_gesturePos;
    QPointF m_position;
    NaturalSwipe::GestureHandover m_handover;
    quint64 m_windowPaints = 0;
    QList<VirtualDesktop *> m_visibleDesktops;
    EffectWindow *m_movingWindow = nullptr;
    AnimationClock m_clock;
    NaturalSwipe::AnimationSpeed m_animationSpeed;
    NaturalSwipe::GestureSettings m_settings;
    struct WindowData {
        EffectWindowVisibleRef visibilityRef;
        bool slidePanel = false;
    };
    QHash<const EffectWindow *, WindowData> m_windows;
    QList<EffectWindow *> m_elevated;
    NaturalSlideEffect *m_parent;
    LogicalOutput *m_screen;
};

class NaturalSlideEffect : public Effect
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KWin.NaturalDesktopSlide")
public:
    NaturalSlideEffect();
    ~NaturalSlideEffect() override;
    static bool supported();
    void reconfigure(ReconfigureFlags) override;
    bool isActive() const override;
    int requestedEffectChainPosition() const override { return 50; }
    void prePaintScreen(ScreenPrePaintData &) override;
    void postPaintScreen() override;
    NaturalSlidePaintResult paintScreen(const RenderTarget &, const RenderViewport &, int, const Region &, LogicalOutput *) override;
    void prePaintWindow(RenderView *, EffectWindow *, WindowPrePaintData &) override;
    NaturalSlidePaintResult paintWindow(const RenderTarget &, const RenderViewport &, EffectWindow *, int, const Region &, WindowPaintData &) override;
    bool shouldSlidePanel(const EffectWindow *window) const;
    bool overscrollEnabled() const;
    bool overscrollSupported() const;
    QString overscrollMode() const;
    bool wallpaperOverscroll() const { return overscrollEnabled() && overscrollMode() == QStringLiteral("wallpaper"); }
    const NaturalSwipe::GestureSettings &gestureSettings() const { return m_gestureSettings.settings(); }
    double settlingSpeed(bool returning) const;
public Q_SLOTS:
    Q_SCRIPTABLE QString panelSnapshot() const;
    Q_SCRIPTABLE QString configurationStatus() const;
    Q_SCRIPTABLE bool setOverscrollMode(const QString &mode);
    void cancelGestureForOutput(const QString &outputName);
    void setGestureRelease(bool flick) { m_releaseFlick = flick; }
private:
    void finishedSwitching();
    void loadGestureSettings();
    void desktopChanged(VirtualDesktop *, VirtualDesktop *, EffectWindow *, LogicalOutput *);
    void desktopChanging(VirtualDesktop *, QPointF, EffectWindow *, LogicalOutput *);
    NaturalSlideScreen *screenState(LogicalOutput *);
    NaturalSlideScreen &ensureScreen(LogicalOutput *);
    bool m_switchingActivity = false;
    bool m_dbusRegistered = false;
    NaturalSwipe::ConfigStore m_config;
    QString m_configPath;
    QString m_configError;
    QString m_overscrollMode; // Nonempty only for a temporary comparison override.
    NaturalSwipe::GestureSettingsStore m_gestureSettings;
    QString m_gesturePath;
    QString m_gestureError;
    bool m_gestureReloadPending = false;
    bool m_releaseFlick = false;
    LogicalOutput *m_paintOutput = nullptr; // Scoped to the current paintScreen call.
    std::map<LogicalOutput *, std::unique_ptr<NaturalSlideScreen>> m_screens;
};
}
