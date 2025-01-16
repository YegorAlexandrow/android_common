package tech.fastsense.common.models.api

import android.os.Bundle
import androidx.core.os.bundleOf
import java.net.URL

data class VideoMessageLinksResp(
    val videoDownloadURL: URL?,
    val coordsDownloadURL: URL?,
    val previewDownloadURL: URL?,
) {
    fun toBundle(): Bundle {
        return bundleOf(
            "videoURL" to videoDownloadURL.toString(),
            "coordsURL" to coordsDownloadURL.toString(),
            "previewURL" to previewDownloadURL.toString(),
        )
    }
}
