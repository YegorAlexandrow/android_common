package tech.fastsense.common.local_logger

import android.content.Context
import android.content.SharedPreferences
import android.provider.Settings
import androidx.preference.PreferenceManager
import com.google.gson.Gson
import com.google.gson.GsonBuilder
import com.google.gson.JsonDeserializationContext
import com.google.gson.JsonDeserializer
import com.google.gson.JsonElement
import com.google.gson.JsonSerializationContext
import com.google.gson.JsonSerializer
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.time.ZoneId
import java.time.ZonedDateTime
import java.util.UUID

class LocalLogger(context: Context) {
    enum class LogLevel { INFO, WARN, ERROR, SPECIAL, DEBUG }
    data class LogEntry(
        val level: LogLevel,
        val timestamp: ZonedDateTime,
        val message: String,
        var uploaded: Boolean = false
    )

    class ZonedDateTimeTypeAdapter : JsonSerializer<ZonedDateTime>,
        JsonDeserializer<ZonedDateTime> {
        override fun serialize(
            src: ZonedDateTime,
            typeOfSrc: java.lang.reflect.Type,
            context: JsonSerializationContext
        ): JsonElement {
            return com.google.gson.JsonPrimitive(src.toString())
        }

        override fun deserialize(
            json: JsonElement,
            typeOfT: java.lang.reflect.Type,
            context: JsonDeserializationContext
        ): ZonedDateTime {
            return ZonedDateTime.parse(json.asString)
        }
    }

    private val sharedPreferences: SharedPreferences =
        context.getSharedPreferences("log_prefs", Context.MODE_PRIVATE)
    private val gson: Gson =
        GsonBuilder().registerTypeAdapter(ZonedDateTime::class.java, ZonedDateTimeTypeAdapter())
            .create()

    init {
        val mSettings = PreferenceManager.getDefaultSharedPreferences(context)
        var firstStart = false
        val bootCount =
            Settings.Global.getInt(context.contentResolver, Settings.Global.BOOT_COUNT, -1)
        val lastBootCount = mSettings.getInt("last_boot_count", -1)

        if (bootCount != -1 && bootCount != lastBootCount) {
            mSettings.edit().putInt("last_boot_count", bootCount).apply()
            firstStart = true
        }
        this(
            "Head app started [${MAX_LOG_ENTRIES - sharedPreferences.all.size}]" + if (firstStart) "\nFirst start since boot" else "",
            LogLevel.SPECIAL, async = false
        )
        val allLogs = sharedPreferences.all
        if (allLogs.size >= MAX_LOG_ENTRIES) CoroutineScope(Dispatchers.IO).launch {
            val sortedLogs = allLogs.entries.sortedBy { (_, value) ->
                gson.fromJson(value as String, LogEntry::class.java).timestamp
            }
            sharedPreferences.edit().apply {
                for (i in 0 until (allLogs.size - MAX_LOG_ENTRIES + 1)) {
                    remove(sortedLogs[i].key)
                }
            }.apply()
        }
    }

    operator fun invoke(message: String, level: LogLevel = LogLevel.INFO, async: Boolean = true) =
        CoroutineScope(Dispatchers.IO).launch {
            val logEntry =
                LogEntry(
                    timestamp = ZonedDateTime.now(ZoneId.systemDefault()),
                    message = message,
                    level = level,
                    uploaded = false
                )
            val logId = UUID.randomUUID().toString()
            val json = gson.toJson(logEntry)
            (if (async) SharedPreferences.Editor::apply else SharedPreferences.Editor::commit)(
                sharedPreferences.edit().putString(logId, json)
            )
        }

    suspend fun getLogs(): List<LogEntry> {
        return withContext(Dispatchers.IO) {
            val allLogs = sharedPreferences.all
            allLogs.values.map { value ->
                gson.fromJson(value as String, LogEntry::class.java)
            }.sortedByDescending { it.timestamp }
        }
    }
    fun markUploaded(id: UUID) {
        val idStr = id.toString()
        val current =
            gson.fromJson(sharedPreferences.getString(idStr, null), LogEntry::class.java)
        current.uploaded = true
        current?.let {
            sharedPreferences.edit().apply { putString(idStr, gson.toJson(current)) }.apply()
        }
    }

    val logsToUpload
        get() = sharedPreferences.all.entries.map { (id, value) ->
            UUID.fromString(id) to gson.fromJson(value as String, LogEntry::class.java)
        }.filter { (_, value) -> !value.uploaded }



    fun clear() = sharedPreferences.edit().clear().apply()

    companion object {
        private const val MAX_LOG_ENTRIES = 1000

        fun convertToPacificTime(zonedDateTime: ZonedDateTime): ZonedDateTime {
            val pacificZoneId = ZoneId.of("America/Los_Angeles")
            return zonedDateTime.withZoneSameInstant(pacificZoneId)
        }
    }
}
