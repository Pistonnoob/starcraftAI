#include "ExampleAIModule.h" 
using namespace BWAPI;

bool analyzed;
bool analysis_just_finished;
BWTA::Region* home;
BWTA::Region* enemy_base;

//This is the startup method. It is called once
//when a new game has been started with the bot.
void ExampleAIModule::onStart()
{
	for(int i = 0; i < 5; i++)
		this->steps[i] = false;
	//Enable flags
	Broodwar->enableFlag(Flag::UserInput);
	//Uncomment to enable complete map information
	Broodwar->enableFlag(Flag::CompleteMapInformation);

	//Start analyzing map data
	BWTA::readMap();
	analyzed=false;
	analysis_just_finished=false;
	//CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)AnalyzeThread, NULL, 0, NULL); //Threaded version
	AnalyzeThread();

    //Send each worker to the mineral field that is closest to it
    for(std::set<Unit*>::const_iterator i=Broodwar->self()->getUnits().begin();i!=Broodwar->self()->getUnits().end();i++)
    {
		if ((*i)->getType().isWorker())
		{
			this->builders.insert(*i);
			this->builders2.push_back(std::pair<Unit *, int>((*i), 0));
			Unit* closestMineral=NULL;
			for(std::set<Unit*>::iterator m=Broodwar->getMinerals().begin();m!=Broodwar->getMinerals().end();m++)
			{
				if (closestMineral==NULL || (*i)->getDistance(*m)<(*i)->getDistance(closestMineral))
				{	
					closestMineral=*m;
				}
			}
			if (closestMineral!=NULL)
			{
				(*i)->rightClick(closestMineral);
				Broodwar->printf("Send worker %d to mineral %d", (*i)->getID(), closestMineral->getID());
			}
		}else if((*i)->getType() == UnitTypes::Terran_Command_Center)
		{
			this->commandCenters.push_back(*i);
		}
	}


}

//Called when a game is ended.
//No need to change this.
void ExampleAIModule::onEnd(bool isWinner)
{
	if (isWinner)
	{
		Broodwar->sendText("I won!");
	}
}

//Finds a guard point around the home base.
//A guard point is the center of a chokepoint surrounding
//the region containing the home base.
Position ExampleAIModule::findGuardPoint()
{
	//Get the chokepoints linked to our home region
	std::set<BWTA::Chokepoint*> chokepoints = home->getChokepoints();
	double min_length = 10000;
	BWTA::Chokepoint* choke = NULL;

	//Iterate through all chokepoints and look for the one with the smallest gap (least width)
	for(std::set<BWTA::Chokepoint*>::iterator c = chokepoints.begin(); c != chokepoints.end(); c++)
	{
		double length = (*c)->getWidth();
		if (length < min_length || choke==NULL)
		{
			min_length = length;
			choke = *c;
		}
	}

	return choke->getCenter();
}

