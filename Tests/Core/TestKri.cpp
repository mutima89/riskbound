#include "RbTest.h"

#include "RbGameState.h"
#include "RbKri.h"

using namespace rb;

static KriDefinition Def(double Noise)
{
	KriDefinition D;
	D.Id = "test";
	D.Name = "Test indicator";
	D.Unit = "%";
	D.Amber = 20.0;
	D.Red = 40.0;
	D.bHigherIsWorse = true;
	D.Baseline = 50.0;
	D.Improved = 10.0;
	D.ImprovingFlag = "fixed";
	D.Noise = Noise;
	D.bPercent = true;
	return D;
}

TEST(Kri_ValueFormula)
{
	// No noise: the value is exactly the target.
	CHECK_NEAR(KriValue(Def(0.0), 0, false, 42, 3), 50.0, 1e-9);
	CHECK_NEAR(KriValue(Def(0.0), 0, true, 42, 3), 10.0, 1e-9);
	// 10% noise: within target x [0.9, 1.1], rounded to one decimal, deterministic.
	for (int Week = -10; Week < 30; ++Week)
	{
		const double V = KriValue(Def(0.1), 2, false, 42, Week);
		CHECK(V >= 45.0 && V <= 55.0);
		CHECK_NEAR(V * 10.0, std::round(V * 10.0), 1e-9);
		CHECK_NEAR(V, KriValue(Def(0.1), 2, false, 42, Week), 0.0);
	}
	// Percent KRIs never exceed 100 even with a large target.
	KriDefinition Big = Def(0.5);
	Big.Baseline = 95.0;
	for (int Week = 0; Week < 20; ++Week) CHECK(KriValue(Big, 0, false, 7, Week) <= 100.0);
}

TEST(Kri_StatusStreakAndEscalation)
{
	const KriDefinition D = Def(0.0);
	KriReading A = MakeReading(D, 19.9, 1, nullptr);
	CHECK(A.Status == EKriStatus::Green);
	CHECK_EQ(A.AmberStreak, 0);
	KriReading B = MakeReading(D, 20.0, 2, &A); // threshold is inclusive
	CHECK(B.Status == EKriStatus::Amber);
	CHECK_EQ(B.AmberStreak, 1);
	CHECK(!B.bEscalate);
	KriReading C = MakeReading(D, 30.0, 3, &B);
	KriReading E = MakeReading(D, 39.9, 4, &C);
	CHECK_EQ(E.AmberStreak, 3);
	CHECK(E.bEscalate); // third consecutive amber week
	KriReading F = MakeReading(D, 40.0, 5, &E);
	CHECK(F.Status == EKriStatus::Red);
	CHECK(F.bEscalate);
	CHECK_EQ(F.AmberStreak, 0);
	KriReading Gap = MakeReading(D, 25.0, 9, &B); // not consecutive with week 2
	CHECK_EQ(Gap.AmberStreak, 1);

	KriDefinition Lower = Def(0.0); // lower is worse, e.g. completion rate
	Lower.bHigherIsWorse = false;
	Lower.Amber = 90.0;
	Lower.Red = 75.0;
	CHECK(MakeReading(Lower, 96.0, 0, nullptr).Status == EKriStatus::Green);
	CHECK(MakeReading(Lower, 90.0, 0, nullptr).Status == EKriStatus::Amber);
	CHECK(MakeReading(Lower, 75.0, 0, nullptr).Status == EKriStatus::Red);
}

TEST(Kri_MonitorHistoryIsDeterministicAndFlagDriven)
{
	GameState G = NewGame("Test", EDifficulty::Professional, 99);
	CHECK_EQ(G.Kris.Series.size(), ValtrenKris().size());
	const KriSeries& Alarm = G.Kris.Series[0];
	CHECK(Alarm.Id == "alarm_ack_no_action");
	REQUIRE(Alarm.History.size() == size_t(KriHistoryWeeksAtStart));
	CHECK_EQ(Alarm.History.front().Week, -11);
	CHECK_EQ(Alarm.History.back().Week, 0);
	for (const KriReading& R : Alarm.History)
	{
		CHECK(R.Value >= 61.2 && R.Value <= 74.8); // 68 +/- 10%: untreated
		CHECK(R.Status == EKriStatus::Red);
		CHECK(R.bEscalate);
	}
	const GameState Same = NewGame("Other", EDifficulty::Executive, 99);
	CHECK_NEAR(Same.Kris.Series[3].History[5].Value, G.Kris.Series[3].History[5].Value, 0.0);

	G.Org.SetFlag("northgate_controls_fixed");
	AdvanceGameWeeks(G, 2);
	CHECK_EQ(G.Kris.Series[0].History.back().Week, 2);
	REQUIRE(G.Kris.Series[0].History.size() == size_t(KriHistoryWeeksAtStart + 2));
	const double Fixed = G.Kris.Series[0].History.back().Value;
	CHECK(Fixed >= 7.2 && Fixed <= 8.8); // improved target 8 +/- 10%
	CHECK(G.Kris.Series[0].History.back().Status == EKriStatus::Green);
	CHECK_EQ(RecordKriWeeks(G.Kris, G.Org), 0); // nothing new to record

	AdvanceGameWeeks(G, 60);
	CHECK_EQ(G.Kris.Series[0].History.size(), size_t(KriHistoryLimit));
}
