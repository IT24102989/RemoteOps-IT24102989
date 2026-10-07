# AI Prompt Log - IT24102989

## Interaction 1
- Tool: Claude (Anthropic)
- Prompt: WSL/Ubuntu setup for C socket programming
- Output: Installation guidance
- Modifications: Followed manually

## Interaction 2
- Tool: Claude
- Prompt: TCP socket code with thread-per-client model
- Output: Initial agent framework with AUTH/QUIT
- Modifications: Expanded to full implementation

## Interaction 3
- Tool: Claude
- Prompt: Debug "ERR 006 UNKNOWN_COMMAND" for SYSINFO
- Output: Complete implementation with all handlers
- Modifications: Tested each command, fixed snprintf warning

You are an expert technical report writer for university-level Computer Science 
coursework. I need you to produce a professional, publication-quality 
Implementation Report for my IE3090 Network Programming assignment called 
"RemoteOps" (a remote system monitoring and management tool over TCP/IP).

=== ASSIGNMENT CONTEXT ===

The report must follow Section 2.7 of the IE3090 brief and meet all 
requirements for the "Protocol Implementation, Personalisation & Report" 
and "Development Process Evidence + Reflection" mark bands.

=== MY PERSONALISED VALUES (use these EXACTLY, verbatim, throughout) ===

- Registration Number: IT24102989
- Agent TCP listening port: 9410 (calculated as 7000 + first 4 digits of 
  numeric part 24102989 = 7000 + 2410)
- Session ID (SID) tag: 9892 (last four digits of registration number 
  2989, reversed)
- Authentication token: OPS-2989 ("OPS-" followed by last four digits 
  of registration number)
- Source file names: agent_989.c, controller_989.c
- Makefile name: Makefile_989
- Log file name: remoteops_IT24102989.log
- File storage path: ./agentfiles/IT24102989/
- Submission ZIP: IE3090_IT24102989.zip

=== MY IMPLEMENTATION SUMMARY ===

Language & Tools:
- C using the standard BSD sockets API (sys/socket.h, netinet/in.h, 
  arpa/inet.h)
- POSIX threads (pthread_create, pthread_detach, pthread_mutex_t)
- Compiled with gcc -Wall -Wextra -O2 -pthread
- Developed and tested on CentOS 10 (x86_64)

Concurrency model:
- Thread-per-client: main() runs a single accept() loop; each accepted 
  connection spawns a detached POSIX thread running handle_client()
- A single pthread_mutex_t log_lock serialises writes to the shared log

Protocol implementation:
- Line-based text protocol terminated by \n
- Commands: AUTH <token>, SYSINFO, LISTPROC, EXEC <name>, PUT 
  <filename> <filesize> + raw bytes, GET <filename> (returns header then 
  raw bytes), MONITOR START <udp_port>, MONITOR STOP, QUIT
- Every response ends with " SID:9892"
- Errors follow ERR <NNN> <REASON> format with codes 001–009
- EXEC whitelist: DATE, UPTIME, DISKFREE, HOSTNAME, WHOAMI (fixed, 
  hard-coded, not extensible)
- PUT limited to 10 MB, stored under ./agentfiles/IT24102989/
- UDP monitoring: detached thread sends "SYSINFO <cpu> <mem> <uptime> 
  SID:9892" every 2 seconds to the Controller's IP and requested port

Key implementation details (mention in Design Rationale):
1. Single reply() helper appends " SID:9892\n" to every response
2. Buffered line reader recv_line() uses memchr and preserves leftover 
   bytes for the next call (handles partial lines and multiple lines per 
   recv)
3. Two-phase PUT receive: Phase 1 drains s->buf, Phase 2 loops on recv() 
   until exactly <filesize> bytes written
4. GET sends header line then raw bytes with no trailing newline; 
   Controller uses recv_exact() to read the body
5. Per-session authenticated flag (no shared state between threads)
6. SIGPIPE ignored at startup in both Agent and Controller
7. Thread-safe logging with log_event() and mutex
8. Graceful disconnect handling: recv() <= 0 ends the session cleanly, 
   detached threads reclaim their own resources

=== REPORT STRUCTURE REQUIRED ===

Produce the report in the following exact structure. Every heading must 
be present. Use British English. Use professional technical writing 
throughout — no casual language. Do not invent screenshots or outputs — 
where a screenshot is needed, insert a clearly-marked placeholder like 
"[Figure X.Y: description of screenshot to insert]".

Section 1: Registration Number & Personalisation Values
- State the registration number
- Table showing every personalised value with its formula and result

Section 2: Architecture Overview
- 2.1 High-level architecture (Agent/Controller split, TCP + UDP channels)
- 2.2 Concurrency model (thread-per-client) with justified comparison 
  against fork and select/poll
- 2.3 Session model (per-connection state, authentication lifecycle)

Section 3: Protocol Implementation Evidence
- A subsection per command (AUTH, SYSINFO, LISTPROC, EXEC, PUT, GET, 
  MONITOR START/STOP, QUIT)
- Each subsection: request format, response format, and a screenshot 
  placeholder

Section 4: Personalisation Proof
- 4.1 ss -tlnp output showing port 9410
- 4.2 Log file remoteops_IT24102989.log with a snippet
- 4.3 Storage folder ./agentfiles/IT24102989/ listing
- 4.4 AUTH exchange showing token and SID

Section 5: Annotated Code Screenshots
- Subsections 5.1 to 5.10, each covering one aspect of the code with a 
  one-paragraph explanation and a screenshot placeholder
- Topics: personalised constants + session_t, listener setup, thread-per-
  client accept loop, AUTH gate, command dispatcher, SYSINFO/LISTPROC 
  handlers, EXEC whitelist, PUT two-phase receive, GET header+body, 
  recv_line framing, UDP monitor thread, log_event()

Section 6: Execution and Output Screenshots
- Subsections for Agent startup, AUTH, SYSINFO, LISTPROC, EXEC DATE, 
  EXEC rm rejected, PUT success, GET success with md5sum match, MONITOR 
  START/STOP, QUIT, log file, ss output, storage folder
- Include a subsection 6.7 for Five Simultaneous Controllers
- Include a subsection 6.8 for error cases

Section 7: Testing Summary
- A table of at least 22 tests with columns: #, Test, Expected result, 
  Actual result
- Cover: pre-AUTH rejection, wrong token, correct AUTH, SYSINFO, 
  LISTPROC, all five EXEC whitelist commands, EXEC disallowed, PUT small 
  file, GET with md5sum match, GET missing file, PUT oversize, MONITOR 
  START/STOP, unknown command, partial line, multiple commands in one 
  segment, five concurrent Controllers, abrupt disconnect, clean QUIT, 
  log inspection, port binding verification, personalised path 
  verification
- Add a summary sub-table showing pass counts by category

Section 8: Design Rationale and Assumptions
- 8.1 Key design decisions (as bullet points, covering: single reply() 
  helper, buffered line reader, two-phase PUT, byte-exact GET, whitelist 
  table, error code table, monitor thread with stop flag, thread-safe 
  logging, per-thread auth state, detached threads, SIGPIPE ignored, 
  thread-per-client choice)
- 8.2 Assumptions (trusted Controllers, Linux target, port free, 
  simulated SYSINFO, filenames assumed safe, PUT ≤ 10 MB, 2-second 
  monitoring interval, ≤ 10 concurrent Controllers, SIGPIPE ignored)
- 8.3 Known Limitations (plain TCP, no auth rate limit, no thread cap, 
  rejected PUT not drained, no path-traversal validation, LISTPROC 
  capped at 20, UDP unreliable, Linux/IPv4 only, simulated SYSINFO)

Section 9: Optional Extension and Limitations
- State clearly which §2.6 optional extension was or was not implemented
- List known limitations

Section 10: Build and Run Instructions
- Prerequisites (gcc, make)
- Build command: make -f Makefile_989
- Run commands for Agent and Controller
- Manual compile fallback
- Runtime files created
- Verification commands
- Common issues table

Section 11: References
- Assignment brief
- POSIX socket API standard
- Man pages
- Stevens, Fenner, Rudoff (2004)
- Kerrisk (2010)

=== STYLE REQUIREMENTS ===

- Write in formal academic English
- Use third person ("the Agent", "the Controller") not first person 
  ("I", "my")
- Every claim must be specific and technical — no vague statements
- Every section must reference the actual protocol behaviour or code 
  structure
- Use tables where comparison is appropriate
- Use numbered or bulleted lists for enumerations
- Do not use emojis, exclamation marks, or casual phrasing
- Do not fabricate screenshots, outputs, or test results — use clearly 
  marked placeholders
- Each figure and table must have a numbered caption

=== OUTPUT FORMAT ===

Produce the full report as a single Markdown document with proper 
headings. Do not add any commentary before or after the report. Start 
directly with the report title.

The report should be thorough enough to satisfy a university-level 
assessment looking for evidence of correct protocol implementation, 
personalisation, concurrency handling, file-transfer integrity, error 
handling, and process evidence. Aim for a report that would score in 
the top mark band.
