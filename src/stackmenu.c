#include "stackmenu.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <time.h>
#include <poll.h>
#include <ctype.h>
#include <errno.h>

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

#define CON  ANSI_COLOR_DARKGRAY
#define COFF ANSI_COLOR_NORMAL

#if 0
/*
 * Horizontal Line - width: 44
 *                      .12345678901234567890123456789012345678901234.
 */
#define HR_SINGLE       "--------------------------------------------"
#define HR_DOUBLE       "============================================"
#define HR_SINGLE2      " ------------------------------------------ "
#endif

#if 0
/*
 * Horizontal Line - width: 55
 *                      .12345678901234567890123456789012345678901234567890.
 */
#define HR_SINGLE       "----------------------------------------" \
                        "---------------"
#define HR_DOUBLE       "========================================" \
                        "==============="
#define HR_SINGLE2      " ---------------------------------------" \
                        "-------------- "
#endif

/*
 * Horizontal Line - width: 65
 *                      .12345678901234567890123456789012345678901234567890.
 */
#define HR_SINGLE       "----------------------------------------" \
                        "-------------------------"
#define HR_DOUBLE       "========================================" \
                        "========================="
#define HR_SINGLE2      " ---------------------------------------" \
                        "------------------------ "

#define MAX_WIDTH       (strlen(HR_SINGLE))
#define MAX_TITLE       ((MAX_WIDTH)-10)
#define POS_MORE        ((MAX_WIDTH)-3)

#define mmsg(fmt, args ...) { \
	fprintf(stdout, fmt "\n", ## args); \
	fflush(stdout); \
}

/* no newline */
#define msgn(fmt, args ...) { \
	fprintf(stdout, fmt, ## args); \
	fflush(stdout); \
}

/* Bold (green) */
#define msgb(fmt, args ...) { \
	fprintf(stdout, ANSI_COLOR_LIGHTGREEN fmt \
			ANSI_COLOR_NORMAL "\n", ## args); \
	fflush(stdout); \
}

/* Property message */
#define msgp(fmt, args ...) { \
	fprintf(stdout, ANSI_COLOR_LIGHTMAGENTA fmt \
			ANSI_COLOR_NORMAL "\n", ## args); \
	fflush(stdout); \
}

/* n indented message */
#define msgt(n, fmt, args ...) { \
	fprintf(stdout, "\e[%dC" fmt "\n",	\
			3 + ((n) * 2), ## args); \
	fflush(stdout); \
}

/* process-id(thread-id) message */
#define pmsg(fmt, args ...) { \
	if (is_pid_show()) \
		fprintf(stdout, "(%5d) ", get_tid()); \
	fprintf(stdout, fmt "\n", ## args); fflush(stdout); \
}

/* bold */
#define pmsgb(fmt, args ...) { \
	if (is_pid_show()) \
		fprintf(stdout, "(%5d) ", get_tid()); \
	fprintf(stdout, ANSI_COLOR_LIGHTGREEN fmt \
			ANSI_COLOR_NORMAL "\n", ## args); fflush(stdout); \
}

/* n-indented */
#define pmsgt(n, fmt, args ...) { \
	if (is_pid_show()) \
		fprintf(stdout, "(%5d) ", get_tid()); \
	fprintf(stdout, "\e[%dC" fmt "\n", 3 + ((n) * 2), ## args); \
	fflush(stdout); \
}

#define MARK_DISABLE            '^'
#define MARK_BLANK_KEY          '_'
#define MARK_SEPARATOR          '-'
#define MARK_ALWAYS_INVOKE      '*'

#define DEFAULT_MENU_MENU       "m"
#define DEFAULT_MENU_PREV       "p"
#define DEFAULT_MENU_QUIT       "q"
#define DEFAULT_MENU_NONE       "-"

#define RESERVED_MENU_STRING \
	CON HR_SINGLE2 COFF "\n" \
	CON " [ " COFF DEFAULT_MENU_PREV CON " ] " COFF "Previous menu\n" \
	CON " [ " COFF DEFAULT_MENU_MENU CON " ] " COFF "Show Menu\n" \
	CON " [ " COFF DEFAULT_MENU_QUIT CON " ] " COFF "Quit"