std::vector<TilePosition> ExampleAIModule::findBuildingSites(Unit* worker, BWAPI::UnitType type, int amount, Unit* baseCenter)
{
	std::vector<TilePosition> returnPositions;
	TilePosition origin(home->getCenter());
	//TilePosition origin2(baseCenter->getRegion()->getCenter());
	TilePosition providedOrigin = baseCenter->getTilePosition();
	TilePosition deltaPos = origin;
	

	//if(baseCenter->getType() == BWAPI::UnitTypes::Terran_Command_Center)
	//{
		deltaPos -= providedOrigin;
		if(deltaPos.x() < 0)
			deltaPos.x() = -1;
		else
			deltaPos.x() = 1;
		if(deltaPos.y() < 0)
			deltaPos.y() = -1;
		else
			deltaPos.y() = 1;
	//}

	//Lets do a switch case for the different building types
	if(type == BWAPI::UnitTypes::Terran_Barracks)
	{
		//Build at center of region.
		returnPositions.push_back(origin);
	}
	else if(type == BWAPI::UnitTypes::Terran_Supply_Depot || type == BWAPI::UnitTypes::Terran_Academy)
	{
		int foundPos = 0;
		int xLimit = 4, yLimit = 3;
		for(int x = 0; x < 4 && foundPos < amount; x++)
		{
			for(int y = 0; y < yLimit && foundPos < amount; y++)
			{
				TilePosition bPos = providedOrigin + TilePosition(0, (UnitTypes::Terran_Command_Center.tileHeight() + 1)*deltaPos.y());
				bPos += TilePosition(x * type.tileWidth() * deltaPos.x(),y * type.tileHeight() * deltaPos.y());
				if(Broodwar->canBuildHere(worker, bPos, type, false))
				{
					foundPos++;
					returnPositions.push_back(bPos);
				}
			}
		}
	}
	else if(type == BWAPI::UnitTypes::Terran_Refinery)
	{
		//Check the map for geysers
		for(std::set<Unit*>::const_iterator geyser=Broodwar->getGeysers().begin(); geyser != Broodwar->getGeysers().end(); geyser++)
		{
			//Check if in same region as passed Command Center
			Broodwar->printf("Geyser region %s. Base region %s", (*geyser)->getRegion()->getID(), baseCenter->getRegion()->getID());
			if((*geyser)->getRegion()->getID() == baseCenter->getRegion()->getID())
			{
				TilePosition geyserPos = (*geyser)->getTilePosition();
				//If AI can build refinery there, push the TilePosition to the returning list
				if(Broodwar->canBuildHere(worker, (*geyser)->getTilePosition(), BWAPI::UnitTypes::Terran_Refinery, false))
					returnPositions.push_back(geyserPos);
			}
		}
	}
	else if(type == BWAPI::UnitTypes::Terran_Factory)
	{
		
	}
	return returnPositions;
}

//Finds a place suitable for a barrack and builds it
void ExampleAIModule::step1()
{
	//Find the current objective within this step
	int obj = 0;
	Unit* barrackPtr = NULL;
	Position guardPnt = this->findGuardPoint();
	int marineCnt = Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Terran_Marine) + Broodwar->self()->incompleteUnitCount(BWAPI::UnitTypes::Terran_Marine);
	int supplyDepotCnt = Broodwar->self()->completedUnitCount(UnitTypes::Terran_Supply_Depot) + Broodwar->self()->incompleteUnitCount(UnitTypes::Terran_Supply_Depot);
	for(std::set<Unit*>::const_iterator i=Broodwar->self()->getUnits().begin();i!=Broodwar->self()->getUnits().end();i++)
	{
		if((*i)->getType() == BWAPI::UnitTypes::Terran_Barracks)
		{
			barrackPtr = (*i);
			obj = 1;
			if(barrackPtr->isCompleted())
				obj = 2;
		}
	}
	if(barrackPtr != NULL && barrackPtr->getRallyPosition() == guardPnt)
		obj = 3;
	if(marineCnt >= 10 && Broodwar->self()->completedUnitCount(UnitTypes::Terran_Supply_Depot) == 2)
		obj = 4;
	//Logic for completion of each objective within this step
	switch(obj)
	{
		case 0:
			if(Broodwar->self()->minerals() >= UnitTypes::Terran_Barracks.mineralPrice())
			{
				for(std::set<Unit*>::const_iterator i=this->builders.begin();i!=this->builders.end();i++)
				{
					if((*i)->getType().isWorker())
					{
						this->constructBuilding(this->findBuildingSites(*i, UnitTypes::Terran_Barracks, 1, this->commandCenters[0]), *i, UnitTypes::Terran_Barracks); 
						//this->buildBarracks(this->findBuildingSites(BWAPI::UnitTypes::Terran_Barracks, 1).front());
						break;
					}
				}
			}
			break;
		case 1:
			Broodwar->printf("Barrack being constructed!");
			break;
		case 2:
			//Set rally point for Barrack
			if(barrackPtr != NULL && barrackPtr->setRallyPoint(guardPnt) && barrackPtr->getRallyPosition() == guardPnt)
				obj++;
			break;
		case 3:
			//Finished with barrack and setting rally point
			//train 10 x Marine
			if(marineCnt < 10)
			{
				//Broodwar->sendText("A %s [%x] has been created at (%d,%d)",unit->getType().getName().c_str(),unit,unit->getPosition().x(),unit->getPosition().y());
				Broodwar->printf("Creating %d %s", 10 - marineCnt, UnitTypes::Terran_Marine.getName().c_str());
				this->trainUnits(barrackPtr, UnitTypes::Terran_Marine, (10 - marineCnt));
				//this->trainUnits((10 - marineCnt));
			}
			//Build 2 supply depot
			if(supplyDepotCnt < 2)
			{
				for(std::set<Unit*>::const_iterator i=this->builders.begin();i!=this->builders.end();i++)
				{
					if((*i)->getType().isWorker())
					{
						std::vector<BWAPI::TilePosition> supplyPos = this->findBuildingSites((*i), BWAPI::UnitTypes::Terran_Supply_Depot, 2 - supplyDepotCnt, this->commandCenters[0]);
						this->constructBuilding(supplyPos, (*i), UnitTypes::Terran_Supply_Depot);
						break;
					}
				}
			}
			break;
		case 4:
			Broodwar->printf("Step 1, Objective 4");
			for(std::set<Unit*>::const_iterator i=this->builders.begin();i!=this->builders.end();i++)
			{
				if((*i)->getType().isWorker())
				{
					if(!(*i)->isGatheringMinerals())
					{
						Unit* closestMineral=NULL;
						for(std::set<Unit*>::iterator m=Broodwar->getMinerals().begin();m!=Broodwar->getMinerals().end();m++)
						{
							if (closestMineral==NULL || (*i)->getDistance(*m)<(*i)->getDistance(closestMineral))
							{	
								closestMineral=*m;
							}
						}
						if (closestMineral!=NULL)
						{
							(*i)->rightClick(closestMineral);
							Broodwar->printf("Send worker %d to mineral %d", (*i)->getID(), closestMineral->getID());
						}
					}
				}
			}
			this->steps[0] = true;
			break;
		default:
			break;
	}


}

