@file:Suppress("PrivateApi", "NewApi")

package tech.fastsense.common.network

import android.net.wifi.SoftApConfiguration
import android.net.wifi.WifiManager
import java.util.concurrent.Executor

val BAND_2GHZ = 1 shl 0
val BAND_5GHZ = 1 shl 1
val BAND_6GHZ = 1 shl 2

private val getSoftApConfigurationMethod =
    WifiManager::class.java.getDeclaredMethod("getSoftApConfiguration")
private val setSoftApConfigurationMethod =
    WifiManager::class.java.getDeclaredMethod(
        "setSoftApConfiguration", SoftApConfiguration::class.java
    )

fun WifiManager.getSoftApConfiguration() =
    getSoftApConfigurationMethod(this) as SoftApConfiguration?

fun WifiManager.setSoftApConfiguration(c: SoftApConfiguration) =
    setSoftApConfigurationMethod(this, c) as Boolean

private val builderClass = Class.forName("android.net.wifi.SoftApConfiguration\$Builder")
private val setPassphraseMethod = builderClass.getDeclaredMethod(
    "setPassphrase", String::class.java, Int::class.java
)
private val setSsidMethod = builderClass.getDeclaredMethod("setSsid", String::class.java)
private val setAutoShutdownEnabledMethod =
    builderClass.getDeclaredMethod("setAutoShutdownEnabled", Boolean::class.java)
private val setBandMethod = builderClass.getDeclaredMethod("setBand", Int::class.java)
private val setChannelMethod =
    builderClass.getDeclaredMethod("setChannel", Int::class.java, Int::class.java)

private val buildMethod = builderClass.getDeclaredMethod("build")

class SoftApConfigurationBuilder(c: SoftApConfiguration?) {
    private val builder: Any? = when (c) {
        null ->
            builderClass.constructors[0].newInstance()

        else ->
            builderClass.constructors[1].newInstance(c)
    }

    fun setSsid(ssid: String) = this.apply {
        setSsidMethod(builder, ssid)
    }

    fun setAutoShutdownEnabled(enable: Boolean) = this.apply {
        setAutoShutdownEnabledMethod(builder, enable)
    }

    fun setPassphrase(passphrase: String, securityType: Int) = this.apply {
        setPassphraseMethod(builder, passphrase, securityType)
    }

    fun setBand(band: Int) = this.apply {
        setBandMethod(builder, band)
    }

    fun setChannel(channel: Int, band: Int) = this.apply {
        setChannelMethod(builder, channel, band)
    }

    fun build() = buildMethod(builder) as SoftApConfiguration
}

val softApCallbackClass = Class.forName("android.net.wifi.WifiManager\$SoftApCallback")
private val registerSoftApCallbackMethod = WifiManager::class.java.getDeclaredMethod(
    "registerSoftApCallback", Executor::class.java, softApCallbackClass
)

fun WifiManager.registerSoftApCallback(e: Executor, p: Any) =
    registerSoftApCallbackMethod(this, e, p) as Unit?
