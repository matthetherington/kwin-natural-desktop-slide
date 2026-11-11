// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#include "configurator.h"
#include "config.h"
#include <KCModule>
#include <KPluginFactory>
#include <QVBoxLayout>

class PanelSlideKCM : public KCModule
{
    Q_OBJECT
public:
    PanelSlideKCM(QObject *parent, const KPluginMetaData &metadata)
        : KCModule(qobject_cast<QWidget *>(parent), metadata)
    {
        // KPluginFactory supplies an QObject parent because KCModule itself
        // is not a QWidget; the KCModule constructor accepts a widget parent.
        if (parent && !parent->isWidgetType()) setParent(parent);
        // The picker has its own explicit Save all/Reload buttons. Do not offer
        // an additional Apply button which would imply a second save mechanism.
        setButtons(NoAdditionalButton);
        auto layout = new QVBoxLayout(widget());
        layout->setContentsMargins(0, 0, 0, 0);
        auto picker = NaturalSwipe::createPanelConfigurator(NaturalSwipe::panelConfigPath());
        layout->addWidget(picker);
    }
};

K_PLUGIN_CLASS_WITH_JSON(PanelSlideKCM, "metadata.json")
#include "panel_slide_kcm.moc"
