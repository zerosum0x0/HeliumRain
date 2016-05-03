#include "../Flare.h"
#include "FlareSectorInterface.h"
#include "FlareSimulatedSector.h"
#include "../Spacecrafts/FlareSpacecraftInterface.h"
#include "../Economy/FlareCargoBay.h"
#include "../Game/FlareGame.h"

#define LOCTEXT_NAMESPACE "FlareSectorInterface"


/*----------------------------------------------------
	Constructor
----------------------------------------------------*/

UFlareSectorInterface::UFlareSectorInterface(const class FObjectInitializer& PCIP)
	: Super(PCIP)
{
}

void UFlareSectorInterface::LoadResourcePrices()
{
	ResourcePrices.Empty();
	for (int PriceIndex = 0; PriceIndex < SectorData.ResourcePrices.Num(); PriceIndex++)
	{
		FFFlareResourcePrice* ResourcePrice = &SectorData.ResourcePrices[PriceIndex];
		FFlareResourceDescription* Resource = Game->GetResourceCatalog()->Get(ResourcePrice->ResourceIdentifier);
		float Price = ResourcePrice->Price;
		ResourcePrices.Add(Resource, Price);
	}
}

void UFlareSectorInterface::SaveResourcePrices()
{
	SectorData.ResourcePrices.Empty();

	for(int32 ResourceIndex = 0; ResourceIndex < Game->GetResourceCatalog()->Resources.Num(); ResourceIndex++)
	{
		FFlareResourceDescription* Resource = &Game->GetResourceCatalog()->Resources[ResourceIndex]->Data;
		if (ResourcePrices.Contains(Resource))
		{
			FFFlareResourcePrice Price;
			Price.ResourceIdentifier = Resource->Identifier;
			Price.Price = ResourcePrices[Resource];
			SectorData.ResourcePrices.Add(Price);
		}
	}
}

FText UFlareSectorInterface::GetSectorName()
{
	if (SectorData.GivenName.ToString().Len())
	{
		return SectorData.GivenName;
	}
	else if (SectorDescription->Name.ToString().Len())
	{
		return SectorDescription->Name;
	}
	else
	{
		return FText::FromString(GetSectorCode());
	}
}

FString UFlareSectorInterface::GetSectorCode()
{
	// TODO cache ?
	return SectorOrbitParameters.CelestialBodyIdentifier.ToString() + "-" + FString::FromInt(SectorOrbitParameters.Altitude) + "-" + FString::FromInt(SectorOrbitParameters.Phase);
}


EFlareSectorFriendlyness::Type UFlareSectorInterface::GetSectorFriendlyness(UFlareCompany* Company)
{
	if (!Company->HasVisitedSector(GetSimulatedSector()))
	{
		return EFlareSectorFriendlyness::NotVisited;
	}

	if (GetSimulatedSector()->GetSectorSpacecrafts().Num() == 0)
	{
		return EFlareSectorFriendlyness::Neutral;
	}

	int HostileSpacecraftCount = 0;
	int NeutralSpacecraftCount = 0;
	int FriendlySpacecraftCount = 0;

	for (int SpacecraftIndex = 0 ; SpacecraftIndex < GetSimulatedSector()->GetSectorSpacecrafts().Num(); SpacecraftIndex++)
	{
		UFlareCompany* OtherCompany = GetSimulatedSector()->GetSectorSpacecrafts()[SpacecraftIndex]->GetCompany();

		if (OtherCompany == Company)
		{
			FriendlySpacecraftCount++;
		}
		else if (OtherCompany->GetWarState(Company) == EFlareHostility::Hostile)
		{
			HostileSpacecraftCount++;
		}
		else
		{
			NeutralSpacecraftCount++;
		}
	}

	if (FriendlySpacecraftCount > 0 && HostileSpacecraftCount > 0)
	{
		return EFlareSectorFriendlyness::Contested;
	}

	if (FriendlySpacecraftCount > 0)
	{
		return EFlareSectorFriendlyness::Friendly;
	}
	else if (HostileSpacecraftCount > 0)
	{
		return EFlareSectorFriendlyness::Hostile;
	}
	else
	{
		return EFlareSectorFriendlyness::Neutral;
	}
}

