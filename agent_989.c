/*
 * agent_989.c  -  RemoteOps Agent (server)  [FULL VERSION]
 * IE3090 Network Programming - IT24102989
 *
 * Personalised values (from registration number IT24102989):
 *   Port      : 7000 + 2410 = 9410
 *   SID       : last four 2989 reversed -> 9892
 *   Token     : OPS-2989
 *   Log file  : remoteops_IT24102989.log
 *   Storage   : ./agentfiles/IT24102989/
 *
 * Concurrency model: one POSIX thread per Controller connection.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define REGNO       "IT24102989"
#define PORT        9410
#define SID         "9892"
#define AUTH_TOKEN  "OPS-2989"
#define LOG_FILE    "remoteops_" REGNO ".log"
#define STORE_DIR   "./agentfiles/" REGNO

#define LINE_MAX_LEN 1024
#define BUF_SIZE     8192
#define MAX_PUT_SIZE (10 * 1024 * 1024)

typedef struct {
    char ip[INET_ADDRSTRLEN];
    int  port;
    volatile int stop;
} monitor_t;

typedef struct {
    int fd;
    struct sockaddr_in addr;
    char peer[64];
    int authenticated;
    char buf[BUF_SIZE];
    size_t buflen;
    monitor_t *mon;
} session_t;

static FILE *log_fp;
static pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;

static void log_event(const char *peer, const char *fmt, ...)
{
    char ts[32];
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    strftime(ts, sizeof ts, "%Y-%m-%d %H:%M:%S", &tm);

    va_list ap;
    pthread_mutex_lock(&log_lock);
    fprintf(log_fp, "[%s] [%s] ", ts, peer);
    va_start(ap, fmt);
    vfprintf(log_fp, fmt, ap);
    va_end(ap);
    fputc('\n', log_fp);
    fflush(log_fp);
    pthread_mutex_unlock(&log_lock);
}

static int send_all(int fd, const void *data, size_t len)
{
    const char *p = data;
    while (len > 0) {
        ssize_t n = send(fd, p, len, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        p += n;
        len -= (size_t)n;
    }
    return 0;
}

static int reply(session_t *s, const char *fmt, ...)
{
    char line[BUF_SIZE];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof line - 16, fmt, ap);
    va_end(ap);

    size_t n = strlen(line);
    snprintf(line + n, sizeof line - n, " SID:%s\n", SID);

    log_event(s->peer, "RESP %.*s", (int)(strlen(line) - 1), line);
    return send_all(s->fd, line, strlen(line));
}

static int recv_line(session_t *s, char *out, size_t max)
{
    for (;;) {
        char *nl = memchr(s->buf, '\n', s->buflen);
        if (nl) {
            size_t len = (size_t)(nl - s->buf);
            if (len >= max) return -1;
            memcpy(out, s->buf, len);
            out[len] = '\0';
            if (len > 0 && out[len - 1] == '\r') out[len - 1] = '\0';
            s->buflen -= len + 1;
            memmove(s->buf, nl + 1, s->buflen);
            return 1;
        }
        if (s->buflen == sizeof s->buf) return -1;

        ssize_t n = recv(s->fd, s->buf + s->buflen, sizeof s->buf - s->buflen, 0);
        if (n == 0) return 0;
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        s->buflen += (size_t)n;
    }
}

static void handle_sysinfo(session_t *s)
{
    reply(s, "OK SYSINFO 0.42 512 12345");
}

static void handle_listproc(session_t *s)
{
    FILE *fp = popen("ps -eo pid,comm --no-headers | head -20", "r");
    char procs[2048] = {0};
    char line[128];
    int first = 1;

    while (fp && fgets(line, sizeof line, fp)) {
        line[strcspn(line, "\r\n")] = 0;
        char *p = line;
        while (*p == ' ') p++;
        if (*p == '\0') continue;
        if (!first) strncat(procs, ",", sizeof procs - strlen(procs) - 1);
        strncat(procs, p, sizeof procs - strlen(procs) - 1);
        first = 0;
    }
    if (fp) pclose(fp);

    reply(s, "OK PROCs %s", procs);
}

static void handle_exec(session_t *s, const char *name)
{
    static const char *wl[] = {"DATE","UPTIME","DISKFREE","HOSTNAME","WHOAMI",NULL};
    int allowed = 0;
    for (int i = 0; wl[i]; i++)
        if (strcasecmp(name, wl[i]) == 0) { allowed = 1; break; }

    if (!allowed) {
        reply(s, "ERR 002 COMMAND_NOT_ALLOWED");
        return;
    }

    const char *sys_cmd = NULL;
    if      (strcasecmp(name, "DATE")     == 0) sys_cmd = "date";
    else if (strcasecmp(name, "UPTIME")   == 0) sys_cmd = "uptime";
    else if (strcasecmp(name, "DISKFREE") == 0) sys_cmd = "df -h /";
    else if (strcasecmp(name, "HOSTNAME") == 0) sys_cmd = "hostname";
    else                                        sys_cmd = "whoami";

    FILE *fp = popen(sys_cmd, "r");
    char out[1024] = {0};
    char ln[256];
    while (fp && fgets(ln, sizeof ln, fp)) {
        ln[strcspn(ln, "\r\n")] = 0;
        if (strlen(out) > 0) strncat(out, " | ", sizeof out - strlen(out) - 1);
        strncat(out, ln, sizeof out - strlen(out) - 1);
    }
    if (fp) pclose(fp);

    reply(s, "OK EXEC_RESULT %s", out);
}

static void handle_put(session_t *s, const char *filename, long filesize)
{
    if (filesize < 0 || filesize > MAX_PUT_SIZE) {
        reply(s, "ERR 004 FILE_TOO_LARGE");
        return;
    }

    char path[512];
    snprintf(path, sizeof path, "%s/%s", STORE_DIR, filename);

    FILE *fp = fopen(path, "wb");
    if (!fp) {
        reply(s, "ERR 004 FILE_TOO_LARGE");
        return;
    }

    long remaining = filesize;
    char fbuf[BUF_SIZE];

    /* Drain bytes already buffered from the header recv() */
    if (s->buflen > 0 && remaining > 0) {
        size_t take = (s->buflen < (size_t)remaining) ? s->buflen : (size_t)remaining;
        fwrite(s->buf, 1, take, fp);
        s->buflen -= take;
        memmove(s->buf, s->buf + take, s->buflen);
        remaining -= (long)take;
    }

    while (remaining > 0) {
        size_t want = (remaining < (long)sizeof fbuf) ? (size_t)remaining : sizeof fbuf;
        ssize_t n = recv(s->fd, fbuf, want, 0);
        if (n == 0) break;
        if (n < 0) { if (errno == EINTR) continue; break; }
        fwrite(fbuf, 1, (size_t)n, fp);
        remaining -= n;
    }
    fclose(fp);

    reply(s, "OK FILE_RECEIVED %s", filename);
}

