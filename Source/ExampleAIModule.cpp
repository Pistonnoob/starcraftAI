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
	this->actObjective = 0;
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
	if(amount < 1) return returnPositions;

	TilePosition origin(home->getCenter());
	//TilePosition origin2(baseCenter->getRegion()->getCenter());
	TilePosition providedOrigin = baseCenter->getTilePosition();
	TilePosition deltaPos = origin;
	
#pragma region
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
#pragma endregion calculate delta

	//Lets do a switch case for the different building types
	if(type == BWAPI::UnitTypes::Terran_Barracks)
	{
		//Build at center of region.
		int foundPos = 0;
		int xLimit = 4, yLimit = 1;
		for(int x = 0; x < xLimit && foundPos < amount; x++)
		{
			for(int y = 0; y < yLimit && foundPos < amount; y++)
			{
				TilePosition bPos = providedOrigin;
				if(deltaPos.x() > 0)
					bPos += TilePosition((BWAPI::UnitTypes::Terran_Command_Center.tileWidth()+1) * deltaPos.x() /*The starting point of the xPos*/, 0 /*The starting point of the yPos which we don't want to change*/);
				else
					bPos += TilePosition(1 * deltaPos.x() /*The starting point of the xPos*/, 0 /*The starting point of the yPos which we don't want to change*/);
				bPos += TilePosition(x * type.tileWidth() * deltaPos.x(), 0 /*y * type.tileHeight() * deltaPos.y()*/);
				if(Broodwar->canBuildHere(worker, bPos, type, false))
				{
					foundPos++;
					returnPositions.push_back(bPos);
				}
			}
		}
		/*returnPositions.push_back(origin);*/
	}
	else if(type == BWAPI::UnitTypes::Terran_Supply_Depot || type == BWAPI::UnitTypes::Terran_Academy)
	{
		int foundPos = 0;
		int xLimit = 4, yLimit = 3;
		for(int x = 0; x < xLimit && foundPos < amount; x++)
		{
			for(int y = 0; y < yLimit && foundPos < amount; y++)
			{
				TilePosition bPos = providedOrigin;
				if(deltaPos.y() < 0)
					bPos += TilePosition(0, (type.tileHeight() + 1)*deltaPos.y());
				else
					bPos += TilePosition(0, (UnitTypes::Terran_Command_Center.tileHeight() + 1)*deltaPos.y());
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
		std::pair<Unit*, int> closest(NULL, -1);
		for(std::set<Unit*>::iterator geyser=Broodwar->getGeysers().begin(); geyser != Broodwar->getGeysers().end(); geyser++)
		{

			//Check if in same region as passed Command Center
			//Broodwar->printf("Geyser region %d. Base region %d", (*geyser)->getRegion()->getID(), baseCenter->getRegion()->getID());
			//Check distance
			if(closest.first == NULL || baseCenter->getDistance((*geyser)) < closest.second)
			{
				closest.first = (*geyser);
				closest.second = baseCenter->getDistance(closest.first);
			}
			
		}
		//If AI can build refinery there, push the TilePosition to the returning list
		if(closest.first != NULL && Broodwar->canBuildHere(worker, closest.first->getTilePosition(), BWAPI::UnitTypes::Terran_Refinery, false))
			returnPositions.push_back(closest.first->getTilePosition());
	}
	//else if(type == BWAPI::UnitTypes::Terran_Refinery)
	//{
	//	//Check the map for geysers
	//	for(std::set<Unit*>::const_iterator geyser=Broodwar->getGeysers().begin(); geyser != Broodwar->getGeysers().end(); geyser++)
	//	{
	//		//Check if in same region as passed Command Center
	//		Broodwar->printf("Geyser region %s. Base region %s", (*geyser)->getRegion()->getID(), baseCenter->getRegion()->getID());
	//		if((*geyser)->getRegion()->getID() == baseCenter->getRegion()->getID())
	//		{
	//			TilePosition geyserPos = (*geyser)->getTilePosition();
	//			//If AI can build refinery there, push the TilePosition to the returning list
	//			if(Broodwar->canBuildHere(worker, (*geyser)->getTilePosition(), BWAPI::UnitTypes::Terran_Refinery, false))
	//				returnPositions.push_back(geyserPos);
	//		}
	//	}
	//}
	else if(type == BWAPI::UnitTypes::Terran_Factory)
	{
		//Build on top of barracks, their yPosition won't fluctuate. Construct on opposit side of supple depot.
		int foundPos = 0;
		int xLimit = 4, yLimit = 1;
		for(int x = 0; x < xLimit && foundPos < amount; x++)
		{
			for(int y = 0; y < yLimit && foundPos < amount; y++)
			{
				TilePosition bPos = providedOrigin;
				if(deltaPos.x() > 0)
					bPos += TilePosition((BWAPI::UnitTypes::Terran_Command_Center.tileWidth()+1) * deltaPos.x() /*The starting point of the xPos*/, -1 * type.tileHeight() * deltaPos.y() /*The starting point of the yPos which we do want to change*/);
				else
					bPos += TilePosition(1 * deltaPos.x() /*The starting point of the xPos*/, -1 * type.tileHeight() * deltaPos.y() /*The starting point of the yPos which we do want to change*/);
				bPos += TilePosition(x * type.tileWidth() * deltaPos.x(), 0 /*y * type.tileHeight() * deltaPos.y()*/);
				if(Broodwar->canBuildHere(worker, bPos, type, false))
				{
					foundPos++;
					returnPositions.push_back(bPos);
				}
			}
		}
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
	//int marineCnt = Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Terran_Marine) + Broodwar->self()->incompleteUnitCount(BWAPI::UnitTypes::Terran_Marine);
	int supplyDepotCnt = Broodwar->self()->completedUnitCount(UnitTypes::Terran_Supply_Depot) + Broodwar->self()->incompleteUnitCount(UnitTypes::Terran_Supply_Depot);
	int marineCnt = this->getUnitCount(UnitTypes::Terran_Marine);
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
			if(Broodwar->canMake(NULL, UnitTypes::Terran_Barracks))
			{
				for(std::set<Unit*>::const_iterator i=this->builders.begin();i!=this->builders.end();i++)
				{
					if((*i)->getType().isWorker())
					{
						this->constructBuilding(this->findBuildingSites(*i, UnitTypes::Terran_Barracks, 2, this->commandCenters.front()), (*i), UnitTypes::Terran_Barracks); 
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
						if(!(*i)->isConstructing())
						{
							std::vector<BWAPI::TilePosition> supplyPos = this->findBuildingSites((*i), BWAPI::UnitTypes::Terran_Supply_Depot, 2 - supplyDepotCnt, this->commandCenters[0]);
							this->constructBuilding(supplyPos, (*i), UnitTypes::Terran_Supply_Depot);
							break;
						}
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
	
	//Built academy
	int academyCnt = Broodwar->self()->completedUnitCount(UnitTypes::Terran_Academy) + Broodwar->self()->incompleteUnitCount(UnitTypes::Terran_Academy);
	int refineryCnt = Broodwar->self()->completedUnitCount(UnitTypes::Terran_Refinery) + Broodwar->self()->incompleteUnitCount(UnitTypes::Terran_Refinery);
	const int wantedMedicCap = 3;
	const int wantedWorkerCap = 5;
	int medicCnt = this->getUnitCount(UnitTypes::Terran_Medic);
	int workerCnt = this->getUnitCount(UnitTypes::Terran_SCV) - 4;	//4 is the initial worker count
	//int obj = 0;	//Build academy
#pragma region
	switch(this->actObjective)
	{
	case 0:
		if(Broodwar->self()->completedUnitCount(UnitTypes::Terran_Academy) >= 1)
		{
			//obj = 1;	//Build refinery
			this->actObjective = 1;
		}
		break;
	case 1:
		if(Broodwar->self()->completedUnitCount(UnitTypes::Terran_Refinery) >= 1)
		{
			//obj = 2;	//Finished building, tell hired worker to gather gas and start building 5 more Terran_SCVs and 3 medics
			this->actObjective = 2;
		}
		break;
	case 2:
		//Fire the hired worker and order it to gather gas.
		//Only execute the objective 2 once before changing to objective 3.
		this->actObjective = 3;
		break;
	case 3:
		if(medicCnt >= wantedMedicCap)
		{
			if(workerCnt >= wantedWorkerCap)
			{
				this->actObjective = 4;
			}
		}
		break;
	case 4:
		//Finished all objectives in step 2. Reset objectiveCnt and check step 2 as True
		this->actObjective = 0;
		this->steps[1] = true;
		break;
	default:
		break;
	}
#pragma endregion Calculate the objective currently being executed
	

	Broodwar->printf("Completing objective %d!", this->actObjective);
	switch(this->actObjective)
	{
	case 0:		//construct an academy. (x,y) = (3, 2)
		if(academyCnt < 1)
		{
			//if(this->hasResFor(UnitTypes::Terran_Academy))
			if(Broodwar->canMake(NULL, BWAPI::UnitTypes::Terran_Academy))
			{
				std::vector<Unit*> hiredWorkers = this->findWorker(21, UnitTypes::Terran_SCV);
				if(hiredWorkers.empty())
					hiredWorkers = this->findAndHire(21, UnitTypes::Terran_SCV, 1);
				if(!hiredWorkers.empty())
				{
					std::vector<BWAPI::TilePosition> buildingSites = this->findBuildingSites(hiredWorkers[0], UnitTypes::Terran_Academy, 3, this->commandCenters[0]);
					this->constructBuilding(buildingSites, hiredWorkers[0], UnitTypes::Terran_Academy);
				}
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
					hiredWorkers = this->findAndHire(21, UnitTypes::Terran_SCV, 1);					
				if(!hiredWorkers.empty() && !this->commandCenters.empty())
				{
					std::vector<BWAPI::TilePosition> buildingSites = this->findBuildingSites(hiredWorkers[0], UnitTypes::Terran_Refinery, 1, this->commandCenters[0]);
					if(!buildingSites.empty())
						this->constructBuilding(buildingSites, hiredWorkers[0], UnitTypes::Terran_Refinery);
				}
			}
		}
		break;
	case 2:		//Fire the hired worker and order it to gather gas.
		{
			std::vector<Unit*> hiredWorkers = this->findWorker(21, UnitTypes::Terran_SCV);
			if(!hiredWorkers.empty())	//If there is a worker with that hireID.
			{
				//Tell it to gather gas and reset its hireID
				for(std::vector<Unit*>::iterator unit = hiredWorkers.begin(); unit != hiredWorkers.end(); unit++)
				{
					//Set it to  gather gas form the refinery
					Unit* refinery = this->getClosestUnit((*unit), BWAPI::UnitTypes::Terran_Refinery, 2000);
					if(refinery != NULL)
						(*unit)->gather(refinery, false);
					else
						Broodwar->printf("Did not find a terran refinery!");
					/*for(std::set<Unit*>::const_iterator refinery = Broodwar->self()->getUnits().begin(); refinery != Broodwar->self()->getUnits().end(); refinery++)
					{
						if((*refinery)->getType() == BWAPI::UnitTypes::Terran_Refinery)
						{
							(*unit)->gather((*refinery), false);
							break;
						}
					}*/
				}
				this->findAndChange(21, 0);
			}
		}
		break;
	case 3:	//Create 5 workers and 3 medics and 1 supply depot
		{

			//Try to create workers before creating medics. To boost the economy
			if(workerCnt < 5)	//Create workers
			{
				if(!this->commandCenters.empty())
				{
					if(Broodwar->canMake(this->commandCenters[0], UnitTypes::Terran_SCV))
					{
						this->trainUnits(this->commandCenters[0], UnitTypes::Terran_SCV, wantedWorkerCap - workerCnt);
					}else
						Broodwar->printf("%s cannot create any %s!", this->commandCenters[0]->getType().getName().c_str(), UnitTypes::Terran_SCV.getName().c_str());
				}
			}
			if(medicCnt < 3)	//Create medics
			{
				for(std::set<Unit*>::const_iterator barrack = Broodwar->self()->getUnits().begin(); barrack != Broodwar->self()->getUnits().end(); barrack++)
				{
					if((*barrack)->getType() == BWAPI::UnitTypes::Terran_Barracks)
					{
						if(Broodwar->canMake(NULL, BWAPI::UnitTypes::Terran_Medic))
						{
							//Try to train medics until the AI player has 3. Also send them to chokepoint.
							this->trainUnits((*barrack), BWAPI::UnitTypes::Terran_Medic, wantedMedicCap - medicCnt);
						}else
							Broodwar->printf("%s cannot create any %s!", (*barrack)->getType().getName().c_str(), UnitTypes::Terran_Medic.getName().c_str());
						break;
					}
				}
			}
			
		}
		break;
	case 4:	//In case something happened to the earlier switch case, redo the actions.
		this->actObjective = 0;
		this->steps[1] = true;
		break;
	default:
		break; //Not necessary
	}
	//Build a supply depot if needed
	if(Broodwar->self()->supplyUsed() >= Broodwar->self()->supplyTotal())
	{
		//Build a supply depot
		if(this->hasResFor(UnitTypes::Terran_Supply_Depot))
		{
			std::vector<Unit*> hiredWorkers = this->findWorker(26, UnitTypes::Terran_SCV);
			if(hiredWorkers.empty())
				hiredWorkers = this->findAndHire(26, UnitTypes::Terran_SCV, 1);					
			if(!hiredWorkers.empty() && !this->commandCenters.empty())
			{
				std::vector<BWAPI::TilePosition> buildingSites = this->findBuildingSites(hiredWorkers[0], UnitTypes::Terran_Supply_Depot, 1, this->commandCenters[0]);
				if(!buildingSites.empty())
					this->constructBuilding(buildingSites, hiredWorkers[0], UnitTypes::Terran_Supply_Depot);
			}
		}
	}else
	{
		this->findAndChange(26, 0, UnitTypes::Terran_SCV);
	}
}

void ExampleAIModule::step3()
{
	//Intel on objectives
	int factoryCnt = Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Terran_Factory) + Broodwar->self()->incompleteUnitCount(BWAPI::UnitTypes::Terran_Factory);
	int factoryAddOnCnt = Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Terran_Machine_Shop) + Broodwar->self()->incompleteUnitCount(BWAPI::UnitTypes::Terran_Machine_Shop);
	//int siegeTankCnt = Broodwar->self()->completedUnitCount(UnitTypes::Terran_Siege_Tank_Siege_Mode) + Broodwar->self()->completedUnitCount(UnitTypes::Terran_Siege_Tank_Tank_Mode) + Broodwar->self()->incompleteUnitCount(UnitTypes::Terran_Siege_Tank_Tank_Mode) + Broodwar->self()->incompleteUnitCount(UnitTypes::Terran_Siege_Tank_Siege_Mode);
	int siegeTankCnt = this->getUnitCount(UnitTypes::Terran_Siege_Tank_Tank_Mode) + this->getUnitCount(UnitTypes::Terran_Siege_Tank_Siege_Mode);
#pragma region
	switch(this->actObjective)
	{
	case 0:		//Starting step 3, we will create a factory
		if(Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Terran_Factory) > 0)
			this->actObjective = 1;
		break;
	case 1:		//When the factory is completed, fire the hired worker so the idle worker check will pick it up
		this->actObjective = 2;
		break;
	case 2:		//Build the addon to the factory. change to objective 3 when the construction has begun.
		if( factoryAddOnCnt	/*Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Terran_Machine_Shop)*/ > 0)
			this->actObjective = 3;
		break;
	case 3:		//Research the siege-tanks siege mode
		//BWAPI::TechTypes
		if(Broodwar->self()->hasResearched(BWAPI::TechTypes::Tank_Siege_Mode) && Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Terran_Machine_Shop) > 0/* || Broodwar->self()->isResearching(BWAPI::TechTypes::Tank_Siege_Mode)*/)
		{
			this->actObjective = 4;
		}
		
		break;
	case 4:
		if(Broodwar->self()->completedUnitCount(UnitTypes::Terran_Siege_Tank_Siege_Mode) + Broodwar->self()->completedUnitCount(UnitTypes::Terran_Siege_Tank_Tank_Mode) > 2)
		{
			this->actObjective = 5;	
		}
		break;
	case 5:
		this->actObjective = 0;
		this->steps[2] = true;
		break;
	default:
		break;
	}
#pragma endregion Calculating objective clearance

#pragma region
	Broodwar->printf("Completing objective %d!", this->actObjective);
	switch(this->actObjective)
	{
	case 0:
		//Build a factory
		if(factoryCnt == 0)
		{
			if(Broodwar->canMake(NULL, UnitTypes::Terran_Factory))
			{
				//Start by hiring a worker
				int hireID = 31;
				std::vector<Unit*> hiredWorkers = this->findWorker(hireID, UnitTypes::Terran_SCV);
				if(hiredWorkers.empty())
				{
					this->findAndHire(hireID, UnitTypes::Terran_SCV, 1, hiredWorkers);
					if(!hiredWorkers.empty())
					{
						if(!this->commandCenters.empty())
						{
							this->constructBuilding(this->findBuildingSites(hiredWorkers[0], UnitTypes::Terran_Factory, 1, this->commandCenters[0]), hiredWorkers[0], UnitTypes::Terran_Factory);
						}
					}else
						Broodwar->printf("Could not find and/or assign any workers to complete objective 1");
				}
			}
		}
		break;
	case 1:
		//Fire the worker
		this->findAndChange(31, 0, UnitTypes::Terran_SCV);
		break;
	case 2:		//Build the machine-shop addon
		if(factoryAddOnCnt == 0)
		{
			if(Broodwar->canMake(NULL, UnitTypes::Terran_Machine_Shop))
			{
				//Start by finding the factory
				for(std::set<Unit*>::const_iterator factory = Broodwar->self()->getUnits().begin(); factory != Broodwar->self()->getUnits().end(); factory++)
				{
					if((*factory)->getType() == UnitTypes::Terran_Factory)
					{
						if((*factory)->buildAddon(UnitTypes::Terran_Machine_Shop))
						{
							Broodwar->printf("Building the factory addon");
						}else
							Broodwar->printf("%s could not build the %s", (*factory)->getType().getName().c_str(), UnitTypes::Terran_Machine_Shop.getName().c_str());
						break;
					}
				}
			}
		}
		break;
	case 3:		//Research the siege mode for the terran tanks. pretty cool.
		{
			TechType type = TechTypes::Tank_Siege_Mode;
			UnitType researchType = UnitTypes::Terran_Machine_Shop;	
			std::pair<Unit*,int> earlistResearched = std::pair<Unit*,int>(NULL, -1);
			if(!Broodwar->self()->hasResearched(type) && !Broodwar->self()->isResearching(type))
			{
				if(Broodwar->canResearch(NULL, type))
				{
					for(std::set<Unit*>::const_iterator researcher = Broodwar->self()->getUnits().begin(); researcher != Broodwar->self()->getUnits().end(); researcher++)
					{
						if((*researcher)->getType() == researchType)
						{
							if(Broodwar->canResearch((*researcher), type))
							//if((*researcher)->research(type))
							{
								int timeLeft = (*researcher)->getRemainingResearchTime();
								if(earlistResearched.first == NULL || earlistResearched.second > timeLeft)
									earlistResearched = std::pair<Unit*,int>((*researcher), timeLeft);
								if(timeLeft == 0)
									break;
							}
						}
					}
					if(earlistResearched.first->research(type))
					{
						Broodwar->printf("%s can research %s", earlistResearched.first->getType().c_str(), type.c_str());
					}else
						Broodwar->printf("%s failed to research %s", earlistResearched.first->getType().c_str(), type.c_str());
				}
			}
		}
		break;
	case 4:
		if(siegeTankCnt < 3)
		{
			if(Broodwar->canMake(NULL, UnitTypes::Terran_Siege_Tank_Tank_Mode))
			{
				//Start by finding the factory
				for(std::set<Unit*>::const_iterator factory = Broodwar->self()->getUnits().begin(); factory != Broodwar->self()->getUnits().end(); factory++)
				{
					if((*factory)->getType() == UnitTypes::Terran_Factory)
					{
						this->trainUnits((*factory), UnitTypes::Terran_Siege_Tank_Tank_Mode, 3 - siegeTankCnt);
					}
				}
			}
		}
		break;
	case 5:
		this->actObjective = 0;
		this->steps[2] = true;
		break;
	default:
		break;
	}
#pragma endregion Completing objective
}

void ExampleAIModule::step4()
{

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
				if(Broodwar->canMake(NULL, unit)/*this->hasResFor(unit)*/)
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

void ExampleAIModule::constructBuilding(std::vector<BWAPI::TilePosition> positions, Unit* worker, BWAPI::UnitType building)
{
	//Check if pos exists
	if(!positions.empty())
	{
		//Check if worker is a worker
		if(worker->getType().isWorker())
		{
			//Check if building is a building
			if(building.isBuilding())
			{
				bool success = false;
				for(std::vector<TilePosition>::const_iterator posItr = positions.begin(); posItr != positions.end() && !success; posItr++)
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
									/*Broodwar->printf("%s could not build %s!",worker->getType().getName().c_str(), building.getName().c_str());*/
								}
							}else
								Broodwar->printf("%s cannot reach designated tile position!", worker->getType().getName().c_str());
						}else
							Broodwar->printf("%s cannot be built at designated position!", building.getName().c_str());
					}else
						Broodwar->printf("Not enough materials to begin construction of %s!", building.getName().c_str());
				}
				if(!success)
					Broodwar->printf("%s could not build %s!",worker->getType().getName().c_str(), building.getName().c_str());
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
		//Check every frame
#pragma region
	//Check if there has been any units created
#pragma region
	if(!this->needToAdd.empty())
	{
		//For every unit, check if it has been finished yet. If so then add it to the responding lists
		for(std::set<Unit*>::iterator unit = this->needToAdd.begin(); unit != this->needToAdd.end(); unit++)
		{
			UnitType unitType = (*unit)->getType();
			if((*unit)->isCompleted())
			{
				//Add to lists according to type
				if(!unitType.isBuilding())
				{
					if(unitType == BWAPI::UnitTypes::Terran_SCV)
					{
						(*unit)->stop();		//We need to make workers idle so they are fetched be the Idle Gather behavior
						this->builders.insert((*unit));
						this->builders2.push_back(std::pair<Unit*, int>((*unit), 0));	//Start it off with a HiredID of 0. May change in the future.
					}else
					{
						this->army.push_back(std::pair<Unit*, int>((*unit), 0));		//Start new army units off with a ID of 0.
					}
				}else
				{
					if(unitType == UnitTypes::Terran_Command_Center)
						this->commandCenters.push_back((*unit));
				}
				//Remove it from this list
				this->needToAdd.erase(unit);
			}
		}
	}
#pragma endregion Unit creation logic

	//Do the army behavior
#pragma region
	for(std::vector<std::pair<Unit*, int>>::iterator armyUnit = this->army.begin(); armyUnit != this->army.end(); armyUnit++)
	{
		BWAPI::UnitType type = armyUnit->first->getType();
		if(type == UnitTypes::Terran_Marine)
		{	//Do the marine behavior
#pragma region
			//Check stim pack cooldown
			if(armyUnit->first->getSpellCooldown() == 0)
			{
				//Check if health is above 50%. If so, use stim pack ability
				if(armyUnit->first->getHitPoints() >= armyUnit->first->getType().maxHitPoints() * 0.5)
				{
					TechType techToUse = TechTypes::Stim_Packs;
					if(armyUnit->first->getEnergy() >= techToUse.energyUsed())
					{
						if(Broodwar->self()->hasResearched(techToUse))
						{
							//The set of abilities the unit has the tech and energy to use
							std::set<BWAPI::TechType> abilities = type.abilities();
							//Check for a techToUse
							if(!abilities.empty() || abilities.find(techToUse) != abilities.end())
							{	//Found the stim pack ability
								if(!armyUnit->first->useTech(techToUse))
								{	//Invalid tech
									Broodwar->printf("%s failed to use %s because it was invalid!", type.c_str(), techToUse.c_str());
								}	
							}
						}
					}
				
				}
			}
#pragma endregion Marine behavior
		}else if(type == UnitTypes::Terran_Siege_Tank_Tank_Mode || type == UnitTypes::Terran_Siege_Tank_Siege_Mode)
		{	//Do the tank behavior
#pragma region
			
			TechType techToUse = TechTypes::Tank_Siege_Mode;
			if(Broodwar->self()->hasResearched(techToUse))
			{
				//If there is a unit within sighrange, go into siege mode, if not then proceed in tank mode
				//check if there are units nearby
				std::set<Unit*> unitsInRange = armyUnit->first->getUnitsInRadius(type.sightRange());
				if(!unitsInRange.empty())
				{
					if(armyUnit->first->getType() == UnitTypes::Terran_Siege_Tank_Tank_Mode)
					{
						//Try to go into siege mode
						//The set of abilities the unit has the tech and energy to use
						std::set<BWAPI::TechType> abilities = type.abilities();
						//Check for a techToUse
						if(!abilities.empty() || abilities.find(techToUse) != abilities.end())
						{	//Found the stim pack ability
							if(!armyUnit->first->useTech(techToUse))
							{	//Invalid tech
								Broodwar->printf("%s could not go into %s!", type.c_str(), UnitTypes::Terran_Siege_Tank_Siege_Mode.c_str());
							}	
						}
					}
				}else
				{
					if(armyUnit->first->getType() == UnitTypes::Terran_Siege_Tank_Siege_Mode)
					{
						//Try to go into tank mode
						//The set of abilities the unit has the tech and energy to use
						std::set<BWAPI::TechType> abilities = type.abilities();
						//Check for a techToUse
						if(!abilities.empty() || abilities.find(techToUse) != abilities.end())
						{	//Found the stim pack ability
							if(!armyUnit->first->useTech(techToUse))
							{	//Invalid tech
								Broodwar->printf("%s could not go into %s!", type.c_str(), UnitTypes::Terran_Siege_Tank_Tank_Mode.c_str());
							}	
						}
					}
				}
			}
#pragma endregion Tank behavior
		}else if(type == UnitTypes::Terran_Medic)
		{	//Do the medic behavior
#pragma region
			//Check if cooldown on healing is off
			if(armyUnit->first->getSpellCooldown() <= 0)
			{
				//Get units in range and find the least healthy unit as healing target
				std::set<Unit*> inRange = armyUnit->first->getUnitsInWeaponRange(type.groundWeapon());
				std::pair<Unit*,int> leastHealth(NULL, -1);
				for(std::set<Unit*>::iterator inRangeUnit = inRange.begin(); inRangeUnit != inRange.end(); inRangeUnit++)
				{
					if((*inRangeUnit)->getPlayer() == Broodwar->self())
					{
						int curHP = (*inRangeUnit)->getHitPoints();
						if(leastHealth.first != NULL || curHP < leastHealth.second)
						{
							leastHealth.first = (*inRangeUnit);
							leastHealth.second = (*inRangeUnit)->getHitPoints();
						}
					}
				}
				if(leastHealth.first != NULL)
				{
					if(!(*armyUnit).first->useTech(TechTypes::Healing, leastHealth.first))
					{	//Invalid tech
						Broodwar->printf("%s could not use %s on %s!", type.c_str(), TechTypes::Healing.c_str(), leastHealth.first->getType().c_str());
					}
				}
			}
			
#pragma endregion Medic behavior
		}else
		{	//Some unknown unit. Don't print anything, if there is a problem it will show on every frame which will be extremly anoying
		}
	}
#pragma endregion Army behavior

#pragma endregion Frame dependant behavior

	//Check every second frame. Slightly less important behaviors such as the Idle Worker behavior
#pragma region
	if(Broodwar->getFrameCount() % 2 == 0)
	{
#pragma region
		//Apply gathering behavior for workers
		for(std::vector<std::pair<Unit*, int>>::iterator unit = this->builders2.begin(); unit != this->builders2.end(); unit++)
		{
			//Make idle workers gather either minerals or gas depending on the need
			if((*unit).second == 0 && (*unit).first->getType() == UnitTypes::Terran_SCV)
			{
				if((*unit).first->isIdle())
				{
					//Assign workload
					int mGatherCnt = 0;
					int gGatherCnt = 0;
					for(std::vector<std::pair<Unit*, int>>::iterator allUnits = this->builders2.begin(); allUnits != this->builders2.end(); allUnits++)
					{
						if(allUnits != unit && allUnits->first->getType() == UnitTypes::Terran_SCV)
						{
							if((*allUnits).first->isGatheringMinerals())
								mGatherCnt++;
							else if((*allUnits).first->isGatheringGas())
								gGatherCnt++;
						}
					}
					Broodwar->printf("Workers gathering gas: %d", gGatherCnt);
					Broodwar->printf("Workers gather minerals: %d", mGatherCnt);
					if(gGatherCnt < 3 && Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Terran_Refinery) > 0)
					{
						Broodwar->printf("Assigning idle worker with hireID %d to gather gas.", (*unit).second);
						Unit* refinery = this->getClosestUnit((*unit).first, BWAPI::UnitTypes::Terran_Refinery, 2000);
						if(refinery != NULL)
							(*unit).first->rightClick(refinery, false);
						else
							Broodwar->printf("Did not find a terran refinery!");
					}else
					{
						Broodwar->printf("Assigning idle worker with hireID %d to gather minerals.", (*unit).second);
						(*unit).first->rightClick(this->getClosestMineral((*unit).first),false);
					}
					
				}
			}
		}
#pragma endregion Idle worker gathering behavior
	}
