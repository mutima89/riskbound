// Automation tests for the workstation tool hub view models (Riskbound.Tools.*).
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "RbGameSubsystem.h"
#include "RbToolsSubsystem.h"

namespace
{
constexpr EAutomationTestFlags ToolFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;

struct FToolSession
{
	UGameInstance* Instance = nullptr;
	URbGameSubsystem* Game = nullptr;
	URbToolsSubsystem* Tools = nullptr;

	FToolSession()
	{
		Instance = NewObject<UGameInstance>(GEngine);
		Instance->AddToRoot();
		Instance->InitializeStandalone();
		Game = Instance->GetSubsystem<URbGameSubsystem>();
		Tools = Instance->GetSubsystem<URbToolsSubsystem>();
		Game->NewGame(ERbDifficulty::Professional);
	}

	~FToolSession()
	{
		UWorld* World = Instance->GetWorld();
		Instance->Shutdown();
		if (World)
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
		Instance->RemoveFromRoot();
	}
};
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRbToolsRegisterTest, "Riskbound.Tools.RegisterAddFilterSearchMatrix", ToolFlags)
bool FRbToolsRegisterTest::RunTest(const FString& Parameters)
{
	FToolSession S;
	if (!TestNotNull(TEXT("tools subsystem"), S.Tools)) return false;
	FString Error;
	FRbRiskInput Bad;
	TestTrue(TEXT("title and owner required"), S.Tools->AddRisk(Bad, Error).IsEmpty() && !Error.IsEmpty());
	FRbRiskInput In;
	In.Title = TEXT("Fuel price spike");
	In.Owner = TEXT("Fleet manager");
	In.Category = TEXT("Financial");
	In.Cause = TEXT("Diesel market volatility");
	In.Likelihood = 4;
	In.Impact = 4;
	const FString Id = S.Tools->AddRisk(In, Error);
	TestEqual(TEXT("new id"), Id, FString(TEXT("R-015")));

	FRbRegisterFilter All;
	TestEqual(TEXT("two risks"), S.Tools->GetRegister(All).Num(), 2);
	FRbRegisterFilter Search;
	Search.Search = TEXT("diesel");
	const TArray<FRbRegisterRow> Hits = S.Tools->GetRegister(Search);
	TestTrue(TEXT("search finds the cause text"), Hits.Num() == 1 && Hits[0].Id == Id);
	FRbRegisterFilter Outside;
	Outside.bOnlyOutsideAppetite = true;
	const TArray<FRbRegisterRow> Breaches = S.Tools->GetRegister(Outside);
	TestTrue(TEXT("16 > financial limit 10"), Breaches.Num() == 1 && Breaches[0].ResidualScore == 16 && Breaches[0].Rating == TEXT("Critical"));
	FRbRegisterFilter Cat;
	Cat.Category = TEXT("Operational");
	TestTrue(TEXT("category filter"), S.Tools->GetRegister(Cat).Num() == 1);

	const TArray<FRbMatrixCell> Cells = S.Tools->GetMatrix();
	TestEqual(TEXT("5x5 cells"), Cells.Num(), 25);
	TestTrue(TEXT("first cell is L5 I1"), Cells[0].Likelihood == 5 && Cells[0].Impact == 1);
	int32 Count44 = 0;
	for (const FRbMatrixCell& C : Cells) if (C.Likelihood == 4 && C.Impact == 4) Count44 = C.Count;
	TestEqual(TEXT("new risk counted at 4x4"), Count44, 1);

	In.Impact = 2;
	TestTrue(TEXT("update"), S.Tools->UpdateRisk(Id, In, Error));
	TestEqual(TEXT("updated impact round-trips"), S.Tools->GetRiskInput(Id).Impact, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRbToolsQuantTest, "Riskbound.Tools.QuantViewModels", ToolFlags)
bool FRbToolsQuantTest::RunTest(const FString& Parameters)
{
	FToolSession S;
	const TArray<FRbScenarioInput> Sc = {{TEXT("minor"), 0.5, 40000.0}, {TEXT("major"), 0.15, 290000.0}, {TEXT("terminate"), 0.03, 1800000.0}};
	const FRbLossView L = S.Tools->EvaluateScenarios(Sc);
	TestTrue(TEXT("valid"), L.bValid);
	TestEqual(TEXT("EL hand value"), L.ExpectedLoss, 117500.0, 1e-6);
	TestEqual(TEXT("worst case"), L.WorstCase, 1800000.0, 1e-6);
	TestFalse(TEXT("p > 1 invalid"), S.Tools->EvaluateScenarios({{TEXT("x"), 1.5, 10.0}}).bValid);
	TestTrue(TEXT("quantification saved"), S.Tools->SaveQuantification(TEXT("R-014"), Sc));
	TestEqual(TEXT("quantification read back"), S.Tools->GetQuantification(TEXT("R-014")).Num(), 3);

	FRbAppraisalInput A;
	A.ExpectedLossBefore = 117500.0;
	A.ExpectedLossAfter = 37500.0;
	A.OneOffCost = 18000.0;
	A.AnnualCost = 12000.0;
	const FRbAppraisalView V = S.Tools->Appraise(A);
	TestTrue(TEXT("cost-effective"), V.bCostEffective && V.Npv > 0.0);
	const TArray<FRbSensitivityRowView> T = S.Tools->Tornado(A, 0.1);
	TestTrue(TEXT("five tornado rows, widest first"), T.Num() == 5 && T[0].Name == TEXT("Expected loss before") && T[0].Swing >= T[1].Swing);
	TestEqual(TEXT("tornado swing hand value"), T[0].Swing, 60561.78, 0.05);

	const TArray<FRbStressPreset> P = S.Tools->GetStressPresets();
	TestEqual(TEXT("four presets"), P.Num(), 4);
	const FRbStressView St = S.Tools->Stress(Sc, P[3], 2000000.0);
	TestEqual(TEXT("combined preset stressed EL"), St.StressedExpectedLoss, 567500.0, 1e-6);
	TestTrue(TEXT("breach"), St.bBreachesCapacity);

	FRbMonteCarloInput Mc;
	Mc.Trials = 4000;
	Mc.Seed = 9;
	const FRbMonteCarloView M1 = S.Tools->MonteCarlo(Mc, 25);
	const FRbMonteCarloView M2 = S.Tools->MonteCarlo(Mc, 25);
	TestEqual(TEXT("seeded: identical mean"), M1.Mean, M2.Mean, 0.0);
	int32 Total = 0;
	for (int32 C : M1.Histogram) Total += C;
	TestEqual(TEXT("histogram counts every trial"), Total, 4000);
	TestTrue(TEXT("percentiles ordered"), M1.P50 <= M1.P90 && M1.P90 <= M1.P95 && M1.P95 <= M1.P99 && M1.P99 <= M1.Max);
	TestEqual(TEXT("median fit: mu = ln(median)"), M1.Mu, FMath::Loge(25000.0), 1e-9);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRbToolsKriActionsTest, "Riskbound.Tools.KrisActionsAndTime", ToolFlags)
bool FRbToolsKriActionsTest::RunTest(const FString& Parameters)
{
	FToolSession S;
	const TArray<FRbKriView> K = S.Tools->GetKris();
	TestEqual(TEXT("five KRIs"), K.Num(), 5);
	TestTrue(TEXT("12 weeks of history"), K.Num() && K[0].History.Num() == 12 && K[0].HistoryWeeks.Last() == 0);
	TestTrue(TEXT("untreated alarm KRI escalates"), K.Num() && K[0].Status == TEXT("Red") && K[0].bEscalate);

	FString Error;
	TestTrue(TEXT("unknown risk rejected"), S.Tools->CreateAction(TEXT("R-999"), TEXT("x"), TEXT("y"), 2, Error).IsEmpty());
	const FString Id = S.Tools->CreateAction(TEXT("R-014"), TEXT("Fix alarm escalation"), TEXT("Site manager"), 1, Error);
	TestEqual(TEXT("first action id"), Id, FString(TEXT("A-001")));
	TestFalse(TEXT("close needs evidence"), S.Tools->CloseAction(Id, TEXT(""), Error));
	S.Tools->AdvanceWeek();
	S.Tools->AdvanceWeek();
	TestEqual(TEXT("week 2"), S.Tools->GetWeek(), 2);
	TestEqual(TEXT("overdue after due week"), S.Tools->GetActions()[0].Status, FString(TEXT("Overdue")));
	TestEqual(TEXT("KRI recorded each week"), S.Tools->GetKris()[0].History.Num(), 14);
	TestTrue(TEXT("close with evidence"), S.Tools->CloseAction(Id, TEXT("Rota published 12 Oct"), Error));
	TestEqual(TEXT("done"), S.Tools->GetActions()[0].Status, FString(TEXT("Done")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRbToolsReportTest, "Riskbound.Tools.ReportExport", ToolFlags)
bool FRbToolsReportTest::RunTest(const FString& Parameters)
{
	FToolSession S;
	S.Tools->SaveQuantification(TEXT("R-014"), {{TEXT("minor"), 0.5, 40000.0}});
	const FString Md = S.Tools->GetReportMarkdown();
	TestTrue(TEXT("report title"), Md.Contains(TEXT("# Executive risk report: Valtren Coldline Logistics")));
	TestTrue(TEXT("EL beside worst case"), Md.Contains(TEXT("| R-014 | 20,000 | 40,000 | 1 |")));
	const FString Path = S.Tools->ExportReport();
	TestTrue(TEXT("exported under Saved/Reports"), !Path.IsEmpty() && Path.Contains(TEXT("Saved/Reports/ExecutiveReport_Week0_")));
	FString OnDisk;
	TestTrue(TEXT("file readable"), FFileHelper::LoadFileToString(OnDisk, *Path));
	TestEqual(TEXT("file equals the on-screen report"), OnDisk, Md);
	IFileManager::Get().Delete(*Path);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
