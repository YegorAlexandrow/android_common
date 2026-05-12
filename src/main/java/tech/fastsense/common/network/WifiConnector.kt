@file:Suppress("MissingPermission", "InlinedApi", "DEPRECATION")

package tech.fastsense.common.network

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.net.NetworkInfo
import android.net.wifi.WifiConfiguration
import android.net.wifi.WifiManager
import android.text.format.Formatter
import android.util.Log
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlinx.coroutines.withTimeout
import kotlin.coroutines.resume

class WifiConnector(private val context: Context) {
    private val wifiManager = context.getSystemService(Context.WIFI_SERVICE) as WifiManager
    private var monitor: BroadcastReceiver? = null

    init {
        runCatching { wifiManager.setWifiEnabled(true) }
    }

    val currentSsid: String?
        get() = wifiManager.connectionInfo?.ssid
            ?.takeUnless { it == WifiManager.UNKNOWN_SSID }
            ?.removeSurrounding("\"")

    fun disconnect() {
        val currentNetworkId = wifiManager.connectionInfo?.networkId ?: return
        if (currentNetworkId != -1) {
            Log.d(TAG, "disabling $currentNetworkId")
            wifiManager.disableNetwork(currentNetworkId)
        }
    }

    fun currentGatewayIp(): String? {
        val gateway = wifiManager.dhcpInfo?.gateway ?: return null
        if (gateway == 0) return null
        return Formatter.formatIpAddress(gateway)
    }

    suspend fun connect(
        ssid: String,
        passphrase: String,
        timeout: Long = 15_000L,
    ): Boolean = withTimeout(timeout) {
        suspendCancellableCoroutine { continuation ->
            val conf = WifiConfiguration().apply {
                SSID = "\"" + ssid + "\""
                preSharedKey = "\"" + passphrase + "\""
                allowedKeyManagement.set(WifiConfiguration.KeyMgmt.WPA2_PSK)
            }
            val netId = wifiManager.addNetwork(conf)
            if (netId == -1 || !wifiManager.enableNetwork(netId, true)) {
                Log.d(TAG, "enableNetwork -> false for $ssid")
                continuation.resume(false)
                return@suspendCancellableCoroutine
            }
            runCatching { monitor?.let(context::unregisterReceiver) }
            monitor = object : BroadcastReceiver() {
                override fun onReceive(ctx: Context?, intent: Intent?) {
                    if (continuation.isCompleted) return
                    if (intent?.action != WifiManager.NETWORK_STATE_CHANGED_ACTION) return
                    val networkInfo: NetworkInfo =
                        intent.getParcelableExtra(WifiManager.EXTRA_NETWORK_INFO) ?: return
                    if (networkInfo.detailedState != NetworkInfo.DetailedState.CONNECTED) return

                    val connectedSsid = currentSsid
                    runCatching { monitor?.let(context::unregisterReceiver) }
                    continuation.resume(connectedSsid == ssid)
                }
            }
            context.registerReceiver(
                monitor,
                IntentFilter().apply { addAction(WifiManager.NETWORK_STATE_CHANGED_ACTION) }
            )
            continuation.invokeOnCancellation {
                runCatching { monitor?.let(context::unregisterReceiver) }
            }
        }
    }

    companion object {
        private const val TAG = "WifiConnector"
    }
}
