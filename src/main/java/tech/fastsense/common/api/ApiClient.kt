package tech.fastsense.common.api

import android.annotation.SuppressLint
import android.content.Context
import android.content.SharedPreferences
import android.util.Log
import androidx.preference.PreferenceManager
import com.android.volley.Request
import com.google.firebase.auth.ktx.auth
import com.google.firebase.ktx.Firebase
import com.google.gson.Gson
import com.google.gson.JsonParser
import com.google.gson.reflect.TypeToken
import org.json.JSONObject
import tech.fastsense.common.local_logger.LocalLogger
import tech.fastsense.common.models.api.IssueCreateResp
import tech.fastsense.common.models.api.Link
import tech.fastsense.common.models.api.Release
import tech.fastsense.common.models.api.ReleaseType
import tech.fastsense.common.models.api.VideoMessage
import tech.fastsense.common.models.api.VideoMessageLinksResp
import tech.fastsense.common.models.api.VideoMessageListResp
import java.lang.reflect.Type
import java.util.UUID


class ApiClient(private val context: Context) {
    private val httpClient: SimpleHttpClient = SimpleHttpClient(context, API_URL)

    private val gson = Gson()

    private val deviceId: String
        get() {
//            val s = PreferenceManager.getDefaultSharedPreferences(context)
//            return s.getString("device_id", "www")!!
            return Firebase.auth.currentUser?.uid ?: "www"
        }

    val idToken: String?
        get() = SimpleHttpClient.idToken

    private fun getLocation(): String {
        val mSettings = PreferenceManager.getDefaultSharedPreferences(context)
        var loc = mSettings.getString("location", "eyes")!!
        if (loc == "earLeft") loc = "ear_left"
        if (loc == "earRight") loc = "ear_right"

        return loc
    }

    /* LINKS */

    fun createNewLink(linkListener: (Link) -> Unit) {
        httpClient.postJson("/links/", mapOf("deviceId" to deviceId)) { resp ->
            run {
                linkListener(Link(gson.fromJson(resp, Map::class.java) as Map<String, Any>))
            }
        }
    }

    fun getLinks(callback: (List<Link>) -> Unit) {
        httpClient.getJson(
            "$API_URL/links/?link_type=temporary&device_id=${deviceId}"
        ) {
            run {
                val gson = Gson()
                val listType: Type = object : TypeToken<List<Link>>() {}.type
                callback(gson.fromJson(it, listType))
            }
        }
    }

    /* ISSUES */

    fun createIssue(callback: (IssueCreateResp) -> Unit) {
        httpClient.postJson(
            "/issues/", mapOf(
                "payload" to "video_to_host",
                "callId" to null,
                "deviceId" to deviceId,
            )
        ) { resp ->
            run {
                callback(IssueCreateResp(gson.fromJson(resp, Map::class.java) as Map<String, Any>))
            }
        }
    }

    /* STATUSES */

    fun logStatus(
        state: String,
        subState: String?,
        version: String,
        batteryPercentage: Float?,
        batteryCharge: Boolean
    ) {
        httpClient.postJson(
            "/statuses/", mapOf(
                "deviceId" to deviceId,
                "loc" to getLocation(),
                "state" to state,
                "subState" to subState,
                "version" to version,
                "battery" to mapOf(
                    "percent" to batteryPercentage,
                    "charge" to batteryCharge,
                )
            )
        ) {}
    }

    /* LOGS */

    fun logLocalLog(
        id: UUID, entry: LocalLogger.LogEntry, onUploaded: () -> Unit
    ) {
        httpClient.postJson(
            "/logs", mapOf(
                "loc" to getLocation(),
                "id" to id,
                "ts" to entry.timestamp.toEpochSecond(),
                "log" to entry.message,
                "level" to entry.level.toString(),
            )
        ) {
            onUploaded()
        }
    }

    /* RELEASES */

    fun getLatestStableRelease(callback: (Release) -> Unit) {
        httpClient.getJson("/releases/latest") {
            run {
                callback(gson.fromJson(it, Release::class.java))
            }
        }
    }

    fun getReleaseDownloadLink(releaseId: String, callback: (String) -> Unit) {
        httpClient.getJson("/releases/$releaseId/link") {
            run {
                callback(it)
            }
        }
    }

    fun getReleases(
        releaseType: ReleaseType?,
        isFrameApp: Boolean = false,
        callback: (List<Release>) -> Unit
    ) {
        val projectParam = if (isFrameApp) "project=frame&" else ""
        httpClient.getJson(
            "/releases/?${projectParam}release_type=${releaseType?.toString() ?: ""}"
        ) {
            run {
                val gson = Gson()
                val listType: Type = object : TypeToken<List<Release>>() {}.type
                callback(gson.fromJson(it, listType))
            }
        }
    }

    /* VIDEO MESSAGES */

    fun getVideoMessageById(id: UUID, callback: (VideoMessage) -> Unit) {
        httpClient.getJson("/videoMessages/$id") {
            run {
                callback(gson.fromJson(it, VideoMessage::class.java))
            }
        }
    }

    fun getVideoMessageLinks(id: UUID, callback: (VideoMessageLinksResp) -> Unit) {
        httpClient.getJson("/videoMessages/$id/links") {
            run {
                callback(gson.fromJson(it, VideoMessageLinksResp::class.java))
            }
        }
    }