void ExampleAIModule::step2()
{	

	
#pragma region
	//Built academy
	int academyCnt = Broodwar->self()->completedUnitCount(UnitTypes::Terran_Academy) + Broodwar->self()->incompleteUnitCount(UnitTypes::Terran_Academy);
	int refineryCnt = Broodwar->self()->completedUnitCount(UnitTypes::Terran_Refinery) + Broodwar->self()->incompleteUnitCount(UnitTypes::Terran_Refinery);
	
	int obj = 0;	//Build academy

	if(Broodwar->self()->completedUnitCount(UnitTypes::Terran_Academy) == 1)
	{
		obj = 1;	//Build refinery

	}
	if(Broodwar->self()->completedUnitCount(UnitTypes::Terran_Refinery) >= 1)
	{
		obj = 2;	//Finished building, tell hired worker to gather gas and start building 5 more Terran_SCVs and 3 medics
	}
	/*for(std::set<Unit*>::const_iterator i = Broodwar->self()->getUnits().begin(); i != Broodwar->self()->getUnits().end(); i++)
	{
		if((*i)->getType() == UnitTypes::Terran_Academy)
		{

		}
	}*/
#pragma endregion checks objective
	Broodwar->printf("Completing objective %d!", obj);
	switch(obj)
	{
	case 0:		//construct an academy. (x,y) = (3, 2)
		if(academyCnt < 1)
		{
			if(this->hasResFor(UnitTypes::Terran_Academy))
			{
				std::vector<Unit*> hiredWorkers = this->findWorker(21, UnitTypes::Terran_SCV);
				if(hiredWorkers.empty())
					hiredWorkers = this->findAndHire(21, UnitTypes::Terran_SCV, 1);
				if(!hiredWorkers.empty())
					this->constructBuilding(this->findBuildingSites(hiredWorkers[0], UnitTypes::Terran_Academy, 3, this->commandCenters[0]), hiredWorkers[0], UnitTypes::Terran_Academy);
			}
		}
		break;
	case 1:		//Construct a Refinery
		if(refineryCnt < 1)
		{
			if(this->hasResFor(UnitTypes::Terran_Refinery))
			{
				std::vector<Unit*> hiredWorkers = this->findWorker(21, UnitTypes::Terran_SCV);
				if(hiredWorkers.empty())
					hiredWorkers = this->findAndHire(21, UnitTypes::Terran_SCV, 1);					//Breaks here
				//if(!hiredWorkers.empty())
					//this->constructBuilding(this->findBuildingSites(hiredWorkers[0], UnitTypes::Terran_Refinery, 1, this->commandCenters[0]), hiredWorkers[0], UnitTypes::Terran_Refinery);
			}
		}
		break;
	case 2:		//Fire the hired worker and order it to gather gas. 
		break;
	case 3:
		break;
	case 4:
		break;
	default:
		break; //Not necessary
	}
}


