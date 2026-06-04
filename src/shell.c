#include "kernel.h"
#include "memory/main.h"
#include "arch/cpuinfo/main.h"
#include "libc/main.h"
#include "multitasking/task.h"
#include "arch/idt.h"
#include "arch/gdt.h"
#include "keyboard/keyboard.h"
#include "display.h"
#include "pci/pci.h"
#include "pit/pit.h"
#include <stdint.h>
#define CMD_BUF_SIZE 128
#define HISTORY_SIZE 16
#define USERNAME_MAX 31
#define PASSWORD_MAX 63
#define USER_DB_LBA 2048u
#define USER_DB_MAGIC 0x54555352u
#define USER_DB_VERSION 1u

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t version;
    uint32_t checksum;
    uint8_t has_user;
    char username[USERNAME_MAX + 1];
    uint32_t password_hash;
    uint8_t reserved[463];
} user_db_sector_t;

typedef char user_db_sector_must_be_512_bytes[(sizeof(user_db_sector_t) == 512) ? 1 : -1];

static user_db_sector_t g_user_db;
static int g_logged_in = 0;
static char g_current_user[USERNAME_MAX + 1];
static char g_command_history[HISTORY_SIZE][CMD_BUF_SIZE];
static int g_history_count = 0;
static int g_history_start = 0;

