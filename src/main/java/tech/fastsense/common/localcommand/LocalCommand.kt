package tech.fastsense.common.localcommand

data class LocalCommand(
    val name: String,
    val payload: Map<String, String> = emptyMap(),
    val timestampMs: Long = System.currentTimeMillis(),
    val sender: String = "",
)
