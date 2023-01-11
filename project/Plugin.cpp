#include "stdafx.h"
#include "Plugin.h"
#include "IExamInterface.h"

using namespace std;

//Called only once, during initialization
void Plugin::Initialize(IBaseInterface* pInterface, PluginInfo& info)
{
	//Retrieving the interface
	//This interface gives you access to certain actions the AI_Framework can perform for you
	m_pInterface = static_cast<IExamInterface*>(pInterface);

	//Bit information about the plugin
	//Please fill this in!!
	info.BotName = "MinionExam";
	info.Student_FirstName = "Palo";
	info.Student_LastName = "Ziman";
	info.Student_Class = "2DAE15";
	//Exploration
	for (int i{ 0 }; i < m_ExplorationDirections.size(); ++i)
	{
		m_ExplorationPoints.push_back(m_ExplorationDirections[i] * m_pInterface->Agent_GetInfo().FOV_Range);
	}
	Action A_ExploreWorld = {"ExploreWorld", [&]()->bool
		{
			m_GoToPos = m_ExplorationPoints[m_GoingToExplorationPoint];
			m_GoToRange = m_pInterface->Agent_GetInfo().FOV_Range;
			m_GoToFaceTarget = false;
			return true;
		},[&]()->void
	{},Explore,{TargetInRange} };

	Action A_ExploreAroundHouse = {"ExploreAroundHouse", [&]()->bool
		{
			if (!m_AroundHousePoints.empty())
			{
				m_GoToPos = m_AroundHousePoints.front();
				m_GoToRange = m_pInterface->Agent_GetInfo().FOV_Range;
				m_GoToFaceTarget = false;
				return true;
			}
			return false;
		},[&]()->void
	{},Explore,{TargetInRange} };

	Action A_ExploreHouse = {"ExploreHouse", [&]()->bool
		{
			if (!m_HousePoints.empty())
			{
				m_GoToPos = m_HousePoints.front();
				m_GoToRange = m_pInterface->Agent_GetInfo().FOV_Range;
				m_GoToFaceTarget = true;
				m_GoToMaxAngleDiff = m_pInterface->Agent_GetInfo().FOV_Angle / 2;
				return true;
			}
			return false;
		},[&]()->void
	{},Explore,{TargetInRange} };
	//TargetInRange
	Action A_GoToTarget = {"GoToTarget", [&]()->bool {return true; },[&]()
	{
		if (m_GoToRange >= 0 && m_GoToPos.Distance(m_pInterface->Agent_GetInfo().Position) > m_GoToRange)
		{
			m_Target = m_GoToPos;
		}
		else if (m_GoToRange < 0 && m_GoToPos.Distance(m_pInterface->Agent_GetInfo().Position) < fabs(m_GoToRange))
		{
			m_Target = (m_pInterface->Agent_GetInfo().Position - m_GoToPos).GetNormalized() * m_GoToRange;
		}
		if (m_GoToFaceTarget)
		{
			m_Steering.AutoOrient = false;
			const float goToAngle = Elite::AngleBetween(
				m_GoToPos - m_pInterface->Agent_GetInfo().Position,
				AgentOrientationVector2());
			if (goToAngle > 0)
				m_Steering.AngularVelocity = -m_pInterface->Agent_GetInfo().MaxAngularSpeed;
			else
				m_Steering.AngularVelocity = m_pInterface->Agent_GetInfo().MaxAngularSpeed;
		}
		else
			m_Steering.AutoOrient = true;
	},TargetInRange,{} };
	//PickUpItem
	Action A_PickUpItem = {"PickUpItem", [&]()->bool
		{
			m_GoToPos = m_ItemsToLoot.front().Location;
			m_GoToRange = m_pInterface->Agent_GetInfo().GrabRange;
			m_GoToFaceTarget = true;
			return true;
		},[&]()
	{
		ItemInfo item;
		if (m_pInterface->Item_Grab(GetEntityAtPos(m_ItemsToLoot.front().Location),item))
		{
			if (m_pInterface->Inventory_AddItem(m_FreeItemSlotAt, item))
			{
				if (item.Type == eItemType::GARBAGE)
					m_pInterface->Inventory_RemoveItem(m_FreeItemSlotAt);
				m_ItemsToLoot.pop_front();
			}
		}
	},IsItemToPickUp, {TargetInRange,FreeInventorySlot} };
	//FreeItemSlot
	Action A_FreeItemSlot = {"FreeInventorySlot" ,[&]()->bool {return true; },[&]()
	{
		std::vector<pair<ItemInfo,int>> items;
	for (int i{ 0 }; i < m_pInterface->Inventory_GetCapacity(); ++i)
	{
		ItemInfo item;
		if (m_pInterface->Inventory_GetItem(i, item))
		{
			for (auto vItems : items)
			{
				if (vItems.first.Type == item.Type)
				{
					if (vItems.first.Type == eItemType::FOOD)
					{
						if (m_pInterface->Food_GetEnergy(vItems.first) <= m_pInterface->Food_GetEnergy(item))
						{
							m_pInterface->Inventory_UseItem(vItems.second);
							m_pInterface->Inventory_RemoveItem(vItems.second);
							return;
						}
						else
						{
							m_pInterface->Inventory_UseItem(i);
							m_pInterface->Inventory_RemoveItem(i);
							return;
						}
					}
					if (vItems.first.Type == eItemType::MEDKIT)
					{
						if (m_pInterface->Medkit_GetHealth(vItems.first) <= m_pInterface->Medkit_GetHealth(item))
						{
							m_pInterface->Inventory_UseItem(vItems.second);
							m_pInterface->Inventory_RemoveItem(vItems.second);
							return;
						}
						else
						{
							m_pInterface->Inventory_UseItem(i);
							m_pInterface->Inventory_RemoveItem(i);
							return;
						}
					}
					if (vItems.first.Type == eItemType::PISTOL || vItems.first.Type == eItemType::SHOTGUN)
					{
						if (m_pInterface->Weapon_GetAmmo(vItems.first) <= m_pInterface->Weapon_GetAmmo(item))
						{
							m_pInterface->Inventory_RemoveItem(vItems.second);
							return;
						}
						else
						{
							m_pInterface->Inventory_RemoveItem(i);
						}
					}
				}
			}
			items.push_back(pair<ItemInfo,int>(item, i));
		}
		else return;
	}
	},FreeInventorySlot,{} };
	//FightEnemy
	Action A_FightEnemy = {"FightEnemy", [&]()->bool
	{

		m_GoToPos = m_EnemysInFOV.front().Location;
		m_GoToRange = -m_pInterface->Agent_GetInfo().FOV_Range - 2;
		m_GoToFaceTarget = true;
		m_GoToMaxAngleDiff = 0.05f;
		//Check has weapon
		for (int i{ 0 }; i < m_pInterface->Inventory_GetCapacity(); ++i)
		{
			ItemInfo item;
			if (m_pInterface->Inventory_GetItem(i, item))
				if (item.Type == eItemType::PISTOL || item.Type == eItemType::SHOTGUN)
					return true;
		}
		return false;
	},[&]()
	{
		//movement
		m_Steering.LinearVelocity = (m_pInterface->Agent_GetInfo().Position - m_EnemysInFOV[0].Location).GetNormalized() * m_pInterface->Agent_GetInfo().MaxLinearSpeed;
		//rotation
		float angleToEnemy = Elite::AngleBetween(
			m_EnemysInFOV.front().Location - m_pInterface->Agent_GetInfo().Position,
			AgentOrientationVector2());
		if (angleToEnemy > 0)
			m_Steering.AngularVelocity = -m_pInterface->Agent_GetInfo().MaxAngularSpeed;
		else
			m_Steering.AngularVelocity = m_pInterface->Agent_GetInfo().MaxAngularSpeed;
		//Shooting
		if (std::abs(Elite::AngleBetween(m_EnemysInFOV[0].Location - m_pInterface->Agent_GetInfo().Position, AgentOrientationVector2())) < 0.05f)
		{
			for (int i{ 0 }; i < m_pInterface->Inventory_GetCapacity(); ++i)
			{
				ItemInfo item;
				if (m_pInterface->Inventory_GetItem(i, item))
					if (item.Type == eItemType::PISTOL || item.Type == eItemType::SHOTGUN)
						if (m_pInterface->Inventory_UseItem(i))
						{
							return;
						}
						else
						{
							m_pInterface->Inventory_RemoveItem(i);
						}
			}
		}
	},FightEnemy,{} };
	//G_HealAndEat
	Action A_Heal = {"Heal", [&]()->bool
	{
		for (int i{ 0 }; i < m_pInterface->Inventory_GetCapacity(); ++i)
		{
			ItemInfo item;
			if (m_pInterface->Inventory_GetItem(i, item))
				if (item.Type == eItemType::FOOD)
				{
					if (m_pInterface->Food_GetEnergy(item) + m_pInterface->Agent_GetInfo().Energy <= 10.f)
						return true;
				}
		}
		return false;
	},[&]()
	{
		for (int i{ 0 }; i < m_pInterface->Inventory_GetCapacity(); ++i)
	{
		ItemInfo item;
		if (m_pInterface->Inventory_GetItem(i, item))
			if (item.Type == eItemType::FOOD)
			{
				if (m_pInterface->Food_GetEnergy(item) + m_pInterface->Agent_GetInfo().Energy <= 10.f)
				{
					m_pInterface->Inventory_UseItem(i);
					m_pInterface->Inventory_RemoveItem(i);
					return;
				}
			}
	}
	},HealAnEat,{} };
	Action A_Eat = {"Eat", [&]()->bool
	{
		for (int i{ 0 }; i < m_pInterface->Inventory_GetCapacity(); ++i)
		{
			ItemInfo item;
			if (m_pInterface->Inventory_GetItem(i, item))
				if (item.Type == eItemType::MEDKIT)
				{
					if (m_pInterface->Medkit_GetHealth(item) + m_pInterface->Agent_GetInfo().Health <= 10)
						return true;
				}
		}
		return false;
	},[&]()
	{
		if (m_pInterface->Agent_GetInfo().Health < 9)
		{
		for (int i{ 0 }; i < m_pInterface->Inventory_GetCapacity(); ++i)
		{
			ItemInfo item;
			if (m_pInterface->Inventory_GetItem(i, item))
				if (item.Type == eItemType::MEDKIT)
				{
					if (m_pInterface->Medkit_GetHealth(item) + m_pInterface->Agent_GetInfo().Health <= 10)
					{
						m_pInterface->Inventory_UseItem(i);
						m_pInterface->Inventory_RemoveItem(i);
						return;
					}
				}

		}
	}
	},HealAnEat,{} };
	//LookAround
	Action A_LookAround = {"LookAround", [&]()->bool {return true; },[&]()
	{
		m_Steering.AutoOrient = false;
		if (!m_IsLookingAround)
		{
			m_IsLookingAround = true;
			m_LookingAroundAngle = 0;
		}
		m_Steering.LinearVelocity = m_pInterface->Agent_GetInfo().LinearVelocity;
		m_Steering.AngularVelocity = m_pInterface->Agent_GetInfo().MaxAngularSpeed;
		m_LookingAroundAngle += m_pInterface->Agent_GetInfo().AngularVelocity;
		if (m_LookingAroundAngle > 135)
		{
			m_IsLookingAround = false;
			return true;
		}
		return false;
	},LookAround,{} };
	//Avoid PurgeZone
	Action A_AvoidPurgeZone = {"AvoidPurgeZone", [&]()->bool
	{
		return m_PurgeZone.Center.Distance(m_pInterface->Agent_GetInfo().Position) < m_PurgeZone.Radius + m_pInterface->Agent_GetInfo().FOV_Range;
	},[&]()
	{
		if (m_PurgeZone.Center.Distance(m_pInterface->Agent_GetInfo().Position) < m_PurgeZone.Radius + m_pInterface->Agent_GetInfo().FOV_Range)
		{
			m_Target = m_pInterface->Agent_GetInfo().Position + (m_pInterface->Agent_GetInfo().Position - m_PurgeZone.Center).GetNormalized() * (m_PurgeZone.Radius + m_pInterface->Agent_GetInfo().FOV_Range);
		}
	},AvoidPurgeZone,{} };
	//Add actions
	m_Actions.push_back(A_AvoidPurgeZone);
	m_Actions.push_back(A_LookAround);
	m_Actions.push_back(A_Heal);
	m_Actions.push_back(A_Eat);
	m_Actions.push_back(A_FightEnemy);
	m_Actions.push_back(A_PickUpItem);
	m_Actions.push_back(A_FreeItemSlot);
	m_Actions.push_back(A_ExploreHouse);
	m_Actions.push_back(A_ExploreAroundHouse);
	m_Actions.push_back(A_ExploreWorld);
	m_Actions.push_back(A_GoToTarget);

}

