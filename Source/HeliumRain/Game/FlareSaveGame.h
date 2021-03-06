#pragma once

#include "FlareCompany.h"
#include "FlareWorld.h"
#include "../Quests/FlareQuestManager.h"

#include "FlareSaveGame.generated.h"

/** Player objective condition data */
USTRUCT()
struct FFlarePlayerObjectiveCondition
{
	GENERATED_USTRUCT_BODY()

	FText                       InitialLabel;
	FText						TerminalLabel;

	float                       Progress;
	float                       MaxProgress;
	int32                       Counter;
	int32                       MaxCounter;
};

/** Player objective target data */
USTRUCT()
struct FFlarePlayerObjectiveTarget
{
	GENERATED_USTRUCT_BODY()

	AActor*                     Actor;
	FVector                     Location;
	float						Radius;
	bool						Active;
};

/** Player objective data */
USTRUCT()
struct FFlarePlayerObjectiveData
{
	GENERATED_USTRUCT_BODY()

	FText                       Name;
	FText                       Description;

	int                         StepsDone;
	int                         StepsCount;

	TArray<FFlarePlayerObjectiveTarget> TargetList;
	TArray<FFlarePlayerObjectiveCondition> ConditionList;
};


/** Player objective */
USTRUCT()
struct FFlarePlayerObjective
{
	GENERATED_USTRUCT_BODY()

	bool                        Set;
	FFlarePlayerObjectiveData   Data;
	int32						Version;
};

/** Game save data */
USTRUCT()
struct FFlarePlayerSave
{
    GENERATED_USTRUCT_BODY()

	/** Unique identifier of the game */
	UPROPERTY(EditAnywhere, Category = Save)
	FName UUID;

	/** Chosen scenario */
	UPROPERTY(EditAnywhere, Category = Save)
	int32 ScenarioId;

    /** Identifier of the company */
    UPROPERTY(EditAnywhere, Category = Save)
    FName CompanyIdentifier;

	/** Identifier of the player fleet */
	UPROPERTY(EditAnywhere, Category = Save)
	FName PlayerFleetIdentifier;

	UPROPERTY(EditAnywhere, Category = Save)
	FFlareQuestSave		QuestData;

	/** Identifier of the last flown ship */
	UPROPERTY(EditAnywhere, Category = Save)
	FName LastFlownShipIdentifier;
};


UCLASS()
class UFlareSaveGame : public USaveGame
{
public:

	GENERATED_UCLASS_BODY()


	/*----------------------------------------------------
		Public data
	----------------------------------------------------*/

	UPROPERTY(VisibleAnywhere, Category = Save)
	FFlarePlayerSave PlayerData;

	UPROPERTY(VisibleAnywhere, Category = Save)
	FFlareCompanyDescription PlayerCompanyDescription;

    UPROPERTY(VisibleAnywhere, Category = Save)
    FFlareWorldSave WorldData;

	UPROPERTY(VisibleAnywhere, Category = Save)
	int32 CurrentImmatriculationIndex;

	UPROPERTY(VisibleAnywhere, Category = Save)
	int32 CurrentIdentifierIndex;
};

