#ifndef __STACKMENU_H__
#define __STACKMENU_H__

#ifdef BACKEND_GLIB
#include <glib.h>
#endif

#define MENU_DATA_SIZE 2048
#define MENU_MAX_ITEMS 99

#define ANSI_COLOR_NORMAL       "\e[0m"

#define ANSI_COLOR_BLACK        "\e[0;30m"
#define ANSI_COLOR_RED          "\e[0;31m"
#define ANSI_COLOR_GREEN        "\e[0;32m"
#define ANSI_COLOR_BROWN        "\e[0;33m"
#define ANSI_COLOR_BLUE         "\e[0;34m"
#define ANSI_COLOR_MAGENTA      "\e[0;35m"
#define ANSI_COLOR_CYAN         "\e[0;36m"
#define ANSI_COLOR_LIGHTGRAY    "\e[0;37m"

#define ANSI_COLOR_DARKGRAY     "\e[1;30m"
#define ANSI_COLOR_LIGHTRED     "\e[1;31m"
#define ANSI_COLOR_LIGHTGREEN   "\e[1;32m"
#define ANSI_COLOR_YELLOW       "\e[1;33m"
#define ANSI_COLOR_LIGHTBLUE    "\e[1;34m"
#define ANSI_COLOR_LIGHTMAGENTA "\e[1;35m"
#define ANSI_COLOR_LIGHTCYAN    "\e[1;36m"
#define ANSI_COLOR_WHITE        "\e[1;37m"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct _stack_menu Stackmenu;

struct _stackmenu_item {
	char *key;
	char *title;
	struct _stackmenu_item *sub_menu;
	int (*callback)(Stackmenu *mm, struct _stackmenu_item *menu,
			void *user_data);
	char *data;
	void *custom_data;
	void (*custom_data_destroy_callback)(void *custom_data);
};
typedef struct _stackmenu_item StackmenuItem;

#ifdef BACKEND_GLIB
Stackmenu *stackmenu_new(StackmenuItem items[], GMainLoop *loop);
#else
Stackmenu *stackmenu_new(StackmenuItem items[]);
#endif

int stackmenu_run(Stackmenu *mm);
int stackmenu_set_user_data(Stackmenu *mm, void *user_data);
void *stackmenu_get_user_data(Stackmenu *mm);

void stackmenu_item_clear(StackmenuItem *md);
int stackmenu_item_count(StackmenuItem *md);
StackmenuItem *stackmenu_item_find(StackmenuItem *md, const char *key);
StackmenuItem *stackmenu_item_find_by_title(StackmenuItem *md,
		const char *title);

int stackmenu_item_disable(StackmenuItem *md);
int stackmenu_item_enable(StackmenuItem *md);

#ifdef __cplusplus
}
#endif

#endif
