#ifndef __STACKMENU_H__
#define __STACKMENU_H__

#ifdef BACKEND_GLIB
#include <glib.h>
#endif

#define MENU_DATA_SIZE 2048
#define MENU_MAX_ITEMS 99
#define MENU_DEFAULT_LEFT_PADDING "       "

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct _stack_menu Stackmenu;

struct _stackmenu_item {
	char *key;
	char *title;
	struct _stackmenu_item *sub_menu;
	int (*callback)(Stackmenu *sm, struct _stackmenu_item *item,
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

int stackmenu_run(Stackmenu *sm);
int stackmenu_set_user_data(Stackmenu *sm, void *user_data);
void *stackmenu_get_user_data(const Stackmenu *sm);

void stackmenu_item_clear(StackmenuItem *item);
int stackmenu_item_count(const StackmenuItem *items);
StackmenuItem *stackmenu_item_find(StackmenuItem *items, const char *key);
StackmenuItem *stackmenu_item_find_by_title(StackmenuItem *items,
		const char *title);

int stackmenu_item_disable(StackmenuItem *item);
int stackmenu_item_enable(StackmenuItem *item);
int stackmenu_item_is_enabled(const StackmenuItem *item);

#ifdef __cplusplus
}
#endif

#endif
