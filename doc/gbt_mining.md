# Direct Bitcoin Core Solo Mining (GetBlockTemplate)

ESP-Miner supports direct, self-sovereign solo mining directly against your own Bitcoin Core node using HTTP JSON-RPC `getblocktemplate` (BIP 22/23) and `submitblock`. This allows you to construct your own block templates and submit newly discovered blocks directly to the Bitcoin peer-to-peer network without any third-party pool or stratum proxy.

---

## Hardware & System Requirements

- **Bitaxe Hardware:** Requires an ESP32-S3 model with **8MB PSRAM** (ESP32-S3-WROOM-1 N16R8), such as Bitaxe Ultra, Supra, Gamma, etc. Streaming real-world 4MB+ Bitcoin blocks requires ~3.8 MB of PSRAM.
- **Bitcoin Core Node:** Bitcoin Core v0.13.0 or higher, fully synchronized with the network.

---

## Bitcoin Core Setup & Configuration

Configure your Bitcoin node via `bitcoin.conf` (typically located at `~/.bitcoin/bitcoin.conf` on Linux or `%APPDATA%\Bitcoin\bitcoin.conf` on Windows).

### 1. Basic RPC Configuration

```ini
# Enable the JSON-RPC server
server=1

# Bind RPC to localhost and your host machine's LAN IP
rpcbind=127.0.0.1
rpcbind=192.168.1.229

# Allow incoming RPC requests from localhost and your Bitaxe's IP (or subnet)
rpcallowip=127.0.0.1
rpcallowip=192.168.1.150
# Or for the whole subnet:
# rpcallowip=192.168.1.0/24

# Set RPC credentials (or use rpcauth as detailed below)
rpcuser=miner
rpcpassword=choose_a_strong_password
```

> [!NOTE]
> In Bitcoin Core, specifying `rpcbind` overrides default interface binding. If you specify your LAN IP, you must also explicitly specify `rpcbind=127.0.0.1` so local tools like `bitcoin-cli` continue to work.

---

## Security Best Practices

> [!WARNING]
> Bitcoin Core's JSON-RPC interface is the administrative control plane of your node. Take the following precautions when exposing RPC on your local network:

1. **Never Expose Port 8332 to the Internet:**
   - Never create a router port-forwarding rule for port 8332.
   - Disable UPnP in your `bitcoin.conf` (`upnp=0`) so ports are not automatically forwarded.

2. **Do Not Bind to All Interfaces (`0.0.0.0`) if VPNs are Active:**
   - If your host machine uses a VPN (e.g. WireGuard or OpenVPN), binding to `0.0.0.0` will expose the RPC port over the VPN tunnel. Bind specifically to your LAN interface IP and `127.0.0.1`.

3. **Do Not Keep Funded Wallets Loaded on a Mining Node:**
   - Anyone with RPC access can potentially spend funds from loaded, unencrypted wallets (`sendtoaddress`).
   - If this node is dedicated to mining, run with `disablewallet=1`.
   - If you must run a wallet on this machine, ensure it remains locked with a strong passphrase.

4. **Restrict RPC Permissions with Command Whitelisting:**
   - Rather than giving the Bitaxe administrative RPC rights, create a dedicated user using `rpcauth` (generated with `share/rpcauth/rpcauth.py` in Bitcoin Core) and limit its permissions to mining methods only:
     ```ini
     # Example rpcauth generated user:
     rpcauth=miner:c3a2...$89e1...

     # Whitelist only methods needed for GBT mining:
     rpcwhitelist=miner:getblocktemplate,submitblock,getmininginfo,getnetworkinfo
     rpcwhitelistdefault=0
     ```
     With `rpcwhitelistdefault=0`, all unlisted commands (such as `stop` or wallet commands) will be blocked.

5. **Firewall Rules:**
   - Restrict incoming traffic on port 8332 to your Bitaxe's IP using `ufw`:
     ```bash
     sudo ufw allow from 192.168.1.150 to any port 8332 proto tcp
     ```

---

## Configuring the Bitaxe (Axe-OS)

### Web Dashboard

1. Navigate to your Bitaxe web dashboard.
2. Open **Settings** -> **Pools**.
3. Select an empty pool slot or configure your primary pool:
   - **Protocol:** Select `GetBlockTemplate (GBT)`.
   - **URL / Host:** `http://<your-node-ip>:8332` (or `<your-node-ip>:8332`).
   - **User:** RPC username configured in `bitcoin.conf`.
   - **Password:** RPC password configured in `bitcoin.conf`.
   - **Payout Address:** Your Bitcoin payout address (Base58, Bech32 `bc1q...`, or Taproot `bc1p...`).
   - **Miner Tag:** (Optional) Custom text to include in the coinbase transaction.
4. Click **Save and Restart** (or save pool settings).

### REST API Example

You can also configure a GBT pool slot via the REST API:

```bash
curl -X PUT http://YOUR-BITAXE-IP/api/system/pools/0 \
     -H "Content-Type: application/json" \
     -d '{
       "stratumProtocol": "GBT",
       "stratumURL": "192.168.1.229:8332",
       "stratumPort": 8332,
       "stratumUser": "miner",
       "stratumPassword": "your_rpc_password",
       "payoutAddress": "bc1qsz776dlrlp4p54rwpxd3tc4s9p2a8ppuceyvdc",
       "minerTag": "Bitaxe Solo"
     }'
```

---

## Troubleshooting & Status Messages

Axe-OS displays the status and diagnostic error messages directly on the dashboard:

| Dashboard Message | Cause | Solution |
| :--- | :--- | :--- |
| `GBT: Connection failed` / `Connection reset by peer` | The node is not listening on the LAN IP, or a firewall is blocking port 8332. | Check that `rpcbind` in `bitcoin.conf` includes your LAN IP, and verify firewall rules. |
| `GBT: Auth rejected (401)` | The RPC username or password sent by Bitaxe was rejected by Bitcoin Core. | Verify `stratumUser` and `stratumPassword` match `rpcuser`/`rpcpassword` or `rpcauth`. |
| `GBT: Node in IBD (500)` | Bitcoin Core is currently performing Initial Block Download (IBD). | Bitcoin Core cannot generate block templates until block sync is complete. The miner will automatically retry or switch to fallback. |
| `GBT: Invalid payout address` | The configured payout address is invalid or has a bad checksum. | Double check your payout address format in pool settings. Supports Base58 (1... / 3...) and Bech32/Bech32m (bc1q... / bc1p...). |
