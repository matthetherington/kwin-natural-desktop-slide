// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#include "output_context.h"
#include <QTest>
#include <limits>
using namespace NaturalSwipe;

class OutputContextTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void sharedAndIndependentTargets()
    {
        const QVector<GestureOutput> outputs{{1, {0, 0, 1920, 1080}, 1, 0},
            {2, {1920, -200, 1280, 720}, 1.5, 0}, {3, {-1080, 0, 1080, 1920}, 2, 1}};
        QCOMPARE(gestureOutputs(outputs, 2, false), QVector<quintptr>({1, 2, 3}));
        QCOMPARE(gestureOutputs(outputs, 2, true), QVector<quintptr>({2}));
        QVERIFY(gestureOutputs(outputs, 4, false).isEmpty());
        QVERIFY(gestureOutputs({}, 1, false).isEmpty());
    }
    void topologyScaleAndTransformInvalidateContext()
    {
        const QVector<GestureOutput> before{{1, {0, 0, 1920, 1080}, 1, 0}, {2, {1920, 0, 1280, 720}, 1.5, 0}};
        auto after = before;
        std::reverse(after.begin(), after.end());
        QVERIFY(sameOutputLayout(before, after));
        for (int mutation = 0; mutation < 5; ++mutation) {
            after = before;
            if (mutation == 0) after.removeLast();
            if (mutation == 1) after[0].identity = 3;
            if (mutation == 2) after[0].geometry.translate(-100, 0);
            if (mutation == 3) after[0].scale = 2;
            if (mutation == 4) after[0].transform = 1;
            QVERIFY(!sameOutputLayout(before, after));
        }
    }
    void invalidLayoutsFailClosed()
    {
        QVERIFY(!validOutputLayout({}));
        QVERIFY(!validOutputLayout({{0, {0, 0, 100, 100}, 1, 0}}));
        QVERIFY(!validOutputLayout({{1, {}, 1, 0}}));
        QVERIFY(!validOutputLayout({{1, {0, 0, 100, 100}, 0, 0}}));
        QVERIFY(!validOutputLayout({{1, {0, 0, 100, 100}, std::numeric_limits<double>::infinity(), 0}}));
        QVERIFY(!validOutputLayout({{1, {0, 0, 100, 100}, 1, 0}, {1, {100, 0, 100, 100}, 1, 0}}));
    }
};
QTEST_GUILESS_MAIN(OutputContextTest)
#include "output_context_test.moc"
