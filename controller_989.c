/*
 * controller_989.c  -  RemoteOps Controller (client) [FULL]
 * IE3090 Network Programming - IT24102989
 * Usage: ./controller_989 [agent_ip] [port]
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define DEFAULT_PORT 9410
#define AUTH_TOKEN   "OPS-2989"
#define BUF_SIZE     8192

static int  sock;
static char rbuf[BUF_SIZE];
static size_t rlen;

static int send_all(int fd, const void *data, size_t len)
{
    const char *p = data;
    while (len > 0) {
        ssize_t n = send(fd, p, len, MSG_NOSIGNAL);
        if (n < 0) { if (errno == EINTR) continue; return -1; }
        p += n; len -= (size_t)n;
    }
    return 0;
}

static int recv_line(char *out, size_t max)
{
    for (;;) {
        char *nl = memchr(rbuf, '\n', rlen);
        if (nl) {
            size_t len = (size_t)(nl - rbuf);
            if (len >= max) return -1;
            memcpy(out, rbuf, len);
            out[len] = '\0';
            rlen -= len + 1;
            memmove(rbuf, nl + 1, rlen);
            return 1;
        }
        if (rlen == sizeof rbuf) return -1;
        ssize_t n = recv(sock, rbuf + rlen, sizeof rbuf - rlen, 0);
        if (n == 0) return 0;
        if (n < 0) { if (errno == EINTR) continue; return -1; }
        rlen += (size_t)n;
    }
}

static int recv_exact(void *dst, size_t want)
{
    char *p = dst;
    if (rlen > 0) {
        size_t take = (rlen < want) ? rlen : want;
        memcpy(p, rbuf, take);
        rlen -= take;
        memmove(rbuf, rbuf + take, rlen);
        p += take; want -= take;
    }
    while (want > 0) {
        ssize_t n = recv(sock, p, want, 0);
        if (n <= 0) return -1;
        p += n; want -= (size_t)n;
    }
    return 0;
}

static void do_put(void)
{
    char path[256], fname[128];
    printf("Local file path: "); fflush(stdout);
    if (!fgets(path, sizeof path, stdin)) return;
    path[strcspn(path, "\r\n")] = 0;

    printf("Remote name: "); fflush(stdout);
    if (!fgets(fname, sizeof fname, stdin)) return;
    fname[strcspn(fname, "\r\n")] = 0;
    if (fname[0] == '\0') {
        const char *slash = strrchr(path, '/');
        snprintf(fname, sizeof fname, "%.127s", slash ? slash + 1 : path);
    }

    FILE *fp = fopen(path, "rb");
    if (!fp) { perror("fopen"); return; }
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char hdr[256];
    snprintf(hdr, sizeof hdr, "PUT %s %ld\n", fname, sz);
    if (send_all(sock, hdr, strlen(hdr)) < 0) { fclose(fp); return; }

    char fbuf[BUF_SIZE]; size_t n;
    while ((n = fread(fbuf, 1, sizeof fbuf, fp)) > 0)
        send_all(sock, fbuf, n);
    fclose(fp);

    char resp[BUF_SIZE];
    if (recv_line(resp, sizeof resp) > 0) printf("%s\n", resp);
}

static void do_get(void)
{
    char fname[128], outpath[256];
    printf("Remote name: "); fflush(stdout);
    if (!fgets(fname, sizeof fname, stdin)) return;
    fname[strcspn(fname, "\r\n")] = 0;

    char cmd[256];
    snprintf(cmd, sizeof cmd, "GET %s\n", fname);
    if (send_all(sock, cmd, strlen(cmd)) < 0) return;

    char resp[BUF_SIZE];
    if (recv_line(resp, sizeof resp) <= 0) return;
    printf("%s\n", resp);

    if (strncmp(resp, "OK FILE_SEND", 12) != 0) return;

    char nm[128] = {0}; long sz = 0;
    sscanf(resp, "OK FILE_SEND %127s %ld", nm, &sz);

    printf("Save as: "); fflush(stdout);
    if (!fgets(outpath, sizeof outpath, stdin)) return;
    outpath[strcspn(outpath, "\r\n")] = 0;
    if (outpath[0] == '\0') snprintf(outpath, sizeof outpath, "downloaded_%s", nm);

    FILE *fp = fopen(outpath, "wb");
    if (!fp) { perror("fopen"); return; }

    long remaining = sz;
    char fbuf[BUF_SIZE];
    while (remaining > 0) {
        size_t want = (remaining < (long)sizeof fbuf) ? (size_t)remaining : sizeof fbuf;
        if (recv_exact(fbuf, want) < 0) break;
        fwrite(fbuf, 1, want, fp);
        remaining -= (long)want;
    }
    fclose(fp);
    printf("[+] Saved %ld bytes to %s\n", sz, outpath);
}

int main(int argc, char *argv[])
{
    const char *host = argc > 1 ? argv[1] : "127.0.0.1";
    int port = argc > 2 ? atoi(argv[2]) : DEFAULT_PORT;

    signal(SIGPIPE, SIG_IGN);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
        fprintf(stderr, "Invalid IP: %s\n", host); return 1;
    }
    if (connect(sock, (struct sockaddr *)&addr, sizeof addr) < 0) {
        perror("connect"); return 1;
    }
    printf("Connected to Agent %s:%d\n", host, port);

    char auth[128];
    snprintf(auth, sizeof auth, "AUTH %s\n", AUTH_TOKEN);
    send_all(sock, auth, strlen(auth));
    char resp[BUF_SIZE];
    if (recv_line(resp, sizeof resp) > 0) printf("%s\n", resp);

    printf("\n===== RemoteOps Menu =====\n");
    printf("1. SYSINFO\n");
    printf("2. LISTPROC\n");
    printf("3. EXEC <cmd>\n");
    printf("4. PUT <file>\n");
    printf("5. GET <file>\n");
    printf("6. MONITOR START <port>\n");
    printf("7. MONITOR STOP\n");
    printf("8. QUIT\n");
    printf("9. Raw command\n");

    char cmd[1024];
    for (;;) {
        printf("\nremoteops> "); fflush(stdout);
        if (!fgets(cmd, sizeof cmd, stdin)) break;
        cmd[strcspn(cmd, "\r\n")] = 0;
        if (cmd[0] == '\0') continue;

        if (strcmp(cmd, "1") == 0) strcpy(cmd, "SYSINFO");
        else if (strcmp(cmd, "2") == 0) strcpy(cmd, "LISTPROC");
        else if (strcmp(cmd, "3") == 0) {
            char name[64];
            printf("Command (DATE/UPTIME/DISKFREE/HOSTNAME/WHOAMI): ");
            fflush(stdout);
            if (!fgets(name, sizeof name, stdin)) continue;
            name[strcspn(name, "\r\n")] = 0;
            snprintf(cmd, sizeof cmd, "EXEC %s", name);
        }
        else if (strcmp(cmd, "4") == 0) { do_put(); continue; }
        else if (strcmp(cmd, "5") == 0) { do_get(); continue; }
        else if (strcmp(cmd, "6") == 0) {
            char p[16];
            printf("UDP port to receive on: "); fflush(stdout);
            if (!fgets(p, sizeof p, stdin)) continue;
            p[strcspn(p, "\r\n")] = 0;
            snprintf(cmd, sizeof cmd, "MONITOR START %s", p);
        }
        else if (strcmp(cmd, "7") == 0) strcpy(cmd, "MONITOR STOP");
        else if (strcmp(cmd, "8") == 0) strcpy(cmd, "QUIT");

        size_t len = strlen(cmd);
        cmd[len] = '\n';
        if (send_all(sock, cmd, len + 1) < 0) { perror("send"); break; }
        cmd[len] = '\0';

        int r = recv_line(resp, sizeof resp);
        if (r <= 0) { printf("Connection closed.\n"); break; }
        printf("%s\n", resp);
        if (strncmp(resp, "OK BYE", 6) == 0) break;
    }
    close(sock);
    return 0;
}
