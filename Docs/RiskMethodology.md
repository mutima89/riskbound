# Risk methodology

Canonical definitions for every calculation in the game. Code: `RbRiskModel.*`, `RbQuant.*`.
If a rule changes, change it here and in code in the same commit, with a test.

## 1. Qualitative assessment (ordinal)
| Item | Rule |
|---|---|
| Scales | Likelihood and impact on 1..N (default N = 5). Inputs clamped to range. |
| Score | `L x I`. Ordinal: ranks risks, has no unit, is never converted to money. |
| Rating bands (5x5 default) | Low < 5 <= Medium < 10 <= High < 16 <= Critical |
| Configurable | `RiskMatrix.Size`, band thresholds, optional per-cell overrides (N x N). Validated by `IsValid()`. |

Limitation: multiplying ordinal scales is a convention, not arithmetic truth. A 2x5 and a 5x2 both score 10 but are different risks; use cell overrides when an organisation weights impact more heavily.

## 2. Control effectiveness
Each control has a design score and an operating score, 0..3.
`Effectiveness = f(min(design, operating))`: 0-1 Ineffective, 2 Partially effective, 3 Effective.
A well-designed control that does not operate is ineffective.

## 3. Residual risk
- Preventive controls reduce likelihood; detective and corrective controls reduce impact.
- Reduction in steps: Ineffective 0, Partial 1, Effective 2.
- Per dimension only the single strongest control counts (no stacking), floor 1.

Assumption: controls are not independent enough to stack safely. Designers needing layered defence must model it explicitly as one stronger control.

## 4. Appetite
Per-category maximum residual score (`AppetiteRule`), default 9. Status: Within (< limit), At limit (= limit), Exceeds (> limit).
Default Meridian limits: Operational 8, Compliance 4, Financial 10, Strategic 12, Cyber 6.

## 5. Expected loss
`EL = sum(p_i x L_i)` over scenarios. Units: currency per year. `p_i` annual probability (0..1), `L_i` loss in currency.
- EL is a long-run average. It is always shown next to `WorstCaseLoss` (largest loss of any scenario with p > 0) so players do not read it as a maximum.
- `ValidateScenarios(..., mutuallyExclusive)` rejects p outside [0,1], negative losses, and (if exclusive) sum(p) > 1.
- ALE = annual rate of occurrence x single loss expectancy.

## 6. Treatment cost-benefit
Inputs: EL before/after (per year), one-off cost (year 0), annual cost (years 1..N), N years, discount rate r.
- `DF_t = 1 / (1+r)^t`
- `PV benefit = sum_t (EL_before - EL_after) x DF_t`
- `PV cost = one-off + sum_t annual x DF_t`
- `NPV = PV benefit - PV cost`; `ROI = NPV / PV cost`; `BCR = PV benefit / PV cost` (0 when PV cost = 0)
- Cost-effective when NPV > 0.

Limitation: EL reduction is only as good as the probability estimates; the debrief states this.

## 7. KRIs
Amber and red thresholds, direction flag (`bHigherIsWorse`). Thresholds are inclusive (value at threshold takes the worse status).
Escalation: Red always; Amber after 3 consecutive periods (configurable).

## 8. Sensitivity analysis
One-at-a-time: each parameter at base x (1 +/- f), others held at base. Output a tornado sorted by swing `|high - low|`. Does not capture interactions.

## 9. Stress testing
Multiplicative shocks to probabilities (capped at 1) and losses. Reports base EL, stressed EL, stressed worst case, and breach of loss capacity (stressed worst case > capacity).

## 10. Monte Carlo
Annual loss = sum of `Poisson(lambda)` events, each `LogNormal(mu, sigma) x (1 - control reduction)`.
Reports mean, sample std dev, P50/P90/P95/P99 (linear interpolation between order statistics), max, P(annual loss > threshold).
- Seeded (`rb::Rng`, xoshiro256** via SplitMix64): same inputs + seed give identical results.
- `LogNormalFromMedianP90` converts expert estimates: `mu = ln(median)`, `sigma = (ln(P90) - mu) / 1.28155`.
- Poisson uses Knuth for lambda <= 30, normal approximation above.
Limitation: frequency and severity are independent; no fat-tail beyond lognormal.

## 11. Mission 001 figures (Meridian, Northgate cold store)
Scenarios (annual, exclusive): minor excursion 0.50 x 40,000; major spoilage + penalty 0.15 x 290,000; contract termination 0.03 x 1,800,000. EL = 117,500/yr; worst case 1,800,000.
Treatments over 3 years at 8%: service + escalation rota (18k + 12k/yr) NPV > 0; plant replacement (400k + 5k/yr) larger reduction but NPV < 0; insurance (35k/yr) recovers only claimable sudden-failure losses above the 100k excess, NPV < 0; accept: no change. All verified in `M001_ContentIsConsistent`.
