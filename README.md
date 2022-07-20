
# ZDoom Backpack Generator

A C++ tool that automate creating multiple backpacks for ZDoom-based games. The tool is designed to use under Windows systems, but the source code does not use Win API and may be compiled under other systems.


## Purpose

This tool is designed to help ZDoom modders with creating multiple backpack items for their mods.
The tool needs an input file "Ammo Config" where user describes all backpacks they want to create with maximum ammo amounts and all things that gives ammo on pickup along with some optional auxiliary information. To produce the required actor definitions the tool also needs DECORATE template where it will put the definition parts it generates on the place of special labels. Additionally, SBARINFO template may be provided to generate ammo max amounts displaying code.


## Theory

[ZDoom](https://zdoom.org/) is a modern source port made to play Doom (1993) and Doom-based video games. It provides great abilities for modding Doom-based games. For modding purposes ZDoom consumes code written on various computer mini-languages e.g. [DECORATE](https://zdoom.org/wiki/DECORATE), [SBARINFO](https://zdoom.org/wiki/SBARINFO) and so on. There are also more modern [GZDoom](https://zdoom.org/wiki/GZDoom) port and [ZScript](https://zdoom.org/wiki/ZScript) language replacing DECORATE and offering more features - these are not covered here.
ZDoom DECORATE allows a modder to create custom entities (things) - [monsters](https://zdoom.org/wiki/Monster), [weapons](https://zdoom.org/wiki/Classes:Weapon), [health items](https://zdoom.org/wiki/Classes:Health), [armor items](https://zdoom.org/wiki/Classes:Armor), [ammo](https://zdoom.org/wiki/Classes:Ammo) and others. Newly created items may work in the very different way comparing to the original Doom items. However, not every type of behavior is easy to implement here. Usually, if you want an item that's already implemented to behave slightly differently you can use inheritance - ```actor MyNewItem: AlreadyImplementedItem { /*...*/ }```. Unfortunately, this does not work with ammo actors, which does not allow to change max amount of ammo player can carry. Inheriting from ammo actors just allows creating more ammo packs, which can give player different amount of ammo on pickup. Moreover, you cannot create multiple [backpack](https://zdoom.org/wiki/Classes:BackpackItem) items using just the inheritance mechanism. Maximum amount of ammo with the backpack is controlled by property ```Ammo.BackpackMaxAmount``` - single property of an ammo class. Other - ```Inventory.MaxAmount``` ammo class property controlls the maximum ammo amount a player can carry without the backpack. And that's all - two properties per an ammo class. This does not allow creating more than one backpack via actor inheritance.
The way you can create multiple backpack items is to use [Custom Inventory](https://zdoom.org/wiki/Classes:CustomInventory) items to check and fix up ammo amounts player carries each time they pick up an item that gives some ammo:

  1. An ammo limit may be exceeded only when player picks up some ammo (in ammo pack, weapon or backpack items);

  2. Custom Inventory is the special actor type that allows to react on item pickups. When player picks up an item whose actor is inherited from Custom Inventory, a special action sequence is executed. This action sequence is labeled as ```Pickup``` in item actor's DECORATE definition.

  3. In action sequences such as ```Pickup``` for Custom Inventory any number of ammo (possibly of different types) can be given to player who picked up the item.

  4. The same ```Pickup``` action sequence allows to check the current ammo amounts player carries and change them, if required.
  
Also, when multiple backpack exist, some sort of indicator is required to track which type of backpack player currently carries. This indicator tells the ammo limits checking code what are the current ammo limits. In DECORATE you can create a simple item inherited from Inventory actor for that purpose: ```actor BackpackMarker: Inventory { Inventory.MaxAmount <number-of-backpacks-with-different-ammo-limits> }```.
The full solution of the multiple backpacks problem looks like this:
  1. Define all new actors for ammo types and set one of the max properties (```Inventory.MaxAmount``` or ```Ammo.BackpackMaxAmmount``` - any property can be used but the other code depends on the choice).

  2. Define new backpack items as Custom Inventory items. Depending of the choice for the previous option they should or should not give the original backpack item because the original backpack item makes the game to check that ammo amounts is less than or equal to ```Ammo.BackpackMaxAmmount``` value instead of ```Inventory.MaxAmount``` value. If you want to customize the ammo amount backpack gives, you can combine [A_GiveInventory](https://zdoom.org/wiki/A_GiveInventory) function calls with property ```Ammo.BackpackAmount``` for ammo types (gives ammo, when player receives original backpack item). If newly created backpack may give some ammo on pickup, then ammo limits must be checked on pickup.

  3. Derive all ammo packs and weapons from ```CustomInventory``` actor. They should give ammo (and weapon for weapon things) on pickup and check ammo limits after this. This is done again with ```A_GiveInventory``` function.

  4. If this solution is used just to add multiple backpacks into the original Doom 2, then alternative variants of Shotgun and Chaingun should be added to the DECORATE. Shotgun and Chaingun guys drop Shotgun and Chaingun on death respectively. These weapons give different amount of ammo on pickup comparing to the regular Shotgun/Chaingun spawned on maps. Then Shotgun and Chaingun guys must be replaced with ones dropping different guns like: ```actor NewShotgunGuy: ShotgunGuy replaces ShotgunGuy { DropItem "MyShotgunDroppedVariant" }``` and similar for Chaingun guy.

  5. SBARINFO should also be modified to display actual ammo limits. This is done by checking BackpackMarker item amount in player's inventory and drawing different numbers according to the value.


## Implementation

One possible implementation of the described solution includes the following:

1. Ammo classes with ```Inventory.MaxAmount``` set to the default ammo limit (without any backpacks) and ```Ammo.BackpackMaxAmount``` set to the biggest ammo amount player can carry (with any backpack). ```Ammo.BackpackAmount``` set to ```0``` and ammo amount backpack gives on pickup is fully controlled by ```A_GiveInventory``` function.

2. Weapon actors has ```Weapon.AmmoGive``` (```Weapon.AmmoGive1```) and ```Weapon.AmmoGive2``` properties set to 0 and ammo amount weapon gives on pickup is again fully controlled by ```A_GiveInventory``` function.

3. All checks of ammo limits and fixes of ammo amounts are done by a single parent actor for all items for pickup - ```AmmoLimitsChecker```, which is derived from ```CustomInventory```. All ammo pack, weapon thing and backpack actors are derived from this actor. The actor uses [A_JumpIfInventory](https://zdoom.org/wiki/A_JumpIfInventory) to check ammo/marker amounts and do different actions depending on these values. Also it uses ```A_GiveInventory```/[A_TakeInventory](https://zdoom.org/wiki/A_TakeInventory) to fix up ammo amounts.

4. The choice of ammo limits to check is controlled by the ```BackpackMarker``` actor (```Inventory.MaxAmount``` set to value equal to the number of backpacks defined).

5. Also, there is some sort of optimization for the case when player does not need more ammo from the item they try to pickup. In this case item is not picked up and player may trigger its ```Pickup``` action sequence again and again while they are standing on it. In this case ```AmmoLimitNotReachedMarker``` inventory item is used to inform that ammo limit is not reached and item can be picked up.

6. All items for pickup are derived from ```AmmoLimitsChecker``` actor and they check ammo limits are not reached before pickup. If ammo limit is reached, they "refuse to be picked up" (weapons may still be picked up if player has no such weapon in their inventory). If ammo limits are not reached, these items give appropriate amount of ammo using ```A_GiveInventory``` and ask ```AmmoLimitsChecker``` to check ammo limits and fix up ammo amounts.

7. SBARINFO is appropriately modified (see example below).


## Code examples

### DECORATE

Example of ammo class and ammo item for pickup (ammo pack):

```
actor ShellEx: Ammo
{
  Inventory.Icon "SHELA0"
  Inventory.Amount 1
  Inventory.MaxAmount 50
  Ammo.BackpackAmount 0
  Ammo.BackpackMaxAmount 200
}

actor SomeShellsEx: AmmoLimitsChecker replaces Shell
{
  Inventory.PickupMessage "$GOTSHELLS"
  Inventory.PickupSound "misc/ammo_pkup"

  States
  {
  Spawn:
    SHEL A -1
    Stop

  Pickup:
    TNT1 A 0 
    Goto Super::ShellExCheckLimitStart

  ShellExCheckLimitAfter:
    TNT1 A 0 A_JumpIfInventory("AmmoLimitNotReachedMarker", 1, "ActualPickup")
    Fail

  ActualPickup:
    TNT1 A 0 A_TakeInventory("AmmoLimitNotReachedMarker")
    TNT1 A 0 A_GiveInventory("ShellEx", 4)
    Goto Super::FixUpAmmoStart
  }
}
```

On item pickup the action sequence under ```Pickup``` label will be executed. ```ShellExCheckLimitStart``` label exists in ```AmmoLimitsChecker``` actor (see below) and there ShellEx ammo limit check is performed. If ShellEx ammo limit is not reached, ```AmmoLimitNotReachedMarker``` will be placed in the player's inventory indicating that ShellEx ammo may be picked up. ```Fail``` means item will not be picked up. ```Goto``` here can be used to jump to labels of the parent actor like ```Goto Super::ShellExCheckLimitStart``` to jump to the ```ShellExCheckLimitStart``` label of ```AmmoLimitsChecker``` actor. At ```FixUpAmmoStart``` label extra ammo will be removed from player's inventory, if player have some.

Example of weapon actor and weapon item for pickup actor (weapon thing):

```
actor PlasmaRifleEx: PlasmaRifle
{
  Weapon.AmmoType "CellEx"
  Weapon.AmmoGive 0
}

actor PlasmaRifleExThing: AmmoLimitsChecker replaces PlasmaRifle
{
  Inventory.PickupMessage "$GOTPLASMA"
  Inventory.PickupSound "misc/w_pkup"
  +WEAPONSPAWN

  States
  {
  Spawn:
    PLAS A -1
    Stop

  Pickup:
    TNT1 A 0 A_JumpIfInventory("PlasmaRifleEx", 1, "Super::CellExCheckLimitStart")
    Goto ActualPickup

  CellExCheckLimitAfter:
    TNT1 A 0 A_JumpIfInventory("AmmoLimitNotReachedMarker", 1, "ActualPickup")
    Fail

  ActualPickup:
    TNT1 A 0 A_TakeInventory("AmmoLimitNotReachedMarker")
    TNT1 A 0 A_GiveInventory("PlasmaRifleEx")
    TNT1 A 0 A_GiveInventory("CellEx", 40)
    Goto Super::FixUpAmmoStart
  }
}
```

Action sequence looks similar except the weapon actor is checked for presence in the player's inventory and given to the player, if not yet.

Example of backpack actor:

```
actor SmallAmmoBag: AmmoLimitsChecker 15000
{
  Inventory.PickupMessage "Picked up a small ammo bag!"

  States
  {
  Spawn:
    BPAK A 0
    BPAK A 0 A_SetScale(0.5, 0.5)
    BPAK A -1
    Stop

  Pickup:
    TNT1 A 0 A_GiveInventory("Backpack", 1)
    TNT1 A 0 A_GiveInventory("ClipEx", 10)
    TNT1 A 0 A_GiveInventory("ShellEx", 4)
    TNT1 A 0 A_GiveInventory("RocketAmmoEx", 1)
    TNT1 A 0 A_GiveInventory("CellEx", 20)
    TNT1 A 0 A_JumpIfInventory("BackpackMarker", 1, 3)
    TNT1 A 0 A_TakeInventory("BackpackMarker")
    TNT1 A 0 A_GiveInventory("BackpackMarker", 1)
    TNT1 A 0 
    Goto Super::FixUpAmmoStart
  }
}
```

Firstly, give the backpack item to widen the real ammo limits game checks. Then give some ammo because backpacks usually do this. Set ```BackpackMarker``` value to the right one (1 corresponds to the Small Ammo Bag backpack; if value is greater than 1, then keep the current value meaning player already carries bigger backpack). Finally, ```Goto Super::FixUpAmmoStart``` - check ammo limits and take extra ammo if player carries any.

AmmoLimitsChecker actor definition with supporting actors looks as follows:

```
actor BackpackMarker: Inventory
{
  Inventory.MaxAmount 3
}


actor AmmoLimitNotReachedMarker: Inventory
{
  Inventory.MaxAmount 1
}

actor AmmoLimitsChecker: CustomInventory
{
  States
  {
  ClipExCheckLimitStart:
    TNT1 A 0 A_JumpIfInventory("BackpackMarker", 3, 6)
    TNT1 A 0 A_JumpIfInventory("BackpackMarker", 2, 4)
    TNT1 A 0 A_JumpIfInventory("BackpackMarker", 1, 2)
    Goto ClipExCheckLimitStart+3

    // ClipExCheckLimitStart+3
    TNT1 A 0 A_JumpIfInventory("ClipEx", 200, "ClipExCheckLimitAfter")
    Goto ClipExCheckLimitStart+7
    TNT1 A 0 A_JumpIfInventory("ClipEx", 300, "ClipExCheckLimitAfter")
    Goto ClipExCheckLimitStart+7
    TNT1 A 0 A_JumpIfInventory("ClipEx", 400, "ClipExCheckLimitAfter")
    Goto ClipExCheckLimitStart+7
    TNT1 A 0 A_JumpIfInventory("ClipEx", 600, "ClipExCheckLimitAfter")
    Goto ClipExCheckLimitStart+7

    // ClipExCheckLimitStart+7
    TNT1 A 0 A_GiveInventory("AmmoLimitNotReachedMarker")
    TNT1 A 0 A_Jump(256, "ClipExCheckLimitAfter")
    Stop

  ShellExCheckLimitStart:
    TNT1 A 0 A_JumpIfInventory("BackpackMarker", 3, 6)
    TNT1 A 0 A_JumpIfInventory("BackpackMarker", 2, 4)
    TNT1 A 0 A_JumpIfInventory("BackpackMarker", 1, 2)
    Goto ShellExCheckLimitStart+3

    // ShellExCheckLimitStart+3
    TNT1 A 0 A_JumpIfInventory("ShellEx", 50, "ShellExCheckLimitAfter")
    Goto ShellExCheckLimitStart+7
    TNT1 A 0 A_JumpIfInventory("ShellEx", 80, "ShellExCheckLimitAfter")
    Goto ShellExCheckLimitStart+7
    TNT1 A 0 A_JumpIfInventory("ShellEx", 100, "ShellExCheckLimitAfter")
    Goto ShellExCheckLimitStart+7
    TNT1 A 0 A_JumpIfInventory("ShellEx", 200, "ShellExCheckLimitAfter")
    Goto ShellExCheckLimitStart+7

    // ShellExCheckLimitStart+7
    TNT1 A 0 A_GiveInventory("AmmoLimitNotReachedMarker")
    TNT1 A 0 A_Jump(256, "ShellExCheckLimitAfter")
    Stop

  RocketAmmoExCheckLimitStart:
    TNT1 A 0 A_JumpIfInventory("BackpackMarker", 3, 6)
    TNT1 A 0 A_JumpIfInventory("BackpackMarker", 2, 4)
    TNT1 A 0 A_JumpIfInventory("BackpackMarker", 1, 2)
    Goto RocketAmmoExCheckLimitStart+3

    // RocketAmmoExCheckLimitStart+3
    TNT1 A 0 A_JumpIfInventory("RocketAmmoEx", 50, "RocketAmmoExCheckLimitAfter")
    Goto RocketAmmoExCheckLimitStart+7
    TNT1 A 0 A_JumpIfInventory("RocketAmmoEx", 80, "RocketAmmoExCheckLimitAfter")
    Goto RocketAmmoExCheckLimitStart+7
    TNT1 A 0 A_JumpIfInventory("RocketAmmoEx", 100, "RocketAmmoExCheckLimitAfter")
    Goto RocketAmmoExCheckLimitStart+7
    TNT1 A 0 A_JumpIfInventory("RocketAmmoEx", 200, "RocketAmmoExCheckLimitAfter")
    Goto RocketAmmoExCheckLimitStart+7

    // RocketAmmoExCheckLimitStart+7
    TNT1 A 0 A_GiveInventory("AmmoLimitNotReachedMarker")
    TNT1 A 0 A_Jump(256, "RocketAmmoExCheckLimitAfter")
    Stop

  CellExCheckLimitStart:
    TNT1 A 0 A_JumpIfInventory("BackpackMarker", 3, 6)
    TNT1 A 0 A_JumpIfInventory("BackpackMarker", 2, 4)
    TNT1 A 0 A_JumpIfInventory("BackpackMarker", 1, 2)
    Goto CellExCheckLimitStart+3

    // CellExCheckLimitStart+3
    TNT1 A 0 A_JumpIfInventory("CellEx", 300, "CellExCheckLimitAfter")
    Goto CellExCheckLimitStart+7
    TNT1 A 0 A_JumpIfInventory("CellEx", 480, "CellExCheckLimitAfter")
    Goto CellExCheckLimitStart+7
    TNT1 A 0 A_JumpIfInventory("CellEx", 600, "CellExCheckLimitAfter")
    Goto CellExCheckLimitStart+7
    TNT1 A 0 A_JumpIfInventory("CellEx", 920, "CellExCheckLimitAfter")
    Goto CellExCheckLimitStart+7

    // CellExCheckLimitStart+7
    TNT1 A 0 A_GiveInventory("AmmoLimitNotReachedMarker")
    TNT1 A 0 A_Jump(256, "CellExCheckLimitAfter")
    Stop

  FixUpAmmoStart:
    TNT1 A 0 A_JumpIfInventory("BackpackMarker", 3, "FixUpAmmo3")
    TNT1 A 0 A_JumpIfInventory("BackpackMarker", 2, "FixUpAmmo2")
    TNT1 A 0 A_JumpIfInventory("BackpackMarker", 1, "FixUpAmmo1")
    TNT1 A 0 
    Stop

  FixUpAmmo1:
    TNT1 A 0 A_JumpIfInventory("ClipEx", 301, 5)
    TNT1 A 0 A_JumpIfInventory("ShellEx", 81, 6)
    TNT1 A 0 A_JumpIfInventory("RocketAmmoEx", 81, 7)
    TNT1 A 0 A_JumpIfInventory("CellEx", 481, 8)
    TNT1 A 0 
    Stop

    // fix up 'ClipEx' amount in player's inventory
    TNT1 A 0 A_TakeInventory("ClipEx")
    TNT1 A 0 A_GiveInventory("ClipEx", 300)
    Goto FixUpAmmo1+1

    // fix up 'ShellEx' amount in player's inventory
    TNT1 A 0 A_TakeInventory("ShellEx")
    TNT1 A 0 A_GiveInventory("ShellEx", 80)
    Goto FixUpAmmo1+2

    // fix up 'RocketAmmoEx' amount in player's inventory
    TNT1 A 0 A_TakeInventory("RocketAmmoEx")
    TNT1 A 0 A_GiveInventory("RocketAmmoEx", 80)
    Goto FixUpAmmo1+3

    // fix up 'CellEx' amount in player's inventory
    TNT1 A 0 A_TakeInventory("CellEx")
    TNT1 A 0 A_GiveInventory("CellEx", 480)
    Goto FixUpAmmo1+4

  FixUpAmmo2:
    TNT1 A 0 A_JumpIfInventory("ClipEx", 401, 5)
    TNT1 A 0 A_JumpIfInventory("ShellEx", 101, 6)
    TNT1 A 0 A_JumpIfInventory("RocketAmmoEx", 101, 7)
    TNT1 A 0 A_JumpIfInventory("CellEx", 601, 8)
    TNT1 A 0 
    Stop

    // fix up 'ClipEx' amount in player's inventory
    TNT1 A 0 A_TakeInventory("ClipEx")
    TNT1 A 0 A_GiveInventory("ClipEx", 400)
    Goto FixUpAmmo2+1

    // fix up 'ShellEx' amount in player's inventory
    TNT1 A 0 A_TakeInventory("ShellEx")
    TNT1 A 0 A_GiveInventory("ShellEx", 100)
    Goto FixUpAmmo2+2

    // fix up 'RocketAmmoEx' amount in player's inventory
    TNT1 A 0 A_TakeInventory("RocketAmmoEx")
    TNT1 A 0 A_GiveInventory("RocketAmmoEx", 100)
    Goto FixUpAmmo2+3

    // fix up 'CellEx' amount in player's inventory
    TNT1 A 0 A_TakeInventory("CellEx")
    TNT1 A 0 A_GiveInventory("CellEx", 600)
    Goto FixUpAmmo2+4

  FixUpAmmo3:
    TNT1 A 0 
    Stop
  }
}
```

Two sets of states here:
  - ```<AmmoTypeName>CheckLimitStart``` - check if player carries maximum amount ammo of the type and give ```AmmoLimitNotReachedMarker``` otherwise (this check depends on the current backpack player carries);
  - ```FixUp*``` - remove extra ammo from player's inventory (again, the final ammo amount value depends on the current backpack player carries).

If you create all new ammo and weapons, you should also change the player class and gameinfo (see MAPINFO below). Example of the new player class definition in DECORATE:

```
actor DoomPlayerEx: DoomPlayer replaces DoomPlayer
{
  Player.StartItem "PistolEx"
  Player.StartItem "Fist"
  Player.StartItem "ClipEx", 50
  Player.WeaponSlot 1, Fist, Chainsaw
  Player.WeaponSlot 2, PistolEx
  Player.WeaponSlot 3, ShotgunEx, SuperShotgunEx
  Player.WeaponSlot 4, ChaingunEx
  Player.WeaponSlot 5, RocketLauncherEx
  Player.WeaponSlot 6, PlasmaRifleEx
  Player.WeaponSlot 7, BFG9000Ex
}
```


### SBARINFO

Here should be put a code responsible for drawing the actual ammo limits. Example:

```
	InInventory BackpackMarker, 3 {
		drawnumber 3, INDEXFONT_DOOM, untranslated, 600, 314, 173;
		drawnumber 3, INDEXFONT_DOOM, untranslated, 200, 314, 179;
		drawnumber 3, INDEXFONT_DOOM, untranslated, 200, 314, 185;
		drawnumber 3, INDEXFONT_DOOM, untranslated, 920, 314, 191;
	}

	InInventory BackpackMarker, 2 { InInventory not BackpackMarker, 3 {
		drawnumber 3, INDEXFONT_DOOM, untranslated, 400, 314, 173;
		drawnumber 3, INDEXFONT_DOOM, untranslated, 100, 314, 179;
		drawnumber 3, INDEXFONT_DOOM, untranslated, 100, 314, 185;
		drawnumber 3, INDEXFONT_DOOM, untranslated, 600, 314, 191;
	}}

	InInventory BackpackMarker, 1 { InInventory not BackpackMarker, 2 {
		drawnumber 3, INDEXFONT_DOOM, untranslated, 300, 314, 173;
		drawnumber 3, INDEXFONT_DOOM, untranslated, 80, 314, 179;
		drawnumber 3, INDEXFONT_DOOM, untranslated, 80, 314, 185;
		drawnumber 3, INDEXFONT_DOOM, untranslated, 480, 314, 191;
	}}

	InInventory not BackpackMarker, 1 {
		drawnumber 3, INDEXFONT_DOOM, untranslated, 200, 314, 173;
		drawnumber 3, INDEXFONT_DOOM, untranslated, 50, 314, 179;
		drawnumber 3, INDEXFONT_DOOM, untranslated, 50, 314, 185;
		drawnumber 3, INDEXFONT_DOOM, untranslated, 300, 314, 191;
	}
```


### MAPINFO

```backpacktype``` property should be set appropriately for gameinfo definition in [MAPINFO](https://zdoom.org/wiki/MAPINFO). This backpack is given to player when they use cheats (e.g. "give all") Example:

```
gameinfo
{
    backpacktype = "HeavyBackpack"
    weaponslot = 1, "Fist", "Chainsaw"
    weaponslot = 2, "PistolEx"
    weaponslot = 3, "ShotgunEx", "SuperShotgunEx"
    weaponslot = 4, "ChaingunEx"
    weaponslot = 5, "RocketLauncherEx"
    weaponslot = 6, "PlasmaRifleEx"
    weaponslot = 7, "BFG9000Ex"
    playerclasses = "DoomPlayerEx"
}
```


## Automation

### Tool description

The process of creating and maintaining the ```AmmoLimitsChecker``` and other actors may be a bit overhead especially if someone just wants several backpacks in their new Doom mod and does not want to write all this code. In other case a modder may have many ammo types, weapons, backpacks and moreover, they may change these from time to time. Each change requires major rewriting of ```AmmoLimitsChecker``` actor code. However, this ```AmmoLimitsChecker``` actor is just an variant of not quite complicated template. Therefore, creating ```AmmoLimitsChecker``` actor code can be automated.
Presented tool automates the process of creating ```AmmoLimitsChecker``` actor definition along with adding the required definition parts in items for pickup and generating SBARINFO code.
The tool consumes Ammo Config file, DECORATE and SBARINFO templates with special ```@BpkGen*``` labels and some other files (if presented) and produce the WAD file which can be used directly (e.g. with [Doom Builder](https://zdoom.org/wiki/Doom_Builder) to add maps and make a map pack) or resulting DECORATE/SBARINFO code can be extracted from there using [Slade 3](https://zdoom.org/wiki/SLADE), for example.


### Ammo Config

Ammo config file should describe all ammo types in the target mod, all weapons, all ammo types, all things that can give ammo on pickup. This file has special format (see example below) and may also describe some additional information. The example of Ammo Config file with all possible data entries is given below:

```
// backpack item to give to the player with
// any backpack thing that may modify ammo limits
// default - "Backpack"
//BackpackItemName: #"Backpack"

// ammo type definition;
// max (number) - maximum amount of ammo without backpack
Ammo: ClipEx#{ max=200 }
    : ShellEx#{ max=50 }
    : RocketAmmoEx#{ max=50 }
    : CellEx#{ max=300 }

// backpacks definition with maximums and give amounts set separately for each ammo type;
// default amount ammo to give is 0, default ammo maximum is 'max' set for the ammo type;
// ammo maximum cannot decrease from backpack to backpack and cannot be less than max set for the ammo type
// (although, backpack definitions may go in any order);
// all <AmmoType>Max (number) properties must be set or not simultaneously for a backpack;
// maxf (number) can be set instead of <AmmoType>Max properties for a backpack meaning that <AmmoType>Max will be equal to
// (maxf * <AmmoType>.max) where <AmmoType>.max is max value from the ammo type definition;
// set 'givebpkitem=false' to not give backpack item with the backpack (works only if ammo limits for the backpack are not set)
Backpacks: SmallAmmoBag#{ ClipExGive=10, ShellExGive=4, RocketAmmoExGive=1, CellExGive=20, ClipExMax=300, ShellExMax=80, RocketAmmoExMax=80, CellExMax=480 }
         : BackpackEx#{ ClipExGive=20, ShellExGive=8, RocketAmmoExGive=2, CellExGive=40, ClipExMax=400, ShellExMax=100, RocketAmmoExMax=100, CellExMax=600 }
         : HeavyBackpack#{ ClipExGive=50, ShellExGive=20, RocketAmmoExGive=5, CellExGive=100, ClipExMax=600, ShellExMax=200, RocketAmmoExMax=200, CellExMax=920 }

// ammo pack definitions - ammo packs will be spawned on the map;
// always goes separately from ammo actors (must always be Custom Inventory items);
// at (string) - type of ammo the pack gives;
// amount (number) - ammo amount the pack gives
AmmoPacks: SingleClipEx#{ at="ClipEx", amount=10 }
         : SomeShellsEx#{ at="ShellEx", amount=4 }
         : SingleRocketEx#{ at="RocketAmmoEx", amount=1 }
         : SmallCellBoxEx#{ at="CellEx", amount=20 }

         : ClipBoxEx#{ at="ClipEx", amount=50 }
         : ShellBoxEx#{ at="ShellEx", amount=20 }
         : RocketBoxEx#{ at="RocketAmmoEx", amount=5 }
         : CellPackEx#{ at="CellEx", amount=100 }

// weapon definitions - real weapon actors (not to be placed on maps), these are separated from weapon things;
// at or at1 (string) - ammo type for primary fire (at1 value always hides at value if both are presented);
// at2 (string) - ammo type for secondary fire
Weapons: PistolEx#{ at="ClipEx" }
       : ShotgunEx#{ at="ShellEx" }
       : SuperShotgunEx#{ at="ShellEx" }
       : ChaingunEx#{ at="ClipEx" }
       : RocketLauncherEx#{ at="RocketAmmoEx" }
       : PlasmaRifleEx#{ at="CellEx" }
       : BFG9000Ex#{ at="CellEx" }

// weapon thing definitions - these will be spawned on maps and give actual weapons with some ammo;
// wpname (string) - weapon actor name to give on pickup;
// ag or ag1 (number) - give amount of at/at1 ammo on pickup of the weapon thing (ag1 value always hides ag value if both are presented);
// ag2 (number) - give amount of at2 ammo on pickup of the weapon thing
WeaponThings: PistolExThing#{ wpname="PistolEx", ag=20 }
            : ShotgunExThing#{ wpname="ShotgunEx", ag=8 } ShotgunExDropped#{ wpname="ShotgunEx", ag=4 }
            : SuperShotgunExThing#{ wpname="SuperShotgunEx", ag=8 }
            : ChaingunExThing#{ wpname="ChaingunEx", ag=20 } ChaingunExDropped#{ wpname="ChaingunEx", ag=10 }
            : RocketLauncherExThing#{ wpname="RocketLauncherEx", ag=2 }
            : PlasmaRifleExThing#{ wpname="PlasmaRifleEx", ag=40 }
            : BFG9000ExThing#{ wpname="BFG9000Ex", ag=40 }


// actors with these names will be created by backpack-gen;
// these values are default for presented item keys
//UtilityActors: BackpackMarker#"BackpackMarker"
//             : AmmoLimitsChecker#"AmmoLimitsChecker"
//             : AmmoLimitNotReachedMarker#"AmmoLimitNotReachedMarker"
```


### Lump templates

DECORATE and SBARINFO templates should contain ordinary DECORATE/SBARINFO code along with ```@BpkGen*``` labels. These labels are processed by the Backpack Generator tool - they are overwritten with the actual generated code. Labels may look like:

  - ```@BpkGenLabelText```, where ```LabelText``` may be any number of chars - alphanumeric OR hyphens OR underscores, ```@``` may be used to terminate this type of label e.g. ```@BpkGenLabelText@Further template text```;

  - ```@BpkGenLabelText(args text)```, where ```LabelText``` can be similar to label with no args and ```args text``` may be any text with closing brackets and backslashes escaped like ```\)``` and ```\\```.

 Escape the text ```@BpkGen``` with leading ```@``` - ```@@BpkGen``` is translated into ```@BpkGen``` text. Backpack Generator tool by default checks for bad labels (misplaced, duplicated, unknown, etc.), this checks may be disabled by the --ignore-template-label-errors parameter.

Valid labels in DECORATE:
  - ```@BpkGenAmmoLimitsCheckerDefinition``` - AmmoLimitsChecker actor definition will be placed instead;
  - ```@BpkGenAmmoLimitsChecker``` - AmmoLimitsChecker actor name will be placed instead;
  - ```@BpkGenMoreProps(ActorName)``` - properties for ActorName generated by the tool will be placed instead (required for ammo types and weapons);
  - ```@BpkGenMoreStates(ActorName)``` - states for ActorName generated by the tool will be placed instead (required for item things for pickup);
  
Valid labels in SBARINFO:
  - ```@BpkGenDrawAmmoMaxAmounts(code)``` - generate the ammo max amounts drawing code for each backpack, ```code``` should draw these max amounts;
  - ```@BpkGenAmmoMaxAmount(AmmoTypeActorName)``` - the tool writes max amount of the ```AmmoTypeActorName``` ammo type instead, used only inside of ```@BpkGenDrawAmmoMaxAmounts(code)``` (so, the closing parenthesis needs to be escaped).


### Usage

ZDoom Backpack Generator is command line tool. How to use the tool (its help command output):

```
Usage:

  backpack-gen.exe <ammo-config-file-path> [<output-file-path>] [<options>]

Arguments:

  <ammo-config-file-path> - ammo config file path (required)

  <output-file-path> - output WAD file path (optional, default - 'backpacks.wad')

Options:

  -addfile <file-path> - add more files 'as is' to the output WAD file

  --decorate-template-file=<file-path> - path of DECORATE lump template file

  --sbarinfo-template-file=<file-path> - path of SBARINFO lump template file

  --ignore-template-label-errors - do not abort on errors related to labels in templates (missing, duplicated, extra, redundant, misplased, bad parameters or others)

  --help, -h or /? - display this message
```


### Build

Visual Studio 2017 was used to create, build and test this project. To build the project:

1. Open the solution in Visual Studio 2017.

2. Choose any platform (x86 or x64) and Release configuration.

3. Build the solution.


### Test

For testing [ZDoom](https://zdoom.org/downloads) is required and an IWAD (DOOM2.WAD, etc.). They should be put together in a folder. Also, ZDoom Backpack Generator tool should be built.

1. Launch Run.bat - it will produce the file "backpacks.wad".

2. Drag backpacks.wad onto zdoom.exe.

3. Start new game. You should see a special map with many ammo, weapon and backpack items.

4. Run around and check that the amount of ammo you can carry is correct. Test version contains three backpacks - they all are presented on the map and all ammo limits may be checked.


## Known issues

Some cheat console commands work incorrectly with the new backpacks. E.g. ```give ammo``` may not respect current ammo limits.


## License

Copyright (c) 2022 Andrey Anisimov <<https://github.com/varyier>>

This software is released under the terms of the MIT License.
See the [LICENSE](LICENSE) file for further information.