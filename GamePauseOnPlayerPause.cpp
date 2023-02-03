// GamePauseOnPlayerPause.cpp

#include "bzfsAPI.h"
#include "StateDatabase.h"
#include "../src/bzfs/bzfs.h"

int checkRange(int min, int max, int amount) {
    int num = 0;
    if ((amount >= min) && (amount <= max)) {
        num = 1;
    } else if ((amount < min) || (amount > max)) {
        num = 0;
    } else {
        num = -1;
    }
    return num;
}

int checkPlayerSlot(int player) {
    return checkRange(0,199,player); // 199 because of array.
}

bool checkIfValidPlayer(int player) {
    bool valid = false;
    bz_BasePlayerRecord *playerRec = bz_getPlayerByIndex ( player );
    if (playerRec) {
        valid = true;
    }
    bz_freePlayerRecord(playerRec);
    return valid;
}

std::string getCallsign(int player) {
    std::string Callsign = "(UNKNOWN)";
    bz_BasePlayerRecord *playerData;

    playerData = bz_getPlayerByIndex(player);
    if (playerData)
        Callsign = playerData->callsign.c_str();
    bz_freePlayerRecord(playerData);
    return Callsign;
}

// Basic utility functions

// Per-player variables:

static void sendPlayerBZDB(int playerID, const std::string& key,
                                         const std::string& value)
{
  void *bufStart = getDirectMessageBuffer();
  void *buf = nboPackUShort(bufStart, 1);
  buf = nboPackUByte (buf, key.length());
  buf = nboPackString(buf, key.c_str(), key.length());
  buf = nboPackUByte (buf, value.length());
  buf = nboPackString(buf, value.c_str(), value.length());

  const int len = (char*)buf - (char*)bufStart;
  directMessage(playerID, MsgSetVar, len, bufStart);
}


static void sendPlayerBZDB(int playerID, const std::string& key,
                                         const float value)
{
  char buf[64];
  snprintf(buf, sizeof(buf), "%f", value);
  sendPlayerBZDB(playerID, key, buf);
}

// Per player variables

void pausePlayers(void) {
    bz_APIIntList *playerList = bz_newIntList();
    if (playerList) {
        bz_getPlayerIndexList(playerList);
        for ( unsigned int i = 0; i < playerList->size(); i++) {
        int playerID = (*playerList)[i];
        bz_BasePlayerRecord *playRec = bz_getPlayerByIndex ( playerID );
        if (playRec != NULL) {
            if (playRec->team != eObservers) {
                sendPlayerBZDB(playerID,"_tankSpeed","0.000001");
                sendPlayerBZDB(playerID,"_shotSpeed","0.001");
                sendPlayerBZDB(playerID,"_jumpVelocity","0.001");
                sendPlayerBZDB(playerID,"_tankAngVel","0.000001");
            }
        }
        bz_freePlayerRecord(playRec);
        }
    }
    bz_deleteIntList(playerList);
}

void resumePlayers(void) {
    bz_APIIntList *playerList = bz_newIntList();
    if (playerList) {
        bz_getPlayerIndexList(playerList);
        for ( unsigned int i = 0; i < playerList->size(); i++) {
        int playerID = (*playerList)[i];
        bz_BasePlayerRecord *playRec = bz_getPlayerByIndex ( playerID );
        if (playRec != NULL) {
            if (playRec->team != eObservers) {
                sendPlayerBZDB(playerID,"_tankSpeed", bz_getBZDBDouble("_tankSpeed"));
                sendPlayerBZDB(playerID,"_shotSpeed", bz_getBZDBDouble("_shotSpeed"));
                sendPlayerBZDB(playerID,"_jumpVelocity", bz_getBZDBDouble("_jumpVelocity"));
                sendPlayerBZDB(playerID,"_tankAngVel", bz_getBZDBDouble("_tankAngVel"));
            }
        }
        bz_freePlayerRecord(playRec);
        }
    }
    bz_deleteIntList(playerList);
}

class GamePauseOnPlayerPause : public bz_Plugin, bz_CustomSlashCommandHandler
{
    public:
    const char* Name(){return "GamePauseOnPlayerPause[0.0.0]";}
    void Init ( const char* /*config*/ );
    void Event(bz_EventData *eventData );
    void Cleanup ( void );
    bool SlashCommand ( int playerID, bz_ApiString command, bz_ApiString message, bz_APIStringList* params );

    int gameInSession = 0;
    int playerPause[200];
    // ^This contains some pretty important logic to use.
    int pauseCount = 0;
};

void GamePauseOnPlayerPause::Init (const char* commandLine) {
  Register(bz_ePlayerJoinEvent);
  Register(bz_ePlayerPartEvent);
  Register(bz_ePlayerPausedEvent);
  Register(bz_ePlayerDieEvent);
  Register(bz_eGameStartEvent);
  Register(bz_eGameEndEvent);
  Register(bz_eShotFiredEvent);
  bz_registerCustomSlashCommand("pause",this);
  bz_registerCustomSlashCommand("resume",this);
  bz_debugMessage(1,"GamePauseOnPlayerPause plugin loaded");
}

void GamePauseOnPlayerPause::Cleanup (void) {
   Flush();
   bz_removeCustomSlashCommand("pause");
   bz_removeCustomSlashCommand("resume");
   bz_debugMessage(1,"GamePauseOnPlayerPause plugin unloaded");
}


