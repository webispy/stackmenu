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

#define DEFAULT_MENU_MENU       "m"
#define DEFAULT_MENU_PREV       "p"
#define DEFAULT_MENU_QUIT       "q"
#define DEFAULT_MENU_NONE       "-"

#define CON ANSI_COLOR_DARKGRAY
#define COFF ANSI_COLOR_NORMAL
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

	struct _stackmenu_item *menu;
	struct _stackmenu_item *saved_item;

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

static void _invoke_item_sync(Stackmenu *mm, struct _stackmenu_item *item)
{
	int ret;

	if (!item->callback)
		return;

	ret = item->callback(mm, item, mm->user_data);
	if (ret < 0) {
		mmsg(ANSI_COLOR_RED "'%s' failed. (ret=%d)" COFF,
				item->title, ret)
	}
}

static void _show_menu(Stackmenu *mm, struct _stackmenu_item menu[])
{
	struct _stackmenu_item *item;
	char title_buf[256] = {0, };
	GList *cur;
	int key_flag;
	int disabled;

	if (!menu)
		return;

	mmsg("\n" HR_DOUBLE)
	msgn(ANSI_COLOR_YELLOW " Main")

	cur = mm->title_stack->head;
	while (cur) {
		msgn(COFF " >> " ANSI_COLOR_YELLOW "%s", (char *)cur->data)

		cur = cur->next;
	}

	mmsg(COFF "\n" HR_SINGLE)

	item = menu;
	while (item->key) {
		key_flag = 1;
		disabled = 0;
		if (strlen(item->key) == 1) {
			key_flag = 0;

			switch (item->key[0]) {
			case '_':
				/* no key */
				msgn("       ")
				break;

			case '-':
				/* separator */
				mmsg(CON HR_SINGLE2 COFF)
				_invoke_item_sync(mm, item);

				item++;

				continue;
				break;

			case '*':
				/* invoke the callback on menu display time */
				_invoke_item_sync(mm, item);
				break;

			default:
				msgn(CON " [" COFF " %c " CON "] " COFF,
						item->key[0])
				break;
			}
		}

		if (key_flag == 1) {
			if (item->key[0] == '^') {
				disabled = 1;
				if (strlen(item->key + 1) == 1) {
					msgn(CON " [ %c ] " COFF, item->key[1])
				} else {
					msgn(CON " [%3s] " COFF, item->key + 1)
				}
			} else {
				msgn(CON " [" COFF "%3s" CON "] " COFF,
						item->key)
			}
		}

		if (item->title) {
			memset(title_buf, 0, 256);
			snprintf(title_buf, MAX_TITLE, "%s", item->title);

			if (strlen(item->title) >= MAX_TITLE) {
				title_buf[MAX_TITLE - 2] = '.';
				title_buf[MAX_TITLE - 1] = '.';
			}

			if (disabled) {
				msgn(CON "%s" COFF, title_buf)
			} else {
				msgn("%s", title_buf)
			}
		}

		if (item->data) {
			if (disabled) {
				msgn(CON "(%s)" COFF, item->data)
			} else {
				msgn(ANSI_COLOR_LIGHTBLUE "(%s)" COFF,
						item->data)
			}
		}

		if (item->sub_menu)
			msgn("\r\e[%dC >", (int)POS_MORE)

		_putc('\n');
		item++;
	}

	if (item == menu)
		mmsg(" No items");

	mmsg(RESERVED_MENU_STRING)
	mmsg(HR_DOUBLE)

	_show_prompt();
}

static void _show_item_data_input_msg(struct _stackmenu_item *item)
{
	mmsg("")
	mmsg(HR_DOUBLE)
	mmsg(" Input [%s] data ", item->title)
	mmsg(HR_SINGLE)
	mmsg(" current = [%s]", item->data)
	msgn(" new >> ")
}

static void _move_menu(Stackmenu *mm, struct _stackmenu_item menu[],
		const char *key)
{
	struct _stackmenu_item *item;
	int i = 0;

	if (!mm->menu)
		return;

	if (!g_strcmp0(DEFAULT_MENU_PREV, key)) {
		if (g_stack_get_length(mm->stack) > 0) {
			mm->menu = g_stack_pop(mm->stack);
			g_stack_pop(mm->title_stack);
		}

		_show_menu(mm, mm->menu);
		mm->buf = mm->key_buffer;

		return;
	} else if (!g_strcmp0(DEFAULT_MENU_MENU, key)) {
		_show_menu(mm, mm->menu);
		return;
	} else if (!g_strcmp0(DEFAULT_MENU_QUIT, key)) {
#ifdef BACKEND_GLIB
		g_main_loop_quit(mm->mainloop);
#else
		mm->exit = 1;
#endif
		return;
	} else if (!g_strcmp0(DEFAULT_MENU_NONE, key)) {
		_show_prompt();
		return;
	}

	while (1) {
		item = menu + i;
		if (item->key == NULL)
			break;

		if (item->key[0] == '^') {
			if (!g_strcmp0(item->key + 1, key)) {
				mmsg("Disabled menu")
				break;
			}
		}

		if (!g_strcmp0(item->key, key)) {
			mm->saved_item = NULL;

			if (item->sub_menu) {
				g_stack_push(mm->stack, mm->menu);
				g_stack_push(mm->title_stack,
						(void *)item->title);

				mm->menu = item->sub_menu;
				_show_menu(mm, mm->menu);
				mm->buf = mm->key_buffer;
			}

			if (item->callback && item->data == NULL) {
				_invoke_item_sync(mm, item);
				_show_prompt();
			}

			if (item->data) {
				_show_item_data_input_msg(item);
				mm->buf = item->data;

				if (item->callback)
					mm->saved_item = item;
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
	Stackmenu *mm = data;
	char local_buf[MENU_DATA_SIZE + 1] = {0, };

	if (fgets(local_buf, MENU_DATA_SIZE, stdin) == NULL)
		return 1;

	if (strlen(local_buf) > 0) {
		if (local_buf[strlen(local_buf) - 1] == '\n')
			local_buf[strlen(local_buf) - 1] = '\0';
	}

	if (mm->buf == mm->key_buffer) {
		if (strlen(local_buf) < 1) {
			_show_prompt();
			return 1;
		}

		_move_menu(mm, mm->menu, local_buf);
	} else {
		if (mm->buf) {
			memset(mm->buf, 0, MENU_DATA_SIZE);
			memcpy(mm->buf, local_buf, MENU_DATA_SIZE);
			_show_input_ok();

			if (mm->saved_item) {
				_invoke_item_sync(mm, mm->saved_item);
				mm->saved_item = NULL;
			}
		}

		mm->buf = mm->key_buffer;
		_move_menu(mm, mm->menu, DEFAULT_MENU_MENU);
	}

	return 1;
}

#else

static int on_readline(Stackmenu *mm, const char *buf, int size)
{
	if (mm->buf == mm->key_buffer) {
		if (size < 1) {
			_show_prompt();
			return 1;
		}

		_move_menu(mm, mm->menu, buf);
		return 1;
	}

	if (mm->buf) {
		memset(mm->buf, 0, MENU_DATA_SIZE);
		memcpy(mm->buf, buf, MENU_DATA_SIZE);
		mmsg(" > Saved.")

		if (mm->saved_item) {
			_invoke_item_sync(mm, mm->saved_item);
			mm->saved_item = NULL;
		}
	}

	mm->buf = mm->key_buffer;
	_move_menu(mm, mm->menu, DEFAULT_MENU_MENU);

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
Stackmenu *stackmenu_new(struct _stackmenu_item items[], GMainLoop *mainloop)
#else
Stackmenu *stackmenu_new(struct _stackmenu_item items[])
#endif
{
	Stackmenu *mm;
#ifdef BACKEND_GLIB
	GIOChannel *channel = g_io_channel_unix_new(STDIN_FILENO);
#endif

	mm = calloc(sizeof(struct _stack_menu), 1);
	if (!mm)
		return NULL;

	mm->stack = g_stack_new();
	mm->title_stack = g_stack_new();
	mm->menu = items;
	mm->exit = 0;

#ifdef BACKEND_GLIB
	mm->mainloop = mainloop;

	/* key input */
	g_io_add_watch(channel, (G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_NVAL),
			on_stack_menu_keyboard, mm);
#endif

	return mm;
}

int stackmenu_run(Stackmenu *mm)
{
#ifndef BACKEND_GLIB
	int nfds;
	struct pollfd pollfds[1];
	char buf;
	ssize_t nread;
	struct _terminal term;

	memset(&term, 0, sizeof(struct _terminal));
#endif

	_show_menu(mm, mm->menu);

	mm->buf = mm->key_buffer;

#ifndef BACKEND_GLIB
	pollfds[0].fd = 0;

	while (!mm->exit) {
		pollfds[0].events |= POLLIN;
		nfds = poll(pollfds, 1, -1);
		if (nfds == -1)
			break;

		if (pollfds[0].revents & POLLIN) {
			nread = read(0, &buf, 1);
			if (nread <= 0)
				break;

			if (_automata(&term, buf) == 1) {
				on_readline(mm, term.buf, term.length);
				memset(&term, 0, sizeof(struct _terminal));
			}
		}
	}
#endif

	return 0;
}

int stackmenu_set_user_data(Stackmenu *mm, void *user_data)
{
	if (!mm)
		return -1;

	mm->user_data = user_data;

	return 0;
}

void *stackmenu_get_user_data(Stackmenu *mm)
{
	if (!mm)
		return NULL;

	return mm->user_data;
}

void stackmenu_item_clear(struct _stackmenu_item *md)
{
	int i = 0;

	if (!md)
		return;

	if (md->key)
		free(md->key);

	if (md->title)
		free(md->title);

	if (md->data)
		free(md->data);

	if (md->custom_data_destroy_callback)
		md->custom_data_destroy_callback(md->custom_data);

	if (md->sub_menu) {
		while (i) {
			if (md->sub_menu[i].key == NULL)
				break;

			stackmenu_item_clear(&(md->sub_menu[i]));

			i++;
		}

		free(md->sub_menu);
	}

	memset(md, 0, sizeof(struct _stackmenu_item));
}

int stackmenu_item_count(struct _stackmenu_item *md)
{
	int i;

	for (i = 0; i < MENU_MAX_ITEMS; i++) {
		if (md[i].key == NULL)
			break;
	}

	return i;
}

struct _stackmenu_item *stackmenu_item_find(struct _stackmenu_item *md, const char *key)
{
	int i;

	if (!key || !md)
		return NULL;

	for (i = 0; i < MENU_MAX_ITEMS; i++) {
		if (!md[i].key)
			break;

		if (md[i].key[0] == '^') {
			if (strcmp(md[i].key + 1, key) == 0)
				return md + i;
		} else {
			if (strcmp(md[i].key, key) == 0)
				return md + i;
		}
	}

	return NULL;
}

struct _stackmenu_item *stackmenu_item_find_by_title(struct _stackmenu_item *md,
		const char *title)
{
	int i;

	if (!title || !md)
		return NULL;

	for (i = 0; i < MENU_MAX_ITEMS; i++) {
		if (!md[i].title)
			continue;

		if (strcmp(md[i].title, title) == 0)
			return md + i;
	}

	return NULL;
}

int stackmenu_item_disable(struct _stackmenu_item *md)
{
	int i, len;

	if (!md)
		return -1;
	if (!md->key)
		return -1;
	if (md->key[0] == '^')
		return 0;

	len = strlen(md->key);
	md->key[len + 1] = '\0';

	for (i = len; i > 0; i--) {
		md->key[i] = md->key[i - 1];
	}
	md->key[0] = '^';

	return 0;
}

int stackmenu_item_enable(struct _stackmenu_item *md)
{
	int i, len;

	if (!md)
		return -1;
	if (!md->key)
		return -1;
	if (md->key[0] != '^')
		return 0;

	len = strlen(md->key);

	for (i = 0; i < len; i++)
		md->key[i] = md->key[i + 1];
	md->key[len - 1] = '\0';

	return 0;
}
