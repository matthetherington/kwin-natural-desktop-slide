// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#include "panel_picker.h"
#include "config.h"
#include "configurator.h"
#include "settings_feedback.h"
#include <QGuiApplication>
#include <QAbstractItemView>
#include <QHeaderView>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QLabel>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QPushButton>
#include <QPointer>
#include <QScreen>
#include <QScrollArea>
#include <QTableWidget>
#include <QTabWidget>
#include <QRegularExpression>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

using namespace NaturalSwipe;

class PanelConfigurator : public QWidget
{
public:
    explicit PanelConfigurator(QString path) : m_path(std::move(path))
    {
        setWindowTitle("Natural Desktop Slide — Panels and gestures");
        auto outer = new QVBoxLayout(this);
        auto tabs = new QTabWidget;
        tabs->setObjectName("settingsTabs");
        outer->addWidget(tabs);
        auto scroll = new QScrollArea;
        scroll->setObjectName("pickerScroll");
        scroll->setWidgetResizable(true);
        // Reserve scrollbar width even before overflow so messages never change
        // the wrapping/height of the controls above them.
        scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
        auto contents = new QWidget;
        auto layout = new QVBoxLayout(contents);
        layout->setSizeConstraint(QLayout::SetMinimumSize);
        layout->setAlignment(Qt::AlignTop);
        const auto text = [](const QString &value) {
            auto label = new QLabel(value);
            label->setTextFormat(Qt::PlainText);
            label->setWordWrap(true);
            label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
            return label;
        };
        layout->addWidget(text("Choose which panels move when you switch desktops. Panels stay fixed unless you select Slide. "
                               "Their contents stay the same on every desktop."));
        layout->addWidget(text("1. Add a panel by clicking it (Esc cancels).\n"
                               "2. Tick Slide to move it, or untick it to keep it fixed.\n"
                               "3. Save panel choices, then Apply saved settings if the effect is running."));
        m_pick = new QPushButton("Add panel by clicking…");
        layout->addWidget(m_pick);
        m_table = new QTableWidget(0, 2);
        m_table->setObjectName("panelChoices");
        m_table->setAccessibleName("Panels selected for editing");
        m_table->setHorizontalHeaderLabels({"Slide", "Panel"});
        m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        m_table->verticalHeader()->hide();
        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setSelectionMode(QAbstractItemView::SingleSelection);
        m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        // A steady list height prevents new status text from stealing space
        // from the table and moving the buttons. Extra rows scroll internally.
        m_table->setFixedHeight(qMax(180, fontMetrics().lineSpacing() * 8
            + m_table->horizontalHeader()->sizeHint().height()));
        layout->addWidget(m_table);
        auto remove = new QPushButton("Remove from this list");
        remove->setObjectName("removePanelChoice");
        remove->setEnabled(false);
        remove->setToolTip("Does not delete existing saved rules; untick Slide and Save to keep a panel fixed.");
        layout->addWidget(remove);
        auto refresh = new QPushButton("Discard edits and reload saved choices");
        refresh->setObjectName("reloadSavedPanelChoices");
        layout->addWidget(refresh);
        layout->addWidget(text("Your saved choices are kept even when this list is empty. Add a panel again to view or change its choice. "
                              "To stop a panel sliding, untick Slide and save; removing it from this list does not change its saved choice. "
                              "Pick panels again after moving or resizing them, or changing display scaling."));
        m_save = new QPushButton("Save panel choices");
        m_save->setObjectName("savePanelChoices");
        m_save->setEnabled(false);
        layout->addWidget(m_save);
        auto reload = new QPushButton("Apply saved settings");
        reload->setObjectName("reloadPanelEffect");
        reload->setToolTip("Reload saved settings in the running effect. This does not turn the effect on.");
        layout->addWidget(reload);
        m_status = new SettingsFeedback("pickerStatus");
        m_status->setText("Ready. Add a panel to view or change its saved choice. This page does not turn the effect on.", "Settings file: " + m_path);
        layout->addWidget(m_status);
        scroll->setWidget(contents);
        tabs->addTab(scroll, "Panels");
        tabs->addTab(NaturalSwipe::createGestureConfigurator(QFileInfo(m_path).dir().filePath("gestures.json")), "Gestures and animation");
        connect(m_pick, &QPushButton::clicked, this, [this]() { pick(); });
        connect(m_save, &QPushButton::clicked, this, [this]() { save(); });
        connect(reload, &QPushButton::clicked, this, [this]() { reloadEffect(); });
        connect(m_table, &QTableWidget::itemSelectionChanged, this, [this, remove]() {
            remove->setEnabled(m_table->currentRow() >= 0);
        });
        connect(m_table, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *item) {
            if (item->column() != 0) return;
            m_dirty = true;
            m_status->setText("Unsaved panel choices. Save panel choices, then Apply saved settings.");
        });
        connect(remove, &QPushButton::clicked, this, [this]() {
            const int row = m_table->currentRow();
            if (row < 0) return;
            m_panels.removeAt(row);
            m_table->removeRow(row);
            m_save->setEnabled(!m_panels.isEmpty());
            m_dirty = !m_panels.isEmpty();
            m_status->setText("Row removed from this edit list only. Existing saved rules are unchanged.");
        });
        connect(refresh, &QPushButton::clicked, this, [this]() {
            if (!m_panels.isEmpty() && QMessageBox::question(this, "Discard panel edits?",
                "Clear the edit list and read the latest saved rules? Unsaved checkbox changes will be lost.",
                QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) return;
            QString error;
            if (!readConfig(&error)) { m_status->setText(error); return; }
            m_configRead = true;
            m_panels.clear();
            m_table->setRowCount(0);
            m_save->setEnabled(false);
            m_dirty = false;
            m_status->setText("Latest saved rules loaded. Add panels again to edit their choices.");
        });
        connect(m_table->horizontalHeader(), &QHeaderView::sectionResized, this,
            [this]() { m_table->resizeRowsToContents(); });
        resize(720, 620);
    }

private:
    bool readConfig(QString *error)
    {
        const bool existed = QFileInfo::exists(m_path);
        QByteArray original;
        if (existed) {
            QFile file(m_path);
            if (!file.open(QIODevice::ReadOnly)) { *error = file.errorString(); return false; }
            original = file.readAll();
            if (file.error() != QFileDevice::NoError) { *error = file.errorString(); return false; }
        } else {
            original = R"({"schemaVersion":1,"defaultAction":"fixed","rules":[]})";
        }
        ConfigStore validator;
        if (!validator.reload(original, error)) return false;
        m_original = original;
        m_existed = existed;
        return true;
    }