//Called only once
void Plugin::DllInit()
{
	//Called when the plugin is loaded
}

//Called only once
void Plugin::DllShutdown()
{
	//Called wheb the plugin gets unloaded
}

//Called only once, during initialization
void Plugin::InitGameDebugParams(GameDebugParams& params)
{
	params.AutoFollowCam = true; //Automatically follow the AI? (Default = true)
	params.RenderUI = true; //Render the IMGUI Panel? (Default = true)
	params.SpawnEnemies = true; //Do you want to spawn enemies? (Default = true)
	params.EnemyCount = 20; //How many enemies? (Default = 20)
	params.GodMode = false; //GodMode > You can't die, can be useful to inspect certain behaviors (Default = false)
	params.LevelFile = "GameLevel.gppl";
	params.AutoGrabClosestItem = true; //A call to Item_Grab(...) returns the closest item that can be grabbed. (EntityInfo argument is ignored)
	params.StartingDifficultyStage = 1;
	params.InfiniteStamina = false;
	params.SpawnDebugPistol = false;
	params.SpawnDebugShotgun = false;
	params.SpawnPurgeZonesOnMiddleClick = true;
	params.PrintDebugMessages = false;
	params.ShowDebugItemNames = true;
	params.Seed = 32;
}

//Only Active in DEBUG Mode
//(=Use only for Debug Purposes)
void Plugin::Update(float dt)
{
	//Demo Event Code
	//In the end your AI should be able to walk around without external input
	if (m_pInterface->Input_IsMouseButtonUp(Elite::InputMouseButton::eLeft))
	{
		//Update target based on input
		Elite::MouseData mouseData = m_pInterface->Input_GetMouseData(Elite::InputType::eMouseButton, Elite::InputMouseButton::eLeft);
		const Elite::Vector2 pos = Elite::Vector2(static_cast<float>(mouseData.X), static_cast<float>(mouseData.Y));
		m_Target = m_pInterface->Debug_ConvertScreenToWorld(pos);
	}
	else if (m_pInterface->Input_IsKeyboardKeyDown(Elite::eScancode_Space))
	{
		m_CanRun = true;
	}
	else if (m_pInterface->Input_IsKeyboardKeyDown(Elite::eScancode_Left))
	{
		m_AngSpeed -= Elite::ToRadians(10);
	}
	else if (m_pInterface->Input_IsKeyboardKeyDown(Elite::eScancode_Right))
	{
		m_AngSpeed += Elite::ToRadians(10);
	}
	else if (m_pInterface->Input_IsKeyboardKeyDown(Elite::eScancode_G))
	{
		m_GrabItem = true;
	}
	else if (m_pInterface->Input_IsKeyboardKeyDown(Elite::eScancode_U))
	{
		m_UseItem = true;
	}
	else if (m_pInterface->Input_IsKeyboardKeyDown(Elite::eScancode_R))
	{
		m_RemoveItem = true;
	}
	else if (m_pInterface->Input_IsKeyboardKeyUp(Elite::eScancode_Space))
	{
		m_CanRun = false;
	}
	else if (m_pInterface->Input_IsKeyboardKeyDown(Elite::eScancode_Delete))
	{
		m_pInterface->RequestShutdown();
	}
	else if (m_pInterface->Input_IsKeyboardKeyDown(Elite::eScancode_KP_Minus))
	{
		if (m_InventorySlot > 0)
			--m_InventorySlot;
	}
	else if (m_pInterface->Input_IsKeyboardKeyDown(Elite::eScancode_KP_Plus))
	{
		if (m_InventorySlot < 4)
			++m_InventorySlot;
	}
	else if (m_pInterface->Input_IsKeyboardKeyDown(Elite::eScancode_Q))
	{
		ItemInfo info = {};
		m_pInterface->Inventory_GetItem(m_InventorySlot, info);
		std::cout << (int)info.Type << std::endl;
	}
}

