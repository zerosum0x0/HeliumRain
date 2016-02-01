
#include "../Flare.h"
#include "FlareGame.h"
#include "FlareSaveGame.h"
#include "FlareAsteroid.h"
#include "FlareGameTools.h"
#include "FlareScenarioTools.h"

#include "../Player/FlareMenuManager.h"
#include "../Player/FlareHUD.h"
#include "../Player/FlarePlayerController.h"
#include "../Spacecrafts/FlareShell.h"
#include "../Spacecrafts/FlareSimulatedSpacecraft.h"
#include "../Quests/FlareQuestManager.h"
#include "../Data/FlareQuestCatalog.h"
#include "../Data/FlareResourceCatalog.h"

#define LOCTEXT_NAMESPACE "FlareGame"


/*----------------------------------------------------
	Constructor
----------------------------------------------------*/

AFlareGame::AFlareGame(const class FObjectInitializer& PCIP)
	: Super(PCIP)
	, CurrentImmatriculationIndex(0)
	, LoadedOrCreated(false)
	, SaveSlotCount(3)
{
	// Game classes
	HUDClass = AFlareHUD::StaticClass();
	PlayerControllerClass = AFlarePlayerController::StaticClass();
	DefaultWeaponIdentifier = FName("weapon-eradicator");
	DefaultTurretIdentifier = FName("weapon-artemis");

	// Menu pawn
	static ConstructorHelpers::FObjectFinder<UBlueprint> MenuPawnBPClass(TEXT("/Game/Gameplay/Menu/BP_MenuPawn"));
	if (MenuPawnBPClass.Object != NULL)
	{
		MenuPawnClass = (UClass*)MenuPawnBPClass.Object->GeneratedClass;
	}

	// Planetary system
	static ConstructorHelpers::FObjectFinder<UBlueprint> PlanetariumBPClass(TEXT("/Game/Environment/BP_Planetarium"));
	if (PlanetariumBPClass.Object != NULL)
	{
		PlanetariumClass = (UClass*)PlanetariumBPClass.Object->GeneratedClass;
	}

	// Data catalogs
	struct FConstructorStatics
	{
		ConstructorHelpers::FObjectFinder<UFlareSpacecraftCatalog> SpacecraftCatalog;
		ConstructorHelpers::FObjectFinder<UFlareSpacecraftComponentsCatalog> ShipPartsCatalog;
		ConstructorHelpers::FObjectFinder<UFlareCustomizationCatalog> CustomizationCatalog;
		ConstructorHelpers::FObjectFinder<UFlareAsteroidCatalog> AsteroidCatalog;
		ConstructorHelpers::FObjectFinder<UFlareCompanyCatalog> CompanyCatalog;
		ConstructorHelpers::FObjectFinder<UFlareSectorCatalog> SectorCatalog;
		ConstructorHelpers::FObjectFinder<UFlareQuestCatalog> QuestCatalog;
		ConstructorHelpers::FObjectFinder<UFlareResourceCatalog> ResourceCatalog;

		FConstructorStatics()
			: SpacecraftCatalog(TEXT("/Game/Gameplay/Catalog/SpacecraftCatalog"))
			, ShipPartsCatalog(TEXT("/Game/Gameplay/Catalog/ShipPartsCatalog"))
			, CustomizationCatalog(TEXT("/Game/Gameplay/Catalog/CustomizationCatalog"))
			, AsteroidCatalog(TEXT("/Game/Gameplay/Catalog/AsteroidCatalog"))
			, CompanyCatalog(TEXT("/Game/Gameplay/Catalog/CompanyCatalog"))
			, SectorCatalog(TEXT("/Game/Gameplay/Catalog/SectorCatalog"))
			, QuestCatalog(TEXT("/Game/Gameplay/Catalog/QuestCatalog"))
			, ResourceCatalog(TEXT("/Game/Gameplay/Catalog/ResourceCatalog"))
		{}
	};
	static FConstructorStatics ConstructorStatics;

	// Push catalog data into storage
	SpacecraftCatalog = ConstructorStatics.SpacecraftCatalog.Object;
	ShipPartsCatalog = ConstructorStatics.ShipPartsCatalog.Object;
	CustomizationCatalog = ConstructorStatics.CustomizationCatalog.Object;
	AsteroidCatalog = ConstructorStatics.AsteroidCatalog.Object;
	CompanyCatalog = ConstructorStatics.CompanyCatalog.Object;
	SectorCatalog = ConstructorStatics.SectorCatalog.Object;
	QuestCatalog = ConstructorStatics.QuestCatalog.Object;
	ResourceCatalog = ConstructorStatics.ResourceCatalog.Object;
}