    void pick()
    {
        m_save->setEnabled(false);
        m_pick->setEnabled(false);
        // Hide/restore the hosting settings dialog when embedded in a KCM,
        // rather than just hiding this widget and leaving a blank dialog.
        QPointer<QWidget> host = window();
        host->hide();
        // Allow the window to disappear before KWin takes its selection grab.
        QTimer::singleShot(150, this, [this, host]() {
            QDBusInterface kwin("org.kde.KWin", "/KWin", "org.kde.KWin");
            kwin.setTimeout(120000);
            auto watcher = new QDBusPendingCallWatcher(kwin.asyncCall("queryWindowInfo"), this);
            connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, host]() {
                QDBusPendingReply<QVariantMap> reply = *watcher;
                watcher->deleteLater();
                if (host) {
                    host->show();
                    host->raise();
                    host->activateWindow();
                }
                m_pick->setEnabled(true);
                m_save->setEnabled(!m_panels.isEmpty());
                if (reply.isError()) {
                    m_status->setText(reply.error().name().endsWith("UserCancel")
                        ? "Panel picking cancelled; nothing changed."
                        : "Picking failed: " + reply.error().message());
                    return;
                }
                QVector<OutputInfo> outputs;
                for (auto screen : QGuiApplication::screens()) outputs.append({screen->name(), screen->geometry()});
                QString error;
                PanelInfo panel;
                if (!pickedPanel(reply.value(), outputs, panel, &error)
                    || (!m_configRead && !readConfig(&error))) {
                    m_status->setText(error);
                    return;
                }
                m_configRead = true;
                for (int i = 0; i < m_panels.size(); ++i) {
                    if (m_panels[i].output == panel.output && m_panels[i].geometry == panel.geometry) {
                        m_table->selectRow(i);
                        m_status->setText("This panel is already in the list. Its current choice has been kept.");
                        return;
                    }
                }
                ConfigStore config;
                config.reload(m_original);
                const int row = m_table->rowCount();
                m_table->insertRow(row);
                auto choice = new QTableWidgetItem;
                choice->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
                choice->setCheckState(selectPanel(panel, config.rules()).action == PanelAction::Slide ? Qt::Checked : Qt::Unchecked);
                m_table->setItem(row, 0, choice);
                const bool horizontal = panel.geometry.width() >= panel.geometry.height();
                const QString edge = horizontal
                    ? (panel.geometry.center().y() < panel.outputGeometry.center().y() ? "Top" : "Bottom")
                    : (panel.geometry.center().x() < panel.outputGeometry.center().x() ? "Left" : "Right");
                auto description = new QTableWidgetItem(edge + " panel on " + panel.output);
                description->setToolTip(describePanel(panel));
                m_table->setItem(row, 1, description);
                m_table->resizeRowsToContents();
                m_table->selectRow(row);
                m_panels.append(panel);
                m_save->setEnabled(true);
                m_dirty = true;
                m_status->setText("Panel added. Choose whether it should slide, then Save panel choices. Nothing saved yet.");
            });
        });
    }

    void save()
    {
        QByteArray updated;
        QString error, backup;
        QVector<PanelChoice> choices;
        for (int i = 0; i < m_panels.size(); ++i) {
            choices.append({m_panels[i], m_table->item(i, 0)->checkState() == Qt::Checked ? PanelAction::Slide : PanelAction::Fixed});
        }
        if (!panelOverrides(m_original, choices, updated, &error)
            || !savePanelConfig(m_path, m_existed, m_original, updated, &backup, &error)) {
            m_status->setText("Could not save panel choices. Your changes are still in this editor. See Technical details.", error);
            return;
        }
        m_original = updated;
        m_existed = true;
        m_dirty = false;
        m_status->setText("Panel choices saved. Choose Apply saved settings to use them in the running effect.",
                          "Settings file: " + m_path + (backup.isEmpty() ? QString() : "\nBackup: " + backup));
    }

    void reloadEffect()
    {
        if (m_dirty) {
            m_status->setText("Save your panel choices before applying them, or discard your edits to use the saved choices.");
            return;
        }
        QDBusInterface effects("org.kde.KWin", "/Effects", "org.kde.kwin.Effects");
        if (!effects.isValid()) {
            m_status->setText("Could not contact the desktop effect. Settings were not applied. See Technical details.", effects.lastError().message());
            return;
        }
        QStringList renderers;
        const QRegularExpression pattern("^kwin_natural_desktop_slide(?:_dev[a-zA-Z0-9]+)?$");
        for (const auto &name : effects.property("loadedEffects").toStringList()) {
            if (pattern.match(name).hasMatch()) renderers.append(name);
        }
        if (renderers.size() != 1) {
            m_status->setText(renderers.isEmpty()
                ? "Natural Desktop Slide is not running. Your saved choices will be used next time it starts. Nothing was turned on."
                : "More than one Natural Desktop Slide effect is running. Settings were not applied. See Technical details.",
                renderers.join('\n'));
            return;
        }
        QDBusInterface diagnostic("org.kde.KWin", "/org/kde/KWin/Effects/NaturalDesktopSlide", "org.kde.KWin.NaturalDesktopSlide");
        QDBusReply<QString> current = diagnostic.call("configurationStatus");
        if (!current.isValid()) {
            m_status->setText("Could not check the running effect. Settings were not applied. See Technical details.", current.error().message());
            return;
        }
        const auto path = QJsonDocument::fromJson(current.value().toUtf8()).object()["path"].toString();
        if (path.isEmpty() || QFileInfo(path).absoluteFilePath() != QFileInfo(m_path).absoluteFilePath()) {
            m_status->setText("This editor and the running effect use different settings files, or the effect could not identify its file. "
                              "Settings were not applied. See Technical details.", "Editor: " + m_path + "\nEffect: " + path + "\n" + current.value());
            return;
        }
        auto reply = effects.call("reconfigureEffect", renderers.front());
        if (reply.type() == QDBusMessage::ErrorMessage) {
            m_status->setText("Could not apply settings. See Technical details.", reply.errorMessage());
            return;
        }
        QDBusReply<QString> status = diagnostic.call("configurationStatus");
        if (!status.isValid()) {
            m_status->setText("Settings were sent, but could not be verified. See Technical details.", status.error().message());
            return;
        }
        m_status->setText(summarizeSettingsReload(status.value(), false).message, status.value());
    }

    QString m_path;
    QByteArray m_original;
    bool m_existed = false;
    bool m_configRead = false;
    bool m_dirty = false;
    QVector<PanelInfo> m_panels;
    QPushButton *m_pick;
    QPushButton *m_save;
    QTableWidget *m_table;
    SettingsFeedback *m_status;
};

QWidget *NaturalSwipe::createPanelConfigurator(const QString &path)
{
    return new PanelConfigurator(path);
}
