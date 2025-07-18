#pragma once

#include "reboot.h"
#include "GameplayStatics.h"
#include "FortLootPackage.h"
#include "FortPickup.h"
#include "BuildingGameplayActor.h"
#include "KismetSystemLibrary.h"

static inline void SpawnBGAs() // hahah not "proper", there's a function that we can hook and it gets called on each spawner whenever playlist gets set, but it's fine.
{
	static auto BGAConsumableSpawnerClass = FindObject<UClass>(L"/Script/FortniteGame.BGAConsumableSpawner");

	if (!BGAConsumableSpawnerClass)
		return;

	auto GameState = Cast<AFortGameStateAthena>(GetWorld()->GetGameState());
	auto MapInfo = GameState->GetMapInfo();

	auto AllBGAConsumableSpawners = UGameplayStatics::GetAllActorsOfClass(GetWorld(), BGAConsumableSpawnerClass);

	LOG_INFO(LogDev, "AllBGAConsumableSpawners.Num(): {}", (int)AllBGAConsumableSpawners.Num());

	for (int i = 0; i < AllBGAConsumableSpawners.Num(); ++i)
	{
		auto BGAConsumableSpawner = AllBGAConsumableSpawners.at(i);
		auto SpawnLocation = BGAConsumableSpawner->GetActorLocation();
		
		static auto bAlignSpawnedActorsToSurfaceOffset = BGAConsumableSpawner->GetOffset("bAlignSpawnedActorsToSurface");
		const bool bAlignSpawnedActorsToSurface = BGAConsumableSpawner->Get<bool>(bAlignSpawnedActorsToSurfaceOffset);

		FTransform SpawnTransform{};
		SpawnTransform.Translation = SpawnLocation;
		SpawnTransform.Scale3D = FVector{ 1, 1, 1 };
		SpawnTransform.Rotation = FQuat();

		if (FBuildingGameplayActorSpawnDetails::GetStruct())
		{
			// todo handle?
		}

		bool bDeferConstruction = true; // hm?

		static auto SpawnLootTierGroupOffset = BGAConsumableSpawner->GetOffset("SpawnLootTierGroup");
		auto& SpawnLootTierGroup = BGAConsumableSpawner->Get<FName>(SpawnLootTierGroupOffset);

		std::vector<LootDrop> LootDrops;

		if (SpawnLootTierGroup.ToString() == "Loot_ForagedItem_GravityRocks") // on 4.0 atleast, LootPackageCategoryMinArray contains all 0s
		{
			LootDrop GravityRockDrop{};
			auto Class = FindObject<UClass>("/Script/FortniteGame.BGAConsumableWrapperItemDefinition");
			auto LowGravItemDef = LoadObject<UFortItemDefinition>(L"/Game/Athena/Items/ForagedItems/LowGravity/Athena_Foraged_LowGravity.Athena_Foraged_LowGravity", Class);
			if (!LowGravItemDef)
			{
				LOG_ERROR(LogDev, "Failed to load LowGravity ItemDefinition!");
				continue;
			}
			GravityRockDrop.ItemEntry = FFortItemEntry::MakeItemEntry(LowGravItemDef);
			LootDrops.push_back(GravityRockDrop);

			bDeferConstruction = false;
		}
		else
		{
			LootDrops = PickLootDrops(SpawnLootTierGroup, GameState->GetWorldLevel());
		}

		LOG_INFO(LogDev, "LootDrops: {} SpawnLootTierGroup: {}", LootDrops.size(), SpawnLootTierGroup.ToString());

		for (int z = 0; z < LootDrops.size(); ++z)
		{
			auto& LootDrop = LootDrops.at(z);

			static auto ConsumableClassOffset = LootDrop->GetItemDefinition()->GetOffset("ConsumableClass");
			auto ConsumableClassSoft = LootDrop->GetItemDefinition()->GetPtr<TSoftObjectPtr<UClass>>(ConsumableClassOffset);

			static auto BlueprintGeneratedClassClass = FindObject<UClass>(L"/Script/Engine.BlueprintGeneratedClass");
			auto StrongConsumableClass = ConsumableClassSoft->Get(BlueprintGeneratedClassClass, true);

			if (!StrongConsumableClass)
			{
				LOG_INFO(LogDev, "Unable to get consumable class!");
				continue;
			}

			auto ConsumableActor = GetWorld()->SpawnActor<ABuildingGameplayActor>(StrongConsumableClass, SpawnTransform, CreateSpawnParameters(ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn, bDeferConstruction));

			if (ConsumableActor)
			{
				FTransform FinalSpawnTransform = SpawnTransform;

				if (bAlignSpawnedActorsToSurface)
				{
					// I DONT KNOW

					/* FHitResult* NewHit = Alloc<FHitResult>(FHitResult::GetStructSize());

					FVector StartLocation = FinalSpawnTransform.Translation;
					FVector EndLocation = StartLocation - FVector(0, 0, 1000);

					bool bTraceComplex = true; // idk
					FName ProfileName = UKismetStringLibrary::Conv_StringToName(L"FindGroundLocationAt");

					UKismetSystemLibrary::LineTraceSingleByProfile(ConsumableActor, StartLocation, EndLocation, ProfileName, bTraceComplex,
						TArray<AActor*>(), EDrawDebugTrace::None, &NewHit, true, FLinearColor(), FLinearColor(), 0);

					// UKismetSystemLibrary::LineTraceSingle(ConsumableActor, StartLocation, EndLocation,
						// ETraceTypeQuery::TraceTypeQuery1, bTraceComplex, TArray<AActor*>(), EDrawDebugTrace::None, true, FLinearColor(), FLinearColor(), 0, &NewHit);

					bool IsBlockingHit = NewHit && NewHit->IsBlockingHit(); // Should we check ret of linetracesingle?

					if (IsBlockingHit)
					{
						FinalSpawnTransform.Translation =  NewHit->GetLocation();
					}
					else
					{
						FinalSpawnTransform.Translation = FVector(0, 0, 0);
					}

					*/
				} 

				if (FinalSpawnTransform.Translation.CompareVectors(FVector(0, 0, 0)))
				{
					LOG_WARN(LogGame, "Invalid BGA spawn location!");
					// ConsumableActor->K2_DestroyActor(); // ??
					continue;
				}

				if (bDeferConstruction)
					UGameplayStatics::FinishSpawningActor(ConsumableActor, FinalSpawnTransform);

				ConsumableActor->ForceNetUpdate();
				ConsumableActor->SetNetDormancy(ENetDormancy::DORM_Awake);

				// ConsumableActor->InitializeBuildingActor(nullptr, nullptr, true); // idk UFortKismetLibrary::SpawnBuildingGameplayActor does this

				LOG_INFO(LogDev, "[{}/{}] Spawned BGA {} at {} {} {}", z + 1, LootDrops.size(), ConsumableActor->GetName(), FinalSpawnTransform.Translation.X, FinalSpawnTransform.Translation.Y, FinalSpawnTransform.Translation.Z);
				break; // ?
			}
		}
	}

	AllBGAConsumableSpawners.Free();

	LOG_INFO(LogDev, "Spawned BGAS!");
}