void ExampleAIModule::trainUnits(Unit* trainer, BWAPI::UnitType unit, int amount)
{
	bool canTrain = true;
	//Check if trainer is capable of producing any unit
	if(trainer->getType().canProduce())
	{
		//Check what kind of factory can create SPECIFIED unit and if the trainer is of said type.
		std::pair< UnitType, int > builder = unit.whatBuilds();
		if(builder.first == trainer->getType())
		{
			//If so, add a order to the training queue equal to the amount specified
			for(int a = 0; a < amount && canTrain; a++)
			{
				if(this->hasResFor(unit))
				{
					if(!trainer->train(unit))
					{
						Broodwar->printf("%s failed to train %s.",trainer->getType().getName().c_str(), unit.getName().c_str());
						canTrain = false;
					}
				}else
					canTrain = false;
			}
		}	
	}
}

void ExampleAIModule::constructBuilding(std::vector<BWAPI::TilePosition> possitions, Unit* worker, BWAPI::UnitType building)
{
	//Check if pos exists
	if(!possitions.empty() && possitions[0] != TilePosition(0, 0))
	{
		//Check if worker is a worker
		if(worker->getType().isWorker())
		{
			//Check if building is a building
			if(building.isBuilding())
			{
				bool success = false;
				for(std::vector<TilePosition>::const_iterator posItr = possitions.begin(); posItr != possitions.end() && !success; posItr++)
				{
					TilePosition pos = (*posItr);
					if(this->hasResFor(building))
					{
						//Check if building can be constructed at DESIGNATED position
						if(Broodwar->canBuildHere(worker, pos, building, false))
						{
							//Check if Worker can build at position
							if(worker->hasPath(BWAPI::Position(pos)))
							{
								//Order walker to build at position
								//BWAPI::UnitCommand::build(
								//bool canBuild = worker->issueCommand(UnitCommand::build(worker,pos,building));
								bool canBuild = worker->build(pos, building);
								if(canBuild)
								{
									Broodwar->printf("Constructing %s", building.getName().c_str());
									success = true;
								}else if(!canBuild && !Broodwar->canBuildHere(worker, pos, building, true))	//If the reason is that the area is not explored
								{
									//Move to the unexplored area
									worker->move(Position(pos), false);
									Broodwar->printf("Moving %s to build area! TilePosition: (%d, %d)", worker->getType().getName().c_str(), pos.x(), pos.y());
									success = true;
								}else
								{
									Broodwar->printf("%s could not build %s!",worker->getType().getName().c_str(), building.getName().c_str());
								}
							}else
								Broodwar->printf("%s cannot reach designated tile position!", worker->getType().getName().c_str());
						}else
							Broodwar->printf("%s cannot be built at designated position!", building.getName().c_str());
					}else
						Broodwar->printf("Not enough materials to begin construction of %s!", building.getName().c_str());
				}
			}else
				Broodwar->printf("Designated %s is not of type: %s!", building.getName().c_str(), UnitTypes::Buildings.getName().c_str());
		}else
			Broodwar->printf("Designated %s is not of type: Worker!", worker->getType().getName().c_str());
	}else
		Broodwar->printf("Did not designate a possition!");
}



