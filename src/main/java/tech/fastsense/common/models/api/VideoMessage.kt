package tech.fastsense.common.models.api

import java.net.URL
import java.util.*

data class VideoMessage(
    val id: UUID,
    val displayName: String,
    val createdBy: String,
    val public: Boolean,

    val createdAt: String,
    val publishedAt: String? = null,
    val previewURL: URL,

    val metadata: Metadata = Metadata()
) {
    data class Metadata(
        val durationMillis: Long = 0,
        val playbackOptions: Map<String, Any> = emptyMap(),
        val crop: Map<String, Any> = emptyMap(),
    )
}
