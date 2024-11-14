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
*/

enum CheatsIndex {
    CI_ENABLE,
    CI_JOHN,
    CI_KILL_BOTS,
    CI_ACCESSORIES,
    CI_KEYS_D2,
    CI_WEAPONS_D2,
    CI_LEVEL_D2,
    CI_BOUNCY,
    CI_NEW_BUDDY,
    CI_MAP,
    CI_EXIT_WARP_D2_1,
    CI_WINGNUT,
    CI_HOMING,
    CI_PERMA_INVULN_D2,
    CI_GODZILLA,
    CI_ACID,
    CI_NO_FIRING_D2,
    CI_EXIT_WARP_D2_2,
    CI_INFIGHTING,
    CI_NORMAL_SHIELD,
    CI_RAPID_FIRE,
    CI_FPS,

    CI_PERMA_INVULN_D1,
    CI_KEYS_D1,
    CI_TEMP_CLOAK,
    CI_100_SHIELDS,
    CI_LEVEL_D1,
    CI_TURBO,
    CI_EXTRA_LIFE,
    CI_GHOST,
    CI_LUNACY,
    CI_WEAPONS_D1,
    CI_EXIT_PATH,
    CI_SHAREWARE_WEAPONS,
    CI_MEGAWOW,
    CI_NO_FIRING_D1,
    Ci_DESTROY_REACTOR,

    CI_HELIUM,
    CI_200_SHIELDS,
    CI_PERMA_CLOAK,
    CI_TEMP_INVULN,
    CI_TELEPORT,
    CI_TROLL,
    CI_DRUNK,
    CI_LAVA_WALLS,
    //CI_EXPLODE_FLARES,

    CI_TOTAL
};

extern short cheatValues[CI_TOTAL];

//Returns whether the char sent in successfully triggered a cheat code
bool CheckCheats(char newKeyIn);

void InitializeCheats();
void ResetCheatStates();
void ResetLavaWalls();