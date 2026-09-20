#include "gbt_client.h"

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>

#include "cJSON.h"
#include "esp_check.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "coinbase_builder.h"
#include "gbt_fsm_parser.h"
#include "miner_job.h"
#include "mining.h"
#include "stratum_api.h"
#include "stratum_task.h"
#include "system.h"
#include "utils.h"

static const char *TAG = "gbt_client";

#define GBT_HTTP_READ_CHUNK_SIZE    (2048)
#define GBT_RPC_TIMEOUT_MS          (30000)
#define GBT_SAFETY_FALLBACK_MS      (30000)
#define GBT_LONGPOLL_TIMEOUT_MS     (65000)

static gbt_template_t s_template = { 0 };
static uint8_t s_cached_coinbase_tx[COINBASE_MAX_TX_LEN];
static size_t s_cached_coinbase_tx_len = 0;
static uint8_t s_cached_merkle_path[COINBASE_MAX_MERKLE_BRANCHES][32];
static size_t s_cached_merkle_path_count = 0;
static uint8_t s_current_merkle_root[32];
static uint8_t s_extranonce[8] = { 0 };
static uint64_t s_extranonce_counter = 0;

static TaskHandle_t s_gbt_task_handle = NULL;
static TaskHandle_t s_longpoll_task_handle = NULL;
static volatile bool s_longpoll_interrupted = false;
static volatile bool s_gbt_should_stop = false;
static char s_current_longpollid[GBT_LONGPOLLID_MAX_LEN] = { 0 };
static uint16_t s_active_pool_idx = 0;

static void serialize_block_header(uint32_t version,
                                   const uint8_t prev_hash[32],
                                   const uint8_t merkle_root[32],
                                   uint32_t ntime,
                                   uint32_t nbits,
                                   uint32_t nonce,
                                   uint8_t header[80])
{
    // Block header: 80 bytes
    // 0..3: version (LE)
    header[0] = version & 0xff;
    header[1] = (version >> 8) & 0xff;
    header[2] = (version >> 16) & 0xff;
    header[3] = (version >> 24) & 0xff;

    // 4..35: prev_hash (32 bytes)
    memcpy(header + 4, prev_hash, 32);

    // 36..67: merkle_root (32 bytes)
    memcpy(header + 36, merkle_root, 32);

    // 68..71: ntime (LE)
    header[68] = ntime & 0xff;
    header[69] = (ntime >> 8) & 0xff;
    header[70] = (ntime >> 16) & 0xff;
    header[71] = (ntime >> 24) & 0xff;

    // 72..75: nbits (LE)
    header[72] = nbits & 0xff;
    header[73] = (nbits >> 8) & 0xff;
    header[74] = (nbits >> 16) & 0xff;
    header[75] = (nbits >> 24) & 0xff;

    // 76..79: nonce (LE)
    header[76] = nonce & 0xff;
    header[77] = (nonce >> 8) & 0xff;
    header[78] = (nonce >> 16) & 0xff;
    header[79] = (nonce >> 24) & 0xff;
}

static esp_err_t http_write_all(esp_http_client_handle_t client, const char *buf, size_t len)
{
    size_t sent = 0;
    while (sent < len) {
        int ret = esp_http_client_write(client, buf + sent, len - sent);
        if (ret <= 0) {
            ESP_LOGE(TAG, "Failed writing to HTTP client (ret=%d)", ret);
            return ESP_FAIL;
        }
        sent += (size_t)ret;
    }
    return ESP_OK;
}

static esp_err_t http_write_hex_bytes(esp_http_client_handle_t client, const uint8_t *data, size_t len)
{
    char hex_chunk[1024];
    size_t chunk_bytes = sizeof(hex_chunk) / 2;

    for (size_t pos = 0; pos < len; pos += chunk_bytes) {
        size_t to_write = (len - pos > chunk_bytes) ? chunk_bytes : (len - pos);
        for (size_t i = 0; i < to_write; i++) {
            uint8_t byte = data[pos + i];
            hex_chunk[i * 2]     = hex_val_table[(byte >> 4) & 0x0f] >= 0 ? "0123456789abcdef"[(byte >> 4) & 0x0f] : '0';
            hex_chunk[i * 2 + 1] = hex_val_table[byte & 0x0f] >= 0        ? "0123456789abcdef"[byte & 0x0f] : '0';
        }
        ESP_RETURN_ON_ERROR(http_write_all(client, hex_chunk, to_write * 2), TAG, "stream hex chunk");
    }
    return ESP_OK;
}