//Update
//This function calculates the new SteeringOutput, called once per frame
SteeringPlugin_Output Plugin::UpdateSteering(float dt)
{
	auto steering = SteeringPlugin_Output();

	//Use the Interface (IAssignmentInterface) to 'interface' with the AI_Framework
	auto agentInfo = m_pInterface->Agent_GetInfo();


	//Use the navmesh to calculate the next navmesh point
	//auto nextTargetPos = m_pInterface->NavMesh_GetClosestPathPoint(checkpointLocation);

	//OR, Use the mouse target
	//auto nextTargetPos = m_pInterface->NavMesh_GetClosestPathPoint(m_Target); //Uncomment this to use mouse position as guidance

	//INVENTORY USAGE DEMO
	//********************

	if (m_GrabItem)
	{
		ItemInfo item;
		//Item_Grab > When DebugParams.AutoGrabClosestItem is TRUE, the Item_Grab function returns the closest item in range
		//Keep in mind that DebugParams are only used for debugging purposes, by default this flag is FALSE
		//Otherwise, use GetEntitiesInFOV() to retrieve a vector of all entities in the FOV (EntityInfo)
		//Item_Grab gives you the ItemInfo back, based on the passed EntityHash (retrieved by GetEntitiesInFOV)
		if (m_pInterface->Item_Grab({}, item))
		{
			//Once grabbed, you can add it to a specific inventory slot
			//Slot must be empty
			m_pInterface->Inventory_AddItem(m_InventorySlot, item);
		}
	}

	if (m_UseItem)
	{
		//Use an item (make sure there is an item at the given inventory slot)
		m_pInterface->Inventory_UseItem(m_InventorySlot);
	}

	if (m_RemoveItem)
	{
		//Remove an item from a inventory slot
		m_pInterface->Inventory_RemoveItem(m_InventorySlot);
	}

	//Simple Seek Behaviour (towards Target)


	//steering.AngularVelocity = m_AngSpeed; //Rotate your character to inspect the world while walking
	steering.AutoOrient = true; //Setting AutoOrient to TRue overrides the AngularVelocity

	steering.RunMode = m_CanRun; //If RunMode is True > MaxLinSpd is increased for a limited time (till your stamina runs out)

	//SteeringPlugin_Output is works the exact same way a SteeringBehaviour output

//@End (Demo Purposes)
	m_GrabItem = false; //Reset State
	m_UseItem = false;
	m_RemoveItem = false;

	//GOAP
	if (GiveFeedback)std::cout << "----- START -----"<< std::endl;
	UpdateWorldStateData();
	for (auto goal : m_Goals)
	{
		if (GiveFeedback)std::cout << "G: " << goal.Name << std::endl;
		if (goal.IsValid())
		{
			if (GiveFeedback)std::cout << "Valid "<<std::endl;
			if (FullFillWorldState(goal.requiredWorldState))
				break;
		}
		if (GiveFeedback)std::cout << "Not Valid" << std::endl;
	}
	if (GiveFeedback)std::cout << "----- END -----" << std::endl;
	//Go to m_Target
	auto nextTargetPos = m_pInterface->NavMesh_GetClosestPathPoint(m_Target);
	m_Steering.LinearVelocity = nextTargetPos - agentInfo.Position; //Desired Velocity
	m_Steering.LinearVelocity.Normalize(); //Normalize Desired Velocity
	m_Steering.LinearVelocity *= agentInfo.MaxLinearSpeed; //Rescale to Max Speed

	if (Distance(nextTargetPos, agentInfo.Position) < 1.f)
	{
		m_Steering.LinearVelocity = Elite::ZeroVector2;
	}

	return m_Steering;
}

