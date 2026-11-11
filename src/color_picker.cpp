// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#include "color_picker.h"
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace NaturalSwipe
{
QColor selectOverscrollColor(const QColor &initial, QWidget *parent)
{
    QDialog dialog(parent);
    dialog.setObjectName("overscrollColorDialog");
    dialog.setWindowTitle("Select Overscroll Background Colour");
    auto layout = new QVBoxLayout(&dialog);
    // Qt's full picker supplies the spectrum, RGB/HSV and hexadecimal controls.
    // Embed it so the additional HSL controls are available on every platform.
    auto picker = new QColorDialog(initial, &dialog);
    picker->setOptions(QColorDialog::DontUseNativeDialog | QColorDialog::NoButtons);
    picker->setWindowFlags(Qt::Widget);
    layout->addWidget(picker);
    auto hsl = new QFormLayout;
    layout->addLayout(hsl);
    auto component = [hsl](const QString &label, const QString &name, double maximum, const QString &suffix) {
        auto spin = new QDoubleSpinBox;
        spin->setObjectName(name);
        spin->setRange(0, maximum);
        spin->setDecimals(1);
        spin->setSuffix(suffix);
        hsl->addRow(label, spin);
        return spin;
    };
    auto hue = component("HSL hue", "hslHue", 359.9, " °");
    auto saturation = component("HSL saturation", "hslSaturation", 100, " %");
    auto lightness = component("HSL lightness", "hslLightness", 100, " %");
    auto sync = [=](const QColor &color) {
        const QSignalBlocker h(hue), s(saturation), l(lightness);
        // Preserve the user's hue for achromatic colours.
        if (color.hslHueF() >= 0) hue->setValue(color.hslHueF() * 360);
        saturation->setValue(color.hslSaturationF() * 100);
        lightness->setValue(color.lightnessF() * 100);
    };
    auto update = [=]() {
        picker->setCurrentColor(QColor::fromHslF(hue->value() / 360,
            saturation->value() / 100, lightness->value() / 100));
    };
    QObject::connect(picker, &QColorDialog::currentColorChanged, &dialog, sync);
    for (auto spin : {hue, saturation, lightness})
        QObject::connect(spin, &QDoubleSpinBox::valueChanged, &dialog, update);
    sync(initial);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    return dialog.exec() == QDialog::Accepted ? picker->currentColor().toRgb() : QColor{};
}
}
