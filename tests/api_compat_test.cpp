// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#include "api_compat.h"
#include <QTest>
#include <optional>
using namespace NaturalSwipe;

namespace
{
struct LegacyEffect { void paint() {} };
struct ModernEffect { bool paint() { return false; } };
struct LegacyAxis { double delta = 0; };
struct ModernAxis { bool autoscroll = false; };
enum class LegacyOutputResult { None, Failed };
static_assert(std::is_void_v<MemberResult<decltype(&LegacyEffect::paint)>>);
static_assert(std::is_same_v<MemberResult<decltype(&ModernEffect::paint)>, bool>);
}
class ApiCompatTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void legacyPaintingCallsOnce()
    {
        int calls = 0;
        QVERIFY(paintSucceeded([&]() { ++calls; }));
        QCOMPARE(calls, 1);
        paintResult<void>([&]() { ++calls; return true; });
        QCOMPARE(calls, 2);
    }
    void modernPaintingPreservesFailure()
    {
        for (bool success : {true, false}) {
            int calls = 0;
            QCOMPARE(paintResult<bool>([&]() {
                return paintSucceeded([&]() { ++calls; return success; });
            }), success);
            QCOMPARE(calls, 1);
        }
    }
    void optionalAutoscrollField()
    {
        QVERIFY(!isAutoscroll(LegacyAxis{}));
        QVERIFY(!isAutoscroll(ModernAxis{false}));
        QVERIFY(isAutoscroll(ModernAxis{true}));
    }
    void outputConfigurationResults()
    {
        QVERIFY(outputConfigurationSucceeded(LegacyOutputResult::None));
        QVERIFY(!outputConfigurationSucceeded(LegacyOutputResult::Failed));
        // std::optional has the same has_value() contract as 6.8's expected.
        QVERIFY(outputConfigurationSucceeded(std::optional<int>(0)));
        QVERIFY(!outputConfigurationSucceeded(std::optional<int>()));
    }
};
QTEST_GUILESS_MAIN(ApiCompatTest)
#include "api_compat_test.moc"
