#ifndef GBT_FSM_PARSER_H_
#define GBT_FSM_PARSER_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GBT_MAX_RAW_TX_BYTES   (3800000)   // 3.8 MB buffer in PSRAM
#define GBT_MAX_TX_COUNT        (4096)      // Up to 4,096 transactions in a block
#define GBT_PREV_HASH_HEX_LEN   (65)
#define GBT_COMMITMENT_HEX_LEN  (96)
#define GBT_LONGPOLLID_MAX_LEN  (128)

typedef struct {
    uint32_t version;
    uint32_t nbits;
    uint32_t curtime;
    uint32_t height;
    uint64_t coinbase_value;
    char previous_block_hash[GBT_PREV_HASH_HEX_LEN];
    char default_witness_commitment[GBT_COMMITMENT_HEX_LEN];
    char longpollid[GBT_LONGPOLLID_MAX_LEN];

    // Static PSRAM buffers
    uint8_t *raw_tx_data;
    size_t raw_tx_len;
    size_t raw_tx_cap;

    uint8_t *txid_hashes;       // 32 bytes per tx (little-endian internal byte order)
    size_t tx_count;
    size_t tx_cap;

    uint32_t *tx_offsets;
    uint32_t *tx_lengths;

    bool allocated;
} gbt_template_t;

typedef enum {
    GBT_FSM_SEEK_KEY,
    GBT_FSM_READ_KEY,
    GBT_FSM_AFTER_KEY,
    GBT_FSM_PARSE_VALUE,
    GBT_FSM_SKIP_STRING,
    GBT_FSM_SKIP_VALUE,
    GBT_FSM_IN_TX_ARRAY,
    GBT_FSM_IN_TX_OBJECT,
    GBT_FSM_TX_READ_KEY,
    GBT_FSM_TX_AFTER_KEY,
    GBT_FSM_TX_PARSE_DATA,
    GBT_FSM_TX_PARSE_TXID,
    GBT_FSM_TX_SKIP_VALUE,
} gbt_fsm_state_t;

typedef struct {
    gbt_fsm_state_t state;
    gbt_fsm_state_t prev_state;

    // Key buffer for tracking key names
    char key_buf[48];
    size_t key_len;

    // Value buffer for scalar values
    char val_buf[140];
    size_t val_len;

    // Parsing context
    int brace_depth;
    int bracket_depth;
    int tx_brace_depth;
    bool in_string;
    bool escape_next;

    // Transaction parsing state
    bool in_tx_data;
    bool has_carry_nibble;
    uint8_t carry_nibble;

    bool in_tx_txid;
    char txid_hex_buf[65];
    size_t txid_hex_len;

    // Target active key type
    int active_key_id;

    // Overall status
    bool has_error;
    char error_msg[64];
    bool template_complete;
} gbt_fsm_parser_t;

/**
 * Allocate the static PSRAM buffers for the template once.
 */
esp_err_t gbt_template_init(gbt_template_t *tmpl);

/**
 * Reset template counters before streaming a new block template.
 * Reuses existing PSRAM allocations with zero realloc overhead.
 */
void gbt_template_reset(gbt_template_t *tmpl);

/**
 * Free template PSRAM buffers if GBT mode is disabled.
 */
void gbt_template_free(gbt_template_t *tmpl);

/**
 * Initialize FSM parser state.
 */
void gbt_fsm_parser_init(gbt_fsm_parser_t *parser);

/**
 * Feed a chunk of incoming JSON bytes into the streaming FSM.
 * Converts hex to binary in-flight directly into tmpl->raw_tx_data.
 */
esp_err_t gbt_fsm_parser_feed(gbt_fsm_parser_t *parser,
                             gbt_template_t *tmpl,
                             const uint8_t *chunk,
                             size_t chunk_len);

#ifdef __cplusplus
}
#endif

#endif // GBT_FSM_PARSER_H_
