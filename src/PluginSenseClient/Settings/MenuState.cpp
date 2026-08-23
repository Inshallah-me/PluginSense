#include "MenuState.hpp"

#include <Windows.h>

namespace vars
{
	float colorAccent[4] = { 157.f / 255.f, 196.f / 255.f, 29.f / 255.f, 1.0f };
	int  menuKey = VK_HOME;
	int  menuKeySuppress = 0;
	int  menuAnimSpeed = 100;
	bool menuConfirmations = false;
	bool menuKeybinds = false;
}

namespace style
{
	float glassWindow = 0.20f;
	float glassPanel = 0.50f;
	float glassWidget = 0.22f;
	float glassPopup = 0.50f;
	int outlineAlpha = 60;
}

namespace menu_state
{
	bool clantagEnabled = false;
	bool animatedClantag = false;
	bool velocityText = false;
	bool velocityGraph = false;
	bool keystrokes = false;
	float velocityOffset = 120.f;
	bool damageLogEnabled = false;
	bool hitlogEnabled = false;
	bool hitlogVictim = false;
	int hitlogType = 0;
	bool damageIndicator = false;
	bool spoof = false;
	bool fakeCooldown = false;
	bool officialBan = false;
	bool vacBan = false;
	int clantagSelection = 0;
	int animationSelection = 0;
	int fakeCooldownValue = 0;
	int fakeCooldownTime = 0;
	int fakeCooldownCustomDays = 30;
	float clantagSpeed = 0.3f;
	float damageScale = 1.f;
	float damageTime = 3.f;
	char playerName[32] = {};
	char customClantag[32] = {};
	ImVec4 lowSpeed = { 1.f, 0.3f, 0.3f, 1.f };
	ImVec4 midSpeed = { 1.f, 0.8f, 0.2f, 1.f };
	ImVec4 highSpeed = { 0.2f, 1.f, 0.3f, 1.f };
	ImVec4 graphColor = { 0.65f, 0.55f, 0.98f, 1.f };
	ImVec4 damageBody = { 1.f, 1.f, 1.f, 1.f };
	ImVec4 damageHead = { 1.f, 1.f, 0.f, 1.f };
	bool chatSpammer = false;
	bool chatSpammerRandom = false;
	bool killSay = false;
	bool killSayRandom = false;
	float sendDelay = 1.f;
	int chatCount = 1;
	int killCount = 1;
	char chatMessages[16][128] = {};
	char killMessages[16][128] = {};

	bool lobbyPremier = false;
	int lobbyPremierRating = 30000;
	int lobbyPremierWins = 999;
	bool lobbyWingman = false;
	int lobbyWingmanRating = 18;
	int lobbyWingmanWins = 999;
	bool lobbyLevel = false;
	int lobbyLevelValue = 40;
	int lobbyXp = 4999;

	WorldScene worldScene;
	WorldWeather worldWeather;

	bool weaponModelEnabled = false;
	int weaponModelWeapon = 0;
	int weaponModelModel = 0;

	bool helperEnabled = false;
	bool autoMove = true;            // 自动走向点位
	bool autoAim = true;             // aim_assist
	bool autoExecute = true;         // auto_release
	int aimSpeed = 18;               // aim_smoothing
	float aimThreshold = 0.35f;      // aim_threshold
	int lockTimeMs = 45;             // lock_time_ms
	int drawDistance = 800;          // draw_distance
	int standDistance = 220;         // stand_distance
	float standRadius = 22.f;        // stand_radius
	float releaseRadius = 6.f;       // release_radius
	float heightTolerance = 8.f;     // height_tolerance
	bool showAction = true;          // show_action
	bool showDistance = true;        // show_distance

	// --- 子弹火花(Bullet sparks)---
	bool bulletSparks = false;
	ImVec4 sparksColor = { 0.68f, 0.75f, 1.0f, 1.0f }; // 淡蓝白

	// --- 死亡效果(Death effect)---
	bool deathEffect = false;
	ImVec4 deathEffectColor = { 0.68f, 0.75f, 1.0f, 1.0f }; // 淡蓝白
}
