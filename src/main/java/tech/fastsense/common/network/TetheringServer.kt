package tech.fastsense.common.network

import android.annotation.SuppressLint
import android.content.Context
import android.net.TetheringManager
import android.net.wifi.SoftApConfiguration
import android.net.wifi.WifiManager
import android.util.Log
import androidx.preference.PreferenceManager
import org.koin.core.component.KoinComponent
import org.koin.core.component.inject
import tech.fastsense.common.local_logger.LocalLogger
import java.lang.reflect.Proxy
import java.util.concurrent.Executors
import java.util.concurrent.ScheduledExecutorService


@SuppressLint("InlinedApi")
class TetheringServer(private val context: Context) : KoinComponent {
    private val localLogger: LocalLogger by inject()

    fun interface OnTetheringStartResult {
        operator fun invoke(result: Boolean)
    }

    private var clientsInfoListener: ClientsInfoListener? = null
    private var availabilityListener: AvailabilityListener? = null

    private var softApListenerEnabled = false
    private var serverSsid =
        PreferenceManager.getDefaultSharedPreferences(context).getString("device_id", "wwwwwwww")!!
    private var serverPass = serverSsid

    private lateinit var backgroundExecutor: ScheduledExecutorService

    private val wifiManager =
        context.getSystemService(Context.WIFI_SERVICE) as WifiManager
    private val tetheringManager =
        context.getSystemService(TetheringManager::class.java) as TetheringManager

    private val softApCallback: Any = Proxy.newProxyInstance(
        context.classLoader, arrayOf(softApCallbackClass)
    ) { _, method, args ->
//        loge("@@@ $method")
        when (method.name) {
            "toString" -> "TetheringServer.softApCallback"
            "onConnectedClientsChanged" -> {
                if (args.size == 1) {
                    val rawInfo = args[0] as List<*>
                    val clientsInfo = mutableListOf<ClientInfo>()
                    rawInfo.forEach { clientsInfo.add(ClientInfo(it.toString())) }
                    clientsInfoListener?.invoke(clientsInfo)
                    localLogger("tethering clients: ${clientsInfo.size} ")
                }
                return@newProxyInstance null
            }

            "onStateChanged" -> {
                // WIFI_AP_STATE_ENABLED = 13;
                availabilityListener?.invoke(args[0] == 13)
                return@newProxyInstance null
            }

            else -> {}
        }
    }

    init {
        enableSoftApListener()
    }

    @SuppressLint("PrivateApi")
    fun start(onResult: OnTetheringStartResult) {
        logi("starting tether")
        val newConfig = SoftApConfigurationBuilder(wifiManager.getSoftApConfiguration())
            .setPassphrase(serverPass, SoftApConfiguration.SECURITY_TYPE_WPA2_PSK)
            .setSsid(serverSsid)
            .build()
        wifiManager.setSoftApConfiguration(newConfig)
        tetheringManager.startTethering(
            TetheringManager.TETHERING_WIFI,
            context.mainExecutor,
            object : TetheringManager.StartTetheringCallback {
                override fun onTetheringStarted() {
                    onResult(true)
                }

                override fun onTetheringFailed(p0: Int) {
                    onResult(false)
                }
            })
    }

    /**
    This API does not have a result callback. Use TETHER_STATE_CHANGE broadcast instead.
    https://android.googlesource.com/platform/prebuilts/fullsdk/sources/android-30/+/refs/heads/androidx-main-release/android/net/TetheringManager.java#781
     */
    fun stop() {
        logi("stopping tether")
        tetheringManager.stopTethering(TetheringManager.TETHERING_WIFI)
    }

    data class ClientInfo(val name: String) //todo

    fun interface ClientsInfoListener {
        operator fun invoke(clients: List<ClientInfo>)
    }

    fun interface AvailabilityListener {
        operator fun invoke(available: Boolean)
    }

    fun setClientsListener(l: ClientsInfoListener) {
        clientsInfoListener = l
    }

    fun setAvailabilityListener(l: AvailabilityListener) {
        availabilityListener = l
    }

    fun enableSoftApListener() {
        if (softApListenerEnabled) return
        logi("enableSoftApListener")
        softApListenerEnabled = true
        backgroundExecutor = Executors.newSingleThreadScheduledExecutor()
        wifiManager.registerSoftApCallback(
            backgroundExecutor,
            softApCallback
        ) //todo unregister reflex
    }

    fun disableSoftApListener() {
        if (!softApListenerEnabled) return
        logi("disableSoftApListener")
        backgroundExecutor.shutdown()
        softApListenerEnabled = false
    }

    companion object {
        private const val TAG = "TetheringServer"
        fun loge(s: String) = Log.e(TAG, s)
        fun logi(s: String) = Log.i(TAG, s)
    }
}