//This function should only be used for rendering debug elements
void Plugin::Render(float dt) const
{
	//This Render function should only contain calls to Interface->Draw_... functions
	m_pInterface->Draw_SolidCircle(m_Target, .7f, { 0,0 }, { 1, 0, 0 });
}

vector<HouseInfo> Plugin::GetHousesInFOV() const
{
	vector<HouseInfo> vHousesInFOV = {};

	HouseInfo hi = {};
	for (int i = 0;; ++i)
	{
		if (m_pInterface->Fov_GetHouseByIndex(i, hi))
		{
			vHousesInFOV.push_back(hi);
			continue;
		}

		break;
	}

	return vHousesInFOV;
}

vector<EntityInfo> Plugin::GetEntitiesInFOV() const
{
	vector<EntityInfo> vEntitiesInFOV = {};

	EntityInfo ei = {};
	for (int i = 0;; ++i)
	{
		if (m_pInterface->Fov_GetEntityByIndex(i, ei))
		{
			vEntitiesInFOV.push_back(ei);
			continue;
		}

		break;
	}

	return vEntitiesInFOV;
}

void Plugin::GoToPos(Elite::Vector2 pos)
{
	auto nextTargetPos = m_pInterface->NavMesh_GetClosestPathPoint(pos);
	m_Steering.LinearVelocity = nextTargetPos - m_pInterface->Agent_GetInfo().Position; //Desired Velocity
	m_Steering.LinearVelocity.Normalize(); //Normalize Desired Velocity
	m_Steering.LinearVelocity *= m_pInterface->Agent_GetInfo().MaxLinearSpeed; //Rescale to Max Speed
	if (Distance(nextTargetPos, m_pInterface->Agent_GetInfo().Position) < 1.f)
	{
		m_Steering.LinearVelocity = Elite::ZeroVector2;
	}
}