/*----------------------------------------------------
	Gameplay
----------------------------------------------------*/

void AFlareGame::StartPlay()
{
	FLOG("AFlareGame::StartPlay");
	Super::StartPlay();
	
	// Spawn planetarium
	Planetarium = GetWorld()->SpawnActor<AFlarePlanetarium>(PlanetariumClass, FVector::ZeroVector, FRotator::ZeroRotator);
}

void AFlareGame::PostLogin(APlayerController* Player)
{
	FLOG("AFlareGame::PostLogin");
	Super::PostLogin(Player);
}

void AFlareGame::Logout(AController* Player)
{
	FLOG("AFlareGame::Logout");

	// Save the world, literally
	AFlarePlayerController* PC = Cast<AFlarePlayerController>(Player);
	SaveGame(PC);
	PC->PrepareForExit();

	Super::Logout(Player);
}


void AFlareGame::ActivateSector(AController* Player, UFlareSimulatedSector* Sector)
{
	if (!Sector)
	{
		// No sector to activate
		return;
	}

	FLOGV("AFlareGame::ActivateSector %s", *Sector->GetSectorName().ToString());
	if (ActiveSector)
	{
		FLOG("AFlareGame::ActivateSector has active sector");
		if (ActiveSector->GetIdentifier() == Sector->GetIdentifier())
		{
			// Sector to activate is already active
			return;
		}

		DeactivateSector(Player);
	}


	FLOGV("AFlareGame::ActivateSector ship count %d", Sector->GetSectorShips().Num());

	bool PlayerHasShip = false;
	for (int ShipIndex = 0; ShipIndex < Sector->GetSectorShips().Num(); ShipIndex++)
	{
		UFlareSimulatedSpacecraft* Ship = Sector->GetSectorShips()[ShipIndex];
		FLOGV("AFlareGame::ActivateSector ship %s", *Ship->GetImmatriculation().ToString());
		FLOGV(" %d", (Ship->GetCompany()->GetPlayerHostility() + 0));

		if (Ship->GetCompany()->GetPlayerHostility()  == EFlareHostility::Owned)
		{
			FLOG("  my ship");
			PlayerHasShip = true;
			break;
		}
	}

	FLOGV("PlayerHasShip %d", PlayerHasShip);
	if (PlayerHasShip)
	{
		// Create the new sector
		ActiveSector = NewObject<UFlareSector>(this, UFlareSector::StaticClass());
		FFlareSectorSave* SectorData = Sector->Save();
		if (SectorData->LocalTime < GetGameWorld()->GetDate())
		{
			// TODO Find time with light
			SectorData->LocalTime = GetGameWorld()->GetDate() * UFlareGameTools::SECONDS_IN_DAY;
		}
		Planetarium->ResetTime();
		ActiveSector->Load(*SectorData, Sector);

		AFlarePlayerController* PC = Cast<AFlarePlayerController>(Player);
		PC->OnSectorActivated();
	}
	GetQuestManager()->OnSectorActivation(Sector);
}

void AFlareGame::DeactivateSector(AController* Player)
{
	if (ActiveSector)
	{
		AFlarePlayerController* PC = Cast<AFlarePlayerController>(Player);

		FName LastFlownShip = "";

		if (PC->GetShipPawn())
		{
			LastFlownShip = PC->GetShipPawn()->GetImmatriculation();
		}

		TArray<FFlareSpacecraftSave> SpacecraftData;
		FFlareSectorSave* SectorData = ActiveSector->Save(SpacecraftData);
		ActiveSector->Destroy();
		ActiveSector = NULL;

		SectorData->LastFlownShip = LastFlownShip;
		PC->SetLastFlownShip(LastFlownShip);

		// Reload  spacecrafts
		for (int i = 0 ; i < SpacecraftData.Num(); i++)
		{
			UFlareSimulatedSpacecraft* Spacecraft = GetGameWorld()->FindSpacecraft(SpacecraftData[i].Immatriculation);
			Spacecraft->Load(SpacecraftData[i]);
		}

		// Reload sector
		UFlareSimulatedSector* Sector = World->FindSector(SectorData->Identifier);
		if (!Sector)
		{
			FLOGV("ERROR: no simulated sector match for active sector '%s'", *SectorData->Identifier.ToString());
		}

		Sector->Load(Sector->GetDescription(), *SectorData, *Sector->GetOrbitParameters());


		PC->OnSectorDeactivated();
	}
}

