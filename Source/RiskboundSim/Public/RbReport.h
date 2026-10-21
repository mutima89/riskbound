// Executive risk report built only from GameState, plus a Markdown export.
// Content and ordering rules: Docs/RiskMethodology.md section 14.
#pragma once

#include "RbApi.h"
#include "RbActions.h"
#include "RbQuant.h"
#include "RbRiskModel.h"

#include <string>
#include <vector>

namespace rb
{
struct GameState;

struct ReportRiskLine
{
	std::string Id;
	std::string Title;
	std::string Category;
	std::string Owner;
	int ResidualLikelihood = 1;
	int ResidualImpact = 1;
	int ResidualScore = 1;          // L x I, ordinal
	ERating Rating = ERating::Low;
	EAppetiteStatus Appetite = EAppetiteStatus::Within;
	int AppetiteLimit = 0;
	ETreatment Treatment = ETreatment::Undecided;
	ERiskStatus Status = ERiskStatus::Open;
};

struct ReportKriLine
{
	std::string Id;
	std::string Name;
	std::string Unit;
	double Value = 0.0;
	EKriStatus Status = EKriStatus::Green;
	int AmberStreak = 0;
	bool bEscalate = false;
};

struct ReportActionLine
{
	std::string Id;
	std::string RiskId;
	std::string Title;
	std::string Owner;
	int DueWeek = 0;
	EActionStatus Status = EActionStatus::Open;
};

struct ReportQuantLine
{
	std::string RiskId;
	double ExpectedLoss = 0.0; // per year, probability-weighted average
	double WorstCase = 0.0;    // largest single scenario loss
	int Scenarios = 0;
};

struct ReportDecisionLine
{
	std::string Id;
	std::string Summary;
	int Week = 0;
	int QualityScore = 0;      // decision quality, independent of outcome
	std::string Outcome;       // "Pending" until the consequence resolves
};

struct ExecutiveReport
{
	std::string OrganisationName;
	int Week = 0;
	int OpenRisks = 0;                          // not Closed
	std::vector<ReportRiskLine> TopRisks;       // highest residual score first, ties by id; at most TopN
	std::vector<ReportRiskLine> AppetiteBreaches; // every risk with appetite Exceeds, same order
	std::vector<ReportKriLine> Kris;            // latest reading per KRI, definition order
	std::vector<ReportActionLine> OpenActions;  // not done and not overdue, by due week then id
	std::vector<ReportActionLine> OverdueActions; // by due week then id
	std::vector<ReportQuantLine> Quantified;    // player-quantified risks, by id
	std::vector<ReportDecisionLine> Decisions;  // in the order they were taken
};

RB_API ExecutiveReport BuildExecutiveReport(const GameState& State, int TopN = 5);
RB_API std::string ReportToMarkdown(const ExecutiveReport& Report);
} // namespace rb