void Plugin::SetIsAttacked()
{
	m_IsAttacked = m_pInterface->Agent_GetInfo().Bitten;
}

void Plugin::SetEnemysInFOV()
{
	auto vEntitiesInFOV = GetEntitiesInFOV(); //uses m_pInterface->Fov_GetEntityByIndex(...)
	vector<EnemyInfo> enemys;
	for (auto& entity : vEntitiesInFOV)
	{

		if (entity.Type == eEntityType::ENEMY)
		{
			EnemyInfo enemy;
			if (m_pInterface->Enemy_GetInfo(entity, enemy))
				enemys.push_back(enemy);
		}
	}
	m_EnemysInFOV = enemys;
}

void Plugin::SetInPurgeZone()
{
	for (auto& e : GetEntitiesInFOV())
	{
		if (e.Type == eEntityType::PURGEZONE)
		{
			PurgeZoneInfo zoneInfo;
			m_pInterface->PurgeZone_GetInfo(e, zoneInfo);
			m_PurgeZone = zoneInfo;
			m_InPurgeZone = true;
			return;
		}
	}
	m_InPurgeZone = false;
}

void Plugin::SetItemInRange()
{
	ItemInfo item;
	m_ItemInRange = m_ItemsToLoot.front().Location.Distance(m_pInterface->Agent_GetInfo().Position) < m_pInterface->Agent_GetInfo().GrabRange && m_pInterface->Item_GetInfo(GetEntityAtPos(m_ItemsToLoot.front().Location), item);
}

