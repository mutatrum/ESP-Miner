#ifndef COINBASE_BUILDER_H_
#define COINBASE_BUILDER_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define COINBASE_MAX_SCRIPT_PUBKEY_LEN  (83)
#define COINBASE_MAX_SCRIPTSIG_LEN      (100)
#define COINBASE_MAX_TX_LEN             (1024)
#define COINBASE_MAX_MERKLE_BRANCHES    (32)
#define COINBASE_DEFAULT_TAG            "/AxeOS/"
#define COINBASE_MAX_TAG_LEN            (64)

/**
 * Validate that an address is a valid Bitcoin address (P2PKH, P2SH, P2WPKH, P2WSH, P2TR).
 */
bool coinbase_validate_address(const char *address);

/**
 * Convert a Bitcoin address to its corresponding scriptPubKey.
 */
esp_err_t coinbase_address_to_script(const char *address,
                                     uint8_t *script,
                                     size_t script_cap,
                                     size_t *script_len);

/**
 * Build a BIP 34 compliant coinbase scriptSig containing block height, miner tag, and extranonce.
 * Strictly verifies consensus length constraint: 2 <= scriptsig_len <= 100 bytes.
 */
esp_err_t coinbase_build_scriptsig(uint32_t height,
                                  const uint8_t *extranonce,
                                  size_t extranonce_len,
                                  const char *tag,
                                  uint8_t *scriptsig,
                                  size_t *scriptsig_len);

/**
 * Build a complete coinbase transaction (both witness and legacy serialized formats).
 * Calculates the 32-byte coinbase txid for Merkle tree insertion.
 */
esp_err_t coinbase_build_tx(uint32_t height,
                            uint64_t coinbase_value,
                            const char *payout_address,
                            const char *witness_commitment_hex,
                            const uint8_t *extranonce,
                            size_t extranonce_len,
                            const char *tag,
                            uint8_t *coinbase_tx,
                            size_t coinbase_tx_cap,
                            size_t *coinbase_tx_len,
                            uint8_t coinbase_txid[32]);

/**
 * Compute the initial Merkle tree and extract the Coinbase Merkle branch.
 * Stores up to MAX_MERKLE_BRANCHES hashes in merkle_path.
 */
esp_err_t coinbase_compute_merkle_path(const uint8_t coinbase_txid[32],
                                       const uint8_t *txid_hashes,
                                       size_t tx_count,
                                       uint8_t merkle_path[COINBASE_MAX_MERKLE_BRANCHES][32],
                                       size_t *merkle_path_count,
                                       uint8_t merkle_root[32]);

/**
 * Fast extranonce roll: updates Merkle root using the cached Coinbase Merkle branch (< 5 microseconds).
 */
void coinbase_update_merkle_root(const uint8_t new_coinbase_txid[32],
                                 const uint8_t merkle_path[COINBASE_MAX_MERKLE_BRANCHES][32],
                                 size_t merkle_path_count,
                                 uint8_t new_merkle_root[32]);

#ifdef __cplusplus
}
#endif

#endif // COINBASE_BUILDER_H_
