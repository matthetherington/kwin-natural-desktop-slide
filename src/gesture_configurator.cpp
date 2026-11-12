// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#include "configurator.h"
#include "gesture_settings.h"
#include "color_picker.h"
#include "settings_feedback.h"
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
QLabel *helpText(const QString &text, const QString &name = {})
{
    auto label = new QLabel(text);
    label->setObjectName(name);
    label->setWordWrap(true);
    label->setTextFormat(Qt::PlainText);
    // Preserve height-for-width after setting the size policy. Otherwise forms
    // allocate a single-line height even when the text wraps at larger fonts.
    QSizePolicy policy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    policy.setHeightForWidth(true);
    label->setSizePolicy(policy);
    return label;
}

bool reloadActiveSettings(const QString &path, QString &message, QString &details)
{
    QDBusInterface effects("org.kde.KWin", "/Effects", "org.kde.kwin.Effects");
    if (!effects.isValid()) {
        message = "Could not contact the desktop effect. Settings were not applied. See Technical details.";
        details = effects.lastError().message();
        return false;
    }
    const auto loaded = effects.property("loadedEffects").toStringList();
    const QRegularExpression renderer("^kwin_natural_desktop_slide(?:_dev[a-zA-Z0-9]+)?$");
    const QRegularExpression controller("^kwin_natural_desktop_slide_controller(?:_dev[a-zA-Z0-9]+)?$");
    QStringList renderers, controllers;
    for (const auto &name : loaded) {
        if (renderer.match(name).hasMatch()) renderers.append(name);
        if (controller.match(name).hasMatch()) controllers.append(name);
    }
    if (renderers.size() != 1 || controllers.size() > 1) {
        message = renderers.isEmpty()
            ? "Natural Desktop Slide is not running. Your saved settings will be used next time it starts. Nothing was turned on."
            : "More than one desktop effect or gesture helper is running. Settings were not applied. See Technical details.";
        details = "Renderers: " + renderers.join(", ") + "\nGesture helpers: " + controllers.join(", ");
        return false;
    }
    struct Component { QString id, path, interface, status; };
    QList<Component> components{{renderers.front(), "/org/kde/KWin/Effects/NaturalDesktopSlide", "org.kde.KWin.NaturalDesktopSlide", "configurationStatus"}};
    if (!controllers.isEmpty()) components.append({controllers.front(), "/org/kde/KWin/Effects/NaturalDesktopSlideController", "org.kde.KWin.NaturalDesktopSlideController", "controllerStatus"});
    // Preflight ALL components before reloading any. Old cached libraries may
    // not support gesture settings; never report success against those versions.
    for (const auto &component : components) {
        QDBusInterface diagnostic("org.kde.KWin", component.path, component.interface);
        QDBusReply<QString> reply = diagnostic.call(component.status);
        details += component.id + ":\n" + (reply.isValid() ? reply.value() : reply.error().message()) + "\n";
        const auto object = reply.isValid() ? QJsonDocument::fromJson(reply.value().toUtf8()).object() : QJsonObject{};
        const auto currentPath = object["gesturePath"].toString();
        if (currentPath.isEmpty() || QFileInfo(currentPath).absoluteFilePath() != QFileInfo(path).absoluteFilePath()) {
            message = "The running effect uses a different settings file or an incompatible version. "
                "Settings were not applied. See Technical details.";
            details += "Editor settings file: " + path;
            return false;
        }
        // Older schema-1 builds reject the new property. Check every component
        // before reloading either, rather than partially applying a new file.
        if (!object["configuredSettings"].toObject().contains("overscrollColor")
            && !object["gestureSettings"].toObject().contains("overscrollColor")) {
            message = "The running effect needs a matching update to support these settings. "
                "Settings were not applied. See Technical details.";
            return false;
        }
    }
    bool deferred = false;
    for (const auto &component : components) {
        const auto reply = effects.call("reconfigureEffect", component.id);
        if (reply.type() == QDBusMessage::ErrorMessage) {
            message = "Could not finish applying settings. Some components may already have updated. See Technical details.";
            details += reply.errorMessage();
            return false;
        }
        QDBusInterface diagnostic("org.kde.KWin", component.path, component.interface);
        QDBusReply<QString> status = diagnostic.call(component.status);
        if (!status.isValid()) {
            message = "Settings were sent, but could not be verified. See Technical details.";
            details += status.error().message();
            return false;
        }
        details += "After applying — " + component.id + ":\n" + status.value() + "\n";
        const auto feedback = summarizeSettingsReload(status.value(), true);
        if (!feedback.success) {
            message = "Could not verify that all gesture and animation settings were applied. Some components may already have updated. See Technical details.";
            return false;
        }
        deferred |= feedback.deferred;
    }
    message = deferred ? "Settings received. They will take effect after the current desktop animation finishes."
        : "Gesture and animation settings applied.";
    if (controllers.isEmpty()) message += " The gesture helper is not running, so input changes will take effect when it starts. Nothing was turned on.";
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
        auto note = helpText("Start a desktop swipe with three fingers. The defaults are a good starting point. "
                               "After making changes, Save gesture settings, then Apply saved settings. "
                               "This page does not turn the effect on or change your gesture bindings.");
        layout->addWidget(note);
        auto group = [layout](const QString &title) {
            auto box = new QGroupBox(title);
            QSizePolicy policy(QSizePolicy::Expanding, QSizePolicy::Minimum);
            policy.setHeightForWidth(true);
            box->setSizePolicy(policy);
            auto form = new QFormLayout(box);
            form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
            form->setRowWrapPolicy(QFormLayout::DontWrapRows);
            layout->addWidget(box);
            return form;
        };
        auto swipe = group("Swipe sensitivity");
        addNumber(swipe, "Slow-swipe completion", "slowSwipeThreshold", 5, 95, 1, " %",
                  "Release at or beyond this progress to switch. Below it, slow swipes return. Quick flicks use the speed rule instead.");
        addNumber(swipe, "Movement per desktop", "distancePerDesktop", 50, 1500, 0, " units",
                  "Lower values make the same finger movement travel farther. These are input units, not screen pixels.");
        addNumber(swipe, "Minimum flick speed", "flickVelocity", 0.1, 20, 2, " desktops/s",
                  "Speed required for short flicks to complete. This controls recognition, not animation speed.");
        auto overscroll = group("Overflow settings");
        overscroll->parentWidget()->setObjectName("overflowSettings");
        m_overscroll = new QCheckBox("Allow movement past desktop edges");
        m_overscroll->setObjectName("overscrollEnabled");
        overscroll->addRow(m_overscroll);
        overscroll->addRow(helpText("Allow a little resisted movement when swiping past the first or last desktop.", "overflowHelp"));
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
        overscroll->addRow("Background colour", m_color);
        connect(m_color, &QPushButton::clicked, this, [this]() {
            const auto color = selectOverscrollColor(QColor(m_colorValue), this);
            if (!color.isValid()) return;
            setColor(color.name(QColor::HexRgb));
            changed();
        });
        auto animations = group("Release animation speed");
        addNumber(animations, "Normal switch speed", "settleSpeed", 0.1, 4, 2, " ×",
                  "Higher values finish faster. 1× uses the recommended speed and respects KDE's global animation speed.");
        addNumber(animations, "Fast-flick switch speed", "flickSettleSpeed", 0.1, 4, 2, " ×",
                  "Independent speed for same-direction fast-flick releases. Does not change flick recognition or finger tracking.");
        addNumber(animations, "Abort / bounce-back speed", "returnSpeed", 0.1, 4, 2, " ×",
                  "Speed of partial-swipe return and edge bounce, independent of switch speed.");
        auto followups = group("Two-finger follow-ups");
        m_followups = new QCheckBox("Allow two-finger follow-ups");
        m_followups->setObjectName("followups");
        m_followups->setToolTip("Three fingers still initiate. During this window ordinary horizontal trackpad scrolling controls desktops; vertical scrolling passes through.");
        followups->addRow(m_followups);
        followups->addRow(helpText("After starting with three fingers, continue switching with two fingers while the animation finishes.", "followupsHelp"));
        m_edgeGrace = new QSpinBox;
        m_edgeGrace->setObjectName("edgeGraceMs");
        m_edgeGrace->setRange(0, 1500);
        m_edgeGrace->setSingleStep(50);
        m_edgeGrace->setSuffix(" ms");
        m_edgeGrace->setToolTip("Entry window after a blocked edge swipe with no animation (for example with overscroll disabled). Zero disables this exception.");
        followups->addRow("Edge follow-up window", m_edgeGrace);
        auto edgeHelp = helpText("When a swipe at the first or last desktop produces no animation, allow a two-finger follow-up for this long. Set to 0 to turn this exception off.", "edgeGraceHelp");
        followups->addRow(edgeHelp);
        auto defaults = new QPushButton("Restore defaults");
        defaults->setToolTip("Reset the controls to their recommended defaults. Nothing is saved or applied until you choose Save and Apply.");
        defaults->setObjectName("restoreGestureDefaults");
        layout->addWidget(defaults);
        auto reloadSaved = new QPushButton("Discard edits and reload saved settings");
        reloadSaved->setObjectName("reloadSavedGestureSettings");
        layout->addWidget(reloadSaved);
        m_save = new QPushButton("Save gesture settings");
        m_save->setObjectName("saveGestureSettings");
        layout->addWidget(m_save);
        auto reload = new QPushButton("Apply saved settings");
        reload->setToolTip("Reload saved settings in the running effect and gesture helper. This does not turn them on.");
        reload->setObjectName("reloadGestureEffects");
        layout->addWidget(reload);
        m_status = new SettingsFeedback("gestureStatus");
        layout->addWidget(m_status); // Always below controls: status growth cannot move buttons.
        scroll->setWidget(contents);
        outer->addWidget(scroll);
        connect(m_overscroll, &QCheckBox::toggled, this, [this]() { updateAvailability(); changed(); });
        connect(m_followups, &QCheckBox::toggled, this, [this]() { updateAvailability(); changed(); });
        connect(m_background, &QComboBox::currentIndexChanged, this, [this]() {
            updateAvailability();
            changed();
        });
        connect(m_edgeGrace, &QSpinBox::valueChanged, this, [this]() { changed(); });
        connect(defaults, &QPushButton::clicked, this, [this]() {
            setControls({});
            m_dirty = true;
            m_status->setText("Defaults restored in this editor. Save gesture settings, then Apply saved settings to use them. Panel choices are unchanged.");
        });
        connect(reloadSaved, &QPushButton::clicked, this, [this]() {
            if (m_dirty && QMessageBox::question(this, "Discard gesture edits?", "Reload saved settings and discard your unsaved changes?",
                QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) return;
            read();
        });
        connect(m_save, &QPushButton::clicked, this, [this]() { save(); });
        connect(reload, &QPushButton::clicked, this, [this]() {
            if (m_dirty) { m_status->setText("Save your changes before applying them, or discard your edits to use the saved settings."); return; }
            QString message, details;
            reloadActiveSettings(m_path, message, details);
            m_status->setText(message, details);
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
        spin->setAccessibleName(label);
        spin->setAccessibleDescription(tip);
        spin->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        auto caption = helpText(label, name + "Label");
        caption->setBuddy(spin);
        form->addRow(caption, spin);
        // A spanning row uses the available section width, not the spin box's
        // compact size hint. Keep help outside a nested fixed-height field.
        form->addRow(helpText(tip, name + "Help"));
        m_numbers.insert(name, spin);
        connect(spin, &QDoubleSpinBox::valueChanged, this, [this]() { changed(); });
    }
    void changed()
    {
        if (m_loading) return;
        m_dirty = true;
        if (m_status) m_status->setText("Unsaved changes. Save gesture settings, then Apply saved settings.");
    }
    void updateAvailability()
    {
        const bool overscroll = m_overscroll->isChecked();
        m_numbers["overscrollLimit"]->setEnabled(overscroll);
        m_background->setEnabled(overscroll);
        m_color->setEnabled(overscroll && m_background->currentData() == "black");
        m_edgeGrace->setEnabled(m_followups->isChecked());
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
        updateAvailability();
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
            if (!file.open(QIODevice::ReadOnly)) {
                m_save->setEnabled(false);
                m_status->setText("Could not read your saved settings. They have not been changed. See Technical details.", file.errorString());
                return;
            }
            original = file.readAll();
            if (file.error() != QFileDevice::NoError) {
                m_save->setEnabled(false);
                m_status->setText("Could not read your saved settings. They have not been changed. See Technical details.", file.errorString());
                return;
            }
        }
        GestureSettingsStore store;
        QString error;
        if (m_existed && !store.reload(original, &error)) {
            m_save->setEnabled(false);
            m_status->setText("The saved settings could not be read. Saving is disabled to protect them. See Technical details.", error);
            return;
        }
        m_original = original;
        setControls(store.settings());
        m_read = true;
        m_dirty = false;
        m_save->setEnabled(true);
        m_status->setText(m_existed ? "Saved settings loaded. Changes are not saved or applied automatically."
                                   : "Using recommended defaults. No settings file has been created.", "Settings file: " + m_path);
    }
    void save()
    {
        if (!m_read) return;
        QString error, backup;
        const auto settings = controls();
        if (!saveGestureSettings(m_path, m_existed, m_original, settings, &backup, &error)) {
            m_status->setText("Could not save settings. Your changes are still in this editor. See Technical details.", error);
            return;
        }
        m_original = QJsonDocument(gestureSettingsJson(settings)).toJson();
        m_existed = true;
        m_dirty = false;
        m_status->setText("Gesture settings saved. Choose Apply saved settings to use them in the running effect.",
                         "Settings file: " + m_path + (backup.isEmpty() ? QString() : "\nBackup: " + backup));
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
    SettingsFeedback *m_status = nullptr;
};
}

QWidget *createGestureConfigurator(const QString &path)
{
    return new GestureConfigurator(path);
}
}
