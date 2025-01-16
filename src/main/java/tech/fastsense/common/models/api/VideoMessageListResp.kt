package tech.fastsense.common.models.api

data class VideoMessageListResp(
    val count: Int,
    val next: Int?,
    val prev: Int?,
    val results: List<VideoMessage>,
)
