package tech.fastsense.common.models.api

import com.google.firebase.Timestamp

data class Link(val map: Map<String, Any>) {
    val _id: String by map
    val userId: String by map
    val deviceId: String by map

    val createAt: Any by map
    val expiresAt: Any by map

    val type: String by map
    val tags: Array<String> by map
}