//This is the method called each frame. This is where the bot's logic
//shall be called.
void ExampleAIModule::onFrame()
{
	//Check every second frame
	if(Broodwar->getFrameCount() % 2 == 0)
	{
		//Check if there has been any units created
		if(!this->newlyCreatedUnits.empty())
		{
			//For every unit, check if it has been finished yet. If so then add it to the responding lists
			for(std::set<Unit*>::iterator unit = this->newlyCreatedUnits.begin(); unit != this->newlyCreatedUnits.end(); unit++)
			{
				if((*unit)->isCompleted())
				{
					//Add to lists according to type
					if((*unit)->getType().isWorker())
					{			
						this->builders.insert((*unit));
						this->builders2.push_back(std::pair<Unit*, int>((*unit), 0));	//Start it off with a HiredID of 0. May change in the future.
					}
					//Remove it from this list
					this->newlyCreatedUnits.erase(unit);
				}
			}
		}
	}
	
	//Call every 100:th frame
	if (Broodwar->getFrameCount() % 100 == 0)
	{
		//Complete first step, if completed, move on to next check
		for(int i = 0; i < 5; i++)
		{
			if(!this->steps[i])
			{
				Broodwar->printf("Entering step %d", i);
				switch(i)
				{
					case 0:
						this->step1();
						break;
					case 1:
						this->step2();
						break;
					case 2:
						break;
					case 3:
						break;
					case 4:
						break;
				}
				//Only initialize one step at a time
				break;
			}
		}
	}
  
	//Draw lines around regions, chokepoints etc.
	if (analyzed)
	{
		drawTerrainData();
	}
	//Broodwar->printf("It works");
}


bool ExampleAIModule::hasResFor(UnitType type)const
{
	bool result = false;
	if(type.mineralPrice() <= Broodwar->self()->minerals() && type.gasPrice() <= Broodwar->self()->gas())
	{
		if(type.supplyRequired() <= Broodwar->self()->supplyTotal() - Broodwar->self()->supplyUsed())
			result = true;
		else if(type.isBuilding())
			result = true;
	}
	return result;
}

std::vector<Unit*> ExampleAIModule::findWorker(int hireID, BWAPI::UnitType type)
{
	std::vector<Unit*> result;
	for(std::vector<std::pair<Unit *, int>>::iterator i = this->builders2.begin(); i != this->builders2.end(); i++)
	{
		if(i->first->getType() == type && i->second == hireID)
		{
			result.push_back(i->first);
		}
	}

	return result;
}

bool ExampleAIModule::findAndHire(int hireID, BWAPI::UnitType type, int amount, std::vector<Unit*> &storeIn)//Returns false if empty
{
	bool result = false;
	std::vector<Unit*> unitList = this->findAndHire(hireID, type, amount);
	result = !unitList.empty();
	if(result)	//If the list was empty, skip adding it to the storage list.
		storeIn.assign(unitList.begin(), unitList.end());
	return result;
}

std::vector<Unit*> ExampleAIModule::findAndHire(int hireID, BWAPI::UnitType type, int amount)
{
	std::vector<Unit*> result;
	for(std::vector<std::pair<Unit *, int>>::iterator i = this->builders2.begin(); i != this->builders2.end(); i++)
		{
			//We are now accessing the pair structure with the builders and their assigned states. Hire a builder and set their ID-state
			if(i->first->getType() == type && i->second == 0) //Check available
			{
				//Give it the ID-state which will represent it being issued this workload
				i->second = hireID;		//2X for step2, X1 for objective 1
				result.push_back(i->first);
				amount--;
				if(amount < 1)
					break;
			}
		}
	return result;
}

int ExampleAIModule::findAndChange(int origID, int resultID)
{
	return this->findAndChange(origID, resultID, BWAPI::UnitTypes::AllUnits);
	Broodwar->self()->getStartLocation();
}

int ExampleAIModule::findAndChange(int origID, int resultID, BWAPI::UnitType type)
{
	int changeCnt = 0;
	for(std::vector<std::pair<Unit*, int>>::iterator i = this->builders2.begin(); i != this->builders2.end(); i++)
	{
		if(i->first->getType() == type)
		{
			if(i->second == origID)
			{
				i->second = resultID;
				changeCnt++;
			}
		}
	}
	return changeCnt;
}