#ifdef BACKEND_GLIB
#define GStack GQueue
#define g_stack_new g_queue_new
#define g_stack_get_length g_queue_get_length
#define g_stack_push g_queue_push_tail
#define g_stack_pop g_queue_pop_tail
#else
/**
 * GStack
 */
typedef struct _GList GList;
struct _GList {
	void *data;
	GList *next;
	GList *prev;
};

typedef struct _GStack GStack;
struct _GStack {
	GList *head;
	GList *tail;
	uint32_t length;
};

static GStack *g_stack_new(void)
{
	GStack *stack;

	stack = calloc(1, sizeof(GStack));
	stack->head = NULL;
	stack->tail = NULL;
	stack->length = 0;

	return stack;
}

static uint32_t g_stack_get_length(GStack *stack)
{
	if (!stack)
		return 0;

	return stack->length;
}

static void g_stack_push(GStack *stack, void *data)
{
	GList *node;

	if (!stack || !data)
		return;

	node = calloc(1, sizeof(GList));
	node->next = NULL;
	node->prev = stack->tail;
	node->data = data;

	if (stack->tail) {
		stack->tail->next = node;
		stack->length++;
	} else {
		stack->head = stack->tail = node;
		stack->length = 1;
	}
}

static void *g_stack_pop(GStack *stack)
{
	GList *node;
	void *data;

	if (!stack)
		return NULL;

	if (!stack->tail)
		return NULL;

	node = stack->tail;
	if (node->prev) {
		node->prev->next = NULL;
		stack->length--;
	} else {
		stack->head = stack->tail = NULL;
		stack->length = 0;
	}

	data = node->data;
	free(node);

	return data;
}

static int g_strcmp0(const char *str1, const char *str2)
{
	if (!str1)
		return -(str1 != str2);

	if (!str2)
		return str1 != str2;

	return strcmp(str1, str2);
}

#endif

static void _putc(char byte)
{
	ssize_t nwritten;

	do {
		nwritten = write(0, &byte, 1);
		if (nwritten < 0 && errno != EINTR)
			break;
	} while (nwritten < 1);
}

/**
 * Menu
 */
struct _stack_menu {
	GStack *stack;
	GStack *title_stack;

	StackmenuItem *items;
	StackmenuItem *saved_item;

	char *buf;
	char key_buffer[MENU_DATA_SIZE];

	void *user_data;
#ifdef BACKEND_GLIB
	GMainLoop *mainloop;
#endif
	int exit;
};

static void _show_prompt(void)
{
	struct timespec tp;
	struct tm now;

	clock_gettime(CLOCK_REALTIME, &tp);
	localtime_r((time_t *)&tp.tv_sec, &now);

	msgn("%02d:%02d:%02d.%03ld >> ", now.tm_hour, now.tm_min, now.tm_sec,
			tp.tv_nsec / 1000000L)
}

static void _invoke_item_sync(Stackmenu *sm, StackmenuItem *item)
{
	int ret;

	if (!item->callback)
		return;

	ret = item->callback(sm, item, sm->user_data);
	if (ret < 0) {
		mmsg(ANSI_COLOR_RED "'%s' failed. (ret=%d)" COFF,
				item->title, ret)
	}
}