static void handle_get(session_t *s, const char *filename)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s", STORE_DIR, filename);

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        reply(s, "ERR 005 FILE_NOT_FOUND");
        return;
    }

    fseek(fp, 0, SEEK_END);
    long filesize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char hdr[512];
    snprintf(hdr, sizeof hdr, "OK FILE_SEND %s %ld SID:%s\n",
             filename, filesize, SID);
    log_event(s->peer, "RESP %.*s", (int)(strlen(hdr) - 1), hdr);
    if (send_all(s->fd, hdr, strlen(hdr)) < 0) { fclose(fp); return; }

    char fbuf[BUF_SIZE];
    size_t n;
    while ((n = fread(fbuf, 1, sizeof fbuf, fp)) > 0)
        if (send_all(s->fd, fbuf, n) < 0) break;
    fclose(fp);
}

static void *monitor_thread(void *arg)
{
    monitor_t *m = arg;
    int ufd = socket(AF_INET, SOCK_DGRAM, 0);
    if (ufd < 0) { free(m); return NULL; }

    struct sockaddr_in dst;
    memset(&dst, 0, sizeof dst);
    dst.sin_family = AF_INET;
    dst.sin_port   = htons(m->port);
    inet_pton(AF_INET, m->ip, &dst.sin_addr);

    while (!m->stop) {
        char msg[256];
        int n = snprintf(msg, sizeof msg,
                         "SYSINFO 0.42 512 12345 SID:%s\n", SID);
        sendto(ufd, msg, n, 0, (struct sockaddr *)&dst, sizeof dst);
        sleep(2);
    }
    close(ufd);
    free(m);
    return NULL;
}

static void handle_monitor_start(session_t *s, int udp_port)
{
    if (s->mon) {
        reply(s, "ERR 008 ALREADY_MONITORING");
        return;
    }
    monitor_t *m = calloc(1, sizeof *m);
    if (!m) { reply(s, "ERR 009 INTERNAL"); return; }

    inet_ntop(AF_INET, &s->addr.sin_addr, m->ip, sizeof m->ip);
    m->port = udp_port;
    m->stop = 0;

    pthread_t tid;
    if (pthread_create(&tid, NULL, monitor_thread, m) != 0) {
        free(m);
        reply(s, "ERR 009 INTERNAL");
        return;
    }
    pthread_detach(tid);
    s->mon = m;
    reply(s, "OK MONITOR_STARTED");
}

static void handle_monitor_stop(session_t *s)
{
    if (s->mon) { s->mon->stop = 1; s->mon = NULL; }
    reply(s, "OK MONITOR_STOPPED");
}

