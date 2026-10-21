#include "RbAiColleaguesSubsystem.h"

#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RbCampaignSubsystem.h"
#include "RbConvert.h"
#include "RbGameSubsystem.h"
#include "RbLog.h"

using namespace RbConvert;

void URbAiColleaguesSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Load();
}

FString URbAiColleaguesSubsystem::ConfigPath() const
{
	return FPaths::ProjectSavedDir() / TEXT("Config/RiskboundAi.cfg");
}

void URbAiColleaguesSubsystem::Load()
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *ConfigPath())) return;
	TArray<FString> Lines;
	Text.ParseIntoArrayLines(Lines);
	for (const FString& L : Lines)
	{
		if (L.StartsWith(TEXT("enabled="))) bEnabled = L.RightChop(8).TrimStartAndEnd() == TEXT("1");
		else if (L.StartsWith(TEXT("key="))) ApiKey = L.RightChop(4).TrimStartAndEnd();
	}
}

void URbAiColleaguesSubsystem::Store() const
{
	FFileHelper::SaveStringToFile(FString::Printf(TEXT("enabled=%d\nkey=%s\n"), bEnabled ? 1 : 0, *ApiKey), *ConfigPath());
}

void URbAiColleaguesSubsystem::SetEnabled(bool bOn)
{
	bEnabled = bOn;
	Store();
}

void URbAiColleaguesSubsystem::SetApiKey(const FString& Key)
{
	ApiKey = Key.TrimStartAndEnd();
	Store();
}

FString URbAiColleaguesSubsystem::MaskedKey() const
{
	return ApiKey.Len() > 8 ? ApiKey.Left(7) + TEXT("...") + ApiKey.Right(4) : FString(ApiKey.IsEmpty() ? TEXT("none") : TEXT("set"));
}

const TArray<rb::AiTurn>& URbAiColleaguesSubsystem::Transcript(const FString& PersonId) const
{
	static const TArray<rb::AiTurn> None;
	const TArray<rb::AiTurn>* T = Transcripts.Find(PersonId);
	return T ? *T : None;
}

void URbAiColleaguesSubsystem::Ask(const FString& PersonId, const FString& Line, TFunction<void(const FString& Error)> OnDone)
{
	if (!IsReady())
	{
		OnDone(TEXT("Turn on AI colleagues and add your API key in Settings first."));
		return;
	}
	if (bWaiting || Line.TrimStartAndEnd().IsEmpty()) return;
	const URbGameSubsystem* Game = GetGameInstance()->GetSubsystem<URbGameSubsystem>();
	const URbCampaignSubsystem* Campaign = GetGameInstance()->GetSubsystem<URbCampaignSubsystem>();
	const rb::CharacterDef* Person = Campaign ? Campaign->GetWorldContent().People.Find(ToStd(PersonId)) : nullptr;
	if (!Game || !Person)
	{
		OnDone(TEXT("Unknown colleague."));
		return;
	}
	const rb::GameState& S = Game->GetState();
	const std::string System = rb::ColleagueSystemPrompt(S, *Person, Campaign->GetCatalogue().Find(S.Campaign.ActiveMissionId));
	TArray<rb::AiTurn>& T = Transcripts.FindOrAdd(PersonId);
	T.Add({true, ToStd(Line.TrimStartAndEnd())});
	const std::vector<rb::AiTurn> Turns(T.GetData(), T.GetData() + T.Num());

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(TEXT("https://api.anthropic.com/v1/messages"));
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("x-api-key"), ApiKey);
	Request->SetHeader(TEXT("anthropic-version"), TEXT("2023-06-01"));
	Request->SetHeader(TEXT("anthropic-beta"), TEXT("server-side-fallback-2026-07-01"));
	Request->SetContentAsString(ToFString(rb::ColleagueRequestJson(System, Turns)));
	Request->SetTimeout(60.0f);
	bWaiting = true;
	TWeakObjectPtr<URbAiColleaguesSubsystem> WeakThis(this);
	Request->OnProcessRequestComplete().BindLambda([WeakThis, PersonId, OnDone](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
	{
		if (!WeakThis.IsValid()) return;
		WeakThis->bWaiting = false;
		if (!bConnected || !Response.IsValid())
		{
			OnDone(TEXT("No connection to the AI service. The rest of the game works offline."));
			return;
		}
		const int32 Code = Response->GetResponseCode();
		std::string Text, Error;
		const bool bOk = rb::ParseColleagueResponse(ToStd(Response->GetContentAsString()), Text, Error);
		if (Code == 401) Error = "The API key was not accepted. Check it in Settings.";
		else if (Code == 429) Error = "Too many requests right now. Try again in a minute.";
		else if (Code >= 500) Error = "The AI service is busy. Try again shortly.";
		if (bOk && Code == 200)
		{
			WeakThis->Transcripts.FindOrAdd(PersonId).Add({false, Text});
			OnDone(FString());
		}
		else
		{
			UE_LOG(LogRiskbound, Warning, TEXT("AI colleagues: HTTP %d"), Code); // never the key or the body
			OnDone(ToFString(Error.empty() ? std::string("The AI service returned an error.") : Error));
		}
	});
	Request->ProcessRequest();
}
