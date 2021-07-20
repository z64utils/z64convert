enum collision_flags {
	CF_WARP = 0,
	CF_CAMERA,
	CF_ECHO,
	CF_LIGHTING,
	CF_TERRAIN,
	CF_SOUND,
	CF_IGNORE,
	CF_CONVEYOR,
	CF_HOOKSHOT,
	CF_NOHORSE,
	CF_RAYCAST,
	CF_FLOOR,
	CF_WALL,
	CF_SPECIAL,
	CF_WATERBOX
};

enum collision_flag_shift {
	SHIFT_FLOOR = (26 - 2),
	SHIFT_WALL = (20 - 0),
	SHIFT_SPECIAL = (13 - 1)
};

enum collision_flag_and {
	AND_FLOOR = 0x3C000000,
	AND_WALL = 0x03E00000,
	AND_SPECIAL = 0x0003E000,
	AND_SOUND = 0xF
};

struct collision_flag {
	char *name, type;
	uint32_t hi, lo, and, name_numchar;
};

struct polytype {
	int type;
	uint32_t hi, lo;
	uint16_t v1, v2, v3;
};

static struct collision_flag collision_flag[] = {
// Ignore Settings
	// Camera can pass through collision
	{ "IGNORE_CAMERA",
		CF_IGNORE,
		0x2000
	},
	// Link, enemies, etc. can pass through collision
	{ "IGNORE_ENTITY",
		CF_IGNORE,
		0x4000
	},
	// Deku Seeds, Arrows, Bombchus, etc. can pass through collision
	{ "IGNORE_AMMO",
		CF_IGNORE,
		0x8000
	},
// Sound Settings (table D_80119E10)
	// Earth/Dirt
	{ "SOUND_DIRT",
		CF_SOUND,
		0,
		0, // NA_SE_PL_WALK_GROUND
		AND_SOUND,
		-1
	},
	// Sand
	{ "SOUND_SAND",
		CF_SOUND,
		0,
		1, // NA_SE_PL_WALK_SAND
		AND_SOUND,
		-1
	},
	// Stone
	{ "SOUND_STONE",
		CF_SOUND,
		0,
		2, // NA_SE_PL_WALK_CONCRETE
		AND_SOUND,
		-1
	},
	// Stone (wet)
	{ "SOUND_STONE_WET",
		CF_SOUND,
		0,
		3, // NA_SE_PL_WALK_DIRT
		AND_SOUND,
		-1
	},
	// Shallow water
	{ "SOUND_SPLASH",
		CF_SOUND,
		0,
		4, // NA_SE_PL_WALK_WATER0
		AND_SOUND,
		-1
	},
	// Shallow water (lower-pitched)
	{ "SOUND_SPLASH_1",
		CF_SOUND,
		0,
		5, // NA_SE_PL_WALK_WATER1
		AND_SOUND,
		-1
	},
	// Underbrush/Grass
	{ "SOUND_GRASS",
		CF_SOUND,
		0,
		6, // NA_SE_PL_WALK_WATER2
		AND_SOUND,
		-1
	},
	// Lava/Goo
	{ "SOUND_LAVA",
		CF_SOUND,
		0,
		7, // NA_SE_PL_WALK_MAGMA
		AND_SOUND,
		-1
	},
	// Earth/Dirt (duplicate)
	{ "SOUND_DIRT_1",
		CF_SOUND,
		0,
		8, // NA_SE_PL_WALK_GRASS
		AND_SOUND,
		-1
	},
	// Wooden
	{ "SOUND_WOOD",
		CF_SOUND,
		0,
		9, // NA_SE_PL_WALK_GLASS
		AND_SOUND,
		-1
	},
	// Packed Earth/Wood
	{ "SOUND_WOOD_STRUCK", /* formerly SOUND_DIRT_PACKED */
		CF_SOUND,
		0,
		10, // NA_SE_PL_WALK_LADDER
		AND_SOUND,
		-1
	},
	// Earth/Dirt (duplicate)
	{ "SOUND_DIRT_2",
		CF_SOUND,
		0,
		11, // NA_SE_PL_WALK_GROUND
		AND_SOUND,
		-1
	},
	// Ceramic
	{ "SOUND_CERAMIC",
		CF_SOUND,
		0,
		12, // NA_SE_PL_WALK_ICE
		AND_SOUND,
		-1
	},
	// Loose Earth/Dirt
	{ "SOUND_DIRT_LOOSE",
		CF_SOUND,
		0,
		13, // NA_SE_PL_WALK_IRON
		AND_SOUND,
		-1
	},
// Floor Settings
	// Small pit, voids Link out
	{ "FLOOR_VOID_SCENE", /* formerly FLOOR_PIT_SMALL */
		CF_FLOOR,
		0x14 << SHIFT_FLOOR,
		0,
		AND_FLOOR,
		-1
	},
	// Instead of jumping, climb down
	{ "FLOOR_JUMP_VINE",
		CF_FLOOR,
		0x18 << SHIFT_FLOOR,
		0,
		AND_FLOOR,
		-1
	},
	// Instead of jumping, hang from ledge
	{ "FLOOR_JUMP_HANG",
		CF_FLOOR,
		0x20 << SHIFT_FLOOR,
		0,
		AND_FLOOR,
		-1
	},
	// Instead of jumping, step off the platform into falling state
	{ "FLOOR_JUMP_FALL",
		CF_FLOOR,
		0x24 << SHIFT_FLOOR,
		0,
		AND_FLOOR,
		-1
	},
	// Instead of jumping, activate diving animation/state
	{ "FLOOR_JUMP_DIVE",
		CF_FLOOR,
		0x2C << SHIFT_FLOOR,
		0,
		AND_FLOOR,
		-1
	},
	// Large pit, voids Link out
	{ "FLOOR_VOID_ROOM", /* formerly FLOOR_PIT_LARGE */
		CF_FLOOR,
		0x30 << SHIFT_FLOOR,
		0,
		AND_FLOOR,
		-1
	},
	// Floor Settings (SPECIAL)
		// Lava
		{ "FLOOR_LAVA",
			CF_SPECIAL,
			0x04 << SHIFT_SPECIAL,
			0,
			AND_SPECIAL,
			-1
		},
		// Lava (TODO: What's the difference?)
		{ "FLOOR_LAVA_1",
			CF_SPECIAL,
			0x06 << SHIFT_SPECIAL,
			0,
			AND_SPECIAL,
			-1
		},
		// Sand
		{ "FLOOR_SAND",
			CF_SPECIAL,
			0x08 << SHIFT_SPECIAL,
			0,
			AND_SPECIAL,
			-1
		},
		// Ice
		{ "FLOOR_ICE",
			CF_SPECIAL,
			0x0A << SHIFT_SPECIAL,
			0,
			AND_SPECIAL,
			-1
		},
		// No Fall Damage
		{ "FLOOR_NOFALLDMG",
			CF_SPECIAL,
			0x0C << SHIFT_SPECIAL,
			0,
			AND_SPECIAL,
			-1
		},
		// Quicksand, passable on horseback
		{ "FLOOR_QUICKHORSE",
			CF_SPECIAL,
			0x0E << SHIFT_SPECIAL,
			0,
			AND_SPECIAL,
			-1
		},
		// Quicksand
		{ "FLOOR_QUICKSAND",
			CF_SPECIAL,
			0x18 << SHIFT_SPECIAL,
			0,
			AND_SPECIAL,
			-1
		},
		// Steep Surface (causes Link to slide)
		{ "FLOOR_STEEP",
			CF_WALL,
			0,
			0x30,
			0x30,
			-1
		},
// Wall Settings (table D_80119D90)
// FIXME change SHIFT_WALL and use 1 2 3 etc not 2 4 6
	// Link will not jump over or attempt to climb the wall,
	// even if it is short enough for these actions
	{ "WALL_BARRIER", /* formerly WALL_NOGRAB */
		CF_WALL,
		0x2 << SHIFT_WALL,
		0,
		AND_WALL,
		-1
	},
	// Ladder
	{ "WALL_LADDER",
		CF_WALL,
		0x4 << SHIFT_WALL,
		0,
		AND_WALL,
		-1
	},
	// Ladder (Top), makes Link climb down onto a ladder
	{ "WALL_LADDER_TOP",
		CF_WALL,
		0x6 << SHIFT_WALL,
		0,
		AND_WALL,
		-1
	},
	// Climbable Vine Wall
	{ "WALL_VINES",
		CF_WALL,
		0x8 << SHIFT_WALL,
		0,
		AND_WALL,
		-1
	},
	// Wall used to activate/deactivate crawling
	{ "WALL_CRAWL",
		CF_WALL,
		0xA << SHIFT_WALL,
		0,
		AND_WALL,
		-1
	},
	// TODO: What's the difference?
	{ "WALL_CRAWL_1",
		CF_WALL,
		0xC << SHIFT_WALL,
		0,
		AND_WALL,
		-1
	},
	// Pushblock
	{ "WALL_PUSHBLOCK",
		CF_WALL,
		0xE << SHIFT_WALL,
		0,
		AND_WALL,
		-1
	},
	// Wall Damage
	{ "WALL_DAMAGE",
		CF_WALL,
		0,
		0x08000000,//1 << 27, // (Although this is organized here, it's stored in lower 32 bits of flags)
		0x08000000,
		-1
	},
// Ungrouped Settings
	// Spawns "blood" particles when struck
	// Overrides struck sound
	// (used in Jabu-Jabu's Belly)
	// (TODO: Walls, floors, or both?)
	{ "SPECIAL_BLEEDWALL",
		CF_SPECIAL,
		0x10 << SHIFT_SPECIAL,
		0,
		AND_SPECIAL,
		-1
	},
	// Instantly void out on contact
	{ "SPECIAL_INSTAVOID",
		CF_SPECIAL,
		0x12 << SHIFT_SPECIAL,
		0,
		AND_SPECIAL,
		-1
	},
	// Causes Link to look upwards (TODO: What does that even mean?) (TODO: Walls, floors, or both?)
	{ "SPECIAL_LOOKUP",
		CF_SPECIAL,
		0x16 << SHIFT_SPECIAL,
		0,
		AND_SPECIAL,
		-1
	},
	// Epona can't walk on the polygon
	{ "NOHORSE",
		CF_NOHORSE,
		0x80000000,
		0,
		0x80000000,
		-1
	},
	// Paths? Decreases surface height in raycast function by 1
	{ "RAYCAST",
		CF_NOHORSE,
		0x40000000,
		0,
		0x40000000,
		-1
	},
	// Hookshot
	{ "HOOKSHOT",
		CF_NOHORSE,
		0,
		0x00020000,
		0x00020000,
		-1
	},
	// Waterbox
	// optionally, WATERBOX_%d_%d_%d (WATERBOX_LIGHT_CAMERA_ROOM)
	// light = environment/lighting setting to use while camera is inside waterbox (max 1F)
	// camera = fixed camera setting to use (max 255)
	// room = room where waterbox is active (optional; if unset, will be active in all rooms) (max 98)
	{ "WATERBOX",
		CF_WATERBOX,
		.name_numchar = 8
	},
// Settings requiring numbers
	// WARP_%d: Scene Exit Table Index
	{ "WARP_",
		CF_WARP,
		8,
		0,
		0x1F00,
		5
	},
	// CAMERA_%d: Mesh Camera Data Index ID
	{ "CAMERA_", /* TODO confirm working */
		CF_CAMERA,
		0,
		0,
		0xFF,
		7
	},
	// ECHO_%d: TODO: Confirm if this controls sound echo, music echo, etc.
	{ "ECHO_",
		CF_ECHO,
		0,
		11,
		0x1F800,
		5
	},
	// LIGHTING_%d: TODO: Confirm if this controls sound echo, music echo, etc.
	{ "LIGHTING_",
		CF_LIGHTING,
		0,
		6,
		0x7C0,
		9
	},
	// CONVEYOR_%d_%d_INHERIT: CONVEYOR_direction_speed_INHERIT
	// direction range 0 - 360
	// speed range 0.none, 1.slow, 2.medium, 3.fast
	// INHERIT is optional; if enabled, a 0-speed conveyor triangle will have the speed of the conveyor triangle stepped on immediately before it
	{ "CONVEYOR_",
		CF_CONVEYOR,
		0,
		18,
		0x07FC0000,
		9
	},
};

static int collision_flag_count = sizeof(collision_flag) / sizeof(collision_flag[0]);