static void _show_menu(Stackmenu *sm, StackmenuItem items[])
{
	StackmenuItem *si;
	char title_buf[256] = {0, };
	GList *cur;
	int key_flag;
	int disabled;

	if (!items)
		return;

	mmsg("\n" HR_DOUBLE)
	msgn(ANSI_COLOR_YELLOW " Main")

	cur = sm->title_stack->head;
	while (cur) {
		msgn(COFF " >> " ANSI_COLOR_YELLOW "%s", (char *)cur->data)

		cur = cur->next;
	}

	mmsg(COFF "\n" HR_SINGLE)

	si = items;
	while (si->key) {
		key_flag = 1;
		disabled = 0;
		if (strlen(si->key) == 1) {
			key_flag = 0;

			switch (si->key[0]) {
			case MARK_BLANK_KEY:
				msgn("       ")
				break;

			case MARK_SEPARATOR:
				mmsg(CON HR_SINGLE2 COFF)
				_invoke_item_sync(sm, si);

				si++;

				continue;
				break;

			case MARK_ALWAYS_INVOKE:
				/* invoke the callback on menu display time */
				_invoke_item_sync(sm, si);
				break;

			default:
				msgn(CON " [" COFF " %c " CON "] " COFF,
						si->key[0])
				break;
			}
		}

		if (key_flag == 1) {
			if (si->key[0] == MARK_DISABLE) {
				disabled = 1;
				if (strlen(si->key + 1) == 1) {
					msgn(CON " [ %c ] " COFF, si->key[1])
				} else {
					msgn(CON " [%3s] " COFF, si->key + 1)
				}
			} else {
				msgn(CON " [" COFF "%3s" CON "] " COFF,
						si->key)
			}
		}

		if (si->title) {
			memset(title_buf, 0, 256);
			snprintf(title_buf, MAX_TITLE, "%s", si->title);

			if (strlen(si->title) >= MAX_TITLE) {
				title_buf[MAX_TITLE - 2] = '.';
				title_buf[MAX_TITLE - 1] = '.';
			}

			if (disabled) {
				msgn(CON "%s" COFF, title_buf)
			} else {
				msgn("%s", title_buf)
			}
		}

		if (si->data) {
			if (disabled) {
				msgn(CON "(%s)" COFF, si->data)
			} else {
				msgn(ANSI_COLOR_LIGHTBLUE "(%s)" COFF,
						si->data)
			}
		}

		if (si->sub_menu)
			msgn("\r\e[%dC >", (int)POS_MORE)

		_putc('\n');
		si++;
	}

	if (si == items)
		mmsg(" No items");

	mmsg(RESERVED_MENU_STRING)
	mmsg(HR_DOUBLE)

	_show_prompt();
}

static void _show_item_data_input_msg(const StackmenuItem *item)
{
	mmsg("")
	mmsg(HR_DOUBLE)
	mmsg(" Input [%s] data ", item->title)
	mmsg(HR_SINGLE)
	mmsg(" current = [%s]", item->data)
	msgn(" new >> ")
}

static void _move_menu(Stackmenu *sm, StackmenuItem items[], const char *key)
{
	StackmenuItem *si;
	int i = 0;

	if (!sm->items)
		return;

	if (!g_strcmp0(DEFAULT_MENU_PREV, key)) {
		if (g_stack_get_length(sm->stack) > 0) {
			sm->items = g_stack_pop(sm->stack);
			g_stack_pop(sm->title_stack);
		}

		_show_menu(sm, sm->items);
		sm->buf = sm->key_buffer;

		return;
	} else if (!g_strcmp0(DEFAULT_MENU_MENU, key)) {
		_show_menu(sm, sm->items);
		return;
	} else if (!g_strcmp0(DEFAULT_MENU_QUIT, key)) {
#ifdef BACKEND_GLIB
		g_main_loop_quit(sm->mainloop);
#else
		sm->exit = 1;
#endif
		return;
	} else if (!g_strcmp0(DEFAULT_MENU_NONE, key)) {
		_show_prompt();
		return;
	}

	while (1) {
		si = items + i;
		if (si->key == NULL)
			break;

		if (si->key[0] == MARK_DISABLE) {
			if (!g_strcmp0(si->key + 1, key)) {
				mmsg("Disabled menu")
				break;
			}
		}

		if (!g_strcmp0(si->key, key)) {
			sm->saved_item = NULL;

			if (si->sub_menu) {
				g_stack_push(sm->stack, sm->items);
				g_stack_push(sm->title_stack,
						(void *)si->title);

				sm->items = si->sub_menu;
				_show_menu(sm, sm->items);
				sm->buf = sm->key_buffer;
			}

			if (si->callback && si->data == NULL) {
				_invoke_item_sync(sm, si);
				_show_prompt();
			}

			if (si->data) {
				_show_item_data_input_msg(si);
				sm->buf = si->data;

				if (si->callback)
					sm->saved_item = si;
			}

			return;
		}

		i++;
	}

	_show_prompt();
}

#ifdef BACKEND_GLIB
static void _show_input_ok(void)
{
	mmsg(" > Saved.")
}

