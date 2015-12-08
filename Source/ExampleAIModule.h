#pragma once
#include <BWAPI.h>

#include <BWTA.h>
#include <windows.h>

extern bool analyzed;
extern bool analysis_just_finished;
extern BWTA::Region* home;
extern BWTA::Region* enemy_base;
DWORD WINAPI AnalyzeThread();

using namespace BWAPI;
using namespace BWTA;

class ExampleAIModule : public BWAPI::AIModule
{
private:
	bool steps[5];
	int actObjective;
	std::set<Unit*> builders;
	std::vector<std::pair<Unit*, int>> builders2;
	std::vector<Unit*> commandCenters;
	std::set<Unit*> newlyCreatedUnits;
	//std::vector<TilePosition> buildPositions;
public:
	//Methods inherited from BWAPI:AIModule
	virtual void onStart();
	virtual void onEnd(bool isWinner);
	virtual void onFrame();
	virtual void onSendText(std::string text);
	virtual void onReceiveText(BWAPI::Player* player, std::string text);
	virtual void onPlayerLeft(BWAPI::Player* player);
	virtual void onNukeDetect(BWAPI::Position target);
	virtual void onUnitDiscover(BWAPI::Unit* unit);
	virtual void onUnitEvade(BWAPI::Unit* unit);
	virtual void onUnitShow(BWAPI::Unit* unit);
	virtual void onUnitHide(BWAPI::Unit* unit);
	virtual void onUnitCreate(BWAPI::Unit* unit);
	virtual void onUnitDestroy(BWAPI::Unit* unit);
	virtual void onUnitMorph(BWAPI::Unit* unit);
	virtual void onUnitRenegade(BWAPI::Unit* unit);
	virtual void onSaveGame(std::string gameName);
	virtual void onUnitComplete(BWAPI::Unit *unit);

	//Own methods
	void drawStats();
	void drawTerrainData();
	void showPlayers();
	void showForces();
	Position findGuardPoint();
	
	//More own methods
	std::vector<TilePosition> findBuildingSites(Unit* worker, BWAPI::UnitType type, int amount, Unit* commandCenter);

	void step1();
	void step2();
	
	void constructBuilding(std::vector<BWAPI::TilePosition> pos, Unit* worker, BWAPI::UnitType building);
	void trainUnits(Unit* trainer, BWAPI::UnitType unit, int amount);
	bool hasResFor(UnitType type)const;
	
	

	std::vector<Unit*> findWorker(int hireID, BWAPI::UnitType type = BWAPI::UnitTypes::Terran_SCV);
	bool findAndHire(int hireID, BWAPI::UnitType type, int amount, std::vector<Unit*> &storeIn);
	std::vector<Unit*> findAndHire(int hireID, BWAPI::UnitType type, int amount);
	int findAndChange(int origID, int resultID);
	int findAndChange(int origID, int resultID, BWAPI::UnitType type);
	
	bool commandGroup(std::vector<Unit*> units, BWAPI::UnitCommand command);
	Unit* getClosestUnit(Unit* mine, BWAPI::UnitType targetType);
	Unit* getClosestUnit(Unit* mine, BWAPI::UnitType targetType, int radius);
	Unit* getClosestMineral(Unit* unit);
};
