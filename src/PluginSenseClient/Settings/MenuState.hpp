#pragma once

#include <string>

#include <ImGui/imgui.h>

namespace vars
{
	extern float colorAccent[4];
	extern int  menuKey;
	extern int  menuKeySuppress;
	extern int  menuAnimSpeed;
	extern bool menuConfirmations;
	extern bool menuKeybinds;
}

namespace style
{
	extern float glassWindow;
	extern float glassPanel;
	extern float glassWidget;
	extern float glassPopup;
	extern int outlineAlpha;
}

namespace menu_state
{
	extern bool clantagEnabled;
	extern bool animatedClantag;
	extern bool velocityText;
	extern bool velocityGraph;
	extern bool keystrokes;
	extern float velocityOffset;
	extern bool damageLogEnabled;
	extern bool hitlogEnabled;
	extern bool hitlogVictim;
	extern int hitlogType;
	extern bool damageIndicator;
	extern bool spoof;
	extern bool fakeCooldown;
	extern bool officialBan;
	extern bool vacBan;
	extern int clantagSelection;
	extern int animationSelection;
	extern int fakeCooldownValue;
	extern int fakeCooldownTime;
	extern int fakeCooldownCustomDays;
	extern float clantagSpeed;
	extern float damageScale;
	extern float damageTime;
	extern char playerName[32];
	extern char customClantag[32];
	extern ImVec4 lowSpeed;
	extern ImVec4 midSpeed;
	extern ImVec4 highSpeed;
	extern ImVec4 graphColor;
	extern ImVec4 damageBody;
	extern ImVec4 damageHead;
	extern bool chatSpammer;
	extern bool killSay;
	extern bool vacnetEnabled;
	extern float sendDelay;
	extern int chatCount;
	extern int killCount;
	extern char chatMessages[16][128];
	extern char killMessages[16][128];

	// Weapon model changer(武器模型更换)
	extern bool weaponModelEnabled;
	extern int weaponModelWeapon; // 武器下拉框选中索引(所有枪械 + Knife)
	extern int weaponModelModel;  // 模型 listbox 选中索引

	// Lobby profile spoof(大厅资料伪装)
	extern bool lobbyPremier;
	extern int lobbyPremierRating;
	extern int lobbyPremierWins;
	extern bool lobbyWingman;
	extern int lobbyWingmanRating;
	extern int lobbyWingmanWins;
	extern bool lobbyLevel;
	extern int lobbyLevelValue;
	extern int lobbyXp;
	extern bool lobbyPrime;

	struct WorldScene
	{
		bool dof = false;
			bool dofFocus = false;
			float dofNearBlurry = 0.0f;
			float dofNearCrisp = 5.0f;
			float dofFarCrisp = 600.0f;
			float dofFarBlurry = 1400.0f;
		bool bloom = false;
		float bloomValue = 2.0f;
		bool gamma = false;
		float gammaValue = 2.2f;
			bool motionCamera = false;
				float camHorOffset = 0.f;
			float camVerOffset = 0.f;
			float camSlack = 40.f;
		bool camCrosshair = false;
			ImVec4 camCrosshairColor = { 1.f, 1.f, 0.f, 1.f };
		float camCrosshairLength = 6.f;
			float camCrosshairThickness = 1.5f;
		};
	extern WorldScene worldScene;

	struct WorldWeather
	{
		bool enabled = false;
		bool wind = false;
		float windStrength = 3.0f;
		float windDirection = 0.0f;
		float windTurbulence = 1.0f;
		int type = 0; // 0=snow, 1=rain, 2=stars
		ImVec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
		bool wetness = false;
		float wetnessDensity = 1.8f;
		float wetnessSpeed = 0.8f;
	};
	extern WorldWeather worldWeather;
}
