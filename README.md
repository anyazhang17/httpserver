This is a multi-threaded HTTP server that manages concurrency through synchronization. The aim is to improve hardware utilization by allowing the server to handle multiple client requests simultaneously. The POSIX threads library (pthreads) is used for multithreading implementation.

The project integrates a thread-safe queue and reader-writer locks into the HTTP server. This enables the server to serve multiple clients concurrently, thereby improving throughput, depending on the design.

The server ensurse that responses to client requests maintain coherence and atomicity, ensuring that its behavior is indistinguishable from a single-threaded server from an external perspective. Additionally, the server generates an audit log to identify the linearization of its operations.

To implement concurrency, the server should adopt a thread-pool design, which consists of worker threads for processing requests and a dispatcher thread for managing connections. The dispatcher thread dispatches requests to the worker threads, typically by utilizing a thread-safe queue.