static int on_stack_menu_keyboard(GIOChannel *src, GIOCondition con,
		gpointer data)
{
	Stackmenu *sm = data;
	char local_buf[MENU_DATA_SIZE + 1] = {0, };

	if (fgets(local_buf, MENU_DATA_SIZE, stdin) == NULL)
		return 1;

	if (strlen(local_buf) > 0) {
		if (local_buf[strlen(local_buf) - 1] == '\n')
			local_buf[strlen(local_buf) - 1] = '\0';
	}

	if (sm->buf == sm->key_buffer) {
		if (strlen(local_buf) < 1) {
			_show_prompt();
			return 1;
		}

		_move_menu(sm, sm->items, local_buf);
	} else {
		if (sm->buf) {
			memset(sm->buf, 0, MENU_DATA_SIZE);
			memcpy(sm->buf, local_buf, MENU_DATA_SIZE);
			_show_input_ok();

			if (sm->saved_item) {
				_invoke_item_sync(sm, sm->saved_item);
				sm->saved_item = NULL;
			}
		}

		sm->buf = sm->key_buffer;
		_move_menu(sm, sm->items, DEFAULT_MENU_MENU);
	}

	return 1;
}

#else

static int on_readline(Stackmenu *sm, const char *buf, int size)
{
	if (sm->buf == sm->key_buffer) {
		if (size < 1) {
			_show_prompt();
			return 1;
		}

		_move_menu(sm, sm->items, buf);
		return 1;
	}

	if (sm->buf) {
		memset(sm->buf, 0, MENU_DATA_SIZE);
		memcpy(sm->buf, buf, MENU_DATA_SIZE);
		mmsg(" > Saved.")

		if (sm->saved_item) {
			_invoke_item_sync(sm, sm->saved_item);
			sm->saved_item = NULL;
		}
	}

	sm->buf = sm->key_buffer;
	_move_menu(sm, sm->items, DEFAULT_MENU_MENU);

	return 1;
}

struct _terminal {
	char buf[MENU_DATA_SIZE + 1];
	int pos;
	int length;
	int escape;
	int is_echo;
};

static void _put_esc_code(char byte)
{
	_putc(0x1B);
	_putc('[');
	_putc(byte);
}

static int _automata(struct _terminal *term, char ch)
{
	switch (term->escape) {
	case 0:
		if (ch == 0x1B)
			term->escape = 1;
		else if ((ch == '\b' || ch == 0x7F) && term->pos > 0) {
			int i;

			_put_esc_code('D');
			_put_esc_code('s');

			term->pos--;
			for (i = term->pos; i <= term->length; i++) {
				term->buf[i] = term->buf[i + 1];
				_putc(term->buf[i]);
			}

			_put_esc_code('K');
			_put_esc_code('u');
		} else if (ch == '\n' || ch == '\r') {
			if (term->is_echo)
				_putc('\n');
			return 1;
		} else if (isprint(ch)) {
			if (term->pos >= MENU_DATA_SIZE)
				return 0;

			term->buf[term->pos] = ch;
			term->pos++;
			if (term->pos > term->length)
				term->length = term->pos;
			if (term->is_echo)
				_putc(ch);
		}
		break;
	case 1:
		if (ch == '[')
			term->escape = 2;
		else
			term->escape = 0;
		break;
	case 2:
		if (ch == 'D') {
			/* left */
			term->pos--;
			if (term->pos < 0)
				term->pos = 0;
			else
				_put_esc_code('D');
		} else if (ch == 'C' && term->length > 0) {
			/* right */
			term->pos++;
			if (term->pos > term->length)
				term->pos = term->length;
			else
				_put_esc_code('C');
		}
		term->escape = 0;
		break;
	default:
		break;
	}

	return 0;
}
#endif

#ifdef BACKEND_GLIB
Stackmenu *stackmenu_new(StackmenuItem items[], GMainLoop *mainloop)
#else
Stackmenu *stackmenu_new(StackmenuItem items[])
#endif
{
	Stackmenu *sm;

	sm = calloc(sizeof(struct _stack_menu), 1);
	if (!sm)
		return NULL;

	sm->stack = g_stack_new();
	sm->title_stack = g_stack_new();
	sm->items = items;
	sm->exit = 0;

#ifdef BACKEND_GLIB
	sm->mainloop = mainloop;
#endif

	return sm;
}