bool ExampleAIModule::commandGroup(std::vector<Unit*> units, BWAPI::UnitCommand command) //returns true if all units could issue the command
{
	bool couldCommandAll = true;
	//Check so that the vector actually has elements
	if(!units.empty())
	{
		for(std::vector<Unit*>::iterator unit = units.begin(); unit != units.end(); unit++)
		{
			if((*unit)->canIssueCommand(command))
			{
				if(!(*unit)->issueCommand(command))
				{
					couldCommandAll = false;
				}
			}
		}
	}else
		couldCommandAll = false;

	return couldCommandAll;
}

Unit* ExampleAIModule::getClosestUnit(Unit* mine, BWAPI::UnitType targetType)	//Will return units in "mine"s sight range
{
	Unit* closest = NULL;
	
	closest = this->getClosestUnit(mine, targetType, mine->getType().sightRange());

	return closest;
}

Unit* ExampleAIModule::getClosestUnit(Unit* mine, BWAPI::UnitType targetType, int radius)	//Will return the closest unit of right type within radius
{
	//Get the set of units within given radius of the unit, itterate through them and select the one closest
	Unit* closest = NULL;
	std::set<Unit*> unitsInRange = mine->getUnitsInRadius(radius);
	int distance = radius;
	for(std::set<Unit*>::const_iterator targetUnit = unitsInRange.begin(); targetUnit != unitsInRange.end(); targetUnit++)
	{
		//Check corrected type
		if((*targetUnit)->getType() == targetType)
		{	//Check distance
			if(mine->getDistance((*targetUnit)) <= distance)
			{
				distance = mine->getDistance((*targetUnit));
				closest = (*targetUnit);
			}
		}
	}
	return closest;
}
























//Is called when text is written in the console window.
//Can be used to toggle stuff on and off.
void ExampleAIModule::onSendText(std::string text)
{
	if (text=="/show players")
	{
		showPlayers();
	}
	else if (text=="/show forces")
	{
		showForces();
	}
	else
	{
		Broodwar->printf("You typed '%s'!",text.c_str());
		Broodwar->sendText("%s",text.c_str());
	}
}

//Called when the opponent sends text messages.
//No need to change this.
void ExampleAIModule::onReceiveText(BWAPI::Player* player, std::string text)
{
	Broodwar->printf("%s said '%s'", player->getName().c_str(), text.c_str());
}

//Called when a player leaves the game.
//No need to change this.
void ExampleAIModule::onPlayerLeft(BWAPI::Player* player)
{
	Broodwar->sendText("%s left the game.",player->getName().c_str());
}

//Called when a nuclear launch is detected.
//No need to change this.
void ExampleAIModule::onNukeDetect(BWAPI::Position target)
{
	if (target!=Positions::Unknown)
	{
		Broodwar->printf("Nuclear Launch Detected at (%d,%d)",target.x(),target.y());
	}
	else
	{
		Broodwar->printf("Nuclear Launch Detected");
	}
}

//No need to change this.
void ExampleAIModule::onUnitDiscover(BWAPI::Unit* unit)
{
	//Broodwar->sendText("A %s [%x] has been discovered at (%d,%d)",unit->getType().getName().c_str(),unit,unit->getPosition().x(),unit->getPosition().y());
}

//No need to change this.
void ExampleAIModule::onUnitEvade(BWAPI::Unit* unit)
{
	//Broodwar->sendText("A %s [%x] was last accessible at (%d,%d)",unit->getType().getName().c_str(),unit,unit->getPosition().x(),unit->getPosition().y());
}

//No need to change this.
void ExampleAIModule::onUnitShow(BWAPI::Unit* unit)
{
	//Broodwar->sendText("A %s [%x] has been spotted at (%d,%d)",unit->getType().getName().c_str(),unit,unit->getPosition().x(),unit->getPosition().y());
}

//No need to change this.
void ExampleAIModule::onUnitHide(BWAPI::Unit* unit)
{
	//Broodwar->sendText("A %s [%x] was last seen at (%d,%d)",unit->getType().getName().c_str(),unit,unit->getPosition().x(),unit->getPosition().y());
}

