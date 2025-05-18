# Multi-threaded HTTP Server in C

A simple yet powerful multi-threaded HTTP server written in C. It demonstrates core networking concepts, threading with `pthreads`, and serves both HTML and image responses. 
Logging with timestamps is supported for easy debugging and monitoring.

---

## Features

- Multi-threaded request handling using `pthread`
- HTTP 1.1 compliant basic request parsing
- Serves HTML content and `.webp` images
- Logs all major server events with timestamps
- Graceful error handling with log messages
- Supports both IPv4 and IPv6 connections

---
## Requirements

- GCC (or any C compiler)
- POSIX-compliant system (Linux/macOS)

---

### Compile

```bash
gcc streamserver.c -o streamserver
./streamserver

```
Test the Server
Root Request
```
curl http://localhost:3490/
```
You will receive an HTML page listing the server features.

Image Request
```
curl http://localhost:3490/luffy --output luffy.webp
```
