#include "gbt_fsm_parser.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "utils.h"

static const char *TAG = "gbt_fsm";

static void decode_txid_hex_to_internal_bin(const char *hex_str, uint8_t dest[32])
{
    // Bitcoin txid in JSON RPC is big-endian display hex.
    // Internal hash order for Merkle tree is little-endian bytes (reversed).
    for (int i = 0; i < 32; i++) {
        int byte_val = hex_decode_byte(&hex_str[i * 2]);
        dest[31 - i] = (byte_val >= 0) ? (uint8_t)byte_val : 0;
    }
}

esp_err_t gbt_template_init(gbt_template_t *tmpl)
{
    if (!tmpl) {
        return ESP_ERR_INVALID_ARG;
    }
    if (tmpl->allocated) {
        return ESP_OK;
    }

    size_t raw_cap = tmpl->raw_tx_cap ? tmpl->raw_tx_cap : GBT_MAX_RAW_TX_BYTES;
    size_t tx_cap = tmpl->tx_cap ? tmpl->tx_cap : GBT_MAX_TX_COUNT;

    memset(tmpl, 0, sizeof(*tmpl));

    tmpl->raw_tx_cap = raw_cap;
    tmpl->tx_cap = tx_cap;

#if CONFIG_SPIRAM
    if (esp_psram_is_initialized()) {
        tmpl->raw_tx_data = (uint8_t *)heap_caps_malloc(tmpl->raw_tx_cap, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        tmpl->txid_hashes = (uint8_t *)heap_caps_malloc(tmpl->tx_cap * 32, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        tmpl->tx_offsets  = (uint32_t *)heap_caps_malloc(tmpl->tx_cap * sizeof(uint32_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        tmpl->tx_lengths  = (uint32_t *)heap_caps_malloc(tmpl->tx_cap * sizeof(uint32_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
#endif

    if (!tmpl->raw_tx_data || !tmpl->txid_hashes || !tmpl->tx_offsets || !tmpl->tx_lengths) {
        // Fallback for host unit tests
        if (!tmpl->raw_tx_data) tmpl->raw_tx_data = (uint8_t *)malloc(tmpl->raw_tx_cap);
        if (!tmpl->txid_hashes) tmpl->txid_hashes = (uint8_t *)malloc(tmpl->tx_cap * 32);
        if (!tmpl->tx_offsets)  tmpl->tx_offsets  = (uint32_t *)malloc(tmpl->tx_cap * sizeof(uint32_t));
        if (!tmpl->tx_lengths)  tmpl->tx_lengths  = (uint32_t *)malloc(tmpl->tx_cap * sizeof(uint32_t));
    }

    if (!tmpl->raw_tx_data || !tmpl->txid_hashes || !tmpl->tx_offsets || !tmpl->tx_lengths) {
        ESP_LOGE(TAG, "Failed to allocate GBT template PSRAM buffers (~3.75 MB required)");
        gbt_template_free(tmpl);
        return ESP_ERR_NO_MEM;
    }

    tmpl->allocated = true;
    ESP_LOGI(TAG, "GBT template PSRAM buffers allocated successfully (~3.8 MB)");
    return ESP_OK;
}

void gbt_template_reset(gbt_template_t *tmpl)
{
    if (!tmpl) return;

    tmpl->version = 0;
    tmpl->nbits = 0;
    tmpl->curtime = 0;
    tmpl->height = 0;
    tmpl->coinbase_value = 0;
    tmpl->previous_block_hash[0] = '\0';
    tmpl->default_witness_commitment[0] = '\0';
    tmpl->longpollid[0] = '\0';

    tmpl->raw_tx_len = 0;
    tmpl->tx_count = 0;
}

void gbt_template_free(gbt_template_t *tmpl)
{
    if (!tmpl) return;

    free(tmpl->raw_tx_data);
    free(tmpl->txid_hashes);
    free(tmpl->tx_offsets);
    free(tmpl->tx_lengths);

    tmpl->raw_tx_data = NULL;
    tmpl->txid_hashes = NULL;
    tmpl->tx_offsets = NULL;
    tmpl->tx_lengths = NULL;
    tmpl->allocated = false;
}

void gbt_fsm_parser_init(gbt_fsm_parser_t *parser)
{
    if (!parser) return;
    memset(parser, 0, sizeof(*parser));
    parser->state = GBT_FSM_SEEK_KEY;
}

enum {
    KEY_NONE = 0,
    KEY_VERSION,
    KEY_BITS,
    KEY_CURTIME,
    KEY_HEIGHT,
    KEY_COINBASEVALUE,
    KEY_PREVBLOCKHASH,
    KEY_WITNESSCOMMITMENT,
    KEY_LONGPOLLID,
    KEY_TRANSACTIONS,
    KEY_TX_DATA,
    KEY_TX_TXID,
};

static int identify_key(const char *key)
{
    if (strcmp(key, "version") == 0) return KEY_VERSION;
    if (strcmp(key, "bits") == 0) return KEY_BITS;
    if (strcmp(key, "curtime") == 0) return KEY_CURTIME;
    if (strcmp(key, "height") == 0) return KEY_HEIGHT;
    if (strcmp(key, "coinbasevalue") == 0) return KEY_COINBASEVALUE;
    if (strcmp(key, "previousblockhash") == 0) return KEY_PREVBLOCKHASH;
    if (strcmp(key, "default_witness_commitment") == 0) return KEY_WITNESSCOMMITMENT;
    if (strcmp(key, "longpollid") == 0) return KEY_LONGPOLLID;
    if (strcmp(key, "transactions") == 0) return KEY_TRANSACTIONS;
    if (strcmp(key, "data") == 0) return KEY_TX_DATA;
    if (strcmp(key, "txid") == 0) return KEY_TX_TXID;
    return KEY_NONE;
}

static void apply_scalar_value(gbt_template_t *tmpl, int key_id, const char *val)
{
    switch (key_id) {
        case KEY_VERSION:
            tmpl->version = (uint32_t)strtoul(val, NULL, 10);
            break;
        case KEY_BITS:
            tmpl->nbits = (uint32_t)strtoul(val, NULL, 16);
            break;
        case KEY_CURTIME:
            tmpl->curtime = (uint32_t)strtoul(val, NULL, 10);
            break;
        case KEY_HEIGHT:
            tmpl->height = (uint32_t)strtoul(val, NULL, 10);
            break;
        case KEY_COINBASEVALUE:
            tmpl->coinbase_value = (uint64_t)strtoull(val, NULL, 10);
            break;
        case KEY_PREVBLOCKHASH:
            strlcpy(tmpl->previous_block_hash, val, sizeof(tmpl->previous_block_hash));
            break;
        case KEY_WITNESSCOMMITMENT:
            strlcpy(tmpl->default_witness_commitment, val, sizeof(tmpl->default_witness_commitment));
            break;
        case KEY_LONGPOLLID:
            strlcpy(tmpl->longpollid, val, sizeof(tmpl->longpollid));
            break;
        default:
            break;
    }
}

esp_err_t gbt_fsm_parser_feed(gbt_fsm_parser_t *parser,
                             gbt_template_t *tmpl,
                             const uint8_t *chunk,
                             size_t chunk_len)
{
    if (!parser || !tmpl || !chunk) {
        return ESP_ERR_INVALID_ARG;
    }
    if (parser->has_error) {
        return ESP_FAIL;
    }

    for (size_t i = 0; i < chunk_len; i++) {
        char c = (char)chunk[i];

        switch (parser->state) {
            case GBT_FSM_SEEK_KEY: {
                if (c == '"') {
                    parser->key_len = 0;
                    parser->state = GBT_FSM_READ_KEY;
                } else if (c == '{') {
                    parser->brace_depth++;
                } else if (c == '}') {
                    if (parser->brace_depth > 0) parser->brace_depth--;
                } else if (c == '[') {
                    parser->bracket_depth++;
                } else if (c == ']') {
                    if (parser->bracket_depth > 0) parser->bracket_depth--;
                }
                break;
            }

            case GBT_FSM_READ_KEY: {
                if (c == '"') {
                    parser->key_buf[parser->key_len] = '\0';
                    parser->active_key_id = identify_key(parser->key_buf);
                    parser->state = GBT_FSM_AFTER_KEY;
                } else {
                    if (parser->key_len < sizeof(parser->key_buf) - 1) {
                        parser->key_buf[parser->key_len++] = c;
                    }
                }
                break;
            }

            case GBT_FSM_AFTER_KEY: {
                if (c == ':') {
                    if (parser->active_key_id == KEY_TRANSACTIONS) {
                        parser->state = GBT_FSM_IN_TX_ARRAY;
                    } else if (parser->active_key_id != KEY_NONE) {
                        parser->val_len = 0;
                        parser->state = GBT_FSM_PARSE_VALUE;
                    } else {
                        parser->state = GBT_FSM_SKIP_VALUE;
                    }
                }
                break;
            }

            case GBT_FSM_PARSE_VALUE: {
                if (c == '"') {
                    // Start of string value
                    parser->in_string = true;
                    parser->val_len = 0;
                } else if (parser->in_string) {
                    if (parser->escape_next) {
                        parser->escape_next = false;
                        if (parser->val_len < sizeof(parser->val_buf) - 1) {
                            parser->val_buf[parser->val_len++] = c;
                        }
                    } else if (c == '\\') {
                        parser->escape_next = true;
                    } else if (c == '"') {
                        // End of string value
                        parser->in_string = false;
                        parser->val_buf[parser->val_len] = '\0';
                        apply_scalar_value(tmpl, parser->active_key_id, parser->val_buf);
                        parser->active_key_id = KEY_NONE;
                        parser->state = GBT_FSM_SEEK_KEY;
                    } else {
                        if (parser->val_len < sizeof(parser->val_buf) - 1) {
                            parser->val_buf[parser->val_len++] = c;
                        }
                    }
                } else {
                    // Non-string scalar value (number, bool, etc.)
                    if (c == ',' || c == '}' || c == ']' || isspace((unsigned char)c)) {
                        if (parser->val_len > 0) {
                            parser->val_buf[parser->val_len] = '\0';
                            apply_scalar_value(tmpl, parser->active_key_id, parser->val_buf);
                            parser->active_key_id = KEY_NONE;
                            if (c == '}') {
                                if (parser->brace_depth > 0) parser->brace_depth--;
                            }
                            parser->state = GBT_FSM_SEEK_KEY;
                        }
                    } else {
                        if (parser->val_len < sizeof(parser->val_buf) - 1) {
                            parser->val_buf[parser->val_len++] = c;
                        }
                    }
                }
                break;
            }

            case GBT_FSM_SKIP_VALUE: {
                if (c == '"') {
                    parser->prev_state = GBT_FSM_SKIP_VALUE;
                    parser->state = GBT_FSM_SKIP_STRING;
                } else if (c == '{') {
                    parser->brace_depth++;
                } else if (c == '}') {
                    if (parser->brace_depth > 0) parser->brace_depth--;
                    parser->state = GBT_FSM_SEEK_KEY;
                } else if (c == '[') {
                    parser->bracket_depth++;
                } else if (c == ']') {
                    if (parser->bracket_depth > 0) parser->bracket_depth--;
                    parser->state = GBT_FSM_SEEK_KEY;
                } else if (c == ',') {
                    parser->state = GBT_FSM_SEEK_KEY;
                }
                break;
            }

            case GBT_FSM_SKIP_STRING: {
                if (parser->escape_next) {
                    parser->escape_next = false;
                } else if (c == '\\') {
                    parser->escape_next = true;
                } else if (c == '"') {
                    parser->state = parser->prev_state;
                }
                break;
            }

            case GBT_FSM_IN_TX_ARRAY: {
                if (c == '{') {
                    // Starting a new transaction object
                    if (tmpl->tx_count < tmpl->tx_cap) {
                        tmpl->tx_offsets[tmpl->tx_count] = (uint32_t)tmpl->raw_tx_len;
                        tmpl->tx_lengths[tmpl->tx_count] = 0;
                    }
                    parser->tx_brace_depth = 1;
                    parser->state = GBT_FSM_IN_TX_OBJECT;
                } else if (c == ']') {
                    // End of transactions array
                    parser->state = GBT_FSM_SEEK_KEY;
                }
                break;
            }

            case GBT_FSM_IN_TX_OBJECT: {
                if (c == '"') {
                    parser->key_len = 0;
                    parser->state = GBT_FSM_TX_READ_KEY;
                } else if (c == '{') {
                    parser->tx_brace_depth++;
                } else if (c == '}') {
                    parser->tx_brace_depth--;
                    if (parser->tx_brace_depth == 0) {
                        // Completed a transaction object
                        if (tmpl->tx_count < tmpl->tx_cap) {
                            tmpl->tx_count++;
                        }
                        parser->state = GBT_FSM_IN_TX_ARRAY;
                    }
                }
                break;
            }

            case GBT_FSM_TX_READ_KEY: {
                if (c == '"') {
                    parser->key_buf[parser->key_len] = '\0';
                    parser->active_key_id = identify_key(parser->key_buf);
                    parser->state = GBT_FSM_TX_AFTER_KEY;
                } else {
                    if (parser->key_len < sizeof(parser->key_buf) - 1) {
                        parser->key_buf[parser->key_len++] = c;
                    }
                }
                break;
            }

            case GBT_FSM_TX_AFTER_KEY: {
                if (c == ':') {
                    if (parser->active_key_id == KEY_TX_DATA) {
                        parser->has_carry_nibble = false;
                        parser->state = GBT_FSM_TX_PARSE_DATA;
                    } else if (parser->active_key_id == KEY_TX_TXID) {
                        parser->txid_hex_len = 0;
                        parser->state = GBT_FSM_TX_PARSE_TXID;
                    } else {
                        parser->state = GBT_FSM_TX_SKIP_VALUE;
                    }
                }
                break;
            }

            case GBT_FSM_TX_PARSE_DATA: {
                if (!parser->in_tx_data) {
                    if (c == '"') {
                        parser->in_tx_data = true;
                        parser->has_carry_nibble = false;
                        if (tmpl->tx_count < tmpl->tx_cap) {
                            tmpl->tx_offsets[tmpl->tx_count] = (uint32_t)tmpl->raw_tx_len;
                        }
                    }
                } else {
                    if (c == '"') {
                        // End of "data" string
                        parser->in_tx_data = false;
                        if (tmpl->tx_count < tmpl->tx_cap) {
                            tmpl->tx_lengths[tmpl->tx_count] = (uint32_t)(tmpl->raw_tx_len - tmpl->tx_offsets[tmpl->tx_count]);
                        }
                        parser->state = GBT_FSM_IN_TX_OBJECT;
                    } else {
                        int nibble = hex_val_table[(unsigned char)c];
                        if (nibble >= 0) {
                            if (!parser->has_carry_nibble) {
                                parser->carry_nibble = (uint8_t)nibble;
                                parser->has_carry_nibble = true;
                            } else {
                                uint8_t byte = (uint8_t)((parser->carry_nibble << 4) | nibble);
                                parser->has_carry_nibble = false;
                                if (tmpl->raw_tx_len < tmpl->raw_tx_cap) {
                                    tmpl->raw_tx_data[tmpl->raw_tx_len++] = byte;
                                } else {
                                    parser->has_error = true;
                                    snprintf(parser->error_msg, sizeof(parser->error_msg),
                                             "Raw tx buffer overflow (%zu bytes)", tmpl->raw_tx_cap);
                                    return ESP_ERR_NO_MEM;
                                }
                            }
                        }
                    }
                }
                break;
            }

            case GBT_FSM_TX_PARSE_TXID: {
                if (!parser->in_tx_txid) {
                    if (c == '"') {
                        parser->in_tx_txid = true;
                        parser->txid_hex_len = 0;
                    }
                } else {
                    if (c == '"') {
                        parser->in_tx_txid = false;
                        if (parser->txid_hex_len == 64 && tmpl->tx_count < tmpl->tx_cap) {
                            parser->txid_hex_buf[64] = '\0';
                            decode_txid_hex_to_internal_bin(parser->txid_hex_buf,
                                                           tmpl->txid_hashes + (tmpl->tx_count * 32));
                        }
                        parser->state = GBT_FSM_IN_TX_OBJECT;
                    } else {
                        if (parser->txid_hex_len < 64 && isxdigit((unsigned char)c)) {
                            parser->txid_hex_buf[parser->txid_hex_len++] = c;
                        }
                    }
                }
                break;
            }

            case GBT_FSM_TX_SKIP_VALUE: {
                if (c == '"') {
                    parser->prev_state = GBT_FSM_TX_SKIP_VALUE;
                    parser->state = GBT_FSM_SKIP_STRING;
                } else if (c == '{') {
                    parser->tx_brace_depth++;
                } else if (c == '}') {
                    parser->tx_brace_depth--;
                    if (parser->tx_brace_depth == 0) {
                        if (tmpl->tx_count < tmpl->tx_cap) {
                            tmpl->tx_count++;
                        }
                        parser->state = GBT_FSM_IN_TX_ARRAY;
                    } else {
                        parser->state = GBT_FSM_IN_TX_OBJECT;
                    }
                } else if (c == ',') {
                    parser->state = GBT_FSM_IN_TX_OBJECT;
                }
                break;
            }
        }
    }

    return ESP_OK;
}
