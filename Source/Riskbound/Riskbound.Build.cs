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
		PrivateDependencyModuleNames.AddRange(new string[] { "RenderCore", "RHI" });
		// Inter UI font (SIL OFL), loaded from loose files at runtime (RbUIStyle).
		RuntimeDependencies.Add("$(ProjectDir)/Resources/Fonts/*.ttf", StagedFileType.UFS);
		RuntimeDependencies.Add("$(ProjectDir)/Resources/Fonts/OFL.txt", StagedFileType.UFS);
		// Mission content (Docs/MissionData.md), read and validated at startup by URbCampaignSubsystem.
		RuntimeDependencies.Add("$(ProjectDir)/Data/Missions/*.json", StagedFileType.UFS);
		RuntimeDependencies.Add("$(ProjectDir)/Data/Achievements.json", StagedFileType.UFS);
		// Living world content (Docs/WorldData.md): people, story clues, inbox events and fallout.
		RuntimeDependencies.Add("$(ProjectDir)/Data/People.json", StagedFileType.UFS);
		RuntimeDependencies.Add("$(ProjectDir)/Data/Story.json", StagedFileType.UFS);
		RuntimeDependencies.Add("$(ProjectDir)/Data/Events.json", StagedFileType.UFS);
		RuntimeDependencies.Add("$(ProjectDir)/Data/Dialogue.json", StagedFileType.UFS);
		RuntimeDependencies.Add("$(ProjectDir)/Data/Glossary.json", StagedFileType.UFS);
		RuntimeDependencies.Add("$(ProjectDir)/Data/Endings.json", StagedFileType.UFS);
	}
}
