# Reflection - IT24102989

## 1. AI Tools Used
I used Claude as a coding assistant for the RemoteOps assignment.

## 2. Where AI Helped
- WSL environment setup
- BSD socket API explanations
- Thread-per-client framework
- Byte-by-byte line framing

## 3. Where AI Got Things Wrong
The first version only had AUTH and QUIT. Had to request full implementation. Fixed snprintf truncation warning with %.127s.

## 4. What I Learned
- TCP is a byte stream, not message-based
- Partial recv() requires line buffering
- Thread-per-client concurrency
- Byte-counting in file transfers
- Thread-safe logging with pthread_mutex
