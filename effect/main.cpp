/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2026 Matt Hetherington
    SPDX-License-Identifier: GPL-2.0-or-later
    Adapted from KWin v6.8.2 Slide: unique, opt-in renderer with overscroll.
*/
#include "natural_desktop_slide.h"
namespace KWin
{
KWIN_EFFECT_FACTORY_SUPPORTED_ENABLED(NaturalSlideEffect, "natural_desktop_slide_metadata.json",
    return NaturalSlideEffect::supported();, return false;)
}
#include "main.moc"
