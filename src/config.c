#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "logger.h"

struct engine_config config_load(void)
{
    struct engine_config config;
    config.port = getenv("ENGINE_PORT") ? atoi(getenv("ENGINE_PORT")) : 8080;
    config.health_port = getenv("ENGINE_HEALTH_PORT") ?
        atoi(getenv("ENGINE_HEALTH_PORT")) : 8081;
    config.max_clients = getenv("ENGINE_MAX_CLIENTS") ?
        atoi(getenv("ENGINE_MAX_CLIENTS")) : 10000;
    config.max_events = getenv("ENGINE_MAX_EVENTS") ?
        atoi(getenv("ENGINE_MAX_EVENTS")) : 1024;
    config.worker_count = getenv("ENGINE_WORKER_COUNT") ?
        atoi(getenv("ENGINE_WORKER_COUNT")) : 0; // 0 = auto-detect CPU count
    config.backlog = getenv("ENGINE_BACKLOG") ?
        atoi(getenv("ENGINE_BACKLOG")) : 128;
    config.conn_rate_limit = getenv("ENGINE_CONN_RATE_LIMIT") ?
        atoi(getenv("ENGINE_CONN_RATE_LIMIT")) : 10;
    config.send_buf_size = getenv("ENGINE_SEND_BUF_SIZE") ?
        atoi(getenv("ENGINE_SEND_BUF_SIZE")) : 262144; // 256KB
    config.recv_buf_size = getenv("ENGINE_RECV_BUF_SIZE") ?
        atoi(getenv("ENGINE_RECV_BUF_SIZE")) : 262144; // 256KB
    config.tcp_keepalive_idle = getenv("ENGINE_TCP_KEEPALIVE_IDLE") ?
        atoi(getenv("ENGINE_TCP_KEEPALIVE_IDLE")) : 60;
    config.tcp_keepalive_intvl = getenv("ENGINE_TCP_KEEPALIVE_INTVL") ?
        atoi(getenv("ENGINE_TCP_KEEPALIVE_INTVL")) : 10;
    config.tcp_keepalive_cnt = getenv("ENGINE_TCP_KEEPALIVE_CNT") ?
        atoi(getenv("ENGINE_TCP_KEEPALIVE_CNT")) : 3;

    char *env_log = getenv("ENGINE_LOG_PATH") ?
        getenv("ENGINE_LOG_PATH") : "logs/server.log";
    char *env_auth_socket = getenv("ENGINE_AUTH_SOCKET") ?
        getenv("ENGINE_AUTH_SOCKET") : "";

    snprintf(config.log_path, sizeof(config.log_path), "%s", env_log);
    snprintf(config.auth_socket, sizeof(config.auth_socket), "%s",
        env_auth_socket);

    return config;
}

void config_log(const struct engine_config *config)
{
    log_info("=== Config setup ===");
    log_info("Network       | Port: %d, Health: %d, Backlog: %d",
        config->port, config-> health_port, config->backlog);

    log_info("Resources     | Max Clients: %d, Max Events: %d, Workers: %d",
        config->max_clients, config->max_events, config->worker_count);

    log_info("TCP Tuning    | SendBuf: %d, RecvBuf: %d, RateLimit: %d/s",
        config->send_buf_size, config->recv_buf_size, config->conn_rate_limit);

    log_info("Keepalive     | Idle: %ds, Intvl: %ds, Cnt: %d", 
        config->tcp_keepalive_idle, config->tcp_keepalive_intvl,
        config->tcp_keepalive_cnt);

    log_info("Paths         | Log: %s", config->log_path);

    // Explicitly check for the empty string sidecar
    log_info("Sidecar       | Auth Socket: %s", 
        (config->auth_socket[0] == '\0') ?
        "DISABLED (using default hook)" : config->auth_socket);

    log_info("====================");
}