bool Plugin::GetFreeInventorySlot()
{
	for (int i{ 0 }; i < m_pInterface->Inventory_GetCapacity(); ++i)
	{
		ItemInfo item;
		if (!m_pInterface->Inventory_GetItem(i, item))
		{
			m_FreeItemSlotAt = i;
			return true;
		}
	}
	m_FreeItemSlotAt = -1;
	return false;
}

void Plugin::SetItemsToLoot()
{
	auto vEntitiesInFOV = GetEntitiesInFOV();
	for (auto& entity : vEntitiesInFOV)
	{
		bool newEntity{ true };
		if (entity.Type == eEntityType::ITEM) {
			ItemInfo itemI;
			m_pInterface->Item_GetInfo(entity, itemI);
			for (auto& mE : m_ItemsToLoot)
			{
				if (mE.Location == entity.Location)
				{
					newEntity = false;
					break;
				}
			}
			if (newEntity)
				m_ItemsToLoot.push_back(itemI);
		}
	}
}

void Plugin::ClearReachedExplorePositions()
{
	auto posInFOV = [&](Elite::Vector2 pos)->bool
	{

		const float angle{ std::abs(Elite::AngleBetween(pos - m_pInterface->Agent_GetInfo().Position, AgentOrientationVector2())) };
		const bool inRange{ pos.Distance(m_pInterface->Agent_GetInfo().Position) < m_pInterface->Agent_GetInfo().FOV_Range };
		return inRange && angle < 0.5f && InHouse();
	};
	auto housePointsToCheckIterator = std::remove_if(m_HousePoints.begin(), m_HousePoints.end(), posInFOV);
	if (!m_HousePoints.empty())
	{
		m_HousePoints.erase(housePointsToCheckIterator, m_HousePoints.end());
		if (m_HousePoints.empty())
		{
			m_HousesExplored.push_back(m_HousesToExplore.front());
			m_HousesToExplore.pop_front();
			if (!m_HousesToExplore.empty())
			{
				m_HousePoints.push_back(Elite::Vector2(m_HousesToExplore.front().Center.x + (m_HousesToExplore.front().Size.x / 2 - m_pInterface->Agent_GetInfo().GrabRange), m_HousesToExplore.front().Center.y + (m_HousesToExplore.front().Size.y / 2 - m_pInterface->Agent_GetInfo().GrabRange)));
				m_HousePoints.push_back(Elite::Vector2(m_HousesToExplore.front().Center.x - (m_HousesToExplore.front().Size.x / 2 - m_pInterface->Agent_GetInfo().GrabRange), m_HousesToExplore.front().Center.y + (m_HousesToExplore.front().Size.y / 2 - m_pInterface->Agent_GetInfo().GrabRange)));
				m_HousePoints.push_back(Elite::Vector2(m_HousesToExplore.front().Center.x - (m_HousesToExplore.front().Size.x / 2 - m_pInterface->Agent_GetInfo().GrabRange), m_HousesToExplore.front().Center.y - (m_HousesToExplore.front().Size.y / 2 - m_pInterface->Agent_GetInfo().GrabRange)));
				m_HousePoints.push_back(Elite::Vector2(m_HousesToExplore.front().Center.x + (m_HousesToExplore.front().Size.x / 2 - m_pInterface->Agent_GetInfo().GrabRange), m_HousesToExplore.front().Center.y - (m_HousesToExplore.front().Size.y / 2 - m_pInterface->Agent_GetInfo().GrabRange)));
			}
		}
	}
	auto posInGrabRange = [&](Elite::Vector2 pos)->bool
	{
		return pos.Distance(m_pInterface->Agent_GetInfo().Position) < m_pInterface->Agent_GetInfo().GrabRange;
	};
	auto pointsToCheckIterator = std::remove_if(m_AroundHousePoints.begin(), m_AroundHousePoints.end(), posInGrabRange);
	m_AroundHousePoints.erase(pointsToCheckIterator, m_AroundHousePoints.end());

	if (m_pInterface->Agent_GetInfo().Position.Distance(m_ExplorationPoints[m_GoingToExplorationPoint]) < m_pInterface->Agent_GetInfo().FOV_Range)
	{
		m_ExplorationPoints[m_GoingToExplorationPoint] += m_ExplorationDirections[m_GoingToExplorationPoint] * m_pInterface->Agent_GetInfo().FOV_Range * 10;
		m_GoingToExplorationPoint++;
		m_GoingToExplorationPoint %= m_ExplorationDirections.size();
	}
}

