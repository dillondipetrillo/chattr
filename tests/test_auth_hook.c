#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "auth_hook.h"
#include "config.h"
#include "test_runner.h"

// Helpers
static struct auth_request make_req(const char *token, int fd)
{
    struct auth_request r;
    r.token = token;
    r.token_len = strlen(token);
    r.conn_fd = fd;
    return r;
}

// Default hook
static void test_default_accepts_any(void)
{
    printf("\n-- default_auth_hook --\n");
    struct auth_result r = default_auth_hook(make_req("anything", 7));
    ASSERT(r.valid == 1, "default hook: accepts any token");
    ASSERT(r.user_id == 7, "default hook: user_id equals conn_fd");
}

static void test_default_empty_token(void)
{
    struct auth_result r = default_auth_hook(make_req("", 3));
    ASSERT(r.valid == 1, "default hook: accepts empty token (dev mode)");
}

// JWT hook
static void test_jwt_no_secret(void)
{
    printf("\n-- jwt_auth_hook --\n");
    unsetenv("ENGINE_AUTH_JWT_SECRET");
    struct auth_result r = jwt_auth_hook(make_req("a.b.c", 1));
    ASSERT(r.valid == 0, "jwt hook: rejects when no secret set");
}

static void test_jwt_malformed(void)
{
    setenv("ENGINE_AUTH_JWT_SECRET", "secret", 1);
    struct auth_result r = jwt_auth_hook(make_req("notajwt", 1));
    ASSERT(r.valid == 0, "jwt hook: rejects malformed token");
    unsetenv("ENGINE_AUTH_JWT_SECRET");
}

static void test_jwt_wrong_signature(void)
{
    setenv("ENGINE_AUTH_JWT_SECRET", "correct-secret", 1);
    // JWT signed with "wrong-secret" - valid structure, wrong MAC
    const char *bad =
        "eyJhbGciOiJIUzI1NiJ9"
        ".eyJzdWIiOiIxIiwiZXhwIjo5OTk5OTk5OTk5fQ"
        ".AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
    struct auth_result r = jwt_auth_hook(make_req(bad, 1));
    ASSERT(r.valid == 0, "jwt hook: rejects wrong signature");
    unsetenv("ENGINE_AUTH_JWT_SECRET");
}

// HTTP hook
static void test_http_no_url(void)
{
    printf("\n-- http_auth_hook --\n");
    unsetenv("ENGINE_AUTH_HTTP_URL");
    struct auth_result r = http_auth_hook(make_req("tok", 1));
    ASSERT(r.valid == 0, "http hook: rejects when no URL configured");
}

static void test_http_unreachable(void)
{
    // Port 19999 should have nothing listening
    setenv("ENGINE_AUTH_HTTP_URL", "http://127.0.0.1:19999/validate", 1);
    setenv("ENGINE_AUTH_HTTP_TIMEOUT_MS", "100", 1);
    struct auth_result r = http_auth_hook(make_req("tok", 1));
    ASSERT(r.valid == 0, "http hook: rejects when server unreachable");
    unsetenv("ENGINE_AUTH_HTTP_URL");
    unsetenv("ENGINE_AUTH_HTTP_TIMEOUT_MS");
}

// Select auth hook
static void test_select_jwt_priority(void)
{
    printf("\n-- select_auth_hook --\n");
    struct engine_config cfg;
    memset(&cfg, 0, sizeof(cfg));
    strncpy(cfg.auth_jwt_secret, "s", sizeof(cfg.auth_jwt_secret) - 1);
    strncpy(cfg.auth_http_url, "http://x/v", sizeof(cfg.auth_http_url) - 1);

    auth_hook_fn fn = select_auth_hook(&cfg);
    ASSERT(fn == jwt_auth_hook, "selector: JWT wins over HTTP when both set");
}

static void test_select_http_only(void)
{
    struct engine_config cfg;
    memset(&cfg, 0, sizeof(cfg));
    strncpy(cfg.auth_http_url, "http://x/v", sizeof(cfg.auth_http_url) - 1);

    auth_hook_fn fn = select_auth_hook(&cfg);
    ASSERT(fn == http_auth_hook,
        "selector: HTTP selected when only HTTP configured");
}

static void test_select_dev_mode(void)
{
    struct engine_config cfg;
    memset(&cfg, 0, sizeof(cfg));
    auth_hook_fn fn = select_auth_hook(&cfg);
    ASSERT(fn == default_auth_hook,
        "selector: dev hook selected when nothing configured");
}

// OpenSSL
static void test_hmac_known_answer(void)
{
    printf("\n-- HMAC-SHA256 via OpenSSL --\n");
    /**
     * Known test vector from RFC 4231.
     * Key: "Jefe"
     * Message: "what do ya want for nothing?"
     * Expected HMAC-SHA256 (hex):
     *  5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964a71b0
     */
    const uint8_t key[] = "Jefe";
    const uint8_t msg[] = "what do ya want for nothing?";
    const uint8_t expected[32] = {
        0x5b,0xdc,0xc1,0x46,0xbf,0x60,0x75,0x4e,
        0x6a,0x04,0x24,0x26,0x08,0x95,0x75,0xc7,
        0x5a,0x00,0x3f,0x08,0x9d,0x27,0x39,0x83,
        0x9d,0xec,0x58,0xb9,0x64,0xec,0x38,0x43
    };
    uint8_t output[32];
    int ok = compute_hmac_sha256(key, 4, msg, 28, output);
    ASSERT(ok == 1, "OpenSSL HMAC: computation succeeds");
    ASSERT(memcmp(output, expected, 32) == 0,
        "OpenSSL HMAC: known-answer test vector matches RFC 4231");
}

int main(void)
{
    printf("=== Auth Hook Unit Tests ===\n");
    test_default_accepts_any();
    test_default_empty_token();
    test_jwt_no_secret();
    test_jwt_malformed();
    test_jwt_wrong_signature();
    test_http_no_url();
    test_http_unreachable();
    test_select_jwt_priority();
    test_select_http_only();
    test_select_dev_mode();
    test_hmac_known_answer();
    TEST_SUMMARY();
}