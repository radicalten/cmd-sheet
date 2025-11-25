#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_INPUT 100
#define MAX_ITEMS 10

/* Game state */
typedef struct {
    int current_room;
    int inventory[MAX_ITEMS];
    int inventory_count;
    int lamp_on;
    int door_unlocked;
    int game_won;
} GameState;

/* Room definitions */
typedef struct {
    char *name;
    char *description;
    int north, south, east, west;
    int items[MAX_ITEMS];
    int item_count;
    int dark;
} Room;

/* Item definitions */
typedef struct {
    char *name;
    char *description;
    int id;
} Item;

enum {
    ROOM_ENTRANCE,
    ROOM_HALLWAY,
    ROOM_LIBRARY,
    ROOM_KITCHEN,
    ROOM_CELLAR,
    ROOM_TREASURE,
    ROOM_COUNT
};

enum {
    ITEM_LAMP,
    ITEM_KEY,
    ITEM_SWORD,
    ITEM_BOOK,
    ITEM_TREASURE,
    ITEM_COUNT
};

/* Global data */
Room rooms[ROOM_COUNT];
Item items[ITEM_COUNT];
GameState state;

/* Function prototypes */
void init_game(void);
void print_room(void);
void parse_command(char *input);
void go(char *direction);
void take(char *item_name);
void drop(char *item_name);
void use(char *item_name);
void examine(char *item_name);
void show_inventory(void);
void show_help(void);
char *str_tolower(char *str);
int find_item_in_room(char *name);
int find_item_in_inventory(char *name);

int main(void) {
    char input[MAX_INPUT];
    
    printf("========================================\n");
    printf("   THE LOST TREASURE OF DARKWOOD MANOR\n");
    printf("========================================\n");
    printf("\nType 'help' for commands.\n\n");
    
    init_game();
    print_room();
    
    while (!state.game_won) {
        printf("\n> ");
        if (fgets(input, MAX_INPUT, stdin) == NULL) break;
        
        input[strcspn(input, "\n")] = 0;
        
        if (strlen(input) == 0) continue;
        
        parse_command(input);
    }
    
    return 0;
}

void init_game(void) {
    /* Initialize game state */
    state.current_room = ROOM_ENTRANCE;
    state.inventory_count = 0;
    state.lamp_on = 0;
    state.door_unlocked = 0;
    state.game_won = 0;
    
    /* Initialize items */
    items[ITEM_LAMP] = (Item){"lamp", "A rusty oil lamp. It might still work.", ITEM_LAMP};
    items[ITEM_KEY] = (Item){"key", "An ornate brass key with strange markings.", ITEM_KEY};
    items[ITEM_SWORD] = (Item){"sword", "A decorative sword hanging on the wall.", ITEM_SWORD};
    items[ITEM_BOOK] = (Item){"book", "A leather-bound book titled 'Secrets of the Manor'.", ITEM_BOOK};
    items[ITEM_TREASURE] = (Item){"treasure", "A chest filled with gold and jewels!", ITEM_TREASURE};
    
    /* Initialize rooms */
    rooms[ROOM_ENTRANCE] = (Room){
        "Manor Entrance",
        "You are standing in the dusty entrance hall of Darkwood Manor.\n"
        "Cobwebs hang from the chandelier above. There is a hallway to the north.",
        ROOM_HALLWAY, -1, -1, -1,
        {ITEM_LAMP}, 1, 0
    };
    
    rooms[ROOM_HALLWAY] = (Room){
        "Main Hallway",
        "A long, dark hallway stretches before you.\n"
        "There are doorways to the east and west, and stairs leading down to the south.\n"
        "The entrance is to the south.",
        -1, ROOM_ENTRANCE, ROOM_LIBRARY, ROOM_KITCHEN,
        {}, 0, 0
    };
    
    rooms[ROOM_LIBRARY] = (Room){
        "Library",
        "You are in a library filled with ancient books.\n"
        "A decorative sword hangs on the wall. The hallway is to the west.",
        -1, -1, -1, ROOM_HALLWAY,
        {ITEM_SWORD, ITEM_BOOK}, 2, 0
    };
    
    rooms[ROOM_KITCHEN] = (Room){
        "Kitchen",
        "An old kitchen with rusty pots and pans.\n"
        "There's a small key on the counter. The hallway is to the east.",
        -1, -1, ROOM_HALLWAY, -1,
        {ITEM_KEY}, 1, 0
    };
    
    rooms[ROOM_CELLAR] = (Room){
        "Dark Cellar",
        "You are in a pitch-black cellar. You can't see anything!\n"
        "Stairs lead up to the north.",
        ROOM_HALLWAY, -1, -1, -1,
        {}, 0, 1
    };
    
    rooms[ROOM_TREASURE] = (Room){
        "Treasure Room",
        "You have found the secret treasure room!\n"
        "A magnificent chest sits in the center of the room.",
        -1, -1, -1, -1,
        {ITEM_TREASURE}, 1, 0
    };
}

