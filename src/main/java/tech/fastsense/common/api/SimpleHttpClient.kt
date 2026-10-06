package tech.fastsense.common.api

import android.content.Context
import com.android.volley.AuthFailureError
import com.android.volley.DefaultRetryPolicy
import com.android.volley.NetworkResponse
import com.android.volley.Request
import com.android.volley.RequestQueue
import com.android.volley.Response
import com.android.volley.toolbox.HttpHeaderParser
import com.android.volley.toolbox.JsonObjectRequest
import com.android.volley.toolbox.StringRequest
import com.android.volley.toolbox.Volley
import com.google.firebase.auth.ktx.auth
import com.google.firebase.ktx.Firebase
import org.json.JSONObject

class SimpleHttpClient(context: Context, private val apiUrl: String = DEFAULT_API_URL) {
    private var requestQueue: RequestQueue = Volley.newRequestQueue(context)

    init {
        getIdToken { } //...
    }

    fun request(
        method: Int,
        route: String,
        body: Map<String, *>?,
        callback: (String) -> Unit,
        errorCallback: (Exception) -> Unit = {},
    ) {
        getIdToken(errorCallback = errorCallback) {
            run {
                val r = AuthorizedRequest(
                    method,
                    "$apiUrl$route",
                    it,
                    body,
                    { run { callback(it.toString()) } },
                    { errorCallback(it) }
                ).apply {
                    retryPolicy = DefaultRetryPolicy(5000, 2, 1.5f)
                }

                requestQueue.add(r)
            }
        }
    }

    fun stringRequest(
        method: Int,
        route: String,
        callback: (String) -> Unit,
        errorCallback: (Exception) -> Unit = {},
    ) {
        getIdToken(errorCallback = errorCallback) {
            run {
                val r = AuthorizedStringRequest(
                    method,
                    "$apiUrl$route",
                    it,
                    { callback(it) },
                    { errorCallback(it) }
                ).apply {
                    retryPolicy = DefaultRetryPolicy(5000, 2, 1.5f)
                }

                requestQueue.add(r)
            }
        }
    }

    fun getJson(
        route: String,
        errorCallback: (Exception) -> Unit = {},
        callback: (String) -> Unit
    ) {
        stringRequest(Request.Method.GET, route, callback, errorCallback)
    }

    /** GET, отдающий вместе с телом заголовки ответа (имена в нижнем регистре). */
    fun getJsonWithHeaders(
        route: String,
        errorCallback: (Exception) -> Unit = {},
        callback: (String, Map<String, String>) -> Unit
    ) {
        getIdToken(errorCallback = errorCallback) {
            run {
                val r = AuthorizedHeadersRequest(
                    "$apiUrl$route",
                    it,
                    { (body, headers) -> callback(body, headers) },
                    { errorCallback(it) }
                ).apply {
                    retryPolicy = DefaultRetryPolicy(5000, 2, 1.5f)
                }

                requestQueue.add(r)
            }
        }
    }

    fun getString(route: String, callback: (String) -> Unit) {
        stringRequest(Request.Method.GET, route, callback)
    }

    fun postString(route: String, callback: (String) -> Unit) {
        stringRequest(Request.Method.POST, route, callback)
    }

    fun postJson(route: String, body: Map<String, *>, callback: (String) -> Unit) {
        request(Request.Method.POST, route, body, callback)
    }

    fun getIdToken(errorCallback: (Exception) -> Unit = {}, callback: (String) -> Unit) {
        if (idToken == null || System.currentTimeMillis() - idTokenUpdatedAt > 1_800_000) {
            val auth = Firebase.auth
            val user = auth.currentUser ?: return

            user.getIdToken(true).addOnSuccessListener {
                run {
                    idToken = it.token
                    idTokenUpdatedAt = System.currentTimeMillis()
                    callback(it.token!!)
                }
            }.addOnFailureListener { exception ->
                errorCallback(exception)
            }
        } else {
            callback(idToken!!)
        }
    }

    private class AuthorizedRequest(
        method: Int,
        path: String,
        private val token: String,
        body: Map<String, *>?,
        listener: Response.Listener<JSONObject>,
        errorListener: Response.ErrorListener?,
    ) : JsonObjectRequest(
        method,
        path,
        body?.let { JSONObject(it) },
        listener,
        errorListener
    ) {
        @Throws(AuthFailureError::class)
        override fun getHeaders(): Map<String, String> {
            return mapOf(
                "Authorization" to "Bearer $token"
            )
        }
    }

    private class AuthorizedStringRequest(
        method: Int,
        path: String,
        private val token: String,
        listener: Response.Listener<String>,
        errorListener: Response.ErrorListener?,
    ) : StringRequest(
        method,
        path,
        listener,
        errorListener
    ) {
        @Throws(AuthFailureError::class)
        override fun getHeaders(): Map<String, String> {
            return mapOf(
                "Authorization" to "Bearer $token"
            )
        }
    }

    private class AuthorizedHeadersRequest(
        path: String,
        private val token: String,
        private val listener: Response.Listener<Pair<String, Map<String, String>>>,
        errorListener: Response.ErrorListener?,
    ) : Request<Pair<String, Map<String, String>>>(Method.GET, path, errorListener) {
        @Throws(AuthFailureError::class)
        override fun getHeaders(): Map<String, String> {
            return mapOf(
                "Authorization" to "Bearer $token"
            )
        }

        override fun parseNetworkResponse(
            response: NetworkResponse
        ): Response<Pair<String, Map<String, String>>> {
            val body = String(
                response.data,
                charset(HttpHeaderParser.parseCharset(response.headers, "UTF-8"))
            )
            val headers = response.headers.orEmpty().mapKeys { it.key.lowercase() }
            return Response.success(
                body to headers,
                HttpHeaderParser.parseCacheHeaders(response)
            )
        }

        override fun deliverResponse(response: Pair<String, Map<String, String>>) =
            listener.onResponse(response)
    }

    companion object {
        //        private const val DEFAULT_API_URL: String = "https://api.wehead.dev/api/v0"  // PROD
        private const val DEFAULT_API_URL: String = "https://staging.wehead.dev/api/v0"  // STAGING

        var idToken: String? = null
        private var idTokenUpdatedAt: Long = -1
    }
}