//Called when a new unit has been created.
//Note: The event is called when the new unit is built, not when it
//has been finished.
void ExampleAIModule::onUnitCreate(BWAPI::Unit* unit)
{
	if (unit->getPlayer() == Broodwar->self())
	{
		Broodwar->sendText("A %s [%x] has been created at (%d,%d)",unit->getType().getName().c_str(),unit,unit->getPosition().x(),unit->getPosition().y());
		//Add it to our list of newlyCreatedUnits
		this->newlyCreatedUnits.insert(unit);
	}
}

//Called when a unit has been destroyed.
void ExampleAIModule::onUnitDestroy(BWAPI::Unit* unit)
{
	if (unit->getPlayer() == Broodwar->self())
	{
		Broodwar->sendText("My unit %s [%x] has been destroyed at (%d,%d)",unit->getType().getName().c_str(),unit,unit->getPosition().x(),unit->getPosition().y());
		//Remove the unit from the builders list
		this->builders.erase(this->builders.find(unit));
		//Remove the unit from the hired builders list
		for(std::vector<std::pair<Unit*, int>>::iterator i = this->builders2.begin(); i != this->builders2.end(); i++)
		{
			if((*i).first == unit)
			{
				this->builders2.erase(i);
				break;
			}
		}

	}
	else
	{
		Broodwar->sendText("Enemy unit %s [%x] has been destroyed at (%d,%d)",unit->getType().getName().c_str(),unit,unit->getPosition().x(),unit->getPosition().y());
	}
}

//Only needed for Zerg units.
//No need to change this.
void ExampleAIModule::onUnitMorph(BWAPI::Unit* unit)
{
	//Broodwar->sendText("A %s [%x] has been morphed at (%d,%d)",unit->getType().getName().c_str(),unit,unit->getPosition().x(),unit->getPosition().y());
}

//No need to change this.
void ExampleAIModule::onUnitRenegade(BWAPI::Unit* unit)
{
	//Broodwar->sendText("A %s [%x] is now owned by %s",unit->getType().getName().c_str(),unit,unit->getPlayer()->getName().c_str());
}

//No need to change this.
void ExampleAIModule::onSaveGame(std::string gameName)
{
	Broodwar->printf("The game was saved to \"%s\".", gameName.c_str());
}

//Analyzes the map.
//No need to change this.
DWORD WINAPI AnalyzeThread()
{
	BWTA::analyze();

	//Self start location only available if the map has base locations
	if (BWTA::getStartLocation(BWAPI::Broodwar->self())!=NULL)
	{
		home = BWTA::getStartLocation(BWAPI::Broodwar->self())->getRegion();
	}
	//Enemy start location only available if Complete Map Information is enabled.
	if (BWTA::getStartLocation(BWAPI::Broodwar->enemy())!=NULL)
	{
		enemy_base = BWTA::getStartLocation(BWAPI::Broodwar->enemy())->getRegion();
	}
	analyzed = true;
	analysis_just_finished = true;
	return 0;
}

//Prints some stats about the units the player has.
//No need to change this.
void ExampleAIModule::drawStats()
{
	std::set<Unit*> myUnits = Broodwar->self()->getUnits();
	Broodwar->drawTextScreen(5,0,"I have %d units:",myUnits.size());
	std::map<UnitType, int> unitTypeCounts;
	for(std::set<Unit*>::iterator i=myUnits.begin();i!=myUnits.end();i++)
	{
		if (unitTypeCounts.find((*i)->getType())==unitTypeCounts.end())
		{
			unitTypeCounts.insert(std::make_pair((*i)->getType(),0));
		}
		unitTypeCounts.find((*i)->getType())->second++;
	}
	int line=1;
	for(std::map<UnitType,int>::iterator i=unitTypeCounts.begin();i!=unitTypeCounts.end();i++)
	{
		Broodwar->drawTextScreen(5,16*line,"- %d %ss",(*i).second, (*i).first.getName().c_str());
		line++;
	}
}

//Draws terrain data aroung regions and chokepoints.
//No need to change this.
void ExampleAIModule::drawTerrainData()
{
	//Iterate through all the base locations, and draw their outlines.
	for(std::set<BWTA::BaseLocation*>::const_iterator i=BWTA::getBaseLocations().begin();i!=BWTA::getBaseLocations().end();i++)
	{
		TilePosition p=(*i)->getTilePosition();
		Position c=(*i)->getPosition();
		//Draw outline of center location
		Broodwar->drawBox(CoordinateType::Map,p.x()*32,p.y()*32,p.x()*32+4*32,p.y()*32+3*32,Colors::Blue,false);
		//Draw a circle at each mineral patch
		for(std::set<BWAPI::Unit*>::const_iterator j=(*i)->getStaticMinerals().begin();j!=(*i)->getStaticMinerals().end();j++)
		{
			Position q=(*j)->getInitialPosition();
			Broodwar->drawCircle(CoordinateType::Map,q.x(),q.y(),30,Colors::Cyan,false);
		}
		//Draw the outlines of vespene geysers
		for(std::set<BWAPI::Unit*>::const_iterator j=(*i)->getGeysers().begin();j!=(*i)->getGeysers().end();j++)
		{
			TilePosition q=(*j)->getInitialTilePosition();
			Broodwar->drawBox(CoordinateType::Map,q.x()*32,q.y()*32,q.x()*32+4*32,q.y()*32+2*32,Colors::Orange,false);
		}
		//If this is an island expansion, draw a yellow circle around the base location
		if ((*i)->isIsland())
		{
			Broodwar->drawCircle(CoordinateType::Map,c.x(),c.y(),80,Colors::Yellow,false);
		}
	}
	//Iterate through all the regions and draw the polygon outline of it in green.
	for(std::set<BWTA::Region*>::const_iterator r=BWTA::getRegions().begin();r!=BWTA::getRegions().end();r++)
	{
		BWTA::Polygon p=(*r)->getPolygon();
		for(int j=0;j<(int)p.size();j++)
		{
			Position point1=p[j];
			Position point2=p[(j+1) % p.size()];
			Broodwar->drawLine(CoordinateType::Map,point1.x(),point1.y(),point2.x(),point2.y(),Colors::Green);
		}
	}
	//Visualize the chokepoints with red lines
	for(std::set<BWTA::Region*>::const_iterator r=BWTA::getRegions().begin();r!=BWTA::getRegions().end();r++)
	{
		for(std::set<BWTA::Chokepoint*>::const_iterator c=(*r)->getChokepoints().begin();c!=(*r)->getChokepoints().end();c++)
		{
			Position point1=(*c)->getSides().first;
			Position point2=(*c)->getSides().second;
			Broodwar->drawLine(CoordinateType::Map,point1.x(),point1.y(),point2.x(),point2.y(),Colors::Red);
		}
	}
}

//Show player information.
//No need to change this.
void ExampleAIModule::showPlayers()
{
	std::set<Player*> players=Broodwar->getPlayers();
	for(std::set<Player*>::iterator i=players.begin();i!=players.end();i++)
	{
		Broodwar->printf("Player [%d]: %s is in force: %s",(*i)->getID(),(*i)->getName().c_str(), (*i)->getForce()->getName().c_str());
	}
}

//Show forces information.
//No need to change this.
void ExampleAIModule::showForces()
{
	std::set<Force*> forces=Broodwar->getForces();
	for(std::set<Force*>::iterator i=forces.begin();i!=forces.end();i++)
	{
		std::set<Player*> players=(*i)->getPlayers();
		Broodwar->printf("Force %s has the following players:",(*i)->getName().c_str());
		for(std::set<Player*>::iterator j=players.begin();j!=players.end();j++)
		{
			Broodwar->printf("  - Player [%d]: %s",(*j)->getID(),(*j)->getName().c_str());
		}
	}
}

//Called when a unit has been completed, i.e. finished built.
void ExampleAIModule::onUnitComplete(BWAPI::Unit *unit)
{
	//Broodwar->sendText("A %s [%x] has been completed at (%d,%d)",unit->getType().getName().c_str(),unit,unit->getPosition().x(),unit->getPosition().y());
}
