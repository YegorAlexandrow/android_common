package tech.fastsense.common.autostart

import com.google.firebase.database.Exclude

data class AutostartPreconditions(
    var autostartEnabled: Boolean? = null,
    var updateNotInProgress: Boolean? = null,
    var internetConnection: Boolean? = null,
    var settingsClosed: Boolean? = null,
    var version: String="---",
    var charge: Int? = null,
    var soundTest: SoundTestState? = null
) {
    enum class SoundTestState {
        PASSED, FAILED, RUNNING, NOT_REQUIRED
    }

    @get:Exclude
    val check
        get() = AutostartPreconditionsResult(
            autostartEnabled = autostartEnabled,
            updateNotInProgress = updateNotInProgress,
            internetConnection = internetConnection,
            settingsClosed = settingsClosed,
            versionOk = with(version) {
                split('.').getOrNull(1)?.toIntOrNull() ?: 0 >= 17 || contains("---")
            },
            chargeOk = charge?.let { it >= 20 },
            soundTestOk = soundTest?.let { it == SoundTestState.PASSED || it == SoundTestState.NOT_REQUIRED }
        )

    @get:Exclude
    val unsatisfiedConditionsMsg: String
        get() {
            val cc = check
            if (cc.all) return "Avatar autostart conditions satisfied."
            var msg = StringBuilder("Avatar autostart unsatisfied conditions:")

            val falseOrNullAppend = { s: Boolean?, falseMsg: String, nullMsg: String ->
                when (s) {
                    false -> msg.append("\n• ").append(falseMsg)
                    null -> msg.append("\n• ").append(nullMsg)
                    else -> {}
                }
            }
            falseOrNullAppend(
                cc.autostartEnabled,
                "autostart disabled in avatar settings",
                "unknown autostart settings"
            )
            falseOrNullAppend(
                cc.updateNotInProgress,
                "app update in progress",
                "unknown app update state"
            )
            falseOrNullAppend(
                cc.internetConnection,
                "no internet connection",
                "unknown internet state"
            )
            falseOrNullAppend(
                cc.settingsClosed,
                "settings opened",
                "unknown settings state"
            )
            falseOrNullAppend(
                cc.versionOk,
                "wrong app version (must be X.17.X or greater, current: $version)",
                "unknown app version"
            )
            falseOrNullAppend(
                cc.chargeOk,
                "not enough battery charge (must be 20% or greater, current: $charge%)",
                "unknown battery charge"
            )
            falseOrNullAppend(
                cc.soundTestOk,
                "sound test not passed (current state: ${soundTest?.name}",
                "unknown sound test state"
            )
            return msg.toString()
        }
}