EFlareSectorBattleState::Type UFlareSectorInterface::GetSectorBattleState(UFlareCompany* Company)
{

	if (GetSectorShipInterfaces().Num() == 0)
	{
		return EFlareSectorBattleState::NoBattle;
	}

	int HostileSpacecraftCount = 0;
	int DangerousHostileSpacecraftCount = 0;


	int FriendlySpacecraftCount = 0;
	int DangerousFriendlySpacecraftCount = 0;
	int CrippledFriendlySpacecraftCount = 0;

	for (int SpacecraftIndex = 0 ; SpacecraftIndex < GetSectorShipInterfaces().Num(); SpacecraftIndex++)
	{

		IFlareSpacecraftInterface* Spacecraft = GetSectorShipInterfaces()[SpacecraftIndex];

		UFlareCompany* OtherCompany = Spacecraft->GetCompany();

		if (!Spacecraft->GetDamageSystem()->IsAlive())
		{
			continue;
		}

		if (OtherCompany == Company)
		{
			FriendlySpacecraftCount++;
			if (Spacecraft->GetDamageSystem()->GetSubsystemHealth(EFlareSubsystem::SYS_Weapon) > 0)
			{
				DangerousFriendlySpacecraftCount++;
			}

			if (Spacecraft->GetDamageSystem()->GetSubsystemHealth(EFlareSubsystem::SYS_Propulsion) == 0)
			{
				CrippledFriendlySpacecraftCount++;
			}
		}
		else if (OtherCompany->GetWarState(Company) == EFlareHostility::Hostile)
		{
			HostileSpacecraftCount++;
			if (Spacecraft->GetDamageSystem()->GetSubsystemHealth(EFlareSubsystem::SYS_Weapon) > 0)
			{
				DangerousHostileSpacecraftCount++;
			}
		}
	}

	// No friendly or no hostile ship
	if (FriendlySpacecraftCount == 0 || HostileSpacecraftCount == 0)
	{
		return EFlareSectorBattleState::NoBattle;
	}

	// No friendly and hostile ship are not dangerous
	if (DangerousFriendlySpacecraftCount == 0 && DangerousHostileSpacecraftCount == 0)
	{
		return EFlareSectorBattleState::NoBattle;
	}

	// No friendly dangerous ship so the enemy have one. Battle is lost
	if (DangerousFriendlySpacecraftCount == 0)
	{
		if (CrippledFriendlySpacecraftCount == FriendlySpacecraftCount)
		{
			return EFlareSectorBattleState::BattleLostNoRetreat;
		}
		else
		{
			return EFlareSectorBattleState::BattleLost;
		}
	}

	if (DangerousHostileSpacecraftCount == 0)
	{
		return EFlareSectorBattleState::BattleWon;
	}

	if (CrippledFriendlySpacecraftCount == FriendlySpacecraftCount)
	{
		return EFlareSectorBattleState::BattleNoRetreat;
	}
	else
	{
		return EFlareSectorBattleState::Battle;
	}
}

FText UFlareSectorInterface::GetSectorFriendlynessText(UFlareCompany* Company)
{
	FText Status;

	switch (GetSectorFriendlyness(Company))
	{
		case EFlareSectorFriendlyness::NotVisited:
			Status = LOCTEXT("Unknown", "UNKNOWN");
			break;
		case EFlareSectorFriendlyness::Neutral:
			Status = LOCTEXT("Neutral", "NEUTRAL");
			break;
		case EFlareSectorFriendlyness::Friendly:
			Status = LOCTEXT("Friendly", "FRIENDLY");
			break;
		case EFlareSectorFriendlyness::Contested:
			Status = LOCTEXT("Contested", "CONTESTED");
			break;
		case EFlareSectorFriendlyness::Hostile:
			Status = LOCTEXT("Hostile", "HOSTILE");
			break;
	}

	return Status;
}

FLinearColor UFlareSectorInterface::GetSectorFriendlynessColor(UFlareCompany* Company)
{
	FLinearColor Color;
	const FFlareStyleCatalog& Theme = FFlareStyleSet::GetDefaultTheme();

	switch (GetSectorFriendlyness(Company))
	{
		case EFlareSectorFriendlyness::NotVisited:
			Color = Theme.UnknownColor;
			break;
		case EFlareSectorFriendlyness::Neutral:
			Color = Theme.NeutralColor;
			break;
		case EFlareSectorFriendlyness::Friendly:
			Color = Theme.FriendlyColor;
			break;
		case EFlareSectorFriendlyness::Contested:
			Color = Theme.DisputedColor;
			break;
		case EFlareSectorFriendlyness::Hostile:
			Color = Theme.EnemyColor;
			break;
	}

	return Color;
}


float UFlareSectorInterface::GetPreciseResourcePrice(FFlareResourceDescription* Resource)
{
	if (!ResourcePrices.Contains(Resource))
	{
		ResourcePrices.Add(Resource, GetDefaultResourcePrice(Resource));
	}

	return ResourcePrices[Resource];
}

void UFlareSectorInterface::SetPreciseResourcePrice(FFlareResourceDescription* Resource, float NewPrice)
{
	ResourcePrices[Resource] = NewPrice;
}

