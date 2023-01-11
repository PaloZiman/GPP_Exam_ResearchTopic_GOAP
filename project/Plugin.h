#pragma once
#include "IExamPlugin.h"
#include "Exam_HelperStructs.h"

class IBaseInterface;
class IExamInterface;

enum WorldState
{
	TargetInRange, FreeInventorySlot, Explore, IsItemToPickUp, FightEnemy, HealAnEat, LookAround, AvoidPurgeZone
};

struct Goal
{
	std::string Name;
	const std::function<bool()> IsValid;
	const WorldState requiredWorldState;
};

struct Action
{
	std::string Name;
	std::function<bool()> IsValid;
	std::function<void()> Perform;
	WorldState fullFillWorldState;
	std::vector<WorldState> requiredWorldState;
};



class Plugin :public IExamPlugin
{
public:
	Plugin() {};
	virtual ~Plugin() {};

	void Initialize(IBaseInterface* pInterface, PluginInfo& info) override;
	void DllInit() override;
	void DllShutdown() override;

	void InitGameDebugParams(GameDebugParams& params) override;
	void Update(float dt) override;

	SteeringPlugin_Output UpdateSteering(float dt) override;
	void Render(float dt) const override;

private:
	const std::vector<std::string> WorldStatesNames{ "TargetInRange", "FreeInventorySlot", "Explore", "IsItemToPickUp", "FightEnemy", "HealAnEat", "LookAround", "AvoidPurgeZone" };
	const bool GiveFeedback{ true };
	//Interface, used to request data from/perform actions with the AI Framework
	IExamInterface* m_pInterface = nullptr;
	SteeringPlugin_Output m_Steering;
	std::vector<HouseInfo> GetHousesInFOV() const;
	std::vector<EntityInfo> GetEntitiesInFOV() const;

	Elite::Vector2 m_Target = {};
	bool m_CanRun = false; //Demo purpose
	bool m_GrabItem = false; //Demo purpose
	bool m_UseItem = false; //Demo purpose
	bool m_RemoveItem = false; //Demo purpose
	float m_AngSpeed = 0.f; //Demo purpose

	UINT m_InventorySlot = 0;

	std::vector<Elite::Vector2> m_ExplorationPoints;
	std::vector<Elite::Vector2> m_ExplorationDirections{ Elite::Vector2(1.f,1.f),Elite::Vector2(1.f,-1.f),Elite::Vector2(-1.f,-1.f),Elite::Vector2(-1.,1.f) };
	int m_GoingToExplorationPoint{ 0 };

	void GoToPos(Elite::Vector2 pos);
	//GOAP
	//WorldStates variables
	Elite::Vector2 m_GoToPos;
	float m_GoToRange{};
	bool m_GoToFaceTarget{ false };
	float m_GoToMaxAngleDiff{ 1 };
	//WorldStates
	bool m_ShouldExplore{ false };
	bool m_PickUpItem{ false };
	bool m_ExploreHouse{ false };
	bool m_IsLookingAround{ false };
	float m_LookingAroundAngle{};
	bool m_IsAttacked{ false };
	void SetIsAttacked();
	std::vector<EnemyInfo> m_EnemysInFOV{};
	void SetEnemysInFOV();

	bool m_AlwaysTrue{ true };

	bool m_InPurgeZone{ false };
	PurgeZoneInfo m_PurgeZone;
	void SetInPurgeZone();

	bool m_ItemInRange{ false };
	void SetItemInRange();

	bool m_FreeItemSlot{ true };
	int m_FreeItemSlotAt{ 0 };
	bool GetFreeInventorySlot();

	std::list<ItemInfo> m_ItemsToLoot;
	void SetItemsToLoot();

	std::list<Elite::Vector2> m_HousePoints;
	std::list<Elite::Vector2> m_AroundHousePoints;
	std::list<HouseInfo> m_HousesToExplore;
	std::list<HouseInfo> m_HousesExplored;
	void ClearReachedExplorePositions();
	void SetHousesToExplore();
	//Goals
	Goal G_Exploring{"Exploring", [&]()->bool {return true; } ,Explore};
	Goal G_CollectItems{"CollectItems", [&]()->bool {return !m_ItemsToLoot.empty(); }, IsItemToPickUp};
	Goal G_FightEnemy{"FightEnemy", [&]()->bool {return !m_EnemysInFOV.empty(); },FightEnemy};
	Goal G_HealAndEat{ "HealAndEat",[&]()->bool {return true; } ,HealAnEat};
	Goal G_LookAround{ "LookAround",[&]()->bool {return m_IsAttacked || m_IsLookingAround; },LookAround};
	Goal G_AvoidPurgeZone{"AvoidPurgeZone", [&]()->bool {return m_InPurgeZone; } ,AvoidPurgeZone};

	bool FullFillWorldState(const WorldState ws);
	void UpdateWorldStateData();
	bool GetWorldState(WorldState ws);
	EntityInfo GetEntityAtPos(Elite::Vector2 pos);
	std::vector<Goal> m_Goals{ G_HealAndEat,G_AvoidPurgeZone ,G_FightEnemy,G_LookAround, G_CollectItems,G_Exploring };
	std::vector<Action> m_Actions;
	//help functinos
	bool ShouldExplore();
	float TotalEnergyLeft();
	float TotalHealthLeft();
	int TotalAmmoLeft();
	bool InHouse();
	Elite::Vector2 AgentOrientationVector2();
};

//ENTRY
//This is the first function that is called by the host program
//The plugin returned by this function is also the plugin used by the host program
extern "C"
{
	__declspec (dllexport) IPluginBase* Register()
	{
		return new Plugin();
	}
}