#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#define MAX_INPUT 128

enum {
    DIR_NORTH = 0,
    DIR_EAST  = 1,
    DIR_SOUTH = 2,
    DIR_WEST  = 3,
    DIR_UP    = 4,
    DIR_DOWN  = 5,
    NUM_DIRS  = 6
};

typedef struct {
    const char *name;
    const char *description;
    int exits[NUM_DIRS]; /* indices of destination rooms, or -1 if none */
    int is_dark;
} Room;

typedef struct {
    const char *name;
    const char *description;
    int location; /* room index, -1 = carried, -2 = gone */
    int takeable;
} Item;

#define NUM_ROOMS 7
#define NUM_ITEMS 4

/* Rooms: indices 0..6 */
static Room rooms[NUM_ROOMS] = {
    {
        "Forest Path",
        "You are on a narrow forest path leading to a lonely cabin to the north. "
        "The trees close in around you.",
        { 1, -1, -1, -1, -1, -1 },
        0
    },
    {
        "Cabin Porch",
        "You stand on the creaking porch of a weathered cabin. The forest lies to the south; "
        "a door leads north inside.",
        { 2, -1, 0, -1, -1, -1 },
        0
    },
    {
        "Main Room",
        "Dusty furniture and cobwebs fill the cabin's main room. "
        "A hallway leads east, stairs climb up, and a dark stairwell descends.",
        { -1, 3, 1, -1, 5, 4 },
        0
    },
    {
        "Study",
        "A cramped study with a desk piled high with yellowed papers. "
        "The only exit is back west.",
        { -1, -1, -1, 2, -1, -1 },
        0
    },
    {
        "Cellar",
        "A damp stone cellar. The air is cold, and water drips from the ceiling. "
        "A heavy iron door stands to the east.",
        { -1, -1, -1, -1, 2, -1 },
        1
    },
    {
        "Loft",
        "The low loft is cluttered with old trunks and crates. Dust hangs in the air.",
        { -1, -1, -1, -1, -1, 2 },
        0
    },
    {
        "Hidden Vault",
        "A small vault carved into the rock. Faint light glints off an object on a stone pedestal.",
        { -1, -1, -1, 4, -1, -1 },
        1
    }
};

enum {
    ITEM_LANTERN = 0,
    ITEM_NOTE    = 1,
    ITEM_KEY     = 2,
    ITEM_RELIC   = 3
};

static Item items[NUM_ITEMS] = {
    {
        "lantern",
        "An old brass lantern with a cracked glass chimney.",
        2,
        1
    },
    {
        "note",
        "A folded, yellowed note. You could read it.",
        3,
        1
    },
    {
        "key",
        "A small iron key, rough with rust.",
        5,
        1
    },
    {
        "relic",
        "An ancient stone idol, cool to the touch.",
        6,
        1
    }
};

static const char *direction_names[NUM_DIRS] = {
    "north", "east", "south", "west", "up", "down"
};

static int current_room = 0;
static int lantern_lit = 0;
static int door_unlocked = 0;

/* Utility functions */

static int direction_from_word(const char *word) {
    if (strcmp(word, "north") == 0 || strcmp(word, "n") == 0) return DIR_NORTH;
    if (strcmp(word, "east")  == 0 || strcmp(word, "e") == 0) return DIR_EAST;
    if (strcmp(word, "south") == 0 || strcmp(word, "s") == 0) return DIR_SOUTH;
    if (strcmp(word, "west")  == 0 || strcmp(word, "w") == 0) return DIR_WEST;
    if (strcmp(word, "up")    == 0 || strcmp(word, "u") == 0) return DIR_UP;
    if (strcmp(word, "down")  == 0 || strcmp(word, "d") == 0) return DIR_DOWN;
    return -1;
}

static int is_direction_word(const char *word) {
    return direction_from_word(word) >= 0;
}

static int find_item_in_room(const char *name, int room_index) {
    int i;
    for (i = 0; i < NUM_ITEMS; i++) {
        if (items[i].location == room_index && strcmp(items[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static int find_item_in_inventory(const char *name) {
    int i;
    for (i = 0; i < NUM_ITEMS; i++) {
        if (items[i].location == -1 && strcmp(items[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

/* Game functions */

static void list_items_in_room(int room_index) {
    int i, found = 0;
    for (i = 0; i < NUM_ITEMS; i++) {
        if (items[i].location == room_index) {
            if (!found) {
                found = 1;
            }
            printf("You see a %s here.\n", items[i].name);
        }
    }
}

static void list_exits(int room_index) {
    int i, first = 1;
    printf("Exits: ");
    for (i = 0; i < NUM_DIRS; i++) {
        if (rooms[room_index].exits[i] >= 0) {
            if (!first) {
                printf(", ");
            }
            printf("%s", direction_names[i]);
            first = 0;
        }
    }
    if (first) {
        printf("none");
    }
    printf(".\n");
}

static void describe_room(void) {
    Room *r = &rooms[current_room];
    printf("\n== %s ==\n", r->name);
    if (r->is_dark && !lantern_lit) {
        printf("It is pitch dark here. You may be eaten by a grue.\n");
        return;
    }
    printf("%s\n", r->description);
    list_items_in_room(current_room);
    list_exits(current_room);
}

static void show_help(void) {
    printf("Commands:\n");
    printf("  look or l                 - look around\n");
    printf("  go <direction>            - move (north/south/east/west/up/down)\n");
    printf("  n,s,e,w,u,d               - short directions\n");
    printf("  take <item>               - pick something up\n");
    printf("  examine/read <item>       - inspect something\n");
    printf("  use <item>                - use an item\n");
    printf("  inventory or i            - list what you're carrying\n");
    printf("  help                      - show this help\n");
    printf("  quit                      - exit the game\n");
}

static void do_look(void) {
    describe_room();
}

static void do_go(const char *direction_word) {
    int dir = direction_from_word(direction_word);
    int dest;

    if (dir < 0) {
        printf("I don't know which way that is.\n");
        return;
    }

    /* Special case: locked door in the cellar */
    if (current_room == 4 && dir == DIR_EAST && !door_unlocked) {
        printf("The iron door to the east is locked.\n");
        return;
    }

    dest = rooms[current_room].exits[dir];
    if (dest < 0) {
        printf("You can't go that way.\n");
        return;
    }

    current_room = dest;
    do_look();
}

static void do_take(const char *name) {
    int item_index;

    item_index = find_item_in_room(name, current_room);
    if (item_index < 0) {
        printf("You don't see that here.\n");
        return;
    }

    if (!items[item_index].takeable) {
        printf("You can't take that.\n");
        return;
    }

    items[item_index].location = -1;
    printf("Taken.\n");
}

static void do_inventory(void) {
    int i, found = 0;
    printf("You are carrying:\n");
    for (i = 0; i < NUM_ITEMS; i++) {
        if (items[i].location == -1) {
            printf("  %s\n", items[i].name);
            found = 1;
        }
    }
    if (!found) {
        printf("  nothing\n");
    }
}

static void do_examine(const char *name) {
    int item_index = find_item_in_inventory(name);
    if (item_index < 0) {
        item_index = find_item_in_room(name, current_room);
    }
    if (item_index < 0) {
        printf("You see no such thing.\n");
        return;
    }

    if (item_index == ITEM_NOTE) {
        printf("The note reads:\n");
        printf("\"Above the dust, the iron waits.\n");
        printf("  Below the stairs, the iron yields.\"\n");
    } else {
        printf("%s\n", items[item_index].description);
    }
}

static void do_use(const char *name) {
    int item_index = find_item_in_inventory(name);

    if (item_index < 0) {
        printf("You don't have that.\n");
        return;
    }

    if (item_index == ITEM_LANTERN) {
        if (lantern_lit) {
            printf("The lantern is already lit.\n");
        } else {
            lantern_lit = 1;
            printf("You light the lantern. The room brightens.\n");
            if (rooms[current_room].is_dark) {
                describe_room();
            }
        }
    } else if (item_index == ITEM_KEY) {
        if (current_room == 4 && !door_unlocked) {
            door_unlocked = 1;
            rooms[4].exits[DIR_EAST] = 6;
            printf("You unlock the iron door to the east.\n");
        } else {
            printf("Nothing happens.\n");
        }
    } else {
        printf("You can't find a good way to use that here.\n");
    }
}

static void check_victory(void) {
    if (items[ITEM_RELIC].location == -1 && current_room == 0) {
        printf("\nClutching the ancient relic, you step back onto the forest path.\n");
        printf("The cabin fades into the trees behind you as a sense of triumph washes over you.\n");
        printf("You have escaped with the relic. You win!\n");
        exit(0);
    }
}

int main(void) {
    char input[MAX_INPUT];

    printf("Welcome to the Cabin of the Lost Relic!\n");
    printf("Type 'help' for a list of commands.\n");
    do_look();

    for (;;) {
        char *p, *verb, *noun;
        size_t len;
        int i;

        printf("\n> ");
        if (!fgets(input, sizeof(input), stdin)) {
            printf("\n");
            break; /* EOF */
        }

        len = strlen(input);
        if (len > 0 && input[len - 1] == '\n') {
            input[len - 1] = '\0';
        }

        /* convert to lowercase */
        for (i = 0; input[i]; i++) {
            input[i] = (char)tolower((unsigned char)input[i]);
        }

        /* skip leading spaces */
        p = input;
        while (*p == ' ' || *p == '\t') {
            p++;
        }

        if (*p == '\0') {
            continue; /* empty line */
        }

        verb = strtok(p, " \t");
        noun = strtok(NULL, "");
        if (noun) {
            while (*noun == ' ' || *noun == '\t') {
                noun++;
            }
            if (*noun == '\0') {
                noun = NULL;
            }
        }

        if (is_direction_word(verb)) {
            do_go(verb);
        } else if (strcmp(verb, "go") == 0 || strcmp(verb, "walk") == 0 || strcmp(verb, "move") == 0) {
            if (!noun) {
                printf("Go where?\n");
            } else {
                do_go(noun);
            }
        } else if (strcmp(verb, "look") == 0 || strcmp(verb, "l") == 0) {
            do_look();
        } else if (strcmp(verb, "take") == 0 || strcmp(verb, "get") == 0 || strcmp(verb, "pick") == 0) {
            if (!noun) {
                printf("Take what?\n");
            } else {
                do_take(noun);
            }
        } else if (strcmp(verb, "inventory") == 0 || strcmp(verb, "inv") == 0 || strcmp(verb, "i") == 0) {
            do_inventory();
        } else if (strcmp(verb, "examine") == 0 || strcmp(verb, "x") == 0 ||
                   strcmp(verb, "inspect") == 0 || strcmp(verb, "read") == 0) {
            if (!noun) {
                printf("Examine what?\n");
            } else {
                do_examine(noun);
            }
        } else if (strcmp(verb, "use") == 0) {
            if (!noun) {
                printf("Use what?\n");
            } else {
                do_use(noun);
            }
        } else if (strcmp(verb, "help") == 0 || strcmp(verb, "?") == 0) {
            show_help();
        } else if (strcmp(verb, "quit") == 0 || strcmp(verb, "exit") == 0) {
            printf("Goodbye.\n");
            break;
        } else {
            printf("I don't understand that.\n");
        }

        check_victory();
    }

    return 0;
}
