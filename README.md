httpserver.c implements a multi-threaded HTTP server using thread-pool design.
The server uses POSIX threads (pthreads) to implement multi-threading.
It also uses structs such as a queue, rwlock, and hashmap. 