bool Plugin::InHouse()
{
	const Elite::Vector2 playerPos = m_pInterface->Agent_GetInfo().Position;
	const HouseInfo house = m_HousesToExplore.front();
	return playerPos.x < house.Center.x + house.Size.x / 2 && playerPos.x > house.Center.x - house.Size.x / 2 && playerPos.y < house.Center.y + house.Size.y / 2 && playerPos.y > house.Center.y - house.Size.y;
}

void Plugin::SetHousesToExplore()
{
	auto vHousesInFOV = GetHousesInFOV();//uses m_pInterface->Fov_GetHouseByIndex(...)
	for (auto& house : vHousesInFOV)
	{
		bool newHouse{ true };
		for (auto& hte : m_HousesToExplore)
		{
			if (hte.Center == house.Center)
			{
				hte = house;
				newHouse = false;
				break;
			}

		}
		int counter{ 0 };
		std::list<HouseInfo>::iterator it;
		for (it = m_HousesExplored.begin(); it != m_HousesExplored.end(); ++it)
		{
			counter++;
			if (it->Center == house.Center)
			{
				newHouse = false;
				if (counter < m_HousesExplored.size() / 2)
				{
					m_HousesToExplore.push_back(*it);
					m_HousesExplored.erase(it);
				}
				break;
			}
		}
		if (newHouse)
		{
			m_HousesToExplore.push_back(house);
			m_AroundHousePoints.push_back(Elite::Vector2(house.Center.x + (house.Size.x / 2 + m_pInterface->Agent_GetInfo().GrabRange), house.Center.y - (house.Size.y / 2 + m_pInterface->Agent_GetInfo().GrabRange)));
			m_AroundHousePoints.push_back(Elite::Vector2(house.Center.x - (house.Size.x / 2 + m_pInterface->Agent_GetInfo().GrabRange), house.Center.y + (house.Size.y / 2 + m_pInterface->Agent_GetInfo().GrabRange)));

			m_HousePoints.push_back(Elite::Vector2(m_HousesToExplore.front().Center.x + (m_HousesToExplore.front().Size.x / 2 - m_pInterface->Agent_GetInfo().GrabRange), m_HousesToExplore.front().Center.y + (m_HousesToExplore.front().Size.y / 2 - m_pInterface->Agent_GetInfo().GrabRange)));
			m_HousePoints.push_back(Elite::Vector2(m_HousesToExplore.front().Center.x - (m_HousesToExplore.front().Size.x / 2 - m_pInterface->Agent_GetInfo().GrabRange), m_HousesToExplore.front().Center.y + (m_HousesToExplore.front().Size.y / 2 - m_pInterface->Agent_GetInfo().GrabRange)));
			m_HousePoints.push_back(Elite::Vector2(m_HousesToExplore.front().Center.x - (m_HousesToExplore.front().Size.x / 2 - m_pInterface->Agent_GetInfo().GrabRange), m_HousesToExplore.front().Center.y - (m_HousesToExplore.front().Size.y / 2 - m_pInterface->Agent_GetInfo().GrabRange)));
			m_HousePoints.push_back(Elite::Vector2(m_HousesToExplore.front().Center.x + (m_HousesToExplore.front().Size.x / 2 - m_pInterface->Agent_GetInfo().GrabRange), m_HousesToExplore.front().Center.y - (m_HousesToExplore.front().Size.y / 2 - m_pInterface->Agent_GetInfo().GrabRange)));

		}
	}
	if (m_HousesToExplore.empty() && m_HousesExplored.size() > 10 && ShouldExplore())
	{
		m_HousesToExplore.push_back(m_HousesExplored.front());
		m_HousesExplored.pop_front();
	}
}

