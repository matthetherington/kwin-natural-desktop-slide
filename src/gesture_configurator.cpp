// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#include "configurator.h"
#include "gesture_settings.h"
#include "color_picker.h"
#include <QPixmap>
#include <QIcon>
#include <QCheckBox>
#include <QComboBox>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QJsonDocument>
#include <QLabel>
#include <QMap>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>

namespace NaturalSwipe
{
namespace
{
bool reloadActiveSettings(const QString &path, QString &message)
{
    QDBusInterface effects("org.kde.KWin", "/Effects", "org.kde.kwin.Effects");
    const auto loaded = effects.property("loadedEffects").toStringList();
    const QRegularExpression renderer("^kwin_natural_slide(?:_preview[0-9]+)?$");
    const QRegularExpression controller("^kwin_natural_swipe_controller(?:_[a-zA-Z0-9]+)?$");
    QStringList renderers, controllers;
    for (const auto &name : loaded) {
        if (renderer.match(name).hasMatch()) renderers.append(name);
        if (controller.match(name).hasMatch()) controllers.append(name);
    }
    if (renderers.size() != 1 || controllers.size() > 1) {
        message = "Exactly one configurable project renderer must be active. Nothing enabled or reloaded; settings remain saved.";
        return false;
    }
    struct Component { QString id, path, interface, status; };
    QList<Component> components{{renderers.front(), "/org/kde/KWin/Effects/NaturalSwipe", "org.kde.KWin.NaturalSwipe", "configurationStatus"}};
    if (!controllers.isEmpty()) components.append({controllers.front(), "/org/kde/KWin/Effects/NaturalSwipeController", "org.kde.KWin.NaturalSwipeController", "controllerStatus"});
    // Preflight ALL components before reloading any. Old cached libraries may
    // not support gesture settings; never report success against those versions.
    for (const auto &component : components) {
        QDBusInterface diagnostic("org.kde.KWin", component.path, component.interface);
        QDBusReply<QString> reply = diagnostic.call(component.status);
        const auto object = reply.isValid() ? QJsonDocument::fromJson(reply.value().toUtf8()).object() : QJsonObject{};
        const auto currentPath = object["gesturePath"].toString();
        if (currentPath.isEmpty() || QFileInfo(currentPath).absoluteFilePath() != QFileInfo(path).absoluteFilePath()) {
            message = "The active component either lacks gesture-settings support or reads another file: " + component.id
                + ". Nothing reloaded. A matching new build is required; installation alone may not replace cached code.";
            return false;
        }
        // Older schema-1 builds reject the new property. Check every component
        // before reloading either, rather than partially applying a new file.
        if (!object["configuredSettings"].toObject().contains("overscrollColor")
            && !object["gestureSettings"].toObject().contains("overscrollColor")) {
            message = "The active component lacks configurable colour support: " + component.id
                + ". Nothing reloaded; install matching renderer/controller builds and start a fresh approved session.";
            return false;
        }
    }
    bool deferred = false;
    for (const auto &component : components) {
        const auto reply = effects.call("reconfigureEffect", component.id);
        if (reply.type() == QDBusMessage::ErrorMessage) { message = "Reload failed: " + reply.errorMessage(); return false; }
        QDBusInterface diagnostic("org.kde.KWin", component.path, component.interface);
        QDBusReply<QString> status = diagnostic.call(component.status);
        if (!status.isValid()) { message = "Reload requested, but verification failed: " + status.error().message(); return false; }
        const auto object = QJsonDocument::fromJson(status.value().toUtf8()).object();
        if (!object["gestureError"].toString().isEmpty()) { message = "Previous valid settings retained: " + object["gestureError"].toString(); return false; }
        deferred |= object["settingsDeferred"].toBool();
    }
    message = deferred ? "Reload queued until the current transition fully settles; new settings start with a new gesture."
        : "Saved gesture settings reloaded in the active project components.";
    if (controllers.isEmpty()) message += " The gesture controller is not active; this did not enable it.";
    return true;
}

class GestureConfigurator : public QWidget
{
public:
    explicit GestureConfigurator(QString path) : m_path(std::move(path))
    {
        auto outer = new QVBoxLayout(this);
        auto scroll = new QScrollArea;
        scroll->setObjectName("gestureScroll");
        scroll->setWidgetResizable(true);
        scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
        auto contents = new QWidget;
        auto layout = new QVBoxLayout(contents);
        layout->setSizeConstraint(QLayout::SetMinimumSize);
        layout->setAlignment(Qt::AlignTop);
        auto note = new QLabel("Three fingers initiate desktop control. These settings never enable an effect or change InputActions bindings. "
                               "Overscroll requires the matching preview renderer; gesture controls require the project controller.");
        note->setWordWrap(true);
        note->setTextFormat(Qt::PlainText);
        note->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        layout->addWidget(note);
        auto group = [layout](const QString &title) {
            auto box = new QGroupBox(title);
            auto form = new QFormLayout(box);
            form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
            layout->addWidget(box);
            return form;
        };
        auto swipe = group("Swipe recognition and completion");
        addNumber(swipe, "Slow-swipe completion", "slowSwipeThreshold", 5, 95, 1, " %",
                  "Release at or beyond this progress to switch. Below it, slow swipes return. Quick flicks use the speed rule instead.");
        addNumber(swipe, "Movement per desktop", "distancePerDesktop", 50, 1500, 0, " units",
                  "Lower values make the same finger movement travel farther. These are input units, not screen pixels.");
        addNumber(swipe, "Minimum flick speed", "flickVelocity", 0.1, 20, 2, " desktops/s",
                  "Speed required for short flicks to complete. This controls recognition, not animation speed.");
        auto overscroll = group("Edge overscroll");
        m_overscroll = new QCheckBox("Allow resisted movement past the first/last desktop");
        m_overscroll->setObjectName("overscrollEnabled");
        overscroll->addRow(m_overscroll);
        addNumber(overscroll, "Maximum overscroll", "overscrollLimit", 1, 50, 1, " %",
                  "Maximum fraction of the desktop revealed beyond an edge. Resistance approaches this limit gradually.");
        m_background = new QComboBox;
        m_background->setObjectName("overscrollBackground");
        m_background->addItem("Solid colour", "black");
        m_background->addItem("Adjacent desktop wallpaper", "wallpaper");
        overscroll->addRow("Exposed background", m_background);
        m_color = new QPushButton;
        m_color->setObjectName("overscrollColor");
        m_color->setToolTip("Choose an opaque background colour using RGB, hexadecimal or HSL controls.");
        overscroll->addRow("Solid background colour", m_color);
        connect(m_color, &QPushButton::clicked, this, [this]() {
            const auto color = selectOverscrollColor(QColor(m_colorValue), this);
            if (!color.isValid()) return;
            setColor(color.name(QColor::HexRgb));
            changed();
        });
        auto animations = group("Release animation speeds");
        addNumber(animations, "Normal switch speed", "settleSpeed", 0.1, 4, 2, " ×",
                  "Higher values settle faster. 1× preserves the tuned animation and KDE's global animation factor.");
        addNumber(animations, "Fast-flick switch speed", "flickSettleSpeed", 0.1, 4, 2, " ×",
                  "Independent speed for same-direction fast-flick releases. Does not change flick recognition or finger tracking.");
        addNumber(animations, "Abort / bounce-back speed", "returnSpeed", 0.1, 4, 2, " ×",
                  "Speed of partial-swipe return and edge bounce, independent of switch speed.");
        auto followups = group("Two-finger follow-ups");
        m_followups = new QCheckBox("Allow horizontal finger-scroll swipes until settling finishes");
        m_followups->setObjectName("followups");
        m_followups->setToolTip("Three fingers still initiate. During this window ordinary horizontal trackpad scrolling controls desktops; vertical scrolling passes through.");
        followups->addRow(m_followups);
        m_edgeGrace = new QSpinBox;
        m_edgeGrace->setObjectName("edgeGraceMs");
        m_edgeGrace->setRange(0, 1500);
        m_edgeGrace->setSingleStep(50);
        m_edgeGrace->setSuffix(" ms");
        m_edgeGrace->setToolTip("Entry window after a blocked edge swipe with no animation (for example with overscroll disabled). Zero disables this exception.");
        followups->addRow("No-animation edge window", m_edgeGrace);
        auto defaults = new QPushButton("Restore tuned gesture defaults (stage only)");
        defaults->setObjectName("restoreGestureDefaults");
        layout->addWidget(defaults);
        auto reloadSaved = new QPushButton("Reload saved gesture settings");
        reloadSaved->setObjectName("reloadSavedGestureSettings");
        layout->addWidget(reloadSaved);
        m_save = new QPushButton("Save gesture settings");
        m_save->setObjectName("saveGestureSettings");
        layout->addWidget(m_save);
        auto reload = new QPushButton("Reload settings in active renderer / controller");
        reload->setObjectName("reloadGestureEffects");
        layout->addWidget(reload);
        m_status = new QLabel;
        m_status->setObjectName("gestureStatus");
        m_status->setWordWrap(true);
        m_status->setTextFormat(Qt::PlainText);
        m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_status->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        layout->addWidget(m_status); // Always below controls: status growth cannot move buttons.
        scroll->setWidget(contents);
        outer->addWidget(scroll);
        connect(m_overscroll, &QCheckBox::toggled, this, [this]() { changed(); });
        connect(m_followups, &QCheckBox::toggled, this, [this]() { changed(); });
        connect(m_background, &QComboBox::currentIndexChanged, this, [this]() {
            m_color->setEnabled(m_background->currentData() == "black");
            changed();
        });
        connect(m_edgeGrace, &QSpinBox::valueChanged, this, [this]() { changed(); });
        connect(defaults, &QPushButton::clicked, this, [this]() {
            setControls({});
            m_dirty = true;
            m_status->setText("Tuned defaults staged. Save explicitly to replace saved gesture settings; panel rules are untouched.");
        });
        connect(reloadSaved, &QPushButton::clicked, this, [this]() {
            if (m_dirty && QMessageBox::question(this, "Discard gesture edits?", "Reload saved settings and discard staged edits?",
                QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) return;
            read();
        });
        connect(m_save, &QPushButton::clicked, this, [this]() { save(); });
        connect(reload, &QPushButton::clicked, this, [this]() {
            if (m_dirty) { m_status->setText("Save or discard staged gesture edits before reloading the active components."); return; }
            QString message;
            reloadActiveSettings(m_path, message);
            m_status->setText(message);
        });
        read();
    }
private:
    void setColor(const QString &color)
    {
        m_colorValue = color;
        m_color->setText(color);
        QPixmap swatch(24, 16);
        swatch.fill(QColor(color));
        m_color->setIcon(QIcon(swatch));
    }
    void addNumber(QFormLayout *form, const QString &label, const QString &name,
                   double min, double max, int decimals, const QString &suffix, const QString &tip)
    {
        auto spin = new QDoubleSpinBox;
        spin->setObjectName(name);
        spin->setRange(min, max);
        spin->setDecimals(decimals);
        spin->setSingleStep(name == "distancePerDesktop" ? 10 : suffix == " %" ? 1 : 0.1);
        spin->setSuffix(suffix);
        spin->setToolTip(tip);
        form->addRow(label, spin);
        m_numbers.insert(name, spin);
        connect(spin, &QDoubleSpinBox::valueChanged, this, [this]() { changed(); });
    }
    void changed()
    {
        if (m_loading) return;
        m_dirty = true;
        if (m_status) m_status->setText("Gesture changes staged; nothing saved or applied yet.");
    }
    void setControls(const GestureSettings &s)
    {
        m_loading = true;
        m_numbers["slowSwipeThreshold"]->setValue(s.slowSwipeThreshold * 100);
        m_numbers["distancePerDesktop"]->setValue(s.distancePerDesktop);
        m_numbers["flickVelocity"]->setValue(s.flickVelocity);
        m_numbers["overscrollLimit"]->setValue(s.overscrollLimit * 100);
        m_numbers["settleSpeed"]->setValue(s.settleSpeed);
        m_numbers["flickSettleSpeed"]->setValue(s.flickSettleSpeed);
        m_numbers["returnSpeed"]->setValue(s.returnSpeed);
        m_overscroll->setChecked(s.overscrollEnabled);
        m_background->setCurrentIndex(m_background->findData(s.overscrollBackground));
        setColor(s.overscrollColor);
        m_color->setEnabled(s.overscrollBackground == "black");
        m_followups->setChecked(s.followups);
        m_edgeGrace->setValue(s.edgeGraceMs);
        m_loading = false;
    }
    GestureSettings controls() const
    {
        GestureSettings s;
        s.slowSwipeThreshold = m_numbers["slowSwipeThreshold"]->value() / 100;
        s.distancePerDesktop = m_numbers["distancePerDesktop"]->value();
        s.flickVelocity = m_numbers["flickVelocity"]->value();
        s.overscrollLimit = m_numbers["overscrollLimit"]->value() / 100;
        s.settleSpeed = m_numbers["settleSpeed"]->value();
        s.flickSettleSpeed = m_numbers["flickSettleSpeed"]->value();
        s.returnSpeed = m_numbers["returnSpeed"]->value();
        s.overscrollEnabled = m_overscroll->isChecked();
        s.overscrollBackground = m_background->currentData().toString();
        s.overscrollColor = m_colorValue;
        s.followups = m_followups->isChecked();
        s.edgeGraceMs = m_edgeGrace->value();
        return s;
    }
    void read()
    {
        m_read = false;
        m_existed = QFileInfo::exists(m_path);
        QByteArray original;
        if (m_existed) {
            QFile file(m_path);
            if (!file.open(QIODevice::ReadOnly)) { m_save->setEnabled(false); m_status->setText(file.errorString()); return; }
            original = file.readAll();
            if (file.error() != QFileDevice::NoError) { m_save->setEnabled(false); m_status->setText(file.errorString()); return; }
        }
        GestureSettingsStore store;
        QString error;
        if (m_existed && !store.reload(original, &error)) {
            m_save->setEnabled(false);
            m_status->setText("Invalid saved settings; refusing to overwrite them: " + error);
            return;
        }
        m_original = original;
        setControls(store.settings());
        m_read = true;
        m_dirty = false;
        m_save->setEnabled(true);
        m_status->setText((m_existed ? "Loaded gesture settings: " : "Using tuned defaults; no file created: ") + m_path);
    }
    void save()
    {
        if (!m_read) return;
        QString error, backup;
        const auto settings = controls();
        if (!saveGestureSettings(m_path, m_existed, m_original, settings, &backup, &error)) {
            m_status->setText("Settings not saved: " + error);
            return;
        }
        m_original = QJsonDocument(gestureSettingsJson(settings)).toJson();
        m_existed = true;
        m_dirty = false;
        m_status->setText("Gesture settings saved to " + m_path + (backup.isEmpty() ? QString() : "\nBackup: " + backup)
            + "\nUse Reload to apply. Panel rules, enabled effects and InputActions are unchanged.");
    }
    QString m_path;
    QByteArray m_original;
    bool m_existed = false, m_read = false, m_dirty = false, m_loading = true;
    QMap<QString, QDoubleSpinBox *> m_numbers;
    QCheckBox *m_overscroll;
    QCheckBox *m_followups;
    QComboBox *m_background;
    QPushButton *m_color;
    QString m_colorValue;
    QSpinBox *m_edgeGrace;
    QPushButton *m_save;
    QLabel *m_status = nullptr;
};
}

QWidget *createGestureConfigurator(const QString &path)
{
    return new GestureConfigurator(path);
}
}
