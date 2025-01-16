package tech.fastsense.common.autostart

import com.google.firebase.database.Exclude

data class AutostartPreconditionsResult(
    val autostartEnabled: Boolean?,
    var updateNotInProgress: Boolean?,
    val internetConnection: Boolean?,
    val settingsClosed: Boolean?,
    val versionOk: Boolean?,
    val chargeOk: Boolean?,
    val soundTestOk: Boolean?
) {
    @get:Exclude
    val all
        get() = autostartEnabled ?: false &&
                updateNotInProgress ?: false &&
                internetConnection ?: false &&
                settingsClosed ?: false &&
                versionOk ?: false &&
                chargeOk ?: false &&
                soundTestOk ?: false
}