static int streq(const char *a, const char *b) {
    while (*a && *b) {
        if (*a != *b) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static int starts_with(const char *s, const char *prefix) {
    while (*prefix) {
        if (*s++ != *prefix++) {
            return 0;
        }
    }
    return 1;
}

static char ascii_lower(char c) {
    if (c >= 'A' && c <= 'Z') {
        return (char)(c + ('a' - 'A'));
    }
    return c;
}

static int contains_text(const char *text, const char *pattern) {
    if (*pattern == '\0') {
        return 1;
    }

    while (*text) {
        const char *text_scan = text;
        const char *pattern_scan = pattern;

        while (*text_scan && *pattern_scan && ascii_lower(*text_scan) == ascii_lower(*pattern_scan)) {
            text_scan++;
            pattern_scan++;
        }

        if (*pattern_scan == '\0') {
            return 1;
        }

        text++;
    }

    return 0;
}

static size_t str_len(const char *s) {
    size_t n = 0;
    while (s[n] != '\0') {
        n++;
    }
    return n;
}

static void mem_zero(void *ptr, size_t n) {
    uint8_t *p = (uint8_t *)ptr;
    for (size_t i = 0; i < n; i++) {
        p[i] = 0;
    }
}

static void str_copy(char *dst, size_t dst_size, const char *src) {
    size_t i = 0;
    if (dst_size == 0) {
        return;
    }
    while (i + 1 < dst_size && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static int parse_two_args(const char *s, char *a, size_t a_size, char *b, size_t b_size) {
    size_t i = 0;
    size_t j = 0;


    while (*s == ' ') {
        s++;
    }

    while (*s && *s != ' ') {
        if (i + 1 >= a_size) {
            return 0;
        }
        a[i++] = *s++;
    }
    a[i] = '\0';

    while (*s == ' ') {
        s++;
    }

    while (*s && *s != ' ') {
        if (j + 1 >= b_size) {
            return 0;
        }
        b[j++] = *s++;
    }
    b[j] = '\0';

    while (*s == ' ') {
        s++;
    }

    if (i == 0 || j == 0 || *s != '\0') {
        return 0;
    }

    return 1;
}

static int parse_one_arg(const char *s, char *a, size_t a_size) {
    size_t i = 0;

    while (*s == ' ') {
        s++;
    }

    while (*s && *s != ' ') {
        if (i + 1 >= a_size) {
            return 0;
        }
        a[i++] = *s++;
    }
    a[i] = '\0';

    while (*s == ' ') {
        s++;
    }

    if (i == 0 || *s != '\0') {
        return 0;
    }

    return 1;
}

static void history_push(const char *cmd) {
    int slot;

    if (cmd[0] == '\0') {
        return;
    }

    if (g_history_count > 0) {
        int last = (g_history_start + g_history_count - 1) % HISTORY_SIZE;
        if (streq(g_command_history[last], cmd)) {
            return;
        }
    }

    if (g_history_count < HISTORY_SIZE) {
        slot = (g_history_start + g_history_count) % HISTORY_SIZE;
        g_history_count++;
    } else {
        slot = g_history_start;
        g_history_start = (g_history_start + 1) % HISTORY_SIZE;
    }

    str_copy(g_command_history[slot], sizeof(g_command_history[slot]), cmd);
}

static void print_history(void) {
    if (g_history_count == 0) {
        console_writeln("History is empty.");
        return;
    }

    for (int i = 0; i < g_history_count; i++) {
        int slot = (g_history_start + i) % HISTORY_SIZE;
        console_writef("%d: %s\n", i + 1, g_command_history[slot]);
    }
}

static const char *g_commands[] = {
    "about",
    "banner",
    "calc",
    "cls",
    "clear",
    "color",
    "commands",
    "cpu",
    "date",
    "deluser",
    "devices",
    "disk",
    "echo",
    "halt",
    "help",
    "history",
    "lines",
    "lock",
    "login",
    "logout",
    "passwd",
    "ptop",
    "reboot",
    "repeat",
    "rect",
    "rect2",
    "swamp",
    "sysinfo",
    "theme",
    "time",
    "Turtle talk",
    "useradd",
    "user",
    "version",
    "whoami",
};

static void show_banner(void) {
    set_console_fg_color(0x00FF00);
    console_writeln("       ");
    console_writeln("   version 0.2.4");
    console_writeln("⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣀⣀⣀⣀⣀⠀⠀⠀⠀");
    console_writeln("⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⡴⠊⠁⠀⠀⠀⠀⠈⠙⢦⡀⠀");
    console_writeln("⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⡜⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢳⠀");
    console_writeln("⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⡇");
    console_writeln("⠀⠀⠀⠀⠀⠀⠀⣀⠤⢄⣀⠀⠀⠀⡇⠀⠀⠀⠀⢰⣶⠄⠀⠀⠀⠀⡇");
    console_writeln("⠀⠀⠀⠀⠀⡴⡋⠀⠀⠀⡨⠓⣄⠀⢳⠀⠀⠀⠀⠀⠉⠀⠀⠀⢀⡼⠀");
    console_writeln("⠀⠀⠀⢀⡞⠀⢸⠓⠒⢺⡀⠀⠈⢣⠈⡇⠀⠀⠀⠀⠀⢠⡤⠴⠋⠀⠀");
    console_writeln("⠀⠀⠀⡼⠒⠒⢏⠀⠀⠀⠙⣦⠖⠉⢧⡿⠀⠈⠙⡖⠚⠉⠀⠀⠀⠀⠀");
    console_writeln("⠀⠀⡖⢧⡀⠀⠈⣦⡤⠤⠊⡏⣀⡴⠊⡹⠀⣠⠞⠀⠀⠀⠀⠀⠀⠀⠀");
    console_writeln("⢶⡞⡟⠦⣌⡓⠾⠥⠤⠴⠒⠋⣁⠴⢊⣤⠞⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀");
    console_writeln("⠀⠀⡇⠀⠀⢉⣙⣒⣒⣒⣚⣉⠁⠀⢣⡤⠃⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀");
    console_writeln("⠀⠀⠙⠒⠒⠚⠒⠋⠉⠉⠀⠈⠓⠚⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀");
    console_writeln("Dermochelys coriacea");
    console_writeln("  ");
    set_console_fg_color(0xFFFFFF);
}

static char get_input_char(void) {
    if (kb_available()) return getchar();
    {
        char polled = keyboard_poll_char();
        if (polled) return polled;
    }
    if (serial_received()) return serial_read();
    return 0;
}

static void read_line_prompt(const char *prompt, char *buf, size_t buf_size, int hide_input) {
    size_t len = 0;
    console_write(prompt);

    for (;;) {
        char c = get_input_char();
        if (c) {
            if (c == '\r' || c == '\n') {
                console_putc('\n');
                break;
            }

            if (c == '\b' || c == 127) {
                if (len > 0) {
                    len--;
                    console_backspace();
                }
                continue;
            }

            if (c >= 32 && c <= 126 && len + 1 < buf_size) {
                buf[len++] = c;
                console_putc(hide_input ? '*' : c);
            }
        } else {
            __asm__ volatile("hlt");
        }
    }

    buf[len] = '\0';
}

static uint32_t hash_password(const char *username, const char *password) {
    uint32_t h = 2166136261u;
    const uint32_t pepper = 0x9E3779B9u;

    while (*username) {
        h ^= (uint8_t)*username++;
        h *= 16777619u;
    }
    h ^= (uint32_t)':';
    h *= 16777619u;
    while (*password) {
        h ^= (uint8_t)*password++;
        h *= 16777619u;
    }

    h ^= pepper;
    h *= 16777619u;
    return h;
}

static uint32_t user_db_checksum(const user_db_sector_t *db) {
    const uint8_t *bytes = (const uint8_t *)db;
    uint32_t sum = 5381u;

    for (size_t i = 0; i < sizeof(user_db_sector_t); i++) {
        if (i >= 8 && i < 12) {
            continue;
        }
        sum = ((sum << 5) + sum) + bytes[i];
    }

    return sum;
}

static int user_db_save(void) {
    g_user_db.checksum = user_db_checksum(&g_user_db);
    return ata_write_sector(USER_DB_LBA, &g_user_db);
}

static void user_db_reset(void) {
    mem_zero(&g_user_db, sizeof(g_user_db));
    g_user_db.magic = USER_DB_MAGIC;
    g_user_db.version = USER_DB_VERSION;
    g_user_db.checksum = user_db_checksum(&g_user_db);
}

static void user_db_load(void) {
    if (!ata_read_sector(USER_DB_LBA, &g_user_db)) {
        user_db_reset();
        return;
    }

    if (g_user_db.magic != USER_DB_MAGIC || g_user_db.version != USER_DB_VERSION) {
        user_db_reset();
        return;
    }

    if (g_user_db.checksum != user_db_checksum(&g_user_db)) {
        user_db_reset();
        return;
    }
}

static int create_user(const char *username, const char *password) {
    if (str_len(username) == 0 || str_len(password) == 0) {
        return 0;
    }
    if (str_len(username) > USERNAME_MAX || str_len(password) > PASSWORD_MAX) {
        return 0;
    }

    user_db_reset();
    g_user_db.has_user = 1;
    str_copy(g_user_db.username, sizeof(g_user_db.username), username);
    g_user_db.password_hash = hash_password(username, password);

    if (!user_db_save()) {
        return 0;
    }

    str_copy(g_current_user, sizeof(g_current_user), username);
    g_logged_in = 1;
    return 1;
}

static int try_login(const char *username, const char *password) {
    uint32_t h;

    if (!g_user_db.has_user) {
        return 0;
    }
    if (!streq(g_user_db.username, username)) {
        return 0;
    }

    h = hash_password(username, password);
    if (h != g_user_db.password_hash) {
        return 0;
    }

    str_copy(g_current_user, sizeof(g_current_user), username);
    g_logged_in = 1;
    return 1;
}

static int change_password(const char *old_password, const char *new_password) {
    if (!g_logged_in || !g_user_db.has_user) {
        return 0;
    }
    if (str_len(new_password) == 0 || str_len(new_password) > PASSWORD_MAX) {
        return 0;
    }
    if (hash_password(g_current_user, old_password) != g_user_db.password_hash) {
        return 0;
    }

    g_user_db.password_hash = hash_password(g_current_user, new_password);
    if (!user_db_save()) {
        return 0;
    }
    return 1;
}

static int delete_user(const char *password) {
    if (!g_logged_in || !g_user_db.has_user) {
        return 0;
    }

    if (hash_password(g_current_user, password) != g_user_db.password_hash) {
        return 0;
    }

    user_db_reset();
    g_logged_in = 0;
    g_current_user[0] = '\0';
    return user_db_save();
}

static void auth_boot_flow(void) {
    char username[USERNAME_MAX + 1];
    char password[PASSWORD_MAX + 1];

    user_db_load();

    if (g_user_db.has_user) {
	console_writeln("--Login--");
	for (int logins = 0; logins < 3; logins ++) {
            console_write("User: "); console_writeln(g_user_db.username);
	    read_line_prompt("Password: ", password, sizeof(password), 1);
	    int success = try_login(g_user_db.username, password);
	    if (success == 1) {
		console_writeln("Success!");
		return;
	    } else {
		console_writeln("Failed!");
		continue;
	    }
	}
	reboot();
    }

    console_writeln("Account");
    for (;;) {
        read_line_prompt("username: ", username, sizeof(username), 0);
        read_line_prompt("password: ", password, sizeof(password), 1);

        if (create_user(username, password)) {
            console_write("Account created. Logged in as ");
            console_writeln(g_current_user);
            break;
        }

        console_writeln("Failed to create account.");
    }
}

void reboot(void) {
    uint8_t good = 0x02;
    while (good & 0x02) {
        good = inb(0x64);
    }
    outb(0x64, 0xFE);
}

static void print_help(void) {
    for (size_t i = 0; i < sizeof(g_commands) / sizeof(g_commands[0]); i++) {
        console_writef("  %s\n", g_commands[i]);
    }
}

static void print_help_filtered(const char *filter) {
    int matches = 0;

    if (filter[0] == '\0') {
        print_help();
        return;
    }

    for (size_t i = 0; i < sizeof(g_commands) / sizeof(g_commands[0]); i++) {
        if (contains_text(g_commands[i], filter)) {
            console_writef("  %s\n", g_commands[i]);
            matches++;
        }
    }

    if (matches == 0) {
        console_writeln("No commands matched that filter.");
    }
}

static void print_uint2(unsigned int n) {
    console_putc((char)('0' + (n / 10) % 10));
    console_putc((char)('0' + n % 10));
}

static int parse_uint_arg(const char *s, unsigned int *value, const char **rest) {
    unsigned int result = 0;
    int saw_digit = 0;

    while (*s == ' ') {
        s++;
    }

    while (*s >= '0' && *s <= '9') {
        saw_digit = 1;
        result = (result * 10u) + (unsigned int)(*s - '0');
        s++;
    }

    if (!saw_digit) {
        return 0;
    }

    while (*s == ' ') {
        s++;
    }

    *value = result;
    *rest = s;
    return 1;
}

static void print_uint(unsigned int n) {
    char buf[12];
    int len = 0;
    if (n == 0) { console_putc('0'); return; }
    while (n > 0) { buf[len++] = (char)('0' + n % 10); n /= 10; }
    for (int i = len - 1; i >= 0; i--) console_putc(buf[i]);
}

static void print_int(int n) {
    if (n < 0) { console_putc('-'); print_uint((unsigned int)-n); }
    else print_uint((unsigned int)n);
}

static uint8_t cmos_read(uint8_t reg) {
    outb(0x70, reg);
    io_wait();
    return inb(0x71);
}

static uint8_t bcd2bin(uint8_t v) {
    return (uint8_t)((v & 0x0F) + ((v >> 4) * 10));
}

static void cmd_date(void) {
    while (cmos_read(0x0A) & 0x80) {}
    uint8_t sec  = bcd2bin(cmos_read(0x00));
    uint8_t min  = bcd2bin(cmos_read(0x02));
    uint8_t hour = bcd2bin(cmos_read(0x04));
    uint8_t day  = bcd2bin(cmos_read(0x07));
    uint8_t mon  = bcd2bin(cmos_read(0x08));
    uint8_t year = bcd2bin(cmos_read(0x09));
    console_write("Date: 20");
    print_uint2(year); console_putc('-');
    print_uint2(mon);  console_putc('-');
    print_uint2(day);
    console_write("  Time: ");
    print_uint2(hour); console_putc(':');
    print_uint2(min);  console_putc(':');
    print_uint2(sec);  console_putc('\n');
}

static void cmd_time(void) {
    while (cmos_read(0x0A) & 0x80) {}
    print_uint2(bcd2bin(cmos_read(0x04)));
    console_putc(':');
    print_uint2(bcd2bin(cmos_read(0x02)));
    console_putc(':');
    print_uint2(bcd2bin(cmos_read(0x00)));
    console_putc('\n');
}

static const char *calc_pos;

static void calc_skip(void) {
    while (*calc_pos == ' ') calc_pos++;
}

static int calc_expr(int *out);

static int calc_factor(int *out) {
    calc_skip();
    if (*calc_pos == '(') {
        calc_pos++;
        if (!calc_expr(out)) return 0;
        calc_skip();
        if (*calc_pos == ')') calc_pos++;
        return 1;
    }
    int neg = 0;
    if (*calc_pos == '-') { neg = 1; calc_pos++; }
    if (*calc_pos < '0' || *calc_pos > '9') return 0;
    int n = 0;
    while (*calc_pos >= '0' && *calc_pos <= '9') { n = n * 10 + (*calc_pos++ - '0'); }
    *out = neg ? -n : n;
    return 1;
}

static int calc_term(int *out) {
    int left;
    if (!calc_factor(&left)) return 0;
    calc_skip();
    while (*calc_pos == '*' || *calc_pos == '/') {
        char op = *calc_pos++;
        int right;
        if (!calc_factor(&right)) return 0;
        if (op == '/') {
            if (right == 0) { console_writeln("Error: division by zero"); return 0; }
            left /= right;
        } else { left *= right; }
        calc_skip();
    }
    *out = left;
    return 1;
}

static int calc_expr(int *out) {
    int left;
    if (!calc_term(&left)) return 0;
    calc_skip();
    while (*calc_pos == '+' || *calc_pos == '-') {
        char op = *calc_pos++;
        int right;
        if (!calc_term(&right)) return 0;
        left = (op == '+') ? left + right : left - right;
        calc_skip();
    }
    *out = left;
    return 1;
}

static void cmd_calc(const char *expr) {
    if (expr[0] == '\0') { console_writeln("Calculator."); return; }
    calc_pos = expr;
    int result;
    if (!calc_expr(&result)) { console_writeln("Error: invalid expression"); return; }
    calc_skip();
    if (*calc_pos != '\0') { console_writeln("Error: unexpected character"); return; }
    console_write("= ");
    print_int(result);
    console_putc('\n');
}

static uint32_t swamp_seed = 0;

static uint32_t swamp_rand(void) {
    if (swamp_seed == 0) {
        uint8_t sec = bcd2bin(cmos_read(0x00));
        uint8_t min = bcd2bin(cmos_read(0x02));
        uint8_t hour = bcd2bin(cmos_read(0x04));
        swamp_seed = ((uint32_t)hour << 16) ^ ((uint32_t)min << 8) ^ sec ^ 0xA5A5u;
        if (swamp_seed == 0) {
            swamp_seed = 1;
        }
    }

    swamp_seed = swamp_seed * 1664525u + 1013904223u;
    return swamp_seed;
}

static int parse_uint(const char *s, unsigned int *out) {
    unsigned int n = 0;
    int saw_digit = 0;

    while (*s == ' ') {
        s++;
    }

    while (*s >= '0' && *s <= '9') {
        saw_digit = 1;
        n = (n * 10u) + (unsigned int)(*s - '0');
        s++;
    }

    while (*s == ' ') {
        s++;
    }

    if (!saw_digit || *s != '\0') {
        return 0;
    }

    *out = n;
    return 1;
}

static void cmd_swamp(const char *arg) {
    static const char chars[] =
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789"
        "!@#$%^&*()_+-=[]{};:'\",.<>/?\\|`~";
    const unsigned int charset_len = (unsigned int)(sizeof(chars) - 1);
    unsigned int lines = 10;
    unsigned int width = 60;

    if (arg[0] != '\0') {
        if (!parse_uint(arg, &lines)) {
            console_writeln("I am not gonna put anything here yet since im lazy :/");
            return;
        }
    }

    if (lines == 0) {
        lines = 1;
    }
    if (lines > 40) {
        lines = 40;
    }

    console_writeln("  ");
    for (unsigned int y = 0; y < lines; y++) {
        for (unsigned int x = 0; x < width; x++) {
            uint32_t r = swamp_rand();
            console_putc(chars[r % charset_len]);
        }
        console_putc('\n');
    }
}

static void turtle_talk(const char *message) {
    if (message[0] == '\0') {
        console_writeln("James: Hello Folks! I am James the Turtle! how are you?");
        return;
    }

    if (contains_text(message, "hello") || contains_text(message, "hi")) {
        console_writeln("James: Hello there.");
        return;
    }

    if (contains_text(message, "how are you") || contains_text(message, "hru")) {
        console_writeln("James: I am doing good, How are you?");
        return;
    }

    if (contains_text(message, "name")) {
        console_writeln("James: My name? Seriously, well whatever folk, my name is James, James the Turtle.");
        return;
    }

    if (contains_text(message, "help")) {
        console_writeln("James: Ya need help? Dont worry, just ask me some things, and I am glad to answer.");
        return;
    }

    if (contains_text(message, "joke")) {
        console_writeln("James: Sorry but I am not very funny");
        return;
    }

    if (contains_text(message, "sad") || contains_text(message, "bad")) {
        console_writeln("James: Dont worry mates, it will be okay... For me I am just a turtle, I hope things get better for ya. Just keep going.");
        return;
    }

    if (contains_text(message, "good") || contains_text(message, "great")) {
        console_writeln("James: Nice! I am glad to hear that!");
        return;
    }

    if (contains_text(message, "bye")) {
        console_writeln("James: See you later.");
        return;
    }

    console_writeln("James: I dont speak very good english, maybe try saying something in Turtalese.");
}

void cmd_ptop() {
	task_info_t* tasks = (task_info_t*)malloc(sizeof(task_info_t) * get_task_count());
	tasks_get_info(tasks, get_task_count());
	for (int task = 0; task < get_task_count(); task++) {
		console_writefln("PID: %d; Name: %s, Active: %d", tasks[task].pid, tasks[task].name, tasks[task].active);	
	}	
}

static void cmd_about(void) {
    console_writeln("Leatherback Sea TurtleOS 0.2.4");
    console_writeln("Target: i386, multiboot framebuffer + serial console");
    console_writef("Tasks: %d\n", get_task_count());
    if (g_logged_in) {
        console_write("User: ");
        console_writeln(g_current_user);
    } else {
        console_writeln("User: guest");
    }
    console_writeln("Commands: help, help <text>, history, about, banner");
}

static void cmd_version(void) {
    console_writeln("Leatherback Sea TurtleOS version 0.2.4");
}

static void cmd_user(void) {
    if (g_logged_in) {
        console_write("Logged in as ");
        console_writeln(g_current_user);
        return;
    }

    if (g_user_db.has_user) {
        console_writeln("No active session.");
        return;
    }

    console_writeln("No user account configured.");
}

static void cmd_disk(void) {
    uint32_t sectors = ata_get_sector_count();
    uint32_t mib = sectors / 2048u;

    if (sectors == 0) {
        console_writeln("Disk information unavailable.");
        return;
    }

    console_writef("ATA sectors: %d\n", (int)sectors);
    console_writef("Approx size: %d MiB\n", (int)mib);
    console_writef("User DB sector: %d\n", (int)USER_DB_LBA);
}

static void cmd_devices(void) {
    size_t storage = 0;
    size_t net = 0;
    size_t display = 0;
    size_t bridge = 0;

    for (size_t i = 0; i < g_pci_bus.count; i++) {
        uint8_t cls = g_pci_bus.devices[i].class_code;
        if (cls == 0x01) storage++;
        else if (cls == 0x02) net++;
        else if (cls == 0x03) display++;
        else if (cls == 0x06) bridge++;
    }

    console_writef("PCI devices: %d\n", (int)g_pci_bus.count);
    console_writef("storage: %d\n", (int)storage);
    console_writef("net: %d\n", (int)net);
    console_writef("display: %d\n", (int)display);
    console_writef("bridge: %d\n", (int)bridge);
}

static void cmd_lock(void) {
    char password[PASSWORD_MAX + 1];

    if (!g_logged_in || !g_user_db.has_user) {
        console_writeln("No active user session.");
        return;
    }

    g_logged_in = 0;
    g_current_user[0] = '\0';
    console_writeln("Session locked.");
    for (;;) {
        read_line_prompt("Password: ", password, sizeof(password), 1);
        if (try_login(g_user_db.username, password)) {
            console_writeln("Unlocked.");
            return;
        }
        console_writeln("Wrong password.");
    }
}

static void cmd_theme(const char *name) {
    if (streq(name, "matrix")) {
        set_console_bg_color(0x000000);
        set_console_fg_color(0x00FF00);
        console_writeln("Theme set to matrix.");
        return;
    }

    if (streq(name, "ocean")) {
        set_console_bg_color(0x001B2E);
        set_console_fg_color(0x7FDBFF);
        console_writeln("Theme set to ocean.");
        return;
    }

    if (streq(name, "sand")) {
        set_console_bg_color(0x2B1D0E);
        set_console_fg_color(0xF4D28C);
        console_writeln("Theme set to sand.");
        return;
    }

    if (streq(name, "classic")) {
        set_console_bg_color(0x000000);
        set_console_fg_color(0xFFFFFF);
        console_writeln("Theme set to classic.");
        return;
    }

    console_writeln("Usage: theme classic|matrix|ocean|sand");
}

static void cmd_repeat(const char *args) {
    unsigned int count = 0;
    const char *text = 0;

    if (!parse_uint_arg(args, &count, &text) || text[0] == '\0') {
        console_writeln("Usage: repeat <count> <text>");
        return;
    }

    if (count == 0) {
        console_writeln("Count must be at least 1.");
        return;
    }

    if (count > 32) {
        console_writeln("Count too large. Max is 32.");
        return;
    }

    for (unsigned int i = 0; i < count; i++) {
        console_writeln(text);
    }
}

static void run_command(const char *cmd) {
    char a[USERNAME_MAX + 1];
    char b[PASSWORD_MAX + 1];

    if (cmd[0] == '\0') {
        return;
    }

    if (streq(cmd, "help")) {
        print_help();
        return;
    }

    if (starts_with(cmd, "help ")) {
        print_help_filtered(cmd + 5);
        return;
    }

    if (streq(cmd, "commands")) {
        print_help();
        return;
    }

    if (streq(cmd, "history")) {
        print_history();
        return;
    }

    if (streq(cmd, "history clear")) {
        g_history_count = 0;
        g_history_start = 0;
        console_writeln("History cleared.");
        return;
    }

    if (streq(cmd, "about")) {
        cmd_about();
        return;
    }

    if (streq(cmd, "version") || streq(cmd, "ver")) {
        cmd_version();
        return;
    }

    if (streq(cmd, "user")) {
        cmd_user();
        return;
    }

    if (streq(cmd, "disk")) {
        cmd_disk();
        return;
    }

    if (streq(cmd, "devices")) {
        cmd_devices();
        return;
    }

    if (streq(cmd, "lock")) {
        cmd_lock();
        return;
    }

    if (streq(cmd, "banner")) {
        show_banner();
        return;
    }

    if (streq(cmd, "ptop")) {
	    cmd_ptop();
	    return;
    }

    if (streq(cmd, "cpu")) {
	    char buffer[49];
	    get_cpu_name(buffer);
	    console_writeln(buffer);
	    return;
    }

    if (streq(cmd, "Turtle talk")) {
        turtle_talk("");
        return;
    }

    if (starts_with(cmd, "Turtle talk ")) {
        turtle_talk(cmd + 12);
        return;
    }

    if (streq(cmd, "lines")) {
	    draw_line(0, 0, 1920, 1080, 0x00FF00);
	    draw_line(1920, 0, 0, 1080, 0x0000FF);
	    return;
    }

    if (streq(cmd, "date")) {
        cmd_date();
        return;
    }

    if (streq(cmd, "time")) {
        cmd_time();
        return;
    }

    if (starts_with(cmd, "calc ")) {
        cmd_calc(cmd + 5);
        return;
    }

    if (streq(cmd, "rect")) {
	    draw_rect(200, 200, 200, 200, 0xFFFFFF);
	    return;
    }
	
    if (streq(cmd, "rect2")) {
	    draw_rect(300, 300, 300, 300, 0xFFFFFF);
	    return;
    }

    if (streq(cmd, "calc")) {
        cmd_calc("");
        return;
    }

    if (streq(cmd, "swamp")) {
        cmd_swamp("");
        return;
    }

    if (starts_with(cmd, "swamp ")) {
        cmd_swamp(cmd + 10);
        return;
    }

    if (streq(cmd, "clear")) {
        console_clear();
        return;
    }

    if (streq(cmd, "cls")) {
        console_clear();
        return;
    }

    if (starts_with(cmd, "color ")) {
        char* color_str = cmd + 9;
	uint32_t color_hex = string_to_hex(color_str);
	if (cmd[6] == 'f' && cmd[7] == 'g') {
            set_console_fg_color(color_hex);
	} else if (cmd[6] == 'b' && cmd[7] == 'g') {
            set_console_bg_color(color_hex);
	}
	return;
    }

    if (starts_with(cmd, "theme ")) {
        cmd_theme(cmd + 6);
        return;
    }

    if (starts_with(cmd, "echo ")) {
        console_writeln(cmd + 5);
        return;
    }

    if (starts_with(cmd, "repeat ")) {
        cmd_repeat(cmd + 7);
        return;
    }

    if (starts_with(cmd, "useradd ")) {
        if (!parse_two_args(cmd + 8, a, sizeof(a), b, sizeof(b))) {
            console_writeln("Usage: useradd");
            return;
        }
        if (g_user_db.has_user) {
            console_writeln("A user already exists");
            return;
        }
        if (create_user(a, b)) {
            console_writeln("User created");
        } else {
            console_writeln("Failed to create user.");
        }
        return;
    }

    if (starts_with(cmd, "deluser ")) {
        if (!parse_one_arg(cmd + 8, a, sizeof(a))) {
            console_writeln("Usage: deluser <password>");
            return;
        }

        if (delete_user(a)) {
            console_writeln("User deleted.");
        } else {
            console_writeln("Failed to delete user.");
        }
        return;
    }

    if (starts_with(cmd, "login ")) {
        if (!parse_two_args(cmd + 6, a, sizeof(a), b, sizeof(b))) {
            console_writeln("Usage: login");
            return;
        }
        if (try_login(a, b)) {
            console_write("Logged in as ");
            console_writeln(g_current_user);
        } else {
            console_writeln("Login failed.");
        }
        return;
    }

    if (streq(cmd, "logout")) {
        if (!g_logged_in) {
            console_writeln("Not logged in.");
            return;
        }
        g_logged_in = 0;
        g_current_user[0] = '\0';
        console_writeln("Logged out.");
        reboot();
        return;
    }

    if (streq(cmd, "whoami")) {
        if (g_logged_in) {
            console_writeln(g_current_user);
        } else {
            console_writeln("Error");
        }
        return;
    }

    if (starts_with(cmd, "passwd ")) {
        if (!parse_two_args(cmd + 7, a, sizeof(a), b, sizeof(b))) {
            console_writeln("Usage: passwd ");
            return;
        }
        if (change_password(a, b)) {
            console_writeln("Password changed.");
        } else {
            console_writeln("Password change failed.");
        }
        return;
    }

    if (streq(cmd, "reboot")) {
        console_writeln("Rebooting...");
        reboot();
        return;
    }

    if (streq(cmd, "sysinfo")) {
	    set_console_fg_color(0x00FF00);
        console_writeln("⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣀⣀⣀⣀⣀⠀⠀⠀⠀");
        console_writeln("⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⡴⠊⠁⠀⠀⠀⠀⠈⠙⢦⡀⠀");
        console_writeln("⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⡜⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢳ OS: Leatherback Sea TurtleOS");
        console_writeln("⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⡇ Kernel: x86_64");
        console_writeln("⠀⠀⠀⠀⠀⠀⠀⣀⠤⢄⣀⠀⠀⠀⡇⠀⠀⠀⠀⢰⣶⠄⠀⠀⠀⠀⡇ Version: 0.2.4");
        console_writeln("⠀⠀⠀⠀⠀⡴⡋⠀⠀⠀⡨⠓⣄⠀⢳⠀⠀⠀⠀⠀⠉⠀⠀⠀⢀⡼⠀");
        console_writeln("⠀⠀⠀⢀⡞⠀⢸⠓⠒⢺⡀⠀⠈⢣⠈⡇⠀⠀⠀⠀⠀⢠⡤⠴⠋⠀⠀");
        console_writeln("⠀⠀⠀⡼⠒⠒⢏⠀⠀⠀⠙⣦⠖⠉⢧⡿⠀⠈⠙⡖⠚⠉⠀⠀⠀⠀⠀");
        console_writeln("⠀⠀⡖⢧⡀⠀⠈⣦⡤⠤⠊⡏⣀⡴⠊⡹⠀⣠⠞⠀⠀⠀⠀⠀⠀⠀⠀");
        console_writeln("⢶⡞⡟⠦⣌⡓⠾⠥⠤⠴⠒⠋⣁⠴⢊⣤⠞⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀");
        console_writeln("⠀⠀⡇⠀⠀⢉⣙⣒⣒⣒⣚⣉⠁⠀⢣⡤⠃⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀");
        console_writeln("⠀⠀⠙⠒⠒⠚⠒⠋⠉⠉⠀⠈⠓⠚⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀");
	    set_console_fg_color(0xFFFFFF);
        return;
    }

    if (streq(cmd, "halt")) {
        console_writeln("Halting...");
        __asm__ volatile("cli");
        for (;;) {
            __asm__ volatile("hlt");
        }
    }

    console_writeln("command not found");
}

void write_prompt() {
	console_write("LST-OS> ");
}

void shell() {
    char cmd_buf[CMD_BUF_SIZE];
    size_t cmd_len = 0;
    show_banner();
    pci_enumerate(&g_pci_bus);
    auth_boot_flow();
    //console_writeln("  ");
    int new_prompt = 1;
    cmd_len = 0;
    while (1) {
	if (new_prompt == 1) {
	    write_prompt();
	    new_prompt = 0;
	}
        char c = get_input_char();
        if (c) {
            if (c == '\r' || c == '\n') {
                console_putc('\n');
                cmd_buf[cmd_len] = '\0';
                history_push(cmd_buf);
                cmd_len = 0;
                run_command(cmd_buf);
                new_prompt = 1;
            } else if (c == '\b' || c == 127) {
                if (cmd_len > 0) {
                    cmd_len--;
                    console_backspace();
                }
            } else if (c >= 32 && c <= 126) {
                if (cmd_len < CMD_BUF_SIZE - 1) {
                    cmd_buf[cmd_len++] = c;
                    console_putc(c);
                }
            }
        }
        render_console();
        __asm__ volatile("hlt");
    }
}