bool Plugin::FullFillWorldState(const WorldState ws)
{
	if (GiveFeedback)std::cout << "WS: " << WorldStatesNames[ws] << std::endl;
	for (auto action : m_Actions)
	{
		if (ws == action.fullFillWorldState)
		{
			if (GiveFeedback)std::cout << "A: " << action.Name <<std::endl;
			if(action.IsValid())
			{
				if (GiveFeedback)std::cout << "Valid" << std::endl;
				for (auto requiredws : action.requiredWorldState)
				{
					if (GiveFeedback)std::cout << "RWS: " << WorldStatesNames[requiredws] << std::endl;
					if (!GetWorldState(requiredws))
					{
						if (GiveFeedback)std::cout << "False" << std::endl;
						return FullFillWorldState(requiredws);
					}
					if (GiveFeedback)std::cout << "True" << std::endl;
				}
				if (GiveFeedback)std::cout << "Perform" << std::endl;
				action.Perform();
				return true;
			}
			if (GiveFeedback)std::cout << "Not Valid" << std::endl;
		}
	}
	std::cout << "No action found" << std::endl;
	return false;
}

void Plugin::UpdateWorldStateData()
{
	SetInPurgeZone();
	SetIsAttacked();
	SetEnemysInFOV();
	ClearReachedExplorePositions();
	SetHousesToExplore();
	SetItemsToLoot();
	GetFreeInventorySlot();

	if (m_pInterface->Agent_GetInfo().Stamina >= 10.f) m_CanRun = true;
	if (m_pInterface->Agent_GetInfo().Stamina <= 0.1f) m_CanRun = false;
	m_Steering.RunMode = m_CanRun;
}

bool Plugin::GetWorldState(WorldState ws)
{
	switch (ws)
	{
	case TargetInRange:
		if (m_GoToRange >= 0 && m_GoToPos.Distance(m_pInterface->Agent_GetInfo().Position) > m_GoToRange)
		{
			return false;
		}
		if (m_GoToRange < 0 && m_GoToPos.Distance(m_pInterface->Agent_GetInfo().Position) < fabs(m_GoToRange))
			return false;
		if (m_GoToFaceTarget)
		{
			m_Steering.AutoOrient = false;
			const float angleToEnemy = fabs(Elite::AngleBetween(
				m_GoToPos - m_pInterface->Agent_GetInfo().Position,
				AgentOrientationVector2()));
			if (angleToEnemy > m_pInterface->Agent_GetInfo().FOV_Angle / 2)
				return false;
		}
		return true;
		break;
	case FreeInventorySlot:
		return GetFreeInventorySlot();
		break;
	default:
		return true;
	}
}

EntityInfo Plugin::GetEntityAtPos(Elite::Vector2 pos)
{
	for (auto entity : GetEntitiesInFOV())
	{
		if (pos == entity.Location)
			return entity;
	}
	return { eEntityType::ITEM,Elite::Vector2{},-1 };
}

Elite::Vector2 Plugin::AgentOrientationVector2()
{
	return Elite::Vector2(cosf(m_pInterface->Agent_GetInfo().Orientation), sinf(m_pInterface->Agent_GetInfo().Orientation));
}

bool Plugin::ShouldExplore()
{
	return TotalEnergyLeft() > 15 && TotalHealthLeft() > 10 && TotalAmmoLeft() > 10;
}

float Plugin::TotalEnergyLeft()
{
	float invEnergy{ 0 };
	for (int i{ 0 }; i < m_pInterface->Inventory_GetCapacity(); ++i)
	{
		ItemInfo item;
		if (m_pInterface->Inventory_GetItem(i, item))
		{
			if (item.Type == eItemType::FOOD)
			{
				invEnergy += m_pInterface->Food_GetEnergy(item);
			}
		}
	}
	return m_pInterface->Agent_GetInfo().Energy + invEnergy;
}

float Plugin::TotalHealthLeft()
{
	float invHealth{ 0 };
	for (int i{ 0 }; i < m_pInterface->Inventory_GetCapacity(); ++i)
	{
		ItemInfo item;
		if (m_pInterface->Inventory_GetItem(i, item))
		{
			if (item.Type == eItemType::MEDKIT)
			{
				invHealth += m_pInterface->Medkit_GetHealth(item);
			}
		}
	}
	return m_pInterface->Agent_GetInfo().Health + invHealth;
}

int Plugin::TotalAmmoLeft()
{
	int ammoLeft{ 0 };
	for (int i{ 0 }; i < m_pInterface->Inventory_GetCapacity(); ++i)
	{
		ItemInfo item;
		if (m_pInterface->Inventory_GetItem(i, item))
		{
			if (item.Type == eItemType::PISTOL || item.Type == eItemType::SHOTGUN)
			{
				ammoLeft += m_pInterface->Weapon_GetAmmo(item);
			}
		}
	}
	return ammoLeft;
}
