package tech.fastsense.common

import android.annotation.SuppressLint
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.BatteryManager
import android.os.Build
import androidx.preference.PreferenceManager
import com.google.firebase.database.DatabaseException
import com.google.firebase.database.FirebaseDatabase
import com.google.firebase.database.ktx.database
import com.google.firebase.ktx.Firebase
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import org.koin.core.component.KoinComponent
import org.koin.core.component.inject
import tech.fastsense.common.api.ApiClient
import tech.fastsense.common.autostart.AutostartPreconditions
import tech.fastsense.common.local_logger.LocalLogger
import java.text.SimpleDateFormat
import java.util.Date
import java.util.TimeZone
import java.util.concurrent.atomic.AtomicBoolean

class StatusLogger(private val context: Context) : KoinComponent {
    companion object {
        var version = "---"
        private var batteryStatus: Intent? = null

        private var state_: String = "on"
        private var subState_: String = "_"

        private var prevState: String? = null
        private var prevSubState: String? = null
        private var lastLogTimestamp = 0L
        private var lastLogLocalLogTimestamp = 0L
        private val loggingLocalLogsInProgress = AtomicBoolean(false)
        private var autoAvatarCond = AutostartPreconditions()
        private var autoAvatarCondCheck = autoAvatarCond.check
        private var autoAvatarCondMsg = autoAvatarCond.unsatisfiedConditionsMsg
        private var autoAvatarCondTs = Date()
        private var humanReadableTimestamp = tsToHumanReadable(autoAvatarCondTs)
        private const val DB_URL =
            "https://wehead-3362c-default-rtdb.europe-west1.firebasedatabase.app/"
        private const val MAX_LOG_INTERVAL_MS = 12 * 60 * 1000

        private const val ISO_8601_24H_FULL = "yyyy-MM-dd'T'HH:mm:ss.SSSZ"

        @SuppressLint("SimpleDateFormat")
        private fun tsToHumanReadable(ts: Date): String {
            val formatter = SimpleDateFormat(ISO_8601_24H_FULL)
            formatter.timeZone = TimeZone.getTimeZone("America/Los_Angeles")
            return formatter.format(ts)
        }
    }

    private val localLogger: LocalLogger by inject()

    val state: String
        get() = state_

    val subState: String
        get() = subState_

    private var db: FirebaseDatabase = Firebase.database(DB_URL)
    private var apiClient: ApiClient = ApiClient(context)

    fun updateAvatarAutostartInfo(cond: AutostartPreconditions) {
        autoAvatarCond = cond
        autoAvatarCondCheck = cond.check
        autoAvatarCondMsg = cond.unsatisfiedConditionsMsg
        autoAvatarCondTs = Date()
        humanReadableTimestamp = tsToHumanReadable(autoAvatarCondTs)
        logStatus()
    }

    private fun getAvatarAutostartInfoMap() = mapOf(
        "status" to autoAvatarCond,
        "checkResults" to autoAvatarCondCheck,
        "overallResult" to autoAvatarCondCheck.all,
        "unsatisfiedConditionsMsg" to autoAvatarCondMsg,
        "timestamp" to autoAvatarCondTs.time,
        "humanReadableTimestamp" to humanReadableTimestamp
    )

    fun updateStatus(newState: String, newSubState: String) {
        state_ = newState
        subState_ = newSubState

        logStatus()
    }

    fun logStatus() {
        batteryStatus = context.registerReceiver(null, IntentFilter(Intent.ACTION_BATTERY_CHANGED))
        val mSettings = PreferenceManager.getDefaultSharedPreferences(context)
        val deviceId = mSettings.getString("device_id", "www")!!
        val loc = mSettings.getString("location", "eyes")!!
        val batteryPercentage = getBatteryPercentage()
        val batteryCharge = getBatteryChargeStatus()
        try {
            db.getReference("/devices/${deviceId}/status/${loc}/").setValue(
                mapOf(
                    "state" to state,
                    "subState" to subState_,
                    "version" to version,
                    "timestamp" to mapOf(".sv" to "timestamp"),
                    "battery" to mapOf("percent" to batteryPercentage, "charge" to batteryCharge),
                    "deviceModel" to Build.MODEL,
                    "buildProduct" to Build.PRODUCT,
                    "mechVersion" to (if (Build.MODEL == "SM-G973F") "0.2.0" else null),
                    "settings" to mSettings.all,
//            TODO        "audioInfo" to (KoinJavaComponent.get<SpeakerTest>(SpeakerTest::class.java)).getDeviceInfo(),
                    "avatarAutostart" to getAvatarAutostartInfoMap()
                )
            )
        } catch (e: DatabaseException) {
            e.printStackTrace()
        }
        if (needLogStatus()) {
            apiClient.logStatus(state, subState_, version, batteryPercentage, batteryCharge)
            lastLogTimestamp = System.currentTimeMillis()
        }
        if (needLogLocalLogs()) {
            val logsSnapshot = localLogger.logsToUpload
            CoroutineScope(Dispatchers.IO).launch {
                if (loggingLocalLogsInProgress.getAndSet(true)) return@launch
                lastLogLocalLogTimestamp = System.currentTimeMillis()
                logsSnapshot.chunked(8).forEachIndexed { i, it ->
                    delay(1000)
                    it.forEach { (id, entry) ->
                        delay(50)
                        apiClient.logLocalLog(id, entry) { localLogger.markUploaded(id) }
                    }
                }
                loggingLocalLogsInProgress.set(false)
            }
        }
        prevState = state
        prevSubState = subState_
    }

    private fun needLogLocalLogs() = needLogStatus() ||
            System.currentTimeMillis() - lastLogLocalLogTimestamp > MAX_LOG_INTERVAL_MS

    private fun needLogStatus() = prevState != state || prevSubState != subState_ ||
            System.currentTimeMillis() - lastLogTimestamp > MAX_LOG_INTERVAL_MS


    private fun getBatteryPercentage(): Float? {
        val batteryPct: Float? = batteryStatus?.let { intent ->
            val level: Int = intent.getIntExtra(BatteryManager.EXTRA_LEVEL, -1)
            val scale: Int = intent.getIntExtra(BatteryManager.EXTRA_SCALE, -1)
            level * 100 / scale.toFloat()
        }

        return batteryPct
    }

    private fun getBatteryChargeStatus(): Boolean {
        val status: Int = batteryStatus?.getIntExtra(BatteryManager.EXTRA_STATUS, -1) ?: -1

        return (status == BatteryManager.BATTERY_STATUS_CHARGING
                || status == BatteryManager.BATTERY_STATUS_FULL)
    }
}