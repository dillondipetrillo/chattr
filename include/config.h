#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

struct engine_config {
    int port;                   // which port to bind to
    int health_port;            // health check port (default 8081)
    int max_clients;            // maximum simultaneous connections
    int max_events;             // epoll_wait max events per call
    int worker_count;           // number of worker processes (0 = auto)
    int backlog;                // listen() backlog queue size
    int conn_rate_limit;        // max new connections per IP per second
    int send_buf_size;          // per-socket send buffer bytes
    int recv_buf_size;          // per-socket receive buffer bytes
    int tcp_keepalive_idle;     // seconds before keepalive probes start
    int tcp_keepalive_intvl;    // seconds between keepalive probes
    int tcp_keepalive_cnt;      // probes before declaring dead
    char log_path[256];         // path to log file
    char auth_socket[256];      // unix socket path for auth sidecar
                                // empty string = use default hook (accept all)
};

// Load configuration from environment variables with compiled=in defaults
struct engine_config config_load(void);

// Log all configuration values at startup
void config_log(const struct engine_config *config);

#endif