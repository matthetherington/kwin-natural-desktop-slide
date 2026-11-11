// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#include "panel_picker.h"
#include "config.h"
#include "configurator.h"
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
        layout->addWidget(text("Click Add panel, then click a panel. Repeat to build your list. "
                               "Tick Slide for each panel that should move; leave it unticked to stay fixed. "
                               "Esc cancels picking. The effect is never enabled automatically."));
        m_pick = new QPushButton("Add panel by clicking…");
        layout->addWidget(m_pick);
        m_table = new QTableWidget(0, 2);
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
        auto remove = new QPushButton("Remove selected row from edit list");
        remove->setToolTip("Does not delete existing saved rules; untick Slide and Save to keep a panel fixed.");
        layout->addWidget(remove);
        auto refresh = new QPushButton("Reload saved rules (discard staged choices)");
        layout->addWidget(refresh);
        layout->addWidget(text("Selection uses output and geometry, not a permanent panel ID. "
                              "Re-pick after moving/resizing a panel or changing display scaling. "
                              "Other saved rules are preserved. Removing a row only removes it from this edit list."));
        m_save = new QPushButton("Save all panel choices");
        m_save->setObjectName("savePanelChoices");
        m_save->setEnabled(false);
        layout->addWidget(m_save);
        auto reload = new QPushButton("Reload configuration in active effect");
        layout->addWidget(reload);
        m_status = text("Configuration: " + m_path);
        m_status->setObjectName("pickerStatus");
        m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
        layout->addWidget(m_status);
        scroll->setWidget(contents);
        tabs->addTab(scroll, "Panels");
        tabs->addTab(NaturalSwipe::createGestureConfigurator(QFileInfo(m_path).dir().filePath("gestures.json")), "Gestures and animation");
        connect(m_pick, &QPushButton::clicked, this, [this]() { pick(); });
        connect(m_save, &QPushButton::clicked, this, [this]() { save(); });
        connect(reload, &QPushButton::clicked, this, [this]() { reloadEffect(); });
        connect(remove, &QPushButton::clicked, this, [this]() {
            const int row = m_table->currentRow();
            if (row < 0) return;
            m_panels.removeAt(row);
            m_table->removeRow(row);
            m_save->setEnabled(!m_panels.isEmpty());
            m_status->setText("Row removed from this edit list only. Existing saved rules are unchanged.");
        });
        connect(refresh, &QPushButton::clicked, this, [this]() {
            if (!m_panels.isEmpty() && QMessageBox::question(this, "Discard staged choices?",
                "Clear the edit list and read the latest saved rules? Unsaved checkbox changes will be lost.",
                QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) return;
            QString error;
            if (!readConfig(&error)) { m_status->setText(error); return; }
            m_configRead = true;
            m_panels.clear();
            m_table->setRowCount(0);
            m_save->setEnabled(false);
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
                        m_status->setText("This panel is already in the list. Its staged choice has been preserved.");
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
                auto description = new QTableWidgetItem(describePanel(panel));
                description->setToolTip(description->text());
                m_table->setItem(row, 1, description);
                m_table->resizeRowsToContents();
                m_table->selectRow(row);
                m_panels.append(panel);
                m_save->setEnabled(true);
                m_status->setText("Panel added. Add more panels or change their checkboxes, then Save all. Nothing saved yet.");
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
            QMessageBox::warning(this, "Configuration not saved", error);
            return;
        }
        m_original = updated;
        m_existed = true;
        m_status->setText("Saved to " + m_path + (backup.isEmpty() ? QString() : "\nPrevious file: " + backup)
                          + "\nUse Reload if the custom effect is active; stock Slide is unchanged.");
    }

    void reloadEffect()
    {
        QDBusInterface effects("org.kde.KWin", "/Effects", "org.kde.kwin.Effects");
        QStringList renderers;
        const QRegularExpression pattern("^kwin_natural_desktop_slide(?:_dev[a-zA-Z0-9]+)?$");
        for (const auto &name : effects.property("loadedEffects").toStringList()) {
            if (pattern.match(name).hasMatch()) renderers.append(name);
        }
        if (renderers.size() != 1) {
            m_status->setText("The custom effect is not active. Saved choices will apply when you enable it. "
                              "Stock Slide has not been changed.");
            return;
        }
        QDBusInterface diagnostic("org.kde.KWin", "/org/kde/KWin/Effects/NaturalDesktopSlide", "org.kde.KWin.NaturalDesktopSlide");
        QDBusReply<QString> current = diagnostic.call("configurationStatus");
        if (!current.isValid()) {
            m_status->setText("Cannot verify the active effect's configuration path: " + current.error().message());
            return;
        }
        const auto path = QJsonDocument::fromJson(current.value().toUtf8()).object()["path"].toString();
        if (path.isEmpty() || QFileInfo(path).absoluteFilePath() != QFileInfo(m_path).absoluteFilePath()) {
            m_status->setText("This editor is using a different configuration file from the active effect. "
                              "The effect reads: " + path + ". Nothing reloaded.");
            return;
        }
        auto reply = effects.call("reconfigureEffect", renderers.front());
        if (reply.type() == QDBusMessage::ErrorMessage) {
            m_status->setText("Reload failed: " + reply.errorMessage());
            return;
        }
        QDBusReply<QString> status = diagnostic.call("configurationStatus");
        m_status->setText(status.isValid() ? status.value() : "Reload requested; status unavailable: " + status.error().message());
    }

    QString m_path;
    QByteArray m_original;
    bool m_existed = false;
    bool m_configRead = false;
    QVector<PanelInfo> m_panels;
    QPushButton *m_pick;
    QPushButton *m_save;
    QTableWidget *m_table;
    QLabel *m_status;
};

QWidget *NaturalSwipe::createPanelConfigurator(const QString &path)
{
    return new PanelConfigurator(path);
}
