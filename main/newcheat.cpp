/*
Copyright (c) 2024 Diamond Wolf

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

Certain implementations may be under the Parallax license:

THE COMPUTER CODE CONTAINED HEREIN IS THE SOLE PROPERTY OF PARALLAX
SOFTWARE CORPORATION ("PARALLAX").  PARALLAX, IN DISTRIBUTING THE CODE TO
END-USERS, AND SUBJECT TO ALL OF THE TERMS AND CONDITIONS HEREIN, GRANTS A
ROYALTY-FREE, PERPETUAL LICENSE TO SUCH END-USERS FOR USE BY SUCH END-USERS
IN USING, DISPLAYING,  AND CREATING DERIVATIVE WORKS THEREOF, SO LONG AS
SUCH USE, DISPLAY OR CREATION IS FOR NON-COMMERCIAL, ROYALTY OR REVENUE
FREE PURPOSES.  IN NO EVENT SHALL THE END-USER USE THE COMPUTER CODE
CONTAINED HEREIN FOR REVENUE-BEARING PURPOSES.  THE END-USER UNDERSTANDS
AND AGREES TO THE TERMS HEREIN AND ACCEPTS THE SAME BY USE OF THIS FILE.
COPYRIGHT 1993-1999 PARALLAX SOFTWARE CORPORATION.  ALL RIGHTS RESERVED.
*/

#include <array>
#include <vector>
#include <cstring>
#include <stdexcept>

#include "ai.h"
#include "bm.h"
#include "controls.h"
#include "misc/error.h"
#include "game.h"
#include "gameseq.h"
#include "gauges.h"
#include "platform/key.h"
#include "laser.h"
#include "platform/mono.h"
#include "newcheat.h"
#include "newdemo.h"
#include "newmenu.h"
#include "player.h"
#include "powerup.h"
#include "scores.h"
#include "stringtable.h"
#include "texmap/texmap.h"
#include "weapon.h"

extern char guidebot_name[];
extern char real_guidebot_name[];

extern void load_background_bitmap();
extern void kill_all_robots();
extern void kill_and_so_forth();
extern int	create_special_path(void);
extern void set_ambient_sound_flags();

char* reverse(const char* str, int len) {
    char* ret = new char[len];
    for (size_t i = 0; i < len - 1; i++) {
        ret[len-i-2] = str[i];
    }
    return ret;
}

struct CheatCode {
    char* code;
    size_t codeLen;
};

CheatCode NewCheat(const char* inCode, int len) {
    CheatCode code;
    code.code = reverse(inCode, len+1);
    code.codeLen = len;
    return code;
}

#define BUFFER_SIZE 20
char buffer[BUFFER_SIZE];

CheatCode allCheats[CI_TOTAL];

short cheatValues[CI_TOTAL];

void FreeCheats() {
    for (int i = 0; i < CI_TOTAL; i++) {
        delete[] allCheats[i].code;
    }
}

