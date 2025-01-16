package tech.fastsense.common.models.api

data class IssueCreateResp(val map: Map<String, Any>) {
    val trackCode: String by map
    val createAt: String by map
}
