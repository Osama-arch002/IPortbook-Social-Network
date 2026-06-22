# IPortbook: Hybrid Custom Social Network Architecture

A custom, multi-threaded social network platform implemented completely from scratch in C. The system relies on a central server managing up to 100 synchronized client profiles, executing communication via a custom application-layer protocol designed over a hybrid TCP/UDP architecture.

---

## 🚀 Key Architectural Features

* **Multi-Threaded Server Kernel:** Utilizes POSIX threads (`pthread`) to achieve independent client connection handling, leveraging detached worker threads and thread-safe shared memory managed through mutual exclusion locks (`pthread_mutex_t`).
* **Hybrid Application-Layer Protocol:** * **Direct TCP Streams:** Handles robust state mutations including account registration (`REGIS`), authentication (`CONNE`), profile synchronization, and real-time point-to-point text messaging.
  * **Asynchronous UDP Notifier Engine:** Automatically pushes real-time event notifications (direct messages, friend actions, and broadcasts) directly to autonomous client listening threads via lightweight 3-byte hexadecimal frames (`[YXX]`).
* **Network-Aware Dynamic Routing:** Engineered with a custom thread-passing data envelope that dynamically extracts incoming client source IPs from low-level TCP headers, decoupling execution from loopback or hardcoded environments.
* **Graph-Traversal Data Flooding:** Implements a state-tracked recursive graph search algorithm to handle cascading message broadcasts across a user's multi-tier relational connection web while preventing infinite routing loops.
* **Custom Binary Memory Serialization:** Packs mixed alphanumeric metadata alongside exact 2-byte binary integer payloads directly into byte streams via bitwise `memcpy` block copying, achieving total architectural compliance with fixed Little-Endian memory models.

---

## 💻 Getting Started

### Prerequisites
* GCC Compiler
* POSIX Threads compatible operating system (Linux, macOS)

### Compilation
Build both target executables cleanly using the automated Makefile assembly layer:
```bash
make
