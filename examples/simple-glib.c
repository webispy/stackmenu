#include <stdio.h>
#include <stdlib.h>
#include <stackmenu.h>

static char data_name[MENU_DATA_SIZE] = "Jayden";
static char data_cnt[MENU_DATA_SIZE] = "0";

static int run_hi(Stackmenu *mm, StackmenuItem *menu, void *user_data)
{
	printf("Hi %s !!\n", data_name);

	return 0;
}

static int run_hello(Stackmenu *mm, StackmenuItem *menu, void *user_data)
{
	printf("Hello %s !!\n", data_name);

	return 0;
}

static int run_always(Stackmenu *mm, StackmenuItem *menu, void *user_data)
{
	int cnt;

	cnt = atoi(data_cnt);
	snprintf(data_cnt, MENU_DATA_SIZE, "%d", cnt + 1);

	return 0;
}

static StackmenuItem menu_sub1[] = {
	{ "1", "Hi", NULL, run_hi },
	NULL, NULL
};

static StackmenuItem menu_main[] = {
	{ "1", "Hello world", NULL, run_hello },
	{ "n", "- Name", NULL, NULL, data_name },
	{ "2", "Next level", menu_sub1 },
	{ "-" },
	{ "*", " Information", NULL, run_always, data_cnt },
	{ "_", "This number indicates how many times the menu" },
	{ "_", "has been re-displayed." },
	NULL, NULL
};

int main(int argc, char *argv[])
{
	GMainLoop *loop;
	Stackmenu *mm;

	loop = g_main_loop_new(NULL, FALSE);

	mm = stackmenu_new(menu_main, loop);
	stackmenu_run(mm);

	g_main_loop_run(loop);

	return 0;
}