void print_room(void) {
    Room *room = &rooms[state.current_room];
    
    printf("\n=== %s ===\n", room->name);
    
    /* Check if room is dark and lamp is not on */
    if (room->dark && !state.lamp_on) {
        printf("It is pitch black. You might be eaten by a grue.\n");
        return;
    }
    
    printf("%s\n", room->description);
    
    /* List items in room */
    if (room->item_count > 0) {
        printf("\nYou can see:\n");
        for (int i = 0; i < room->item_count; i++) {
            printf("  - a %s\n", items[room->items[i]].name);
        }
    }
}

void parse_command(char *input) {
    char *cmd = strtok(input, " ");
    char *arg = strtok(NULL, " ");
    
    if (cmd == NULL) return;
    
    cmd = str_tolower(cmd);
    
    if (strcmp(cmd, "go") == 0 || strcmp(cmd, "n") == 0 || 
        strcmp(cmd, "s") == 0 || strcmp(cmd, "e") == 0 || strcmp(cmd, "w") == 0) {
        if (strcmp(cmd, "go") == 0) {
            go(arg);
        } else {
            go(cmd);
        }
    } else if (strcmp(cmd, "north") == 0) {
        go("n");
    } else if (strcmp(cmd, "south") == 0) {
        go("s");
    } else if (strcmp(cmd, "east") == 0) {
        go("e");
    } else if (strcmp(cmd, "west") == 0) {
        go("w");
    } else if (strcmp(cmd, "take") == 0 || strcmp(cmd, "get") == 0) {
        take(arg);
    } else if (strcmp(cmd, "drop") == 0) {
        drop(arg);
    } else if (strcmp(cmd, "use") == 0) {
        use(arg);
    } else if (strcmp(cmd, "examine") == 0 || strcmp(cmd, "x") == 0) {
        examine(arg);
    } else if (strcmp(cmd, "inventory") == 0 || strcmp(cmd, "i") == 0) {
        show_inventory();
    } else if (strcmp(cmd, "look") == 0 || strcmp(cmd, "l") == 0) {
        print_room();
    } else if (strcmp(cmd, "help") == 0) {
        show_help();
    } else if (strcmp(cmd, "quit") == 0 || strcmp(cmd, "exit") == 0) {
        printf("Thanks for playing!\n");
        exit(0);
    } else {
        printf("I don't understand that command. Type 'help' for a list of commands.\n");
    }
}

void go(char *direction) {
    if (direction == NULL) {
        printf("Go where?\n");
        return;
    }
    
    direction = str_tolower(direction);
    
    int new_room = -1;
    Room *room = &rooms[state.current_room];
    
    if (strcmp(direction, "n") == 0 || strcmp(direction, "north") == 0) {
        new_room = room->north;
    } else if (strcmp(direction, "s") == 0 || strcmp(direction, "south") == 0) {
        new_room = room->south;
    } else if (strcmp(direction, "e") == 0 || strcmp(direction, "east") == 0) {
        new_room = room->east;
    } else if (strcmp(direction, "w") == 0 || strcmp(direction, "west") == 0) {
        new_room = room->west;
    } else {
        printf("That's not a valid direction.\n");
        return;
    }
    
    /* Special case: going down to cellar from hallway */
    if (state.current_room == ROOM_HALLWAY && 
        (strcmp(direction, "down") == 0 || strcmp(direction, "d") == 0)) {
        if (!state.door_unlocked) {
            printf("The cellar door is locked. You need a key.\n");
            return;
        }
        state.current_room = ROOM_CELLAR;
        print_room();
        return;
    }
    
    if (new_room == -1) {
        printf("You can't go that way.\n");
        return;
    }
    
    state.current_room = new_room;
    print_room();
}

void take(char *item_name) {
    if (item_name == NULL) {
        printf("Take what?\n");
        return;
    }
    
    item_name = str_tolower(item_name);
    
    int item_index = find_item_in_room(item_name);
    
    if (item_index == -1) {
        printf("There's no %s here.\n", item_name);
        return;
    }
    
    if (state.inventory_count >= MAX_ITEMS) {
        printf("You're carrying too much!\n");
        return;
    }
    
    Room *room = &rooms[state.current_room];
    int item_id = room->items[item_index];
    
    /* Add to inventory */
    state.inventory[state.inventory_count++] = item_id;
    
    /* Remove from room */
    for (int i = item_index; i < room->item_count - 1; i++) {
        room->items[i] = room->items[i + 1];
    }
    room->item_count--;
    
    printf("Taken.\n");
}

void drop(char *item_name) {
    if (item_name == NULL) {
        printf("Drop what?\n");
        return;
    }
    
    item_name = str_tolower(item_name);
    
    int inv_index = find_item_in_inventory(item_name);
    
    if (inv_index == -1) {
        printf("You don't have that.\n");
        return;
    }
    
    Room *room = &rooms[state.current_room];
    int item_id = state.inventory[inv_index];
    
    /* Add to room */
    room->items[room->item_count++] = item_id;
    
    /* Remove from inventory */
    for (int i = inv_index; i < state.inventory_count - 1; i++) {
        state.inventory[i] = state.inventory[i + 1];
    }
    state.inventory_count--;
    
    /* Turn off lamp if dropped */
    if (item_id == ITEM_LAMP) {
        state.lamp_on = 0;
    }
    
    printf("Dropped.\n");
}

void use(char *item_name) {
    if (item_name == NULL) {
        printf("Use what?\n");
        return;
    }
    
    item_name = str_tolower(item_name);
    
    int inv_index = find_item_in_inventory(item_name);
    
    if (inv_index == -1) {
        printf("You don't have that.\n");
        return;
    }
    
    int item_id = state.inventory[inv_index];
    
    if (item_id == ITEM_LAMP) {
        state.lamp_on = !state.lamp_on;
        printf("The lamp is now %s.\n", state.lamp_on ? "on" : "off");
        if (state.current_room == ROOM_CELLAR) {
            print_room();
            if (state.lamp_on) {
                printf("\nThe lamplight reveals a passage to the east!\n");
            }
        }
    } else if (item_id == ITEM_KEY) {
        if (state.current_room == ROOM_HALLWAY) {
            printf("You unlock the cellar door with the key. You can now go down.\n");
            state.door_unlocked = 1;
        } else {
            printf("There's nothing to unlock here.\n");
        }
    } else if (item_id == ITEM_SWORD) {
        if (state.current_room == ROOM_CELLAR && state.lamp_on) {
            printf("You swing the sword at the eastern wall. It crumbles, revealing a hidden room!\n");
            rooms[ROOM_CELLAR].east = ROOM_TREASURE;
        } else {
            printf("You swing the sword around. Nothing happens.\n");
        }
    } else {
        printf("You can't use that.\n");
    }
}

void examine(char *item_name) {
    if (item_name == NULL) {
        print_room();
        return;
    }
    
    item_name = str_tolower(item_name);
    
    int inv_index = find_item_in_inventory(item_name);
    int room_index = find_item_in_room(item_name);
    
    if (inv_index != -1) {
        printf("%s\n", items[state.inventory[inv_index]].description);
    } else if (room_index != -1) {
        Room *room = &rooms[state.current_room];
        printf("%s\n", items[room->items[room_index]].description);
    } else {
        printf("You don't see that here.\n");
    }
}

void show_inventory(void) {
    if (state.inventory_count == 0) {
        printf("You're not carrying anything.\n");
        return;
    }
    
    printf("You are carrying:\n");
    for (int i = 0; i < state.inventory_count; i++) {
        printf("  - a %s\n", items[state.inventory[i]].name);
    }
}

void show_help(void) {
    printf("\nAvailable commands:\n");
    printf("  go <direction>, n/s/e/w/north/south/east/west - Move in a direction\n");
    printf("  take/get <item>     - Pick up an item\n");
    printf("  drop <item>         - Drop an item\n");
    printf("  use <item>          - Use an item\n");
    printf("  examine/x <item>    - Examine an item\n");
    printf("  inventory/i         - Show your inventory\n");
    printf("  look/l              - Look around\n");
    printf("  help                - Show this help\n");
    printf("  quit/exit           - Quit the game\n");
}

char *str_tolower(char *str) {
    static char buffer[MAX_INPUT];
    int i = 0;
    
    if (str == NULL) return NULL;
    
    while (str[i] && i < MAX_INPUT - 1) {
        buffer[i] = tolower(str[i]);
        i++;
    }
    buffer[i] = '\0';
    
    return buffer;
}

int find_item_in_room(char *name) {
    Room *room = &rooms[state.current_room];
    
    if (room->dark && !state.lamp_on) {
        return -1;
    }
    
    for (int i = 0; i < room->item_count; i++) {
        if (strcmp(items[room->items[i]].name, name) == 0) {
            return i;
        }
    }
    
    return -1;
}

int find_item_in_inventory(char *name) {
    for (int i = 0; i < state.inventory_count; i++) {
        if (strcmp(items[state.inventory[i]].name, name) == 0) {
            return i;
        }
    }
    
    return -1;
}