    fun getVideoMessages(
        page: Int = 0,
        pageSize: Int = 25,
        sort: String = "-createdAt",
        displayName: String? = null,
        callback: (VideoMessageListResp) -> Unit
    ) {
        httpClient.getJson(
            "/videoMessages/?page=$page&pageSize=$pageSize&sort=$sort" + (if (displayName != null) "&displayName=$displayName" else "")
        ) {
            run {
                callback(gson.fromJson(it, VideoMessageListResp::class.java))
            }
        }
    }

    fun unsendVideoMessage(id: UUID, callback: (String) -> Unit) {
        httpClient.postString(
            "/videoMessages/$id/unsend"
        ) {
            run {
                callback(it)
            }
        }
    }

    /* AVATARS */

    fun callAvatar(nodeId: String? = null, callback: (Boolean) -> Unit) {
        val body = mutableMapOf(
            "device_id" to deviceId,
            "node_id" to nodeId
        )

        httpClient.request(
            Request.Method.POST,
            "/nodes/call",
            body,
            { println("@@@@ $it"); callback(true) },
            { println("@@@@ $it"); callback(false) })
    }

    fun checkAvatars(nodeId: String? = null, callback: (Boolean) -> Unit) {
        httpClient.getString(
            "/nodes/check_call" + (if (nodeId == null) "" else "node_id=$nodeId")
        ) { callback(it == "true") }
    }

    @SuppressLint("ApplySharedPref")
    fun getAvatarSettings(
        errorCallback: (Exception) -> Unit = {},
        extrasCallback: (String, String, String, String) -> Unit = { s: String, s1: String, s2: String, s3: String -> },
        callback: (Map<String, Any?>) -> Unit,
    ) {
        log("deviceId: $deviceId")
        httpClient.getJson(
            "/settings?deviceId=$deviceId",
            errorCallback
        ) {
            run {
//                logJson(TAG, it)
                val m = (gson.fromJson(
                    it, Map::class.java
                ) as Map<String, Any?>)["settings"] as Map<String, Any?>

                val editor = PreferenceManager.getDefaultSharedPreferences(context).edit()
                val settingsObj =
                    JsonParser.parseString(it).asJsonObject.getAsJsonObject("settings")
                extrasCallback(
                    settingsObj?.get("extras")?.takeIf { !it.isJsonNull }?.toString() ?: "{}",
                    settingsObj?.get("vert")?.takeIf { !it.isJsonNull }?.asString ?: "",
                    settingsObj?.get("geom")?.takeIf { !it.isJsonNull }?.asString ?: "",
                    settingsObj?.get("frag")?.takeIf { !it.isJsonNull }?.asString ?: ""
                )

                fun putMap(
                    vararg mapping: Pair<String, String>,
                    putter: SharedPreferences.Editor.(String, Any) -> Unit
                ) = mapping.forEach { (key, mKey) ->
                    m.getOrDefault(mKey, null)?.let { value -> putter(editor, key, value) }
                }

                putMap(
                    "avatar_detection_enabled" to "detection",
                    "avatar_follow_enabled" to "follow",
                    "avatar_talk_enabled" to "talk",
                    "avatar_welcome_start" to "welcomeStart",
                    "avatar_welcome_see" to "welcomeSee",
                    "avatar_zero_pos_enabled" to "zeroPos",
                    "avatar_smart_mute_enabled" to "mute",
                    "avatar_exhibition_mode" to "exhibitionMode",
                    "avatar_autostart" to "autostart",
                    "debug_mode" to "debugMode",
                ) { key, value -> putBoolean(key, value as Boolean) }

                putMap(
                    "avatar_follow_timeout" to "followTimeout",
                    "avatar_follow_dist" to "followDist",
                    "avatar_follow_angle" to "followAngle",
                    "avatar_follow_pitch_offset" to "pitchOffset",
                    "avatar_talk_dist" to "talkDist",
                    "avatar_talk_angle" to "talkAngle",
                    "avatar_talk_timeout" to "talkTimeout",
                    "avatar_cancel_call_timeout" to "cancelCallTime",
                    "avatar_zero_pos_pitch" to "zeroPosPitch",
                    "avatar_zero_pos_yaw" to "zeroPosYaw",
                    "avatar_data_transfer" to "dataTransfer",
                ) { key, value -> putString(key, value.toString()) }

                editor.putString("node_id", m.getOrDefault("nodeId", null)?.toString() ?: "")
                editor.commit()

                callback(m)
            }
        }
    }

    companion object {

        fun logJson(tag: String, json: String) = try {
            val jsonObject = JSONObject(json)
            Log.e(tag, jsonObject.toString(2))
        } catch (e: Exception) {
            Log.e(tag, "Invalid JSON: ${e.message}")
        }

        const val TAG = "ApiClient"
        private fun log(s: String) = Log.e(TAG, s)
        //                private const val API_URL: String = "https://api.wehead.dev/api/v0"  // PROD
//        private const val API_URL: String = "https://link-srv-staging-ule2kkd6ca-ew.a.run.app/api/v0" // STAGING
//        private const val API_URL: String = "http://192.168.50.27:8000/api/v0"
        const val API_URL: String = "https://staging.wehead.dev/api/v0"  // STAGING

//        const val API_URL = "https://link-srv-dev-ule2kkd6ca-ew.a.run.app/api/v0"
    }
}
