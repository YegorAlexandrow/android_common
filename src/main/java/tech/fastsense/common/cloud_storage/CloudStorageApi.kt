package tech.fastsense.common.cloud_storage

import java.io.OutputStream

interface CloudStorageApi {
    fun openDebugStream(fileName: String): OutputStream
}