static void gbt_longpoll_task(void *pvParameters)
{
    GlobalState *gs = (GlobalState *)pvParameters;
    char longpoll_url[160];
    char post_data[256];

    while (!s_gbt_should_stop) {
        // Wait until notified of a new longpollid
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (s_gbt_should_stop || s_current_longpollid[0] == '\0') {
            continue;
        }

        PoolConfig *pool = &gs->SYSTEM_MODULE.pools[s_active_pool_idx];
        if (!pool->url || pool->url[0] == '\0') {
            continue;
        }

        snprintf(longpoll_url, sizeof(longpoll_url), "%s:%u", pool->url, pool->port > 0 ? pool->port : 8332);
        snprintf(post_data, sizeof(post_data),
                 "{\"jsonrpc\":\"1.0\",\"id\":\"lp\",\"method\":\"getblocktemplate\",\"params\":[{\"rules\":[\"segwit\"],\"longpollid\":\"%s\"}]}",
                 s_current_longpollid);

        esp_http_client_config_t config = {
            .url = longpoll_url,
            .method = HTTP_METHOD_POST,
            .timeout_ms = GBT_LONGPOLL_TIMEOUT_MS,
        };

        esp_http_client_handle_t client = esp_http_client_init(&config);
        if (!client) {
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        if (pool->user && pool->user[0] != '\0') {
            esp_http_client_set_username(client, pool->user);
            esp_http_client_set_password(client, pool->pass ? pool->pass : "");
            esp_http_client_set_authtype(client, HTTP_AUTH_TYPE_BASIC);
        }
        esp_http_client_set_header(client, "Content-Type", "application/json");

        esp_err_t err = esp_http_client_open(client, strlen(post_data));
        if (err == ESP_OK) {
            http_write_all(client, post_data, strlen(post_data));
            esp_http_client_fetch_headers(client);

            int status = esp_http_client_get_status_code(client);
            if (status == 200) {
                ESP_LOGI(TAG, "Long-poll unblocked! New block detected on network (< 25ms)");
                s_longpoll_interrupted = true;
                if (s_gbt_task_handle) {
                    xTaskNotifyGive(s_gbt_task_handle);
                }
            }
            esp_http_client_close(client);
        }
        esp_http_client_cleanup(client);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    s_longpoll_task_handle = NULL;
    vTaskDelete(NULL);
}

bool gbt_probe_pool(GlobalState *gs, uint16_t pool_idx)
{
    if (!gs || pool_idx >= MAX_POOLS) return false;
    PoolConfig *pool = &gs->SYSTEM_MODULE.pools[pool_idx];
    if (!pool->url || pool->url[0] == '\0') return false;

    char url[160];
    snprintf(url, sizeof(url), "%s:%u", pool->url, pool->port > 0 ? pool->port : 8332);

    const char *ping_post = "{\"jsonrpc\":\"1.0\",\"id\":\"ping\",\"method\":\"getblockcount\",\"params\":[]}";
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 3000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) return false;

    if (pool->user && pool->user[0] != '\0') {
        esp_http_client_set_username(client, pool->user);
        esp_http_client_set_password(client, pool->pass ? pool->pass : "");
        esp_http_client_set_authtype(client, HTTP_AUTH_TYPE_BASIC);
    }
    esp_http_client_set_header(client, "Content-Type", "application/json");

    bool ok = false;
    if (esp_http_client_open(client, strlen(ping_post)) == ESP_OK) {
        http_write_all(client, ping_post, strlen(ping_post));
        if (esp_http_client_fetch_headers(client) >= 0) {
            int status = esp_http_client_get_status_code(client);
            if (status == 200 || status == 401) {
                ok = true;
            }
        }
        esp_http_client_close(client);
    }
    esp_http_client_cleanup(client);
    return ok;
}

esp_err_t gbt_run(GlobalState *gs, uint16_t pool_idx)
{
    if (!gs || pool_idx >= MAX_POOLS) {
        return ESP_ERR_INVALID_ARG;
    }

    PoolConfig *pool = &gs->SYSTEM_MODULE.pools[pool_idx];
    s_active_pool_idx = pool_idx;
    s_gbt_task_handle = xTaskGetCurrentTaskHandle();
    s_gbt_should_stop = false;

    // Strict validation: payout_address must decode to a valid Bitcoin script
    if (!coinbase_validate_address(pool->payout_address)) {
        ESP_LOGE(TAG, "Pool %u GBT payout address '%s' is INVALID. Aborting GBT.",
                 pool_idx, pool->payout_address ? pool->payout_address : "");
        return ESP_ERR_INVALID_ARG;
    }

    // Lazy initialization of 3.75 MB PSRAM buffers
    ESP_RETURN_ON_ERROR(gbt_template_init(&s_template), TAG, "allocate GBT PSRAM buffers");

    // Spawn background long-poll task if not already running
    if (!s_longpoll_task_handle) {
        xTaskCreate(gbt_longpoll_task, "gbt_longpoll", 4096, gs, 3, &s_longpoll_task_handle);
    }

    char rpc_url[160];
    snprintf(rpc_url, sizeof(rpc_url), "%s:%u", pool->url, pool->port > 0 ? pool->port : 8332);

    const char *gbt_req = "{\"jsonrpc\":\"1.0\",\"id\":\"gbt\",\"method\":\"getblocktemplate\",\"params\":[{\"rules\":[\"segwit\"]}]}";
    gbt_fsm_parser_t parser;

    ESP_LOGI(TAG, "Starting GBT mining against node %s (Payout: %s)", rpc_url, pool->payout_address);

    while (!stratum_reconnect_requested()) {
        gbt_template_reset(&s_template);
        gbt_fsm_parser_init(&parser);
        s_longpoll_interrupted = false;

        esp_http_client_config_t config = {
            .url = rpc_url,
            .method = HTTP_METHOD_POST,
            .timeout_ms = GBT_RPC_TIMEOUT_MS,
        };

        esp_http_client_handle_t client = esp_http_client_init(&config);
        if (!client) {
            ESP_LOGE(TAG, "Failed to init HTTP client");
            return ESP_FAIL;
        }

        if (pool->user && pool->user[0] != '\0') {
            esp_http_client_set_username(client, pool->user);
            esp_http_client_set_password(client, pool->pass ? pool->pass : "");
            esp_http_client_set_authtype(client, HTTP_AUTH_TYPE_BASIC);
        }
        esp_http_client_set_header(client, "Content-Type", "application/json");

        int64_t fetch_start_us = esp_timer_get_time();
        esp_err_t err = esp_http_client_open(client, strlen(gbt_req));
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to open GBT POST request: %s", esp_err_to_name(err));
            esp_http_client_cleanup(client);
            return err;
        }

        http_write_all(client, gbt_req, strlen(gbt_req));
        int content_length = esp_http_client_fetch_headers(client);
        (void)content_length;
        int status_code = esp_http_client_get_status_code(client);

        if (status_code != 200) {
            ESP_LOGE(TAG, "GBT request failed with HTTP %d", status_code);
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            return ESP_FAIL;
        }

        // Stream JSON directly into the streaming FSM in 2 KB stack chunks!
        uint8_t chunk[GBT_HTTP_READ_CHUNK_SIZE];
        int read_len = 0;
        while ((read_len = esp_http_client_read(client, (char *)chunk, sizeof(chunk))) > 0) {
            err = gbt_fsm_parser_feed(&parser, &s_template, chunk, (size_t)read_len);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Streaming FSM parser error: %s", parser.error_msg);
                break;
            }
        }

        esp_http_client_close(client);
        esp_http_client_cleanup(client);

        if (err != ESP_OK || s_template.height == 0 || s_template.nbits == 0) {
            ESP_LOGE(TAG, "Failed to stream complete GBT template");
            return ESP_FAIL;
        }

        int64_t elapsed_ms = (esp_timer_get_time() - fetch_start_us) / 1000;
        ESP_LOGI(TAG, "GBT Template #%" PRIu32 " streamed in %" PRId64 " ms: %zu txs, %zu raw bytes in PSRAM",
                 s_template.height, elapsed_ms, s_template.tx_count, s_template.raw_tx_len);

        // Build rolling extranonce
        s_extranonce_counter++;
        s_extranonce[0] = (uint8_t)(s_extranonce_counter & 0xff);
        s_extranonce[1] = (uint8_t)((s_extranonce_counter >> 8) & 0xff);
        s_extranonce[2] = (uint8_t)((s_extranonce_counter >> 16) & 0xff);
        s_extranonce[3] = (uint8_t)((s_extranonce_counter >> 24) & 0xff);
        s_extranonce[4] = (uint8_t)(esp_random() & 0xff);
        s_extranonce[5] = (uint8_t)((esp_random() >> 8) & 0xff);
        s_extranonce[6] = (uint8_t)((esp_random() >> 16) & 0xff);
        s_extranonce[7] = (uint8_t)((esp_random() >> 24) & 0xff);

        uint8_t coinbase_txid[32];
        err = coinbase_build_tx(s_template.height,
                                s_template.coinbase_value,
                                pool->payout_address,
                                s_template.default_witness_commitment,
                                s_extranonce,
                                sizeof(s_extranonce),
                                pool->miner_tag,
                                s_cached_coinbase_tx,
                                sizeof(s_cached_coinbase_tx),
                                &s_cached_coinbase_tx_len,
                                coinbase_txid);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed building coinbase tx");
            return err;
        }

        err = coinbase_compute_merkle_path(coinbase_txid,
                                           s_template.txid_hashes,
                                           s_template.tx_count,
                                           s_cached_merkle_path,
                                           &s_cached_merkle_path_count,
                                           s_current_merkle_root);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed computing Merkle path");
            return err;
        }

        // Notify long-poll task of new longpollid
        if (s_template.longpollid[0] != '\0') {
            strlcpy(s_current_longpollid, s_template.longpollid, sizeof(s_current_longpollid));
            if (s_longpoll_task_handle) {
                xTaskNotifyGive(s_longpoll_task_handle);
            }
        }

        // Activate job in ring buffer and notify create_jobs_task
        uint8_t target_slot = (gs->active_job_slot_idx + 1) % 2;
        miner_job_t *target_job = miner_job_get_slot(target_slot);

        target_job->type = JOB_TYPE_GBT;
        target_job->pool_id = (uint8_t)pool_idx;
        target_job->pool_diff = networkDifficulty(s_template.nbits);
        target_job->version_mask = 0x1fffe000;
        target_job->version = s_template.version;
        target_job->ntime = s_template.curtime;
        target_job->nbits = s_template.nbits;
        target_job->clean_jobs = true;

        // Convert previousblockhash hex to binary (reversed internal byte order)
        hex2bin(s_template.previous_block_hash, target_job->prev_hash, 32);
        reverse_endianness_per_word(target_job->prev_hash);

        memcpy(target_job->merkle_root, s_current_merkle_root, 32);
        strlcpy(target_job->job_id, "gbt", sizeof(target_job->job_id));

        SYSTEM_notify_new_ntime(gs, target_job->ntime);
        gs->SYSTEM_MODULE.work_received++;
        snprintf(gs->SYSTEM_MODULE.pool_connection_info, sizeof(gs->SYSTEM_MODULE.pool_connection_info),
                 "GBT: Block %" PRIu32 " (Diff %.2f)", s_template.height, target_job->pool_diff);

        if (gs->create_jobs_task_handle) {
            xTaskNotify(gs->create_jobs_task_handle, target_slot, eSetValueWithOverwrite);
        }

        // Wait for long-poll unblock or safety fallback timeout (30 seconds)
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(GBT_SAFETY_FALLBACK_MS));

        if (s_longpoll_interrupted) {
            ESP_LOGI(TAG, "Switching to next block template immediately via Long-Poll interrupt");
        }
    }

    s_gbt_should_stop = true;
    return ESP_OK;
}

esp_err_t gbt_submit_block(GlobalState *gs,
                           const bm_job *active_job,
                           uint32_t nonce,
                           uint32_t rolled_version)
{
    if (!gs || !active_job) {
        return ESP_ERR_INVALID_ARG;
    }

    PoolConfig *pool = &gs->SYSTEM_MODULE.pools[s_active_pool_idx];
    if (!pool->url || pool->url[0] == '\0') {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    ESP_LOGI(TAG, "FOUND SOLVED BLOCK ON BITCOIN NETWORK! SUBMITTING BLOCK!");
    ESP_LOGI(TAG, "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");

    char rpc_url[160];
    snprintf(rpc_url, sizeof(rpc_url), "%s:%u", pool->url, pool->port > 0 ? pool->port : 8332);

    uint8_t header[80];
    uint32_t final_version = rolled_version != 0 ? rolled_version : active_job->version;
    serialize_block_header(final_version,
                           active_job->prev_block_hash,
                           active_job->merkle_root,
                           active_job->ntime,
                           active_job->target,
                           nonce,
                           header);

    // Calculate tx count varint (1 coinbase + s_template.tx_count)
    uint8_t varint_buf[9];
    size_t total_txs = 1 + s_template.tx_count;
    size_t varint_len = 0;
    if (total_txs < 0xfd) {
        varint_buf[0] = (uint8_t)total_txs;
        varint_len = 1;
    } else if (total_txs <= 0xffff) {
        varint_buf[0] = 0xfd;
        varint_buf[1] = total_txs & 0xff;
        varint_buf[2] = (total_txs >> 8) & 0xff;
        varint_len = 3;
    } else {
        varint_buf[0] = 0xfe;
        varint_buf[1] = total_txs & 0xff;
        varint_buf[2] = (total_txs >> 8) & 0xff;
        varint_buf[3] = (total_txs >> 16) & 0xff;
        varint_buf[4] = (total_txs >> 24) & 0xff;
        varint_len = 5;
    }

    const char *prefix = "{\"jsonrpc\":\"1.0\",\"id\":\"submit\",\"method\":\"submitblock\",\"params\":[\"";
    const char *suffix = "\"]}";

    size_t block_hex_len = (80 + varint_len + s_cached_coinbase_tx_len + s_template.raw_tx_len) * 2;
    size_t content_length = strlen(prefix) + block_hex_len + strlen(suffix);

    esp_http_client_config_t config = {
        .url = rpc_url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = GBT_RPC_TIMEOUT_MS,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to init HTTP client for submitblock");
        return ESP_FAIL;
    }

    if (pool->user && pool->user[0] != '\0') {
        esp_http_client_set_username(client, pool->user);
        esp_http_client_set_password(client, pool->pass ? pool->pass : "");
        esp_http_client_set_authtype(client, HTTP_AUTH_TYPE_BASIC);
    }
    esp_http_client_set_header(client, "Content-Type", "application/json");

    esp_err_t err = esp_http_client_open(client, content_length);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open submitblock request: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    // 1. JSON prefix
    http_write_all(client, prefix, strlen(prefix));

    // 2. 80-byte winning header in hex
    http_write_hex_bytes(client, header, sizeof(header));

    // 3. Tx count varint in hex
    http_write_hex_bytes(client, varint_buf, varint_len);

    // 4. Coinbase tx in hex
    http_write_hex_bytes(client, s_cached_coinbase_tx, s_cached_coinbase_tx_len);

    // 5. Stream the entire 3.6 MB PSRAM raw tx data in 1 KB chunks directly from byte 0 to end!
    if (s_template.raw_tx_len > 0) {
        http_write_hex_bytes(client, s_template.raw_tx_data, s_template.raw_tx_len);
    }

    // 6. JSON suffix
    http_write_all(client, suffix, strlen(suffix));

    esp_http_client_fetch_headers(client);
    int status = esp_http_client_get_status_code(client);

    char response_buf[512] = { 0 };
    esp_http_client_read(client, response_buf, sizeof(response_buf) - 1);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    ESP_LOGI(TAG, "submitblock response (HTTP %d): %s", status, response_buf);

    cJSON *root = cJSON_Parse(response_buf);
    if (root) {
        cJSON *res = cJSON_GetObjectItem(root, "result");
        cJSON *err_item = cJSON_GetObjectItem(root, "error");
        if ((!res || cJSON_IsNull(res)) && (!err_item || cJSON_IsNull(err_item))) {
            ESP_LOGI(TAG, "SUCCESS! BLOCK ACCEPTED BY BITCOIN NETWORK!");
            gs->SYSTEM_MODULE.block_found = true;
            gs->SYSTEM_MODULE.shares_accepted++;
            cJSON_Delete(root);
            return ESP_OK;
        }
        cJSON_Delete(root);
    }

    return ESP_FAIL;
}

int gbt_submit_share(GlobalState *gs,
                     const bm_job *active_job,
                     uint32_t nonce,
                     uint32_t rolled_version,
                     uint64_t *sent_time_us)
{
    if (!gs || !active_job) {
        return -1;
    }

    if (sent_time_us) {
        *sent_time_us = esp_timer_get_time();
    }

    double diff = test_nonce_value(active_job, nonce, rolled_version);
    double net_diff = networkDifficulty(active_job->target);

    ESP_LOGI(TAG, "GBT Share evaluated: diff=%.2f, net_diff=%.2f, nonce=0x%08" PRIx32,
             diff, net_diff, nonce);

    if (diff >= net_diff) {
        esp_err_t err = gbt_submit_block(gs, active_job, nonce, rolled_version);
        return (err == ESP_OK) ? 0 : -1;
    }

    // Standard share does not meet network difficulty; in solo GBT mining, only full blocks are submitted
    return 0;
}
