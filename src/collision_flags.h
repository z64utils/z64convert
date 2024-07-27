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

enum collision_flag_word {
	CF_HIGH,
	CF_LOW,
};

enum collision_flag_shift {
	SHIFT_FLOOR = (26 - 2),
	SHIFT_WALL = (20 - 0),
	SHIFT_SPECIAL = (13)
};

enum collision_flag_and {
	AND_FLOOR = 0x3C000000,
	AND_WALL = 0x03E00000,
	AND_SPECIAL = 0x0003E000,
	AND_SOUND = 0xF
};

struct collision_flag {
	char *name, type;
	enum collision_flag_word word;
	uint32_t bits, name_numchar;
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
		CF_HIGH,
		0x2000
	},
	// Link, enemies, etc. can pass through collision
	{ "IGNORE_ENTITY",
		CF_IGNORE,
		CF_HIGH,
		0x4000
	},
	// Deku Seeds, Arrows, Bombchus, etc. can pass through collision
	{ "IGNORE_AMMO",
		CF_IGNORE,
		CF_HIGH,
		0x8000
	},
// Sound Settings (table D_80119E10)
	// Earth/Dirt
	{ "SOUND_DIRT",
		CF_SOUND,
		CF_LOW,
		0, // NA_SE_PL_WALK_GROUND
		-1
	},
	// Sand
	{ "SOUND_SAND",
		CF_SOUND,
		CF_LOW,
		1, // NA_SE_PL_WALK_SAND
		-1
	},
	// Stone
	{ "SOUND_STONE",
		CF_SOUND,
		CF_LOW,
		2, // NA_SE_PL_WALK_CONCRETE
		-1
	},
	// Stone (wet)
	{ "SOUND_STONE_WET",
		CF_SOUND,
		CF_LOW,
		3, // NA_SE_PL_WALK_DIRT
		-1
	},
	// Shallow water
	{ "SOUND_SPLASH",
		CF_SOUND,
		CF_LOW,
		4, // NA_SE_PL_WALK_WATER0
		-1
	},
	// Shallow water (lower-pitched)
	{ "SOUND_SPLASH_1",
		CF_SOUND,
		CF_LOW,
		5, // NA_SE_PL_WALK_WATER1
		-1
	},
	// Underbrush/Grass
	{ "SOUND_GRASS",
		CF_SOUND,
		CF_LOW,
		6, // NA_SE_PL_WALK_WATER2
		-1
	},
	// Lava/Goo
	{ "SOUND_LAVA",
		CF_SOUND,
		CF_LOW,
		7, // NA_SE_PL_WALK_MAGMA
		-1
	},
	// Earth/Dirt (duplicate)
	{ "SOUND_DIRT_1",
		CF_SOUND,
		CF_LOW,
		8, // NA_SE_PL_WALK_GRASS
		-1
	},
	// Wooden
	{ "SOUND_WOOD",
		CF_SOUND,
		CF_LOW,
		9, // NA_SE_PL_WALK_GLASS
		-1
	},
	// Packed Earth/Wood
	{ "SOUND_WOOD_STRUCK", /* formerly SOUND_DIRT_PACKED */
		CF_SOUND,
		CF_LOW,
		10, // NA_SE_PL_WALK_LADDER
		-1
	},
	// Earth/Dirt (duplicate)
	{ "SOUND_DIRT_2",
		CF_SOUND,
		CF_LOW,
		11, // NA_SE_PL_WALK_GROUND
		-1
	},
	// Ceramic
	{ "SOUND_CERAMIC",
		CF_SOUND,
		CF_LOW,
		12, // NA_SE_PL_WALK_ICE
		-1
	},
	// Loose Earth/Dirt
	{ "SOUND_DIRT_LOOSE",
		CF_SOUND,
		CF_LOW,
		13, // NA_SE_PL_WALK_IRON
		-1
	},
// Floor Settings
	// Small pit, voids Link out
	{ "FLOOR_VOID_SCENE", /* formerly FLOOR_PIT_SMALL */
		CF_FLOOR,
		CF_HIGH,
		0x14 << SHIFT_FLOOR,
		-1
	},
	// Instead of jumping, climb down
	{ "FLOOR_JUMP_VINE",
		CF_FLOOR,
		CF_HIGH,
		0x18 << SHIFT_FLOOR,
		-1
	},
	// Instead of jumping, hang from ledge
	{ "FLOOR_JUMP_HANG",
		CF_FLOOR,
		CF_HIGH,
		0x20 << SHIFT_FLOOR,
		-1
	},
	// Instead of jumping, step off the platform into falling state
	{ "FLOOR_JUMP_FALL",
		CF_FLOOR,
		CF_HIGH,
		0x24 << SHIFT_FLOOR,
		-1
	},
	// Instead of jumping, activate diving animation/state
	{ "FLOOR_JUMP_DIVE",
		CF_FLOOR,
		CF_HIGH,
		0x2C << SHIFT_FLOOR,
		-1
	},
	// Large pit, voids Link out
	{ "FLOOR_VOID_ROOM", /* formerly FLOOR_PIT_LARGE */
		CF_FLOOR,
		CF_HIGH,
		0x30 << SHIFT_FLOOR,
		-1
	},
	// Floor Settings (SPECIAL)
		// Lava
		{ "FLOOR_LAVA",
			CF_SPECIAL,
			CF_HIGH,
			2 << SHIFT_SPECIAL,
			-1
		},
		// Lava (TODO: What's the difference?)
		{ "FLOOR_LAVA_1",
			CF_SPECIAL,
			CF_HIGH,
			3 << SHIFT_SPECIAL,
			-1
		},
		// Sand
		{ "FLOOR_SAND",
			CF_SPECIAL,
			CF_HIGH,
			4 << SHIFT_SPECIAL,
			-1
		},
		// Ice
		{ "FLOOR_ICE",
			CF_SPECIAL,
			CF_HIGH,
			5 << SHIFT_SPECIAL,
			-1
		},
		// No Fall Damage
		{ "FLOOR_NOFALLDMG",
			CF_SPECIAL,
			CF_HIGH,
			6 << SHIFT_SPECIAL,
			-1
		},
		// Quicksand, passable on horseback
		{ "FLOOR_QUICKHORSE",
			CF_SPECIAL,
			CF_HIGH,
			12 << SHIFT_SPECIAL,
			-1
		},
		// Quicksand
		{ "FLOOR_QUICKSAND",
			CF_SPECIAL,
			CF_HIGH,
			7 << SHIFT_SPECIAL,
			-1
		},
		// Steep Surface (causes Link to slide)
		{ "FLOOR_STEEP",
			CF_WALL,
			CF_LOW,
			0x10,
			-1
		},
// Wall Settings (table D_80119D90)
// FIXME change SHIFT_WALL and use 1 2 3 etc not 2 4 6
	// Link will not jump over or attempt to climb the wall,
	// even if it is short enough for these actions
	{ "WALL_BARRIER", /* formerly WALL_NOGRAB */
		CF_WALL,
		CF_HIGH,
		0x2 << SHIFT_WALL,
		-1
	},
	// Ladder
	{ "WALL_LADDER",
		CF_WALL,
		CF_HIGH,
		0x4 << SHIFT_WALL,
		-1
	},
	// Ladder (Top), makes Link climb down onto a ladder
	{ "WALL_LADDER_TOP",
		CF_WALL,
		CF_HIGH,
		0x6 << SHIFT_WALL,
		-1
	},
	// Climbable Vine Wall
	{ "WALL_VINES",
		CF_WALL,
		CF_HIGH,
		0x8 << SHIFT_WALL,
		-1
	},
	// Wall used to activate/deactivate crawling
	{ "WALL_CRAWL",
		CF_WALL,
		CF_HIGH,
		0xA << SHIFT_WALL,
		-1
	},
	// TODO: What's the difference?
	{ "WALL_CRAWL_1",
		CF_WALL,
		CF_HIGH,
		0xC << SHIFT_WALL,
		-1
	},
	// Pushblock
	{ "WALL_PUSHBLOCK",
		CF_WALL,
		CF_HIGH,
		0xE << SHIFT_WALL,
		-1
	},
	// Wall Damage
	{ "WALL_DAMAGE",
		CF_WALL,
		CF_LOW,
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
		CF_HIGH,
		8 << SHIFT_SPECIAL,
		-1
	},
	// Instantly void out on contact
	{ "SPECIAL_INSTAVOID",
		CF_SPECIAL,
		CF_HIGH,
		9 << SHIFT_SPECIAL,
		-1
	},
	// Causes Link to look upwards (TODO: What does that even mean?) (TODO: Walls, floors, or both?)
	{ "SPECIAL_LOOKUP",
		CF_SPECIAL,
		CF_HIGH,
		11 << SHIFT_SPECIAL,
		-1
	},
	// Epona can't walk on the polygon
	{ "NOHORSE",
		CF_NOHORSE,
		CF_HIGH,
		0x80000000,
		-1
	},
	// Paths? Decreases surface height in raycast function by 1
	{ "RAYCAST",
		CF_NOHORSE,
		CF_HIGH,
		0x40000000,
		-1
	},
	// Hookshot
	{ "HOOKSHOT",
		CF_HOOKSHOT,
		CF_LOW,
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
	{ "WARP,",
		CF_WARP,
		CF_HIGH,
		0x1F00,
		5
	},
	// CAMERA_%d: Mesh Camera Data Index ID
	{ "CAMERA,", /* TODO confirm working */
		CF_CAMERA,
		CF_HIGH,
		0xFF,
		7
	},
	// ECHO_%d: TODO: Confirm if this controls sound echo, music echo, etc.
	{ "ECHO,",
		CF_ECHO,
		CF_LOW,
		0x1F800,
		5
	},
	// LIGHTING_%d: TODO: Confirm if this controls sound echo, music echo, etc.
	{ "LIGHTING,",
		CF_LIGHTING,
		CF_LOW,
		0x7C0,
		9
	},
	// CONVEYOR_%d_%d_INHERIT: CONVEYOR_direction_speed_INHERIT
	// direction range 0 - 360
	// speed range 0.none, 1.slow, 2.medium, 3.fast
	// INHERIT is optional; if enabled, a 0-speed conveyor triangle will have the speed of the conveyor triangle stepped on immediately before it
	{ "CONVEYOR,",
		CF_CONVEYOR,
		CF_LOW,
		0x07FC0000,
		9
	},
};

static int collision_flag_count = sizeof(collision_flag) / sizeof(collision_flag[0]);