void InitializeCheats() {
    
    int i = 0;

    allCheats[i++] = NewCheat("gabbagabbahey", 13); 
    allCheats[i++] = NewCheat("pigfarmer", 9); 
    allCheats[i++] = NewCheat("spaniard", 8); 
    allCheats[i++] = NewCheat("alifalafel", 10); 
    allCheats[i++] = NewCheat("oralgroove", 10); 
    allCheats[i++] = NewCheat("honestbob", 9); 
    allCheats[i++] = NewCheat("freespace", 9); 
    allCheats[i++] = NewCheat("duddaboo", 8); 
    allCheats[i++] = NewCheat("helpvishnu", 10); 
    allCheats[i++] = NewCheat("rockrgrl", 8); 
    allCheats[i++] = NewCheat("delshiftb", 9); 
    allCheats[i++] = NewCheat("gowingnut", 9); 
    allCheats[i++] = NewCheat("lpnlizard", 9); 
    allCheats[i++] = NewCheat("almighty", 8); 
    allCheats[i++] = NewCheat("godzilla", 8); 
    allCheats[i++] = NewCheat("bittersweet", 11); 
    allCheats[i++] = NewCheat("imagespace", 10); 
    allCheats[i++] = NewCheat("fopkjewa", 8); 
    allCheats[i++] = NewCheat("silkwing", 8);
    allCheats[i++] = NewCheat("blueorb", 7); 
    allCheats[i++] = NewCheat("wildfire", 8); 
    allCheats[i++] = NewCheat("frametime", 9); 
    
    allCheats[i++] = NewCheat("racerx", 6); 
    allCheats[i++] = NewCheat("mitzi", 5); 
    allCheats[i++] = NewCheat("guile", 5); 
    allCheats[i++] = NewCheat("twilight", 8); 
    allCheats[i++] = NewCheat("farmerjoe", 9); 
    allCheats[i++] = NewCheat("buggin", 6); 
    allCheats[i++] = NewCheat("bruin", 5); 
    allCheats[i++] = NewCheat("astral", 6); 
    allCheats[i++] = NewCheat("lunacy", 6); 
    allCheats[i++] = NewCheat("bigred", 6); 
    allCheats[i++] = NewCheat("flash", 5); 
    allCheats[i++] = NewCheat("scourge", 7);
    allCheats[i++] = NewCheat("porgys", 6); 
    allCheats[i++] = NewCheat("ahimsa", 6); 
    allCheats[i++] = NewCheat("poboys", 6);

    allCheats[i++] = NewCheat("helium", 6); 
    allCheats[i++] = NewCheat("yellowsun", 9); 
    allCheats[i++] = NewCheat("overhere", 8); 
    allCheats[i++] = NewCheat("ironskin", 8); 
    allCheats[i++] = NewCheat("stargazer", 9);
    allCheats[i++] = NewCheat("ahayweh", 7); 
    allCheats[i++] = NewCheat("vertigo", 7); //Drunk shader
    allCheats[i++] = NewCheat("firebrand", 9); 
    allCheats[i++] = NewCheat("whosyodaddy", 11);

    Assert(i == CI_TOTAL);

    memset(cheatValues, 0, CI_TOTAL * sizeof(*cheatValues));

    atexit(FreeCheats);

}

void do_cheat_penalty()
{
	digi_play_sample(SOUND_CHEATER, F1_0);
	Cheats_enabled = 1;
	Players[Player_num].score = 0;
}

int FindCheatIndex() {
    for (int i = 0; i < CI_TOTAL; i++) {
        //mprintf((1, "Checking %s against %s\n", allCheats[i].code, buffer));
        if (!strncmp(buffer, allCheats[i].code, allCheats[i].codeLen)) {
            return i;
        }
    }
    return -1;
}

int old_IntMethod;
std::vector<int8_t> homingFlags;
std::vector<int8_t> weaponChildren;
std::vector<int> tmapFlags;
std::vector<fix> tmapDamage;

void ResetLavaWalls() {

    if (cheatValues[CI_LAVA_WALLS]) {
        Assert(tmapFlags.size() == activeBMTable->tmaps.size());
        Assert(tmapDamage.size() == activeBMTable->tmaps.size());
        for (int i = 0; i < activeBMTable->tmaps.size(); i++) {
            activeBMTable->tmaps[i].flags = tmapFlags[i];
            activeBMTable->tmaps[i].damage = tmapDamage[i];
        }
    }

    cheatValues[CI_LAVA_WALLS] = 0;

}

void ResetCheatStates() {
    
    if (cheatValues[CI_ACID]) 
        Interpolation_method = old_IntMethod;
    
    if (cheatValues[CI_WINGNUT])
        strcpy(guidebot_name, real_guidebot_name);

    if (cheatValues[CI_HOMING]) {
        Assert(homingFlags.size() == activeBMTable->weapons.size());
        for (int i = 0; i < activeBMTable->weapons.size(); i++) {
            activeBMTable->weapons[i].flags = homingFlags[i];
        }
    }

    if (cheatValues[CI_EXPLODE_FLARES]) {
        Assert(weaponChildren.size() == activeBMTable->weapons.size());
        for (int i = 0; i < activeBMTable->weapons.size(); i++) {
            activeBMTable->weapons[i].children = weaponChildren[i];
        }
    }

    ResetLavaWalls();
        
}