#pragma endregion SecondFrameUpdates


#pragma region
	//Call every 100:th frame
	if (Broodwar->getFrameCount() % 60 == 0)
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
						this->step3();
						break;
					case 3:
						this->step4();
						break;
					case 4:	/*currently no fifth step that we want to achive within the AI behaviour*/
						break;
					default:	//So we have an exit
						break;
				}
				//Only initialize one step at a time
				break;
			}
		}
	}
#pragma endregion Call functions with AI logic divitions
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
	int hireCnt = 0;
	for(std::vector<std::pair<Unit *, int>>::iterator i = this->builders2.begin(); i != this->builders2.end() && hireCnt < amount; i++)
		{
			//We are now accessing the pair structure with the builders and their assigned states. Hire a builder and set their ID-state
			if(i->first->getType() == type) //Check type
			{
				if(i->second == 0 || i->second == hireID)	//Check available
				{
					//Give it the ID-state which will represent it being issued this workload
					i->second = hireID;		//2X for step 2, X1 for objective 1
					result.push_back(i->first);	//Store the unit pointer, may be a better idéa to store the ID.
					hireCnt++;	//Increase the hired unit counter.
				}
			}
		}
	return result;
}

int ExampleAIModule::findAndChange(int origID, int resultID)
{
	return this->findAndChange(origID, resultID, BWAPI::UnitTypes::AllUnits);
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

int ExampleAIModule::getUnitCount(UnitType type, bool completedOnly)
{
	int amount = Broodwar->self()->completedUnitCount(type);
	if(!completedOnly)
	{
		for(std::set<Unit*>::const_iterator unit = this->needToAdd.begin(); unit != this->needToAdd.end(); unit++)
		{
			if((*unit)->getType() == type)
			{
				amount++;
			}
		}
	}
	return amount;
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
	return this->getClosestUnit(mine, targetType, mine->getType().sightRange());
}

Unit* ExampleAIModule::getClosestUnit(Unit* mine, BWAPI::UnitType targetType, int radius)	//Will return the closest unit of right type within radius
{
	//Get the set of units within given radius of the unit, itterate through them and select the one closest
	Unit* closest = NULL;
	std::set<Unit*> unitsInRange = mine->getUnitsInRadius(radius);
	int distance(radius);
	for(std::set<Unit*>::const_iterator targetUnit = unitsInRange.begin(); targetUnit != unitsInRange.end(); targetUnit++)
	{
		//Check corrected type
		if((*targetUnit)->getType() == targetType)
		{	//Check distance
			if(mine->getDistance((*targetUnit)) < distance)
			{
				distance = mine->getDistance((*targetUnit));
				closest = (*targetUnit);
			}
		}
	}
	return closest;
}

Unit* ExampleAIModule::getClosestMineral(Unit* unit)
{
	Unit* closestMineral = NULL;
	for(std::set<Unit*>::iterator m=Broodwar->getMinerals().begin();m!=Broodwar->getMinerals().end();m++)
	{
		if (closestMineral==NULL || unit->getDistance(*m)<unit->getDistance(closestMineral))
		{	
			closestMineral=*m;
		}
	}
	return closestMineral;
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
		//Our logic for inserting it into our lists
		this->needToAdd.insert(unit);
	}
}

//Called when a unit has been destroyed.
void ExampleAIModule::onUnitDestroy(BWAPI::Unit* unit)
{
	if (unit->getPlayer() == Broodwar->self())
	{
		Broodwar->sendText("My unit %s [%x] has been destroyed at (%d,%d)",unit->getType().getName().c_str(),unit,unit->getPosition().x(),unit->getPosition().y());
		if(!unit->getType().isBuilding())
		{
			//Remove the unit from the builders list
			if(unit->getType() == BWAPI::UnitTypes::Terran_SCV)
			{	
				this->builders.erase(this->builders.find(unit));
				//Remove the unit from the hired builders list
				for(std::vector<std::pair<Unit*, int>>::iterator i = this->builders2.begin(); i != this->builders2.end(); i++)
				{
					if((*i).first->getID() == unit->getID())
					{
						this->builders2.erase(i);
						break;
					}
				}
			}else
			{
				//It is PROBABLY a army unit
				for(std::vector<std::pair<Unit*, int>>::iterator armyUnit = this->army.begin(); armyUnit != this->army.end(); armyUnit++)
				{
					if((*armyUnit).first->getID() == unit->getID())
					{
						this->army.erase(armyUnit);
						break;
					}
				}
			}
		}else
		{
			if(unit->getType() == UnitTypes::Terran_Command_Center)
			{
				for(std::vector<Unit*>::iterator building = this->commandCenters.begin(); building != this->commandCenters.end(); building++)
				{
					if((*building) == unit)
					{
						this->commandCenters.erase(building);
						break;
					}
				}
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
	if(unit->getPlayer() == Broodwar->self())
	{
		//Add it to our list of needToAdd
		/*if(!unit->getType().isBuilding())*/
		/*this->needToAdd.insert(unit);*/
	}
}