void AFlareGame::Tick(float DeltaSeconds)
{
	if (QuestManager)
	{
		QuestManager->OnTick(DeltaSeconds);
	}
}


/*----------------------------------------------------
	Save slots
----------------------------------------------------*/

void AFlareGame::ReadAllSaveSlots()
{
	// Setup
	SaveSlots.Empty();
	FVector2D EmblemSize = 128 * FVector2D::UnitVector;
	UMaterial* BaseEmblemMaterial = Cast<UMaterial>(FFlareStyleSet::GetIcon("CompanyEmblem")->GetResourceObject());

	// Get all saves
	for (int32 Index = 1; Index <= SaveSlotCount; Index++)
	{
		FFlareSaveSlotInfo SaveSlotInfo;
		SaveSlotInfo.EmblemBrush.ImageSize = EmblemSize;
		UFlareSaveGame* Save = AFlareGame::ReadSaveSlot(Index);
		SaveSlotInfo.Save = Save;

		if (Save)
		{
			// Basic setup
			UFlareCustomizationCatalog* Catalog = GetCustomizationCatalog();
			FLOGV("AFlareGame::ReadAllSaveSlots : found valid save data in slot %d", Index);

			// Count player ships
			SaveSlotInfo.CompanyShipCount = 0;

			// Find player company and count ships
			FFlareCompanySave* PlayerCompany = NULL;
            for (int32 CompanyIndex = 0; CompanyIndex < Save->WorldData.CompanyData.Num(); CompanyIndex++)
			{
                const FFlareCompanySave& Company = Save->WorldData.CompanyData[CompanyIndex];
				if (Company.Identifier == Save->PlayerData.CompanyIdentifier)
				{
                    PlayerCompany = &(Save->WorldData.CompanyData[CompanyIndex]);
					SaveSlotInfo.CompanyShipCount = Save->WorldData.CompanyData[CompanyIndex].ShipData.Num();
				}
			}

			// Company info
			if (PlayerCompany)
			{
				const FFlareCompanyDescription* Desc = &Save->PlayerCompanyDescription;

				// Money and general infos
				SaveSlotInfo.CompanyMoney = PlayerCompany->Money;
				SaveSlotInfo.CompanyName = Desc->Name;

				// Emblem material
				SaveSlotInfo.Emblem = UMaterialInstanceDynamic::Create(BaseEmblemMaterial, GetWorld());
				SaveSlotInfo.Emblem->SetVectorParameterValue("BasePaintColor", Catalog->GetColor(Desc->CustomizationBasePaintColorIndex));
				SaveSlotInfo.Emblem->SetVectorParameterValue("PaintColor", Catalog->GetColor(Desc->CustomizationPaintColorIndex));
				SaveSlotInfo.Emblem->SetVectorParameterValue("OverlayColor", Catalog->GetColor(Desc->CustomizationOverlayColorIndex));
				SaveSlotInfo.Emblem->SetVectorParameterValue("GlowColor", Catalog->GetColor(Desc->CustomizationLightColorIndex));

				// Create the brush dynamically
				SaveSlotInfo.EmblemBrush.SetResourceObject(SaveSlotInfo.Emblem);
			}
		}
		else
		{
			SaveSlotInfo.Save = NULL;
			SaveSlotInfo.Emblem = NULL;
			SaveSlotInfo.EmblemBrush = FSlateNoResource();
			SaveSlotInfo.CompanyShipCount = 0;
			SaveSlotInfo.CompanyMoney = 0;
			SaveSlotInfo.CompanyName = FText();
		}

		SaveSlots.Add(SaveSlotInfo);
	}

	FLOG("AFlareGame::ReadAllSaveSlots : all slots found");
}

int32 AFlareGame::GetSaveSlotCount() const
{
	return SaveSlotCount;
}

int32 AFlareGame::GetCurrentSaveSlot() const
{
	return CurrentSaveIndex;
}

void AFlareGame::SetCurrentSlot(int32 Index)
{
	FLOGV("AFlareGame::SetCurrentSlot : now using slot %d", Index);
	CurrentSaveIndex = Index;
}

bool AFlareGame::DoesSaveSlotExist(int32 Index) const
{
	int32 RealIndex = Index - 1;
	return RealIndex < SaveSlots.Num() && SaveSlots[RealIndex].Save;
}

const FFlareSaveSlotInfo& AFlareGame::GetSaveSlotInfo(int32 Index)
{
	int32 RealIndex = Index - 1;
	return SaveSlots[RealIndex];
}

UFlareSaveGame* AFlareGame::ReadSaveSlot(int32 Index)
{
	FString SaveFile = "SaveSlot" + FString::FromInt(Index);
	if (UGameplayStatics::DoesSaveGameExist(SaveFile, 0))
	{
		UFlareSaveGame* Save = Cast<UFlareSaveGame>(UGameplayStatics::CreateSaveGameObject(UFlareSaveGame::StaticClass()));
		Save = Cast<UFlareSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveFile, 0));
		return Save;
	}
	else
	{
		return NULL;
	}
}

bool AFlareGame::DeleteSaveSlot(int32 Index)
{
	FString SaveFile = "SaveSlot" + FString::FromInt(Index);
	if (UGameplayStatics::DoesSaveGameExist(SaveFile, 0))
	{
		return UGameplayStatics::DeleteGameInSlot(SaveFile, 0);
	}
	else
	{
		return false;
	}
}


/*----------------------------------------------------
	Save
----------------------------------------------------*/

void AFlareGame::CreateGame(AFlarePlayerController* PC, FText CompanyName, int32 ScenarioIndex, bool PlayTutorial)
{
	FLOGV("AFlareGame::CreateGame ScenarioIndex %d", ScenarioIndex);
	FLOGV("AFlareGame::CreateGame CompanyName %s", *CompanyName.ToString());

	PlayerController = PC;

	// Create the new world
	World = NewObject<UFlareWorld>(this, UFlareWorld::StaticClass());
	FFlareWorldSave WorldData;
	WorldData.Date = 3168;
	World->Load(WorldData);

	// Create companies
	for (int32 Index = 0; Index < GetCompanyCatalogCount(); Index++)
	{
		CreateCompany(Index);
	}

	// Manually setup the player company before creating it
	FFlareCompanyDescription CompanyData;
	CompanyData.Name = CompanyName;
	CompanyData.ShortName = *FString("PLY"); // TODO : Extract better short name
	CompanyData.Emblem = NULL; // TODO
	CompanyData.CustomizationBasePaintColorIndex = 0;
	CompanyData.CustomizationPaintColorIndex = 4;
	CompanyData.CustomizationOverlayColorIndex = 13;
	CompanyData.CustomizationLightColorIndex = 8;
	CompanyData.CustomizationPatternIndex = 1;
	PC->SetCompanyDescription(CompanyData);

	// Player company
	FFlarePlayerSave PlayerData;
	UFlareCompany* PlayerCompany = CreateCompany(-1);
	PlayerData.CompanyIdentifier = PlayerCompany->GetIdentifier();
	PlayerData.ScenarioId = ScenarioIndex;
	PlayerData.QuestData.PlayTutorial = PlayTutorial;
	PC->SetCompany(PlayerCompany);

	UFlareScenarioTools* ScenarioTools = NewObject<UFlareScenarioTools>(this, UFlareScenarioTools::StaticClass());
	ScenarioTools->Init(PlayerCompany, &PlayerData);

	switch(ScenarioIndex)
	{
		case -1: // Empty
			ScenarioTools->GenerateEmptyScenario();
		break;
		case 0: // Fighter
			ScenarioTools->GenerateFighterScenario();
		break;
		case 1: // Debug
			ScenarioTools->GenerateDebugScenario();
		break;
	}

	// Load
	PC->Load(PlayerData);

	// Init the quest manager
	QuestManager = NewObject<UFlareQuestManager>(this, UFlareQuestManager::StaticClass());
	QuestManager->Load(PlayerData.QuestData);

	// End loading
	LoadedOrCreated = true;
	PC->OnLoadComplete();
}