bool GamePauseOnPlayerPause::SlashCommand(int playerID,bz_ApiString command,bz_ApiString message,bz_APIStringList* params) {
    if (command == "resume") {
        if(gameInSession != 0) {
            bz_resumeCountdown(playerID);
            pauseCount = 0;
            resumePlayers();
        } else {
            bz_sendTextMessage(BZ_SERVER, playerID,"No Game running!");
        }
        return true;
    }

    if (command == "pause") {
        if(gameInSession != 0){
            bz_pauseCountdown(playerID);
            pauseCount += 1;
            pausePlayers();
        } else {
            bz_sendTextMessage(BZ_SERVER, playerID,"No Game running!");
        }
        return true;
    }
    
    if (command == "gamepause") {
        bz_sendTextMessage(BZ_SERVER,playerID,"Game Pause on Player Pause");
        bz_sendTextMessage(BZ_SERVER,playerID,"==========================");
        bz_sendTextMessage(BZ_SERVER,playerID,"Pauses game when player(s) pause or use /pause");
        bz_sendTextMessage(BZ_SERVER,playerID,"Resumes game when player(s) unpause or use resume");
        bz_sendTextMessage(BZ_SERVER,playerID,"To pause a game, pause use: /pause");
        bz_sendTextMessage(BZ_SERVER,playerID,"To resume a game, unpause or use: /resume");
        bz_sendTextMessage(BZ_SERVER,playerID,"Game resumes automatically when no players are paused.");
        bz_sendTextMessage(BZ_SERVER,playerID,"==========================");
    }
    return false;
}


void GamePauseOnPlayerPause::Event(bz_EventData *eventData ) {
   switch (eventData->eventType) {
    case bz_ePlayerJoinEvent: {
      bz_PlayerJoinPartEventData_V1* joinData = (bz_PlayerJoinPartEventData_V1*)eventData;
      int player = joinData->playerID;
      if (checkPlayerSlot(player)==1) {
        playerPause[player]=0;
        bz_sendTextMessage(BZ_SERVER, player,"Game Pause on Player Pause");
        bz_sendTextMessage(BZ_SERVER, player,"For info, use: /gamepause");
      }
    }break;

    case bz_ePlayerPartEvent: {
      bz_PlayerJoinPartEventData_V1* partData = (bz_PlayerJoinPartEventData_V1*)eventData;
      int player = partData->playerID;
      if (checkPlayerSlot(player)==1) {
        if (playerPause[player] == 1) {
            if (pauseCount >= 1) {
                pauseCount -= 1;
                // We don't reset and wind in negatives.
                if (pauseCount == 0) {
                    resumePlayers();
                    bz_resumeCountdown(BZ_SERVER);
                }
            }
        }
      } 
    }break;

    case bz_ePlayerPausedEvent: {
      bz_PlayerPausedEventData_V1* pauseData = (bz_PlayerPausedEventData_V1*)eventData;
      int player = pauseData->playerID;
      if (checkPlayerSlot(player)==1) {
        if (gameInSession == 1) {
        // ^ We don't care if players are pausing and no game is on going.
            if (pauseData->pause == true) {
                if (pauseCount == 0) {
                    pausePlayers();
                    bz_pauseCountdown(player);
                    bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, 
                        "%s has paused. Game has been paused.", getCallsign(player).c_str());
                        // While we could even use the tick event to make pause/resuming more in sync...
                        // It's not really worth the effort.
                }
                pauseCount += 1;
                playerPause[player]=1;
            } else {
                if (pauseCount >= 1) {
                    pauseCount -= 1;
                    if (pauseCount == 0) {
                        resumePlayers();
                        bz_resumeCountdown(player);
                        bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, 
                            "%s has unpaused. Game has resumed.", getCallsign(player).c_str());
                        // Here it seems to make sense, since players can adjust their movements 5 seconds before...
                        // ..the game begins, but not move all over the place.
                    }
                }
                playerPause[player]=0;
            }
        }
      } 
    }break;

      case bz_ePlayerDieEvent: {
      bz_PlayerDieEventData_V2* deathData = (bz_PlayerDieEventData_V2*)eventData;
      int killer = deathData->killerID;
      int player = deathData->playerID;
      if (checkPlayerSlot(player)==1) {
          if (playerPause[player] == 1) {
            // Simple way of not screwing up pauseCount
            playerPause[player]=0;
            if (pauseCount >= 1) {
                pauseCount -= 1;
                if (pauseCount == 0) {
                    pausePlayers();
                    bz_pauseCountdown(player);
                }
            }
          
          }
          
      }
      if (checkPlayerSlot(killer)==1) {
          if (pauseCount >= 1) {
              bz_sendTextMessagef(BZ_SERVER, killer, "%s, don't kill players. Game is paused.", getCallsign(killer).c_str());
              //@TODO consider if it is worth it to implement spawn control or mutual kills.
          }
      }
    }break;



    case bz_eGameStartEvent: {
      gameInSession = 1;
      resumePlayers();
    }break;

    case bz_eGameEndEvent: {
      gameInSession = 0;
      pauseCount = 0;
    }break;

    case bz_eShotFiredEvent: {
      bz_ShotFiredEventData_V1* shotData = (bz_ShotFiredEventData_V1*)eventData;
      int player = shotData->playerID;
      if (pauseCount >= 1) {
        bz_sendTextMessagef(BZ_SERVER, player, "%s, don't shoot. Game is paused.", getCallsign(player).c_str());
      } 
    }break;

    default:{ 
    }break;
  }
}

BZ_PLUGIN(GamePauseOnPlayerPause)
