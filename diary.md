# Design Diary - IT24102989

## Week 1: Environment Setup
- Installed WSL Ubuntu, build-essential, git, nano
- Created agentfiles/IT24102989/ folder structure

## Week 2: Initial Implementation
- Wrote first version with AUTH and QUIT only
- Faced "Address already in use" - learned to kill old process

## Week 3: Full Implementation
- Replaced with full version: SYSINFO, LISTPROC, EXEC, PUT, GET, MONITOR
- Added UDP monitoring thread
- Fixed snprintf warning

## Week 4: Testing
- All commands tested and working
- Captured screenshots for report

Design Diary — Completed Version
Session 1 — Understanding the brief (6 Oct, 22:55)
Started late, about 24 hours before the deadline, so I planned the work in hourly blocks and committed after each feature. Calculated my values from IT24102989: port 7000 + 2410 = 9410, SID 2989 reversed = 9892, token OPS-2989, log remoteops_IT24102989.log, storage ./agentfiles/IT24102989/ and files ending in _989. Put them all in one #define block at the top of agent_989.c.

Session 2 — First Agent version (6 Oct, 23:14)
Wrote the accept loop with one POSIX thread per Controller, the per-session line buffer (recv_line), AUTH, QUIT and timestamped logging with a mutex. Tested a line split over three writes ("AU" + "TH OPS-2989\nQU" + "IT\n") and six Controllers at once.

Did it compile first time? No — the first compile failed with -bash: ./agent_989: No such file or directory because I tried to run the binary before compiling. I then tried gcc Makefile_989.c -o Makefile_989, which produced fatal error: Makefile_989.c: No such file or directory — I had confused the Makefile with a C source file. Fixed by running make -f Makefile_989 (which invokes gcc correctly via the -pthread and -o flags). After that, ./agent_989 started and printed Agent listening on port 9410 (SID:9892).

Session 3 — Architecture diagram and report outline (6 Oct, 23:41)
Drew the architecture diagram and numbered the nine mandatory requirements of Section 2.2 on it, then drafted the report structure with a testing table and screenshot list.

Session 4 — Protocol commands (7 Oct, 09:09)
Obstacle: my report named handlers (cmd_exec, cmd_put, …) that my Step 1 code did not have, because every command was still inline in handle_client. Also discovered that SYSINFO returned ERR 006 UNKNOWN_COMMAND — the Step 1 code only implemented AUTH and QUIT. Rebuilt the Agent with one function per command so report and code match. Split EXEC into a whitelist-check-and-reply stage and a do_exec stage that runs only the fixed string with popen. Added PUT/GET, MONITOR with a UDP thread every 2 s, and a matching UDP receiver in the Controller.

Session 5 — Testing and screenshots (7 Oct, evening)
What I ran and what passed on my own machine:

./agent_989 — started and bound to port 9410; ss -tlnp | grep 9410 confirmed LISTEN.

AUTH: correct token → OK AUTHENTICATED SID:9892; wrong token → ERR 001 AUTH_FAILED SID:9892.

Pre-AUTH command → ERR 003 NOT_AUTHENTICATED SID:9892.

SYSINFO → OK SYSINFO 0.42 512 12345 SID:9892.

LISTPROC → OK PROCs … (20 processes).

EXEC DATE, UPTIME, DISKFREE, HOSTNAME, WHOAMI → all returned OK EXEC_RESULT …; EXEC rm → ERR 002 COMMAND_NOT_ALLOWED SID:9892.

PUT/GET round trip on a text file → md5sum matched (byte-for-byte).

MONITOR START 9500 / STOP → datagrams arrived every 2 s, then stopped cleanly.

QUIT → OK BYE SID:9892.

Five Controllers at once → all served; log showed five distinct peer ports.

Abrupt Controller kill (Ctrl+C) → Agent stayed up and continued serving.

What failed and how I fixed it:

bind: Address already in use — a previous Agent process was still running from an earlier terminal. Fixed with pkill -f agent_989 before starting the new instance.

ERR 006 UNKNOWN_COMMAND for SYSINFO — the Step 1 code only handled AUTH and QUIT. Replaced with the full implementation covering all nine mandatory commands.

Compile error expected identifier before '{' — a stray // had been accidentally placed before send_all in controller_989.c, commenting out the function. Removed the // and rebuilt.

git push rejected with "Password authentication is not supported" — GitHub no longer accepts account passwords for Git. Generated a Personal Access Token (classic) with repo scope and used it as the password.

Key decisions
Thread-per-client: simple blocking code per session, private state, one mutex for the shared log; at least 5 clients is a small number of threads.

All responses go through one reply() function that appends SID:9892, so the tag can never be missing.

EXEC whitelist is a fixed table; the client's text is compared, never passed to the shell. Output lines are joined with " | " to stay one protocol line.

Uploads limited to 10 MB; files written to the personalised storage path. Filenames from the network are used verbatim under the trusted-Controller assumption; stricter validation is noted as a limitation.

UDP monitor runs on its own detached thread with a volatile stop flag, so MONITOR STOP, QUIT, or an abrupt disconnect all stop the stream cleanly.

SIGPIPE ignored in both Agent and Controller so a dead peer cannot kill the process.

Obstacles and fixes
Report and code did not match → rebuilt the Agent with handlers named as in the report.

PUT bytes arriving in the same recv() as the command line → handle_put drains s->buf first, then loops on recv() until exactly <filesize> bytes are written.

A rejected PUT leaves file bytes in the stream → noted as a known limitation; the correct fix is to read and discard the announced bytes before replying, or to close the connection on ERR 004.

Client killed while monitoring → the Agent's session-teardown path sets mon->stop = 1 so the UDP thread exits; SIGPIPE ignored.

GitHub authentication failure after first push attempt → created a PAT (classic) with the repo scope and used it as the password for git push -u origin main.

Token accidentally pasted into chat → immediately revoked on GitHub and generated a fresh one, then stored it in a password manager rather than reusing it.