uint64 UFlareSectorInterface::GetResourcePrice(FFlareResourceDescription* Resource, EFlareResourcePriceContext::Type PriceContext)
{
	float DefaultPrice = GetPreciseResourcePrice(Resource);

	switch(PriceContext)
	{
		case EFlareResourcePriceContext::Default:
			return DefaultPrice;
		break;
		case EFlareResourcePriceContext::FactoryOutput:
			return DefaultPrice * 0.99f;
		break;
		case EFlareResourcePriceContext::FactoryInput:
			return DefaultPrice * 1.01f;
		break;
		case EFlareResourcePriceContext::ConsumerConsumption:
			return DefaultPrice * 2.f;
		break;
		default:
			return 0;
			break;
	}
}

float UFlareSectorInterface::GetDefaultResourcePrice(FFlareResourceDescription* Resource)
{
	// DEBUGInflation
	float Margin = 1.2;

	// Base
	static float FuelPrice = 1500 * Margin;

	// Raw
	static float H2Price = ((FuelPrice * 10 + 10000) / 40.) * Margin;
	static float FeoPrice = ((FuelPrice * 10 + 10000) / 10.)* Margin;
	static float Ch4Price = ((FuelPrice * 10 + 10000) / 20.) * Margin;
	static float Sio2Price = ((FuelPrice * 10 + 10000) / 10.) * Margin;
	static float He3Price = ((FuelPrice * 10 + 10000) / 10.) * Margin;
	static float H2oPrice = ((FuelPrice * 10 + 10000) / 50.) * Margin;


	// Product
	static float SteelPrice = ((20 * FeoPrice + 40 * H2oPrice + 10 * FuelPrice + 10000)) / 10. * Margin;
	static float CPrice = ((10 * Ch4Price + 10 * FuelPrice + 10000)) / 10.0 * Margin;
	static float PlasticPrice = ((10 * Ch4Price + 10 * FuelPrice + 10000)) / 10.0 * Margin;
	static float FSPrice = ((10 * SteelPrice + 20* PlasticPrice + 10 * FuelPrice + 10000)) / 10.0 * Margin;
	static float FoodPrice = ((10 * CPrice + 10 * H2oPrice + 10 * FuelPrice + 10000)) / 10.0 * Margin;
	static float ToolsPrice = ((10 * SteelPrice + 10 * PlasticPrice + 10000)) / 10.0 * Margin;
	static float TechPrice = ((20 * Sio2Price+ 40 * H2Price + 50 * FuelPrice + 50000)) / 10.0 * Margin;



	// TODO better
	if (Resource->Identifier == "h2")
	{
		return H2Price;
	}
	else if (Resource->Identifier == "feo")
	{
		return FeoPrice;
	}
	else if (Resource->Identifier == "ch4")
	{
		return Ch4Price;
	}
	else if (Resource->Identifier == "sio2")
	{
		return Sio2Price;
	}
	else if (Resource->Identifier == "he3")
	{
		return He3Price;
	}
	else if (Resource->Identifier == "h2o")
	{
		return H2oPrice;
	}
	else if (Resource->Identifier == "steel")
	{
		return SteelPrice;
	}
	else if (Resource->Identifier == "c")
	{
		return CPrice;
	}
	else if (Resource->Identifier == "plastics")
	{
		return PlasticPrice;
	}
	else if (Resource->Identifier == "fleet-supply")
	{
		return FSPrice;
	}
	else if (Resource->Identifier == "food")
	{
		return FoodPrice;
	}
	else if (Resource->Identifier == "fuel")
	{
		return FuelPrice;
	}
	else if (Resource->Identifier == "tools")
	{
		return ToolsPrice;
	}
	else if (Resource->Identifier == "tech")
	{
		return TechPrice;

	}
	else
	{
		FLOGV("Unknown resource %s", *Resource->Identifier.ToString());
		return 0;
	}


}

uint32 UFlareSectorInterface::GetTransfertResourcePrice(IFlareSpacecraftInterface* SourceSpacecraft, IFlareSpacecraftInterface* DestinationSpacecraft, FFlareResourceDescription* Resource)
{
	IFlareSpacecraftInterface* Station = NULL;
	if (SourceSpacecraft->IsStation())
	{
		Station = SourceSpacecraft;
	}
	else if (DestinationSpacecraft->IsStation())
	{
		Station = DestinationSpacecraft;
	}
	else
	{
		// Both are ships
		return GetResourcePrice(Resource, EFlareResourcePriceContext::Default);
	}

	FFlareSpacecraftDescription* SpacecraftDescription =  Station->GetDescription();

	// Load factories
	for (int FactoryIndex = 0; FactoryIndex < SpacecraftDescription->Factories.Num(); FactoryIndex++)
	{
		FFlareFactoryDescription* FactoryDescription = &SpacecraftDescription->Factories[FactoryIndex]->Data;

		for (int32 ResourceIndex = 0 ; ResourceIndex < FactoryDescription->CycleCost.InputResources.Num() ; ResourceIndex++)
		{
			const FFlareFactoryResource* FactoryResource = &FactoryDescription->CycleCost.InputResources[ResourceIndex];
			if(&FactoryResource->Resource->Data == Resource)
			{
				// Is input resource of a station
				return GetResourcePrice(Resource, EFlareResourcePriceContext::FactoryInput);
			}
		}

		for (int32 ResourceIndex = 0 ; ResourceIndex < FactoryDescription->CycleCost.OutputResources.Num() ; ResourceIndex++)
		{
			const FFlareFactoryResource* FactoryResource = &FactoryDescription->CycleCost.OutputResources[ResourceIndex];
			if(&FactoryResource->Resource->Data == Resource)
			{
				// Is output resource of a station
				return GetResourcePrice(Resource, EFlareResourcePriceContext::FactoryOutput);
			}
		}	
	}

	if (SpacecraftDescription->Capabilities.Contains(EFlareSpacecraftCapability::Consumer) && Game->GetResourceCatalog()->IsCustomerResource(Resource))
	{
		// Customer resource
		return GetResourcePrice(Resource, EFlareResourcePriceContext::FactoryInput);
	}

	if (SpacecraftDescription->Capabilities.Contains(EFlareSpacecraftCapability::Maintenance) && Game->GetResourceCatalog()->IsMaintenanceResource(Resource))
	{
		// Maintenance resource
		return GetResourcePrice(Resource, EFlareResourcePriceContext::FactoryInput);
	}

	return GetResourcePrice(Resource, EFlareResourcePriceContext::Default);
}


uint32 UFlareSectorInterface::TransfertResources(IFlareSpacecraftInterface* SourceSpacecraft, IFlareSpacecraftInterface* DestinationSpacecraft, FFlareResourceDescription* Resource, uint32 Quantity)
{
	// TODO Check docking capabilities

	if(SourceSpacecraft->GetCurrentSectorInterface() != DestinationSpacecraft->GetCurrentSectorInterface())
	{
		FLOG("Warning cannot transfert resource because both ship are not in the same sector");
		return 0;
	}

	if(SourceSpacecraft->IsStation() && DestinationSpacecraft->IsStation())
	{
		FLOG("Warning cannot transfert resource between 2 stations");
		return 0;
	}

	uint32 ResourcePrice = GetTransfertResourcePrice(SourceSpacecraft, DestinationSpacecraft, Resource);
	uint32 QuantityToTake = Quantity;

	if (SourceSpacecraft->GetCompany() != DestinationSpacecraft->GetCompany())
	{
		// Limit transaction bay available money
		uint32 MaxAffordableQuantity = DestinationSpacecraft->GetCompany()->GetMoney() / ResourcePrice;
		QuantityToTake = FMath::Min(QuantityToTake, MaxAffordableQuantity);
	}
	uint32 ResourceCapacity = DestinationSpacecraft->GetCargoBay()->GetFreeSpaceForResource(Resource);

	QuantityToTake = FMath::Min(QuantityToTake, ResourceCapacity);


	uint32 TakenResources = SourceSpacecraft->GetCargoBay()->TakeResources(Resource, QuantityToTake);
	uint32 GivenResources = DestinationSpacecraft->GetCargoBay()->GiveResources(Resource, TakenResources);

	if (GivenResources > 0 && SourceSpacecraft->GetCompany() != DestinationSpacecraft->GetCompany())
	{
		// Pay
		uint32 Price = ResourcePrice * GivenResources;
		DestinationSpacecraft->GetCompany()->TakeMoney(Price);
		SourceSpacecraft->GetCompany()->GiveMoney(Price);

		SourceSpacecraft->GetCompany()->GiveReputation(DestinationSpacecraft->GetCompany(), 0.5f, true);
		DestinationSpacecraft->GetCompany()->GiveReputation(SourceSpacecraft->GetCompany(), 0.5f, true);
	}

	return GivenResources;
}

bool UFlareSectorInterface::CanUpgrade(UFlareCompany* Company)
{
	EFlareSectorBattleState::Type BattleState = GetSectorBattleState(Company);
	if(BattleState != EFlareSectorBattleState::NoBattle
			&& BattleState != EFlareSectorBattleState::BattleWon)
	{
		return false;
	}

	for(int StationIndex = 0 ; StationIndex < GetSectorStationInterfaces().Num(); StationIndex ++ )
	{
		IFlareSpacecraftInterface* StationInterface = GetSectorStationInterfaces()[StationIndex];
		if (StationInterface->GetCompany()->GetWarState(Company) != EFlareHostility::Hostile)
		{
			return true;
		}
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