UFlareCompany* AFlareGame::CreateCompany(int32 CatalogIdentifier)
{
	if (!World)
	{
		FLOG("AFlareGame::CreateCompany failed: no loaded world");
		return NULL;
	}

	UFlareCompany* Company = NULL;
	FFlareCompanySave CompanyData;

	// Generate identifier
	CurrentImmatriculationIndex++;
	FString Immatriculation = FString::Printf(TEXT("CPNY-%06d"), CurrentImmatriculationIndex);
	CompanyData.Identifier = *Immatriculation;

	// Generate arbitrary save data
	CompanyData.CatalogIdentifier = CatalogIdentifier;
	CompanyData.Money = 0;
	CompanyData.FleetImmatriculationIndex = 0;
	CompanyData.TradeRouteImmatriculationIndex = 0;
	// Create company
	Company = World->LoadCompany(CompanyData);
	FLOGV("AFlareGame::CreateCompany : Created company '%s'", *Company->GetName());

	return Company;
}

bool AFlareGame::LoadGame(AFlarePlayerController* PC)
{
	FLOGV("AFlareGame::LoadGame : loading from slot %d", CurrentSaveIndex);
	PlayerController = PC;

	UFlareSaveGame* Save = ReadSaveSlot(CurrentSaveIndex);

	// Load from save
	if (PC && Save)
	{
		PC->SetCompanyDescription(Save->PlayerCompanyDescription);

        // Create the new world
        World = NewObject<UFlareWorld>(this, UFlareWorld::StaticClass());

		FLOGV("AFlareGame::LoadGame date=%lld", Save->WorldData.Date);

        World->Load(Save->WorldData);
		CurrentImmatriculationIndex = Save->CurrentImmatriculationIndex;
		
        // TODO check if load is ok for ship event before the PC load

		// Load the player
		PC->Load(Save->PlayerData);
		PC->GetCompany()->SetupEmblem();

		// Init the quest manager
		QuestManager = NewObject<UFlareQuestManager>(this, UFlareQuestManager::StaticClass());
		QuestManager->Load(Save->PlayerData.QuestData);

		LoadedOrCreated = true;
		PC->OnLoadComplete();
		return true;
	}

	// No file existing
	else
	{
		FLOGV("AFlareGame::LoadWorld : could lot load slot %d", CurrentSaveIndex);
		return false;
	}
}

bool AFlareGame::SaveGame(AFlarePlayerController* PC)
{
	if (!IsLoadedOrCreated())
	{
		FLOG("AFlareGame::SaveGame : no game loaded, aborting");
		return false;
	}

	FLOGV("AFlareGame::SaveGame : saving to slot %d", CurrentSaveIndex);
	UFlareSaveGame* Save = Cast<UFlareSaveGame>(UGameplayStatics::CreateSaveGameObject(UFlareSaveGame::StaticClass()));

	// Save process
	if (PC && Save)
	{
		// Save the player
		PC->Save(Save->PlayerData, Save->PlayerCompanyDescription);
		Save->WorldData = *World->Save(ActiveSector);
		Save->CurrentImmatriculationIndex = CurrentImmatriculationIndex;
		Save->PlayerData.QuestData = *QuestManager->Save();

		FLOGV("AFlareGame::SaveGame date=%lld", Save->WorldData.Date);
		// Save
		UGameplayStatics::SaveGameToSlot(Save, "SaveSlot" + FString::FromInt(CurrentSaveIndex), 0);
		return true;
	}

	// No PC
	else
	{
		FLOG("AFlareGame::SaveGame failed");
		return false;
	}
}

void AFlareGame::UnloadGame()
{
	FLOG("AFlareGame::UnloadGame");

	if (ActiveSector)
	{
		ActiveSector->Destroy();
		ActiveSector = NULL;
	}
	World = NULL;
	LoadedOrCreated = false;
}




/*----------------------------------------------------
	Immatriculations
----------------------------------------------------*/

void AFlareGame::Immatriculate(UFlareCompany* Company, FName TargetClass, FFlareSpacecraftSave* SpacecraftSave)
{
	FString Immatriculation;
	FString NickName;
	CurrentImmatriculationIndex++;
	FFlareSpacecraftDescription* SpacecraftDesc = SpacecraftCatalog->Get(TargetClass);
	bool IsStation = IFlareSpacecraftInterface::IsStation(SpacecraftDesc);

	// Company name
	Immatriculation += Company->GetShortName().ToString();
	Immatriculation += "-";

	// Class
	Immatriculation += SpacecraftDesc->ImmatriculationCode.ToString();

	// Name
	if (SpacecraftDesc->Size == EFlarePartSize::L && !IsStation)
	{
		NickName = PickCapitalShipName().ToString();
	}
	else
	{
		NickName = FString::Printf(TEXT("%04d"), CurrentImmatriculationIndex);
	}
	Immatriculation += FString::Printf(TEXT("-%s"), *NickName);

	// Update data
	FLOGV("AFlareGame::Immatriculate (%s) : %s", *TargetClass.ToString(), *Immatriculation);
	SpacecraftSave->Immatriculation = FName(*Immatriculation);
	SpacecraftSave->NickName = FName(*NickName);
}

