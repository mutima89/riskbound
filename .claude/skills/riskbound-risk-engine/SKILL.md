---
name: riskbound-risk-engine
description: Add or change Riskbound risk and financial calculations (matrix, controls, residual, appetite, expected loss, NPV/ROI, KRIs, sensitivity, stress, Monte Carlo) in RiskboundSim with documented formulas and unit tests. Use whenever a number the player sees is computed.
---

# Riskbound risk engine

Canonical rules: `Docs/RiskMethodology.md`. Code: `Source/RiskboundSim/{Public,Private}/RbRiskModel.*`, `RbQuant.*`, `RbRng.h`. Tests: `Tests/Core/TestRiskModel.cpp`, `TestQuant.cpp`.

## Before changing anything
- `grep -rn "<FunctionName>" Source Tests` to find the single existing implementation. Extend it; never add a second formula for the same quantity (including in Blueprints or UI).

## Rules
- Pure functions, no UE types, no exceptions, no RTTI, no global mutable state. Randomness only through a seeded `rb::Rng` passed or created from an explicit seed.
- State units in the header comment (currency, per year, 0..1, ordinal).
- Ordinal scores never become money. Expected loss always ships with a worst-case figure.
- Validate inputs (clamp or return a validity flag); document which.

## Done means
1. Formula, units, assumptions and limitations written in `Docs/RiskMethodology.md`.
2. Test with hand-computed expected values, including an edge case.
3. Mutation check: break the formula once, see the test fail, revert.
4. `Scripts/run_core_tests.sh` (or `.bat`) green; result line added to `Docs/QAReport.md`.