int stackmenu_run(Stackmenu *sm)
{
#ifndef BACKEND_GLIB
	int nfds;
	struct pollfd pollfds[1];
	char buf;
	ssize_t nread;
	struct _terminal term;

	memset(&term, 0, sizeof(struct _terminal));
#else
	GIOChannel *channel = g_io_channel_unix_new(STDIN_FILENO);

	/* key input */
	g_io_add_watch(channel, (G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_NVAL),
			on_stack_menu_keyboard, sm);
	g_io_channel_unref(channel);
#endif

	_show_menu(sm, sm->items);

	sm->buf = sm->key_buffer;

#ifndef BACKEND_GLIB
	pollfds[0].fd = 0;

	while (!sm->exit) {
		pollfds[0].events |= POLLIN;
		nfds = poll(pollfds, 1, -1);
		if (nfds == -1)
			break;

		if (pollfds[0].revents & POLLIN) {
			nread = read(0, &buf, 1);
			if (nread <= 0)
				break;

			if (_automata(&term, buf) == 1) {
				on_readline(sm, term.buf, term.length);
				memset(&term, 0, sizeof(struct _terminal));
			}
		}
	}
#endif

	return 0;
}

int stackmenu_set_user_data(Stackmenu *sm, void *user_data)
{
	if (!sm)
		return -1;

	sm->user_data = user_data;

	return 0;
}

void *stackmenu_get_user_data(const Stackmenu *sm)
{
	if (!sm)
		return NULL;

	return sm->user_data;
}

void stackmenu_item_clear(StackmenuItem *item)
{
	if (!item)
		return;

	if (item->key)
		free(item->key);

	if (item->title)
		free(item->title);

	if (item->data)
		free(item->data);

	if (item->custom_data_destroy_callback)
		item->custom_data_destroy_callback(item->custom_data);

	if (item->sub_menu) {
		int i = 0;

		for (; item->sub_menu[i].key; i++)
			stackmenu_item_clear(&(item->sub_menu[i]));

		free(item->sub_menu);
	}

	memset(item, 0, sizeof(StackmenuItem));
}

int stackmenu_item_count(const StackmenuItem *items)
{
	int i = 0;

	if (!items)
		return -1;

	for (; i < MENU_MAX_ITEMS; i++) {
		if (!items[i].key)
			break;
	}

	return i;
}

StackmenuItem *stackmenu_item_find(StackmenuItem *items, const char *key)
{
	int i;

	if (!key || !items)
		return NULL;

	for (i = 0; i < MENU_MAX_ITEMS; i++) {
		if (!items[i].key)
			break;

		if (items[i].key[0] == MARK_DISABLE) {
			if (strcmp(items[i].key + 1, key) == 0)
				return items + i;
		} else {
			if (strcmp(items[i].key, key) == 0)
				return items + i;
		}
	}

	return NULL;
}

StackmenuItem *stackmenu_item_find_by_title(StackmenuItem *items,
		const char *title)
{
	int i;

	if (!title || !items)
		return NULL;

	for (i = 0; i < MENU_MAX_ITEMS; i++) {
		if (!items[i].key)
			break;

		if (!items[i].title)
			continue;

		if (strcmp(items[i].title, title) == 0)
			return items + i;
	}

	return NULL;
}

int stackmenu_item_disable(StackmenuItem *item)
{
	int i, len;

	if (!item)
		return -1;
	if (!item->key)
		return -1;
	if (item->key[0] == MARK_DISABLE)
		return 0;

	len = strlen(item->key);
	item->key[len + 1] = '\0';

	for (i = len; i > 0; i--) {
		item->key[i] = item->key[i - 1];
	}
	item->key[0] = MARK_DISABLE;

	return 0;
}

int stackmenu_item_enable(StackmenuItem *item)
{
	int i, len;

	if (!item)
		return -1;
	if (!item->key)
		return -1;
	if (item->key[0] != MARK_DISABLE)
		return 0;

	len = strlen(item->key);

	for (i = 0; i < len; i++)
		item->key[i] = item->key[i + 1];
	item->key[len - 1] = '\0';

	return 0;
}