// convertToRoman:
//   In:  val: value to convert.
//        res: buffer to hold result.
//   Out: n/a
//   Cav: caller responsible for buffer size.

static FString ConvertToRoman(unsigned int val)
{
	FString Roman;

	const char *huns[] = {"", "C", "CC", "CCC", "CD", "D", "DC", "DCC", "DCCC", "CM"};
	const char *tens[] = {"", "X", "XX", "XXX", "XL", "L", "LX", "LXX", "LXXX", "XC"};
	const char *ones[] = {"", "I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX"};
	int size []  = { 0,   1,    2,    3,     2,   1,    2,     3,      4,    2};

	//  Add 'M' until we drop below 1000.
	while (val >= 1000) {
		Roman += FString("M");
		val -= 1000;
	}

	// Add each of the correct elements, adjusting as we go.

	Roman += FString(huns[val/100]);
	val = val % 100;
	Roman += FString(tens[val/10]);
	val = val % 10;
	Roman += FString(ones[val]);
	return Roman;
}

FName AFlareGame::PickCapitalShipName()
{
	if (BaseImmatriculationNameList.Num() == 0)
	{
		InitCapitalShipNameDatabase();
	}
	int32 PickIndex = FMath::RandRange(0,BaseImmatriculationNameList.Num()-1);

	FName BaseName = BaseImmatriculationNameList[PickIndex];

	// Check unicity
	bool Unique;
	int32 NameIncrement = 1;
	FName CandidateName;
	do
	{
		Unique = true;
		FString Suffix;
		if (NameIncrement > 1)
		{
			FString Roman = ConvertToRoman(NameIncrement);
			Suffix = FString("-") + Roman;
		}
		else
		{
			Suffix = FString("");
		}

		CandidateName = FName(*(BaseName.ToString()+Suffix));

		// Browse all existing ships the check if the name is unique
		// TODO check ship in travel (not in a sector with travels will be implemented)
		for (int SectorIndex = 0; SectorIndex < World->GetSectors().Num(); SectorIndex++)
		{
			UFlareSimulatedSector* Sector = World->GetSectors()[SectorIndex];

			for (int ShipIndex = 0; ShipIndex < Sector->GetSectorShips().Num(); ShipIndex++)
			{
				UFlareSimulatedSpacecraft* SpacecraftCandidate = Sector->GetSectorShips()[ShipIndex];
				if (SpacecraftCandidate && SpacecraftCandidate->GetNickName() == CandidateName)
				{
					FLOGV("Not unique %s", *CandidateName.ToString());
					Unique = false;
					break;
				}
			}
		}
		NameIncrement++;
	} while(!Unique);

	FLOGV("OK for %s", *CandidateName.ToString());

	return CandidateName;
}

void AFlareGame::InitCapitalShipNameDatabase()
{
	BaseImmatriculationNameList.Empty();
	BaseImmatriculationNameList.Add("Revenge");
	BaseImmatriculationNameList.Add("Sovereign");
	BaseImmatriculationNameList.Add("Stalker");
	BaseImmatriculationNameList.Add("Leviathan");
	BaseImmatriculationNameList.Add("Resolve");
	BaseImmatriculationNameList.Add("Explorer");
	BaseImmatriculationNameList.Add("Arrow");
	BaseImmatriculationNameList.Add("Intruder");
	BaseImmatriculationNameList.Add("Goliath");
	BaseImmatriculationNameList.Add("Shrike");
	BaseImmatriculationNameList.Add("Thunder");
	BaseImmatriculationNameList.Add("Enterprise");
	BaseImmatriculationNameList.Add("Sahara");
}


/*----------------------------------------------------
	Getters
----------------------------------------------------*/

inline const FFlareCompanyDescription* AFlareGame::GetPlayerCompanyDescription() const
{
	AFlarePlayerController* PC = Cast<AFlarePlayerController>(GetWorld()->GetFirstPlayerController());
	return PC->GetCompanyDescription();
}


#undef LOCTEXT_NAMESPACE