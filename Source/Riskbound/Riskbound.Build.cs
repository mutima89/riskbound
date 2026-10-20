using UnrealBuildTool;

// Gameplay and presentation: first-person character, interaction, UMG workstations,
// UObject adapters over RiskboundSim. Simulation rules must not live here.
public class Riskbound : ModuleRules
{
	public Riskbound(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "UMG", "Slate", "SlateCore", "RiskboundSim" });
	}
}
