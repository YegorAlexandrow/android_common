package tech.fastsense.common.cloud_storage

import android.content.Context
import androidx.preference.PreferenceManager
import com.google.firebase.storage.FirebaseStorage
import java.io.InputStream
import java.io.OutputStream
import java.io.PipedInputStream
import java.io.PipedOutputStream

class FirebaseStorageApi(private val context: Context) : CloudStorageApi {
    private val storage = FirebaseStorage.getInstance("gs://wehead-3362c-lmt8d/")

    private val streams = mutableMapOf<String, InputStream>()

    private val deviceId
        get() =
            PreferenceManager.getDefaultSharedPreferences(context).getString("device_id", "no_id")!!

    private val loc
        get() =
            PreferenceManager.getDefaultSharedPreferences(context).getString("location", "no_loc")!!

    override fun openDebugStream(fileName: String): OutputStream {
        if (streams.containsKey(fileName)) throw Exception("Multiple streams opened to the same file")
        val ostream = PipedOutputStream()
        streams[fileName] = PipedInputStream(ostream)
        storage.reference.child("debug/$deviceId/$loc/$fileName")
            .putStream(streams[fileName]!!)
            .addOnSuccessListener {
                streams[fileName]!!.close()
                streams.remove(fileName)
            }
        return ostream
    }

}