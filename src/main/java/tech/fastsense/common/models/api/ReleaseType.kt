package tech.fastsense.common.models.api

import com.google.gson.annotations.SerializedName

enum class ReleaseType(private val s: String) {
    @SerializedName("stable")
    STABLE("stable"),
    @SerializedName("rc")
    RC("rc"),
    @SerializedName("beta")
    BETA("beta"),
    @SerializedName("alpha")
    ALPHA("alpha"),
    @SerializedName("experiment")
    EXPERIMENT("experiment");

    override fun toString(): String {
        return s
    }
}
