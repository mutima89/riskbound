#include "RbSaveSubsystem.h"

#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "RbConvert.h"
#include "RbGameSubsystem.h"
#include "RbLog.h"
#include "RbSave.h"

using namespace RbConvert;

void URbSaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<URbGameSubsystem>();
}

FString URbSaveSubsystem::GetSlotPath() const
{
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("SaveGames") / (SlotName + TEXT(".rbsave")));
}

bool URbSaveSubsystem::HasSave() const
{
	const FString Path = GetSlotPath();
	IFileManager& Files = IFileManager::Get();
	return Files.FileExists(*Path) || Files.FileExists(*ToFString(rb::BackupPath(ToStd(Path))));
}

bool URbSaveSubsystem::Save(FString& OutError)
{
	const URbGameSubsystem* Game = GetGameInstance()->GetSubsystem<URbGameSubsystem>();
	if (!Game || !Game->HasGame())
	{
		OutError = TEXT("No game in progress.");
		return false;
	}
	const FString Path = GetSlotPath();
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	std::string Error;
	const bool bOk = rb::SaveGameToFile(ToStd(Path), Game->GetState(), &Error);
	OutError = ToFString(Error);
	UE_LOG(LogRiskbound, Display, TEXT("Save to %s: %s %s"), *Path, bOk ? TEXT("ok") : TEXT("FAILED"), *OutError);
	return bOk;
}

bool URbSaveSubsystem::Autosave()
{
	URbGameSubsystem* Game = GetGameInstance()->GetSubsystem<URbGameSubsystem>();
	FString Error;
	const bool bOk = Save(Error);
	if (Game)
	{
		Game->Notify(bOk ? FString(TEXT("Progress autosaved")) : FString::Printf(TEXT("Autosave failed: %s"), *Error));
	}
	return bOk;
}

ERbLoadOutcome URbSaveSubsystem::Load(FString& OutMessage)
{
	URbGameSubsystem* Game = GetGameInstance()->GetSubsystem<URbGameSubsystem>();
	const FString Path = GetSlotPath();
	rb::GameState Loaded;
	const rb::LoadResult Result = rb::LoadGameFromFile(ToStd(Path), Loaded);
	OutMessage = ToFString(Result.Message);
	UE_LOG(LogRiskbound, Display, TEXT("Load %s: %s %s"), *Path, *ToFString(rb::ToString(Result.Status)), *OutMessage);

	switch (Result.Status)
	{
	case rb::ELoadStatus::Ok:
		Game->SetLoadedState(MoveTemp(Loaded));
		Game->ApplySettings(false);
		return ERbLoadOutcome::Ok;
	case rb::ELoadStatus::RecoveredFromBackup:
		Game->SetLoadedState(MoveTemp(Loaded));
		Game->ApplySettings(false);
		OutMessage = TEXT("Your latest save was damaged, so the previous save was restored from backup. ") + OutMessage;
		Game->Notify(OutMessage);
		return ERbLoadOutcome::RecoveredFromBackup;
	case rb::ELoadStatus::NotFound:
		OutMessage = TEXT("No saved game was found.");
		return ERbLoadOutcome::NotFound;
	case rb::ELoadStatus::UnsupportedVersion:
		OutMessage = TEXT("This save was made by a newer version of Riskbound. ") + OutMessage;
		return ERbLoadOutcome::UnsupportedVersion;
	case rb::ELoadStatus::Corrupt:
		break;
	}
	OutMessage = TEXT("The save and its backup are both damaged and cannot be loaded. ") + OutMessage;
	return ERbLoadOutcome::Corrupt;
}