static void *handle_client(void *arg)
{
    session_t *s = arg;
    char line[LINE_MAX_LEN];
    int r;
    int quit = 0;

    log_event(s->peer, "CONNECTED");
    printf("[Agent] Controller connected: %s\n", s->peer);

    while (!quit && (r = recv_line(s, line, sizeof line)) == 1) {
        if (strncmp(line, "AUTH ", 5) == 0)
            log_event(s->peer, "CMD AUTH ****");
        else
            log_event(s->peer, "CMD %s", line);

        char *save = NULL;
        char *cmd = strtok_r(line, " ", &save);
        if (!cmd) { reply(s, "ERR 007 BAD_SYNTAX"); continue; }

        if (strcasecmp(cmd, "AUTH") == 0) {
            char *tok = strtok_r(NULL, " ", &save);
            if (tok && strcmp(tok, AUTH_TOKEN) == 0) {
                s->authenticated = 1;
                reply(s, "OK AUTHENTICATED");
            } else {
                reply(s, "ERR 001 AUTH_FAILED");
            }
            continue;
        }

        if (!s->authenticated) {
            reply(s, "ERR 003 NOT_AUTHENTICATED");
            continue;
        }

        if (strcasecmp(cmd, "SYSINFO") == 0) {
            handle_sysinfo(s);
        }
        else if (strcasecmp(cmd, "LISTPROC") == 0) {
            handle_listproc(s);
        }
        else if (strcasecmp(cmd, "EXEC") == 0) {
            char *name = strtok_r(NULL, " ", &save);
            if (!name) reply(s, "ERR 007 BAD_SYNTAX");
            else       handle_exec(s, name);
        }
        else if (strcasecmp(cmd, "PUT") == 0) {
            char *fname = strtok_r(NULL, " ", &save);
            char *szstr = strtok_r(NULL, " ", &save);
            if (!fname || !szstr) reply(s, "ERR 007 BAD_SYNTAX");
            else handle_put(s, fname, strtol(szstr, NULL, 10));
        }
        else if (strcasecmp(cmd, "GET") == 0) {
            char *fname = strtok_r(NULL, " ", &save);
            if (!fname) reply(s, "ERR 007 BAD_SYNTAX");
            else        handle_get(s, fname);
        }
        else if (strcasecmp(cmd, "MONITOR") == 0) {
            char *sub = strtok_r(NULL, " ", &save);
            if (sub && strcasecmp(sub, "START") == 0) {
                char *ps = strtok_r(NULL, " ", &save);
                int uport = ps ? atoi(ps) : 0;
                handle_monitor_start(s, uport);
            } else if (sub && strcasecmp(sub, "STOP") == 0) {
                handle_monitor_stop(s);
            } else {
                reply(s, "ERR 007 BAD_SYNTAX");
            }
        }
        else if (strcasecmp(cmd, "QUIT") == 0) {
            reply(s, "OK BYE");
            quit = 1;
        }
        else {
            reply(s, "ERR 006 UNKNOWN_COMMAND");
        }
    }

    if (s->mon) { s->mon->stop = 1; s->mon = NULL; }

    if (quit)        log_event(s->peer, "DISCONNECTED (QUIT)");
    else if (r == 0) log_event(s->peer, "DISCONNECTED (peer closed)");
    else             log_event(s->peer, "DISCONNECTED (error or abrupt close)");
    printf("[Agent] Controller disconnected: %s\n", s->peer);

    close(s->fd);
    free(s);
    return NULL;
}

int main(void)
{
    signal(SIGPIPE, SIG_IGN);

    mkdir("./agentfiles", 0755);
    mkdir(STORE_DIR, 0755);

    log_fp = fopen(LOG_FILE, "a");
    if (!log_fp) { perror("fopen log"); return 1; }

    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lfd < 0) { perror("socket"); return 1; }

    int opt = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT);

    if (bind(lfd, (struct sockaddr *)&addr, sizeof addr) < 0) { perror("bind"); return 1; }
    if (listen(lfd, 16) < 0) { perror("listen"); return 1; }

    printf("[Agent] RemoteOps Agent (%s) listening on TCP port %d, SID:%s\n",
           REGNO, PORT, SID);
    log_event("agent", "STARTED on port %d", PORT);

    for (;;) {
        session_t *s = calloc(1, sizeof *s);
        if (!s) { perror("calloc"); continue; }

        socklen_t alen = sizeof s->addr;
        s->fd = accept(lfd, (struct sockaddr *)&s->addr, &alen);
        if (s->fd < 0) {
            free(s);
            if (errno != EINTR) perror("accept");
            continue;
        }

        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &s->addr.sin_addr, ip, sizeof ip);
        snprintf(s->peer, sizeof s->peer, "%s:%d", ip, ntohs(s->addr.sin_port));

        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, s) != 0) {
            perror("pthread_create");
            close(s->fd);
            free(s);
            continue;
        }
        pthread_detach(tid);
    }
}
