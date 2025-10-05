# WEBTEL – Secure SSH-based Text Web Server


## 1. INTRODUCTION

WebTel is an educational project written in C that demonstrates how to
design a multi-threaded network server communicating securely through
the SSH protocol.

Unlike a typical web server (HTTP + HTML), WebTel serves plain-text
pages (.nwt files) through encrypted SSH sessions. Each connected client
interacts with the server via a command-line interface, browsing
text-based pages by typing single-letter commands such as H (home), Q
(quit), or d (documentation).

The entire communication is handled by the libssh library, which
provides the encryption, authentication, and secure session management.
WebTel focuses on the application layer: loading text pages, managing
multiple users, and logging activity.

The project was developed as part of the "Programmation Système et
Événements (PSE)" course at École des Mines de Saint-Étienne – ISMIN
campus.

---

## 2. CONCEPT AND OBJECTIVES

The goal of WebTel is to illustrate how a real server architecture works
under Linux, without relying on pre-made frameworks. The project covers
multiple system-level concepts at once:

- Secure communication via SSH (handled by libssh)
- Multithreading: one thread per connected client
- File I/O: serving .nwt text files
- Synchronization: mutexes used to protect log files
- Signal handling: clean shutdown on Ctrl+C (SIGINT)
- Logging: automatic recording of connections, pages, and commands in
  CSV files

In short: WebTel behaves like a "text-based website" accessible through
SSH instead of HTTP. Each client connects securely, navigates between
pages, and the server keeps a complete trace of activity.

---

## 3. TECHNICAL OVERVIEW

**Architecture summary:**

 Client (ssh) → Secure SSH channel → WebTel server
→ .nwt pages / CSV logs


**Main components:**

- **webteld.c**  
  Entry point of the program.  
  Initializes the SSH binding (port, key), handles signals, and accepts clients.  
  For each connection, it spawns a dedicated thread.  

- **datathread.c / datathread.h**  
  Manages a list of running threads.  
  Provides functions to initialize, join, and clean up threads at shutdown.  

- **navwebtel.c / navwebtel.h**  
  Core navigation engine: reads and sends `.nwt` pages to the client.  
  Parses user input to determine which page to load next.  
  Each command typed by the client (H, Q, d, etc.) is logged.  

- **journal.c / journal.h**  
  Responsible for thread-safe logging in CSV files.  
  Files used:  
  - `connexions.csv` → client IDs, timestamps, IP addresses  
  - `commandes.csv` → every command typed  
  - `pages.csv` → every page opened  

- **docs/webteld.1**  
  UNIX manual page describing usage, dependencies, and build instructions.

---

## 4. SECURITY (LIBSSH)

WebTel does not reimplement encryption itself. Instead, it relies on the
libssh library, which handles:  
- RSA key handshake  
- Session key negotiation  
- Channel encryption and authentication  

Each connection is therefore encrypted and authenticated in the same way
as a standard SSH session. The server uses a private key (`rsa.key`)
generated locally. This key must never be published or shared.

**To generate your own key before running WebTel:**
```bash
ssh-keygen -t rsa -b 2048 -m PEM -f rsa.key -N ""
````

---

## 5. MULTITHREADING

Each connected client is handled by a separate thread. This allows
multiple users to browse simultaneously.

Threads share access to log files, so WebTel uses POSIX mutexes to
ensure thread-safe writes. The server can gracefully stop: when SIGINT
(Ctrl+C) is received, all threads are joined and resources are freed.

---

## 6. HOW TO BUILD AND RUN

Requirements: - Linux or WSL - libssh-dev - build-essential (gcc, make)

Build: sudo apt update sudo apt install -y libssh-dev build-essential
make

Generate key: ssh-keygen -t rsa -b 2048 -m PEM -f rsa.key -N ""

Run: ./webteld 2222 rsa.key

Connect as client: ssh -p 2222 localhost

Exit the session: type Q and press Enter.

Clean: make clean

---

## 7. DIRECTORY STRUCTURE

```text
webtel/
├── src/
│   ├── webteld.c
│   ├── datathread.c/.h
│   ├── navwebtel.c/.h
│   ├── journal.c/.h
│
├── nwt/
│   ├── index.nwt
│   ├── documentation.nwt
│   ├── communication.nwt
│   ├── multithreading.nwt
│   ├── journaux.nwt
│   ├── syntaxe.nwt
│   └── structure/
│       ├── webteld.nwt
│       ├── navwebtel.nwt
│       └── journal.nwt
│
├── docs/
│   └── webteld.1
│
├── Makefile
├── .gitignore
└── README_WEBTEL.txt
````


---

## 8. EXAMPLE SESSION

**Server terminal:**


```text
./webteld 2222 rsa.key

0> Client #0 connected
1> Client #1 connected


Client terminal: ssh -p 2222 localhost

---------------------------------index.nwt---------------------------------
Welcome to WebTel!

(H) → Home
(Q) → Quit
(d) → Documentation
````
---------------------------------


