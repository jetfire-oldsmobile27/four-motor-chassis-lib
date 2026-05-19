/**
 * @file opi_demo.c
 * @brief Orange Pi — TCP socket server on port 8327
 *
 * Protocol (newline-terminated ASCII):
 *   FORWARD <ms>   BACKWARD <ms>
 *   LEFT <ms>      RIGHT <ms>
 *   CURVE_FL <ms>  CURVE_FR <ms>
 *   CURVE_BL <ms>  CURVE_BR <ms>
 *   STOP           QUIT
 *
 * ms = 0 → continuous until next command.
 */

#include "../src/four_motor_chassis.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

/* ── pins ───────────────────────────────────────────────────────────────── */
#define FL_FWD 256
#define FL_REV 257
#define FR_FWD 259
#define FR_REV 258   /* swapped — mirror motor */
#define RL_FWD 260
#define RL_REV 76
#define RR_FWD 270
#define RR_REV 271

#define PORT    8327
#define BUF_SZ  256

static fmcc_chassis_t g_chassis;
static volatile int   g_running = 1;

/* Total commands received this session — shown in status line */
static unsigned long g_cmd_count = 0;
static char g_last_cmd[BUF_SZ] = "—";
static char g_client_ip[INET_ADDRSTRLEN] = "none";

static void _sig(int s) { (void)s; g_running = 0; }

/* ── status line (overwrites itself) ───────────────────────────────────── */
static void print_status(void)
{
    /* \r returns to line start; no \n → stays on same line */
    fprintf(stderr, "\r[srv] client=%-15s  cmds=%-6lu  last=%-30s",
            g_client_ip, g_cmd_count, g_last_cmd);
    fflush(stderr);
}

/* ── command dispatcher ─────────────────────────────────────────────────── */
/* Returns 1 → disconnect client, 0 → keep going */
static int dispatch(fmcc_chassis_t *ch, const char *line, int fd)
{
    char cmd[64] = {0};
    unsigned int ms = 0;
    if (sscanf(line, "%63s %u", cmd, &ms) < 1) return 0;

    for (char *p = cmd; *p; p++)
        if (*p >= 'a' && *p <= 'z') *p -= 32;

    const char *reply = "OK\n";

    if      (!strcmp(cmd, "FORWARD"))   fmcc_forward(ch, ms);
    else if (!strcmp(cmd, "BACKWARD"))  fmcc_backward(ch, ms);
    else if (!strcmp(cmd, "LEFT"))      fmcc_turn_left(ch, ms);
    else if (!strcmp(cmd, "RIGHT"))     fmcc_turn_right(ch, ms);
    else if (!strcmp(cmd, "CURVE_FL"))  fmcc_curve_forward_left(ch, ms);
    else if (!strcmp(cmd, "CURVE_FR"))  fmcc_curve_forward_right(ch, ms);
    else if (!strcmp(cmd, "CURVE_BL"))  fmcc_curve_backward_left(ch, ms);
    else if (!strcmp(cmd, "CURVE_BR"))  fmcc_curve_backward_right(ch, ms);
    else if (!strcmp(cmd, "STOP"))      fmcc_stop(ch);
    else if (!strcmp(cmd, "QUIT"))    { send(fd, "BYE\n", 4, 0); return 1; }
    else                                reply = "ERR unknown\n";

    send(fd, reply, strlen(reply), 0);

    snprintf(g_last_cmd, sizeof(g_last_cmd), "%s", line);
    g_cmd_count++;
    print_status();

    return 0;
}

/* ── main ───────────────────────────────────────────────────────────────── */
int main(void)
{
    signal(SIGINT,  _sig);
    signal(SIGTERM, _sig);
    signal(SIGPIPE, SIG_IGN);

    /* chassis init */
    fmcc_config_t cfg = FMCC_CONFIG_INIT;
    fmcc_motor_set(&cfg, FMCC_MOTOR_FL, FL_FWD, FL_REV);
    fmcc_motor_set(&cfg, FMCC_MOTOR_FR, FR_FWD, FR_REV);
    fmcc_motor_set(&cfg, FMCC_MOTOR_RL, RL_FWD, RL_REV);
    fmcc_motor_set(&cfg, FMCC_MOTOR_RR, RR_FWD, RR_REV);

    if (fmcc_init(&g_chassis, &cfg) != 0) {
        fprintf(stderr, "fmcc_init failed\n");
        return 1;
    }

    /* server socket */
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    int yes = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port        = htons(PORT)
    };
    if (bind(srv, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); fmcc_deinit(&g_chassis); return 1;
    }
    listen(srv, 1);
    fprintf(stderr, "[srv] chassis ready, listening on :%d\n", PORT);

    /* initial blank status line */
    print_status();

    /* accept loop */
    while (g_running) {
        fd_set rfds; FD_ZERO(&rfds); FD_SET(srv, &rfds);
        struct timeval tv = {1, 0};
        if (select(srv + 1, &rfds, NULL, NULL, &tv) <= 0) continue;

        struct sockaddr_in cli_addr;
        socklen_t cli_len = sizeof(cli_addr);
        int cli = accept(srv, (struct sockaddr *)&cli_addr, &cli_len);
        if (cli < 0) continue;

        inet_ntop(AF_INET, &cli_addr.sin_addr, g_client_ip, sizeof(g_client_ip));
        g_cmd_count = 0;
        snprintf(g_last_cmd, sizeof(g_last_cmd), "—");

        /* newline before connection banner so status line isn't overwritten */
        fprintf(stderr, "\n[srv] client connected: %s\n", g_client_ip);
        print_status();

        send(cli, "READY\n", 6, 0);

        /* per-client read loop */
        char buf[BUF_SZ];
        char line[BUF_SZ];
        int  llen = 0;

        while (g_running) {
            ssize_t r = recv(cli, buf, sizeof(buf) - 1, 0);
            if (r <= 0) break;
            buf[r] = '\0';

            for (int i = 0; i < (int)r; i++) {
                char c = buf[i];
                if (c == '\r') continue;
                if (c == '\n') {
                    line[llen] = '\0';
                    if (llen > 0 && dispatch(&g_chassis, line, cli))
                        goto client_done;
                    llen = 0;
                } else if (llen < BUF_SZ - 1) {
                    line[llen++] = c;
                }
            }
        }

    client_done:
        fmcc_stop(&g_chassis);
        close(cli);
        snprintf(g_client_ip, sizeof(g_client_ip), "none");
        fprintf(stderr, "\n[srv] client disconnected — motors stopped\n");
        print_status();
    }

    fprintf(stderr, "\n[srv] shutdown\n");
    close(srv);
    fmcc_deinit(&g_chassis);
    return 0;
}