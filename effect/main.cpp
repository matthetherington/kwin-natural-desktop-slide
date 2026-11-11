/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2026 Matt Hetherington
    SPDX-License-Identifier: GPL-2.0-or-later
    Adapted from KWin v6.8.2 Slide on 2026-11-11: unique, opt-in plugin.
*/
#include "natural_slide.h"
namespace KWin
{
#ifdef NATURAL_SWIPE_OVERSCROLL_PREVIEW
KWIN_EFFECT_FACTORY_SUPPORTED_ENABLED(NaturalSlideEffect, "overscroll_metadata.json",
    return NaturalSlideEffect::supported();, return false;)
#else
KWIN_EFFECT_FACTORY_SUPPORTED_ENABLED(NaturalSlideEffect, "natural_slide_metadata.json",
    return NaturalSlideEffect::supported();, return false;)
#endif
}
#include "main.moc"
