package tech.fastsense.common.models.api

data class IssueCreateIn(val map: Map<String, Any>) {
    val payload: String by map
    val callId: String by map
}
