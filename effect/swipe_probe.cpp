// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#include "swipe_observer.h"
#include "compat/runtime_version.h"
#include "effect/effect.h"
#include "effect/effecthandler.h"
#include "input.h"
#include <QDBusConnection>
#include <QDebug>
#include <QJsonDocument>
#include <memory>

namespace KWin
{

class SwipeProbe : public Effect
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KWin.NaturalDesktopSlideProbe")
public:
    SwipeProbe()
    {
        m_observer = std::make_unique<SwipeObserver>();
        input()->installInputEventSpy(m_observer.get());
        connect(effects, &EffectsHandler::screenLockingChanged, this, [this](bool locked) {
            if (locked && m_observer) m_observer->clear();
        });
        m_registered = QDBusConnection::sessionBus().registerObject(
            QString::fromLatin1(path), this, QDBusConnection::ExportScriptableSlots);
        if (!m_registered) qWarning() << "Natural Desktop Slide probe: could not register diagnostic object";
    }

    ~SwipeProbe() override
    {
        m_observer.reset();
        if (m_registered) QDBusConnection::sessionBus().unregisterObject(QString::fromLatin1(path));
    }

    // No painting/fullscreen-effect ownership; input observation is independent
    // of the effect render chain and works alongside either desktop animation.
    bool isActive() const override { return false; }
    static bool supported() { return NaturalSwipe::runtimeMatchesBuild() && input() != nullptr; }

public Q_SLOTS:
    Q_SCRIPTABLE QString gestureStatus() const
    {
        return QString::fromUtf8(QJsonDocument(m_observer->status()).toJson());
    }

private:
    static constexpr auto path = "/org/kde/KWin/Effects/NaturalDesktopSlideProbe";
    std::unique_ptr<SwipeObserver> m_observer;
    bool m_registered = false;
};

KWIN_EFFECT_FACTORY_SUPPORTED_ENABLED(SwipeProbe, "probe_metadata.json",
    return SwipeProbe::supported();, return false;)

}

#include "swipe_probe.moc"
