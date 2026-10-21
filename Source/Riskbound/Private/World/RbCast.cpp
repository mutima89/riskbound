#include "RbCast.h"

#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"

namespace RbCast
{
namespace
{
// Rocketbox avatars import in centimetres facing +Y, like the engine mannequin.
constexpr float AvatarYaw = -90.0f;

struct FCastEntry
{
	const TCHAR* Person;
	const TCHAR* Avatar;
};

// Named people. Jordan (the player's partner) is never shown in the office.
const FCastEntry Cast[] = {
	{TEXT("priya"), TEXT("Business_Female_01")},   {TEXT("celeste"), TEXT("Business_Female_02")},
	{TEXT("helen"), TEXT("Business_Female_03")},   {TEXT("ruth"), TEXT("Business_Female_04")},
	{TEXT("adrian"), TEXT("Business_Male_02")},    {TEXT("hollis"), TEXT("Business_Male_04")},
	{TEXT("rafe"), TEXT("Business_Male_01")},      {TEXT("tomasz"), TEXT("Business_Male_03")},
	{TEXT("callum"), TEXT("Business_Male_06")},    {TEXT("jonah"), TEXT("Business_Male_07")},
	{TEXT("grace"), TEXT("Female_Adult_01")},      {TEXT("amara"), TEXT("Female_Adult_02")},
	{TEXT("mei"), TEXT("Female_Adult_03")},        {TEXT("imogen"), TEXT("Female_Adult_04")},
};

// Background staff: avatars no named person uses, then a few repeats far apart in the office.
const TCHAR* const Staff[] = {
	TEXT("Male_Adult_01"), TEXT("Female_Adult_05"), TEXT("Business_Male_05"), TEXT("Male_Adult_02"), TEXT("Female_Adult_03"),
	TEXT("Male_Adult_03"), TEXT("Business_Female_04"), TEXT("Male_Adult_04"), TEXT("Female_Adult_02"),
};

// Idles cycle so nobody in a room moves in step with their neighbour.
const TCHAR* const StaffIdles[] = {TEXT("idle_neutral_01"), TEXT("idle_breathe_01"), TEXT("idle_neutral_02"), TEXT("idle_look_around_01"),
	TEXT("cell_phone_listen_01"), TEXT("idle_neutral_02")};

UAnimSequence* Anim(bool bFemale, const TCHAR* Name)
{
	const FString Path = FString::Printf(TEXT("/Game/Riskbound/Characters/Anims/A_%s_%s.A_%s_%s"), bFemale ? TEXT("f") : TEXT("m"), Name, bFemale ? TEXT("f") : TEXT("m"), Name);
	return LoadObject<UAnimSequence>(nullptr, *Path);
}

FLook Avatar(const TCHAR* Name, const TCHAR* IdleName)
{
	FLook L;
	const FString Path = FString::Printf(TEXT("/Game/Riskbound/Characters/People/%s/SK_%s.SK_%s"), Name, Name, Name);
	L.Mesh = LoadObject<USkeletalMesh>(nullptr, *Path);
	if (L.Mesh)
	{
		const bool bFemale = FString(Name).Contains(TEXT("Female"));
		L.Idle = Anim(bFemale, IdleName);
		if (!L.Idle) L.Idle = Anim(bFemale, TEXT("idle_neutral_01")); // not every loop exists for both bodies
		L.MeshYaw = AvatarYaw;
		L.bFallback = false;
		return L;
	}
	// Stand-in: the engine mannequin.
	const bool bFemale = FString(Name).Contains(TEXT("Female"));
	L.Mesh = LoadObject<USkeletalMesh>(nullptr, bFemale ? TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple")
		: TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	L.Idle = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle.MM_Idle"));
	return L;
}
}

FLook ForPerson(const FString& PersonId, int32 Idle)
{
	const TCHAR* IdleName = Idle == 1 ? TEXT("gestic_listen_neutral_01") : Idle == 2 ? TEXT("idle_breathe_01") : TEXT("idle_neutral_01");
	for (const FCastEntry& E : Cast)
	{
		if (PersonId == E.Person) return Avatar(E.Avatar, IdleName);
	}
	return Avatar(Staff[GetTypeHash(PersonId) % UE_ARRAY_COUNT(Staff)], IdleName);
}

FLook ForStaff(int32 Index)
{
	const int32 I = FMath::Abs(Index);
	return Avatar(Staff[I % UE_ARRAY_COUNT(Staff)], StaffIdles[I % UE_ARRAY_COUNT(StaffIdles)]);
}
}