bool CheckCheats(char newKeyIn) {

    int i;

    for (i = BUFFER_SIZE - 1; i > 0; i--) 
        buffer[i] = buffer[i-1];

    char newCharIn = key_to_ascii(newKeyIn);
    buffer[0] = newCharIn;

    int index = FindCheatIndex();

    if (index < 0)
        return false;

    mprintf((1, "Cheater! %d\n", index));

    newmenu_item m;
    char text[10];
    int item;

    switch (index) { //Only process codes that don't require cheats to be active

        case CI_ENABLE:
            do_cheat_penalty();
            HUD_init_message(TXT_CHEATS_ENABLED);
            return true;
        break;

        case CI_ACID:

            if (cheatValues[CI_ACID])
            {
                cheatValues[CI_ACID] = 0;
                Interpolation_method = old_IntMethod;
                HUD_init_message("Coming down...");
            }
            else
            {
                cheatValues[CI_ACID] = 1;
                old_IntMethod = Interpolation_method;
                Interpolation_method = 1;
                HUD_init_message("Going up!");
            }
            return true;
        break;

        case CI_HELIUM:
            cheatValues[CI_HELIUM]++;
            
            switch (cheatValues[CI_HELIUM]) {
                case 0:
                case 3:
                    HUD_init_message("All normal!");
                    cheatValues[CI_HELIUM] = 0;
                break;

                case 1:
                    HUD_init_message("Everything squeaks!");
                break;

                case 2:
                    HUD_init_message("That's not helium...");
                break;
            }

            digi_play_sample(SOUND_CHEATER, F1_0);

            return true;
        break;

        case CI_TROLL:
            digi_play_sample(SOUND_CHEATER, F1_0);
            Players[Player_num].shields = i2f(1);
			Players[Player_num].energy = i2f(1);
            HUD_init_message("Gotcha! :D");

            return true;
        break;

        case CI_JOHN:
            cheatValues[CI_JOHN] = !cheatValues[CI_JOHN];
            load_background_bitmap();
            fill_background();
            HUD_init_message(cheatValues[CI_JOHN] ? "Hi John!!" : "Bye John!!");

            return true;
        break;

        case CI_FPS:
            cheatValues[CI_FPS] = !cheatValues[CI_FPS];

            return true;
        break;

    }

    if (!Cheats_enabled || Game_mode & GM_MULTI) { 
        return false;
    } 

    switch (index) {

        case CI_NORMAL_SHIELD:
            if (Players[Player_num].shields < MAX_SHIELDS) {
                fix boost = 3 * F1_0 + 3 * F1_0 * (NDL - Difficulty_level);
                if (Difficulty_level == 0)
                    boost += boost / 2;
                Players[Player_num].shields += boost;
                if (Players[Player_num].shields > MAX_SHIELDS)
                    Players[Player_num].shields = MAX_SHIELDS;
                powerup_basic(0, 0, 15, SHIELD_SCORE, "%s %s %d", TXT_SHIELD, TXT_BOOSTED_TO, f2ir(Players[Player_num].shields));
                do_cheat_penalty();
            }
            else
                HUD_init_message(TXT_MAXED_OUT, TXT_SHIELD);

            return true;
        break;

        case CI_NEW_BUDDY:
            do_cheat_penalty();
            HUD_init_message("What's this? Another buddy bot!");
            create_buddy_bot();

            return true;
        break;

        case CI_WINGNUT:
            do_cheat_penalty();
            cheatValues[CI_WINGNUT] = !cheatValues[CI_WINGNUT];
            if (cheatValues[CI_WINGNUT]) {
                HUD_init_message("%s gets angry!", guidebot_name);
                strcpy(guidebot_name, "Wingnut");
            }
            else {
                strcpy(guidebot_name, real_guidebot_name);
                HUD_init_message("%s calms down", guidebot_name);
            }

            return true;
        break;

        case CI_GODZILLA:
            cheatValues[CI_GODZILLA] = !cheatValues[CI_GODZILLA];
            do_cheat_penalty();
            HUD_init_message(cheatValues[CI_GODZILLA] ? "Oh no, there goes Tokyo!" : "What have you done, I'm shrinking!!");

            return true;
        break;

        case CI_BOUNCY:
            do_cheat_penalty();
            cheatValues[CI_BOUNCY] = !cheatValues[CI_BOUNCY];
            if (cheatValues[CI_BOUNCY])
                HUD_init_message("Bouncing weapons!");
            else
                HUD_init_message("Boring weapons.");

                return true;
        break;

        case CI_LEVEL_D1:
        case CI_LEVEL_D2:
            int new_level_num;
            //digi_play_sample( SOUND_CHEATER, F1_0);
            text[0] = '\0';
            m.type = NM_TYPE_INPUT; m.text_len = 10; m.text = text;
            item = newmenu_do(NULL, TXT_WARP_TO_LEVEL, 1, &m, NULL);
            if (item != -1) {
                new_level_num = atoi(m.text);
                if (new_level_num != 0 && new_level_num >= 0 && new_level_num <= Last_level) {
                    StartNewLevel(new_level_num, 0);
                    do_cheat_penalty();
                }
            }
            
            return true;
        break;

        case CI_WEAPONS_D1:
        case CI_WEAPONS_D2:

            HUD_init_message(TXT_WOWIE_ZOWIE);
            do_cheat_penalty();

            if (currentGame == G_DESCENT_2) {
                if (CurrentLogicVersion < LogicVer::FULL_1_1)
                {
                    if (CurrentLogicVersion == LogicVer::FULL_1_0)
                        Players[Player_num].primary_weapon_flags &= ~HAS_FLAG(SUPER_LASER_INDEX);

                    Players[Player_num].primary_weapon_flags = ~((1 << PHOENIX_INDEX) | (1 << OMEGA_INDEX) | (1 << FUSION_INDEX));
                    Players[Player_num].secondary_weapon_flags = ~((1 << SMISSILE4_INDEX) | (1 << MEGA_INDEX) | (1 << SMISSILE5_INDEX));
                }
                else
                {
                    Players[Player_num].primary_weapon_flags = 0xffff ^ HAS_FLAG(SUPER_LASER_INDEX);		//no super laser
                    Players[Player_num].secondary_weapon_flags = 0xffff;
                }
            } else {
                Players[Player_num].primary_weapon_flags = 0x1f;
                Players[Player_num].secondary_weapon_flags = 0x1f;
            }

            for (i = 0; i < MAX_PRIMARY_WEAPONS; i++)
                if (Players[Player_num].primary_weapon_flags & (1 << i))
                    Players[Player_num].primary_ammo[i] = Primary_ammo_max[i];

            for (i = 0; i < MAX_SECONDARY_WEAPONS; i++)
                if (Players[Player_num].secondary_weapon_flags & (1 << i))	
                    Players[Player_num].secondary_ammo[i] = Secondary_ammo_max[i];

            if (CurrentLogicVersion < LogicVer::FULL_1_1)
            {
                Players[Player_num].secondary_ammo[SMISSILE4_INDEX] = 0;
                Players[Player_num].secondary_ammo[SMISSILE5_INDEX] = 0;
                Players[Player_num].secondary_ammo[MEGA_INDEX] = 0;
            }

            if (Game_mode & GM_HOARD)
                Players[Player_num].secondary_ammo[PROXIMITY_INDEX] = 12;

            if (Newdemo_state == ND_STATE_RECORDING)
                newdemo_record_laser_level(Players[Player_num].laser_level, MAX_LASER_LEVEL);

            Players[Player_num].energy = MAX_ENERGY;
            Players[Player_num].laser_level = (currentGame == G_DESCENT_2 ? MAX_SUPER_LASER_LEVEL : MAX_LASER_LEVEL);
            Players[Player_num].flags |= PLAYER_FLAGS_QUAD_LASERS;
            update_laser_weapon_info();

            return true;

        break;

        case CI_KEYS_D1:
        case CI_KEYS_D2:
            do_cheat_penalty();
            HUD_init_message(TXT_ALL_KEYS);
            Players[Player_num].flags |= PLAYER_FLAGS_BLUE_KEY | PLAYER_FLAGS_RED_KEY | PLAYER_FLAGS_GOLD_KEY;

            return true;
        break;

        case CI_PERMA_INVULN_D1:
        case CI_PERMA_INVULN_D2:
            Players[Player_num].flags ^= PLAYER_FLAGS_INVULNERABLE;
            if (Players[Player_num].flags & PLAYER_FLAGS_INVULNERABLE) {
                do_cheat_penalty();
                Players[Player_num].invulnerable_time = GameTime + i2f(10000);
            }
            HUD_init_message("%s %s!", TXT_INVULNERABILITY, (Players[Player_num].flags & PLAYER_FLAGS_INVULNERABLE) ? TXT_ON : TXT_OFF);

            return true;
        break;

        case CI_ACCESSORIES:
            if (currentGame != G_DESCENT_1) {
                do_cheat_penalty();
                Players[Player_num].flags |= PLAYER_FLAGS_HEADLIGHT;
                Players[Player_num].flags |= PLAYER_FLAGS_AFTERBURNER;
                Players[Player_num].flags |= PLAYER_FLAGS_AMMO_RACK;
                Players[Player_num].flags |= PLAYER_FLAGS_CONVERTER;

                Afterburner_charge = f1_0;

                HUD_init_message("Accessories!!");
            } else {
                HUD_init_message("Wrong game, silly!");
            }

            return true;
        break;

        case CI_MAP:
            do_cheat_penalty();
            Players[Player_num].flags |= PLAYER_FLAGS_MAP_ALL;
            HUD_init_message("Full Map!!");
        
            return true;
        break;

        case CI_HOMING:
            if (!cheatValues[CI_HOMING]) {
                cheatValues[CI_HOMING] = !cheatValues[CI_HOMING];
                if (cheatValues[CI_HOMING]) {
                    do_cheat_penalty();
                    homingFlags.resize(activeBMTable->weapons.size());
                    for (i = 0; i < 20; i++)
                    {
                        homingFlags[i] = activeBMTable->weapons[i].homing_flag;
                        activeBMTable->weapons[i].homing_flag = 1;
                    }
                    HUD_init_message("Homing weapons!");
                } else {
                    Assert(homingFlags.size() == activeBMTable->weapons.size());
                    for (i = 0; i < 20; i++)
                    {
                        activeBMTable->weapons[i].homing_flag = homingFlags[i];
                    }
                    HUD_init_message("Dumb weapons.");
                }
            }

            return true;
        break;

        case CI_KILL_BOTS:
            do_cheat_penalty();
		    kill_all_robots();
            
            return true;
        break;

        case CI_EXIT_WARP_D2_1:
        case CI_EXIT_WARP_D2_2:
            do_cheat_penalty();
		    kill_and_so_forth();

            return true;
        break;

        case CI_INFIGHTING:
            cheatValues[CI_INFIGHTING] = !cheatValues[CI_INFIGHTING];
            if (cheatValues[CI_INFIGHTING]) {
                HUD_init_message("Rabid robots!");
                do_cheat_penalty();
            }
            else
                HUD_init_message("Kill the player!");

            return true;
        break;

        case CI_NO_FIRING_D1:
        case CI_NO_FIRING_D2:
            cheatValues[CI_NO_FIRING_D1] = !cheatValues[CI_NO_FIRING_D1];
            if (cheatValues[CI_NO_FIRING_D1]) {
                HUD_init_message("%s", "Robot firing OFF!");
                do_cheat_penalty();
            }
            else
                HUD_init_message("%s", "Robot firing ON!");

            return true;
        break;

        case CI_RAPID_FIRE:
            cheatValues[CI_RAPID_FIRE] = !cheatValues[CI_RAPID_FIRE];
            if (cheatValues[CI_RAPID_FIRE])
                do_cheat_penalty();
            HUD_init_message("Rapid fire %s!", cheatValues[CI_RAPID_FIRE] ? TXT_ON : TXT_OFF);

            return true;
        break;

        case CI_PERMA_CLOAK:
            Players[Player_num].flags ^= PLAYER_FLAGS_CLOAKED;
            if (Players[Player_num].flags & PLAYER_FLAGS_CLOAKED) {
                do_cheat_penalty();
                Players[Player_num].cloak_time = GameTime + i2f(10000);
            }
            HUD_init_message("%s %s!", PLAYER_FLAGS_CLOAKED, (Players[Player_num].flags & PLAYER_FLAGS_CLOAKED) ? TXT_ON : TXT_OFF);
            
            return true;
        break;

        case Ci_DESTROY_REACTOR:
            do_cheat_penalty();
            do_controlcen_destroyed_stuff(nullptr);

            return true;
        break;

        case CI_TEMP_CLOAK:
        do_cheat_penalty();
            Players[Player_num].flags ^= PLAYER_FLAGS_CLOAKED;
            HUD_init_message("%s %s!", TXT_CLOAK, (Players[Player_num].flags & PLAYER_FLAGS_CLOAKED) ? TXT_ON : TXT_OFF);
            digi_play_sample(SOUND_CHEATER, F1_0);
            if (Players[Player_num].flags & PLAYER_FLAGS_CLOAKED) 
            {
                ai_do_cloak_stuff();
                Players[Player_num].cloak_time = GameTime;
            }

            return true;
        break;

        case CI_TEMP_INVULN:
            do_cheat_penalty();
            Players[Player_num].flags ^= PLAYER_FLAGS_INVULNERABLE;
            HUD_init_message("%s %s!", TXT_INVULNERABILITY, (Players[Player_num].flags & PLAYER_FLAGS_INVULNERABLE) ? TXT_ON : TXT_OFF);
            if (Players[Player_num].flags & PLAYER_FLAGS_INVULNERABLE)
            {
                Players[Player_num].invulnerable_time = GameTime;
            }

            return true;
        break;

        case CI_100_SHIELDS:
            do_cheat_penalty();
            HUD_init_message(TXT_FULL_SHIELDS);
            Players[Player_num].shields = MAX_SHIELDS / 2;

            return true;
        break;

        case CI_200_SHIELDS:
            do_cheat_penalty();
            HUD_init_message(TXT_FULL_SHIELDS);
            Players[Player_num].shields = MAX_SHIELDS;

            return true;
        break;

        case CI_GHOST:
            cheatValues[CI_GHOST] = !cheatValues[CI_GHOST];
            if (cheatValues[CI_GHOST])
                do_cheat_penalty();
            HUD_init_message("%s %s!", "Ghosty mode", cheatValues[CI_GHOST] ? TXT_ON : TXT_OFF);

            return true;
        break;

        case CI_MEGAWOW:
            do_cheat_penalty();
	        Players[Player_num].secondary_weapon_flags = 0xffff;
            do_megawow_powerup(200);

            return true;
        break;

        case CI_TURBO:
            do_cheat_penalty();
            cheatValues[CI_TURBO] = !cheatValues[CI_TURBO];
            HUD_init_message("%s %s!", "Turbo mode", cheatValues[CI_TURBO] ? TXT_ON : TXT_OFF);
            
            return true;
        break;

        case CI_EXTRA_LIFE:
            do_cheat_penalty();
            Players[Player_num].lives++;
		    powerup_basic(15, 15, 15, 0, TXT_EXTRA_LIFE);

            return true;
        break;

        case CI_EXIT_PATH:
            if (create_special_path()) {
                do_cheat_penalty();
                HUD_init_message("Exit path illuminated!");
            } else {
                HUD_init_message("Could not create exit path");
            }

            return true;
        break;

        /*case CI_LUNACY:
            cheatValues[CI_LUNACY] = !cheatValues[CI_LUNACY];
            if (cheatValues[CI_LUNACY]) {
                do_cheat_penalty();
                do_lunacy_on();
                HUD_init_message("It's a full moon!");
            } else {
                do_lunacy_off();
                HUD_init_message("What moon?");
            }

            return true;
        break;*/

        case CI_SHAREWARE_WEAPONS:
            do_cheat_penalty();

            if (currentGame == G_DESCENT_1) {
                Players[Player_num].primary_weapon_flags = (1 << LASER_INDEX) | (1 << VULCAN_INDEX) | (1 << SPREADFIRE_INDEX);
                Players[Player_num].secondary_weapon_flags = (1 << CONCUSSION_INDEX) | (1 << HOMING_INDEX) | (1 << PROXIMITY_INDEX);
                Players[Player_num].laser_level = 3;
            } else {
                Players[Player_num].primary_weapon_flags = ~((1 << PHOENIX_INDEX) | (1 << OMEGA_INDEX) | (1 << FUSION_INDEX));
                Players[Player_num].secondary_weapon_flags = ~((1 << SMISSILE4_INDEX) | (1 << MEGA_INDEX) | (1 << SMISSILE5_INDEX));
                Players[Player_num].laser_level = 5;
            }

            for (int i = 0; i < MAX_PRIMARY_WEAPONS; i++) {
                if (Players[Player_num].primary_weapon_flags & HAS_FLAG(i)) 
                    Players[Player_num].primary_ammo[i] = Primary_ammo_max[i];
            }

            for (int i = 0; i < MAX_SECONDARY_WEAPONS; i++) {
                if (Players[Player_num].secondary_weapon_flags & HAS_FLAG(i)) 
                    Players[Player_num].secondary_ammo[i] = Secondary_ammo_max[i];
            }

            return true;
        break;

        case CI_TELEPORT:
            int newSegNum;
            text[0] = '\0';
            m.type = NM_TYPE_INPUT; m.text_len = 10; m.text = text;
            item = newmenu_do(NULL, "Go to which segment?", 1, &m, NULL);
            if (item != -1) {
                try {
                    newSegNum = std::stoi(m.text);
                    if (newSegNum >= 0 && newSegNum < Num_segments) {
                        auto& segment = Segments[newSegNum];
                        int objnum = Players[Player_num].objnum;
                        vms_vector segCenter;
                        compute_segment_center(&segCenter, &segment);
                        Objects[objnum].pos = segCenter;
                        obj_relink(objnum, newSegNum);
                    }
                } catch (std::invalid_argument) {}
            }
            
            return true;    
        break;

        case CI_EXPLODE_FLARES:
            cheatValues[CI_EXPLODE_FLARES] = !cheatValues[CI_EXPLODE_FLARES];
            if (cheatValues[CI_EXPLODE_FLARES]) {
                do_cheat_penalty();
                weaponChildren.resize(activeBMTable->weapons.size());
                for (i = 0; i < activeBMTable->weapons.size(); i++)
                {
                    weaponChildren[i] = activeBMTable->weapons[i].children;
                    if (activeBMTable->weapons[i].children < 0 && i != FLARE_ID)
                    activeBMTable->weapons[i].children = FLARE_ID;
                }
                HUD_init_message("Crazy fraggy!");
            } else {
                Assert(weaponChildren.size() == activeBMTable->weapons.size());
                for (i = 0; i < activeBMTable->weapons.size(); i++)
                {
                    activeBMTable->weapons[i].children = weaponChildren[i];
                }
                HUD_init_message("Nevermind.");
            }

            return true;
        break;

        case CI_LAVA_WALLS:
            cheatValues[CI_LAVA_WALLS] = !cheatValues[CI_LAVA_WALLS];
            if (cheatValues[CI_LAVA_WALLS]) {
                do_cheat_penalty();
                tmapFlags.resize(activeBMTable->tmaps.size());
                tmapDamage.resize(activeBMTable->tmaps.size());
                for (i = 0; i < activeBMTable->tmaps.size(); i++)
                {
                    tmapFlags[i] = activeBMTable->tmaps[i].flags;
                    tmapDamage[i] = activeBMTable->tmaps[i].damage;
                    activeBMTable->tmaps[i].flags |= TMI_VOLATILE;
                    if (activeBMTable->tmaps[i].damage <= 0)
                        activeBMTable->tmaps[i].damage = i2f(8); //match D2's red lava
                }
                HUD_init_message("The floor is lava!?");
            } else {
                Assert(tmapFlags.size() == activeBMTable->tmaps.size());
                Assert(tmapDamage.size() == activeBMTable->tmaps.size());
                for (i = 0; i < activeBMTable->tmaps.size(); i++)
                {
                    activeBMTable->tmaps[i].flags = tmapFlags[i];
                    activeBMTable->tmaps[i].damage = tmapDamage[i];
                }
                HUD_init_message("Rock solid!");
            }
            set_ambient_sound_flags();

            return true;
        break;
    }

    return false;
    
    
}
