package tech.fastsense.common.models.api

data class Release(
   val id: String,
   val notes: String,
   val versionCode: Int,
   val versionName: String,
   val createAt: Any,
   val directDownloadUri: String,
   val releaseType: ReleaseType
)
