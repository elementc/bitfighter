//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef _LEVEL_H_
#define _LEVEL_H_

#include "gridDB.h"     // Parent class

#include "LevelSource.h"      // For LevelInfo def
#include "teamInfo.h"
#include "WallEdgeManager.h"

#include "tnlTypes.h"
#include "tnlNetBase.h"

#include "boost/smart_ptr/shared_ptr.hpp"

#include "Test.h"

using namespace std;
using namespace TNL;


namespace Zap
{

class BotNavMeshZone;
class Game;
class GameType;
class PolyWall;
class WallItem;

class Level : public GridDatabase
{
   typedef GridDatabase Parent;

private:
   S32 mVersion;
   F32 mLegacyGridSize; 
   U32 mDatabaseId;
   RefPtr<GameType> mGameType;
   string mLevelHash;

   bool mLoaded;

   LevelInfo mLevelInfo;      // Summary of the level

   Game *mGame;               // Game we've been added to -- NULL until onAddedToGame() is called

   WallEdgeManager mWallEdgeManager;

   U32 mLevelDatabaseId;

   Vector<string> mRobotLines;
   Vector<string> mTeamChangeLines;

   boost::shared_ptr<TeamManager> mTeamManager;

   // Zone-related
   GridDatabase mBotZoneDatabase;
   Vector<BotNavMeshZone *> mAllZones;

   void initialize();
   void parseLevelLine(const string &line, const string &levelFileName);
   bool processLevelLoadLine(U32 argc, S32 id, const char **argv, string &errorMsg);  
   bool processLevelParam(S32 argc, const char **argv);

public:
   Level();                         // Constructor
   Level(const string &levelCode);  // Constructor with passed levelcode, primarily used for testing
   virtual ~Level();                // Destructor

   Level *clone() const;            // Duplicate level
   static const U32 CurrentLevelFormat = 2;

   void onAddedToGame(Game *game);

   bool loadLevelFromFile(const string &filename, U64 sqliteLevelId);   
   void loadLevelFromString(const string &contents, U64 sqliteLevelId, const string &filename = "");
   void loadLevelFromStream(istream &stream, const string &streamSource, const string &hash, U64 sqliteLevelId);
   
   void validateLevel();
   U64 getSqliteLevelId() const;

   //LevelInfo &getLevelInfo();

   void buildWallEdgeGeometry(Vector<Point> &wallEdgePoints);
   void snapAllEngineeredItems(bool onlyUnsnapped) const;

   string toLevelCode() const;

   U32 getLevelDatabaseId() const;
   void setLevelDatabaseId(U32 id);

   void addWallItem(WallItem *wall, Game *game = NULL);
   const GridDatabase *getWallEdgeDatabase() const;
   const WallEdgeManager *getWallEdgeManager() const;

   void beginBatchGeomUpdate();                                     
   void endBatchGeomUpdate(GridDatabase *gameObjectDatabase, 
                           const Vector<WallSegment const *> &wallSegments, 
                           Vector<Point> &wallEdgePoints,    // <== gets modified!
                           bool modifiedWalls);

   F32 getLegacyGridSize() const;
   GameType *getGameType() const;
   void setGameType(GameType *gameType);

   // Note that these return modifiable copies!
   GridDatabase &getBotZoneDatabase();
   Vector<BotNavMeshZone *> &getBotZoneList();

   string getHash() const;

   bool getAddedToGame() const;

   void addBots(Game *game);

   // Manipulating objects
   void deleteObject(S32 serialNumber);
   void swapObject(S32 replaceThisObject, const BfObject *withThisObject);

   BfObject *findObjBySerialNumber(S32 serialNumber) const;

   // Level metadata
   string getLevelName() const;
   string getLevelCredits() const;
   S32 getWinningScore() const;


   // Team methods
   boost::shared_ptr<TeamManager> getTeamManager() const;
   void setTeamManager(boost::shared_ptr<TeamManager> teamManager);

   S32 getTeamCount() const;

   StringTableEntry getTeamName(S32 index) const;
   void setTeamName(S32 index, const string &name) const;

   const Color &getTeamColor         (S32 index) const;
   const Color &getTeamHealthBarColor(S32 index) const;
   void setTeamColor(S32 index, const Color &color) const;

   void removeTeam(S32 teamIndex) const; 

   void addTeam(AbstractTeam *team) const;
   void addTeam(AbstractTeam *team, S32 index) const;

   AbstractTeam *getTeam(S32 teamIndex) const;
   void setTeamHasFlag(S32 teamIndex, bool hasFlag) const;
   bool getTeamHasFlag(S32 teamIndex) const;

   void replaceTeam(AbstractTeam *team, S32 index) const;
   void clearTeams() const;             
   string getTeamLevelCode(S32 index) const;

   void clearAllObjects();


   bool makeSureTeamCountIsNotZero() const;        // Because zero teams can cause crashiness


   S32 getBotCount() const;

   ///// Testing
   friend class EditorTest;
   FRIEND_TEST(EditorTest, findSnapVertexTest);
   FRIEND_TEST(EditorTest, wallCentroidForRotationTest);

   friend class ObjectTest;      // TODO: This is probably not handled quite right...
};


}

#endif
