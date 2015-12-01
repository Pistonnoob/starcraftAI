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
	bool stepsCompleted[5];
	std::set<Unit*> builders;
	//std::vector<std::pair<Unit*, int>> builders2;
	std::vector<TilePosition> buildPositions;
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

	std::vector<TilePosition> findBuildingSites(Unit* worker, BWAPI::UnitType type, int amount, Unit* commandCenter);

	void step1();
	void step2();
	void buildBarracks(BWAPI::TilePosition pos);
	void buildSupplyDepot(BWAPI::TilePosition pos, Unit* worker);
	void constructBuilding(std::vector<BWAPI::TilePosition> pos, Unit* worker, BWAPI::UnitType building);
	void trainUnits(int amount);
	void trainUnits(Unit* trainer, BWAPI::UnitType unit, int amount);
};
