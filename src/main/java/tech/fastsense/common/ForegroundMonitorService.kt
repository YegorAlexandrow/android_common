package tech.fastsense.common

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.os.IBinder
import androidx.core.app.NotificationCompat
import androidx.preference.PreferenceManager
import org.koin.core.component.KoinComponent
import org.koin.core.component.inject
import java.util.Timer
import java.util.TimerTask


class ForegroundMonitorService: Service(), KoinComponent {
    private val statusLogger: StatusLogger by inject()
    private lateinit var monitorTimer: Timer

    private lateinit var notificationManager: NotificationManager
    private lateinit var notificationBuilder: NotificationCompat.Builder

    companion object {
        private var prevState: String = "off"
        private var bringForegroundAllowed: Boolean = true
        private var outOfFocusAt: Long = -1L

        private const val NOTIFICATION_ID: Int = 8999
        private const val OUT_OF_FOCUS_TIMEOUT: Int = 55 * 1000
        private const val NOTIFICATION_DURATION: Int = 10 * 1000
    }

    private var monitorTimerTask: TimerTask = object : TimerTask() {
        override fun run() {
            val currState = statusLogger.state
            val curSubState = statusLogger.subState

            val currTime = System.currentTimeMillis()
            val dt = OUT_OF_FOCUS_TIMEOUT - NOTIFICATION_DURATION

            val settings = PreferenceManager.getDefaultSharedPreferences(applicationContext)
            val isExh = settings.getBoolean("avatar_exhibition_mode", false)

            if (currState == "off" && (curSubState == "user_closed" || curSubState == "any_closed") && isExh) {
                if (currState != prevState) {
                    outOfFocusAt = currTime
                    bringForegroundAllowed = true
                }

                if (bringForegroundAllowed && outOfFocusAt > 0) {
                    if (currTime - outOfFocusAt > OUT_OF_FOCUS_TIMEOUT) {
                        goHome()
                    } else if (currTime - outOfFocusAt in dt..(dt+999)) {
                        showNotification()
                    }
                }
            }

            prevState = currState
        }
    }

    private fun goHome() {
        val startMain = Intent(Intent.ACTION_MAIN)
        startMain.addCategory(Intent.CATEGORY_HOME)
        startMain.flags = Intent.FLAG_ACTIVITY_NEW_TASK
        startActivity(startMain)
    }

    class ActionReceiver : BroadcastReceiver() {
        override fun onReceive(context: Context?, intent: Intent?) {
            bringForegroundAllowed = false
        }
    }

    override fun onBind(intent: Intent?): IBinder? {
        return null
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        prepareNotification()

        this.monitorTimer = Timer()
        this.monitorTimer.schedule(monitorTimerTask, 0, 1000)

        return START_STICKY
    }

    override fun onDestroy() {
        super.onDestroy()

        try {
            monitorTimer.cancel()
        } catch (e: Exception) {

        }
    }

    private fun showNotification() {
        notificationManager.notify("foo", NOTIFICATION_ID, notificationBuilder.build())
    }

    private fun prepareNotification() {
        val cancelIntent = Intent(applicationContext, ActionReceiver::class.java).apply {
            putExtra("action", "cancel")
        }
        val cancelPendingIntent: PendingIntent =
            PendingIntent.getBroadcast(applicationContext, 146, cancelIntent, PendingIntent.FLAG_IMMUTABLE)

        notificationBuilder = NotificationCompat.Builder(applicationContext, "head_request_foreground")

        notificationBuilder
            .addAction(
                R.drawable.ic_baseline_access_alarm_24,
                getString(R.string.foreground_notification_cancel),
                cancelPendingIntent
            )
            .setSmallIcon(R.drawable.ic_baseline_access_alarm_24)
            .setDeleteIntent(cancelPendingIntent)
            .setContentTitle(getString(R.string.foreground_notification_title))
            .setContentText(getString(R.string.foreground_notification_subtitle))
            .setPriority(Notification.PRIORITY_HIGH)
            .setStyle(NotificationCompat.BigTextStyle()
                .bigText(getString(R.string.foreground_notification_subtitle))
                .setBigContentTitle(getString(R.string.foreground_notification_title)))

        notificationManager = getSystemService(NOTIFICATION_SERVICE) as NotificationManager

        val channelId = "head_request_foreground"
        val channel = NotificationChannel(
            channelId,
            "Foreground request",
            NotificationManager.IMPORTANCE_HIGH
        )
        notificationManager.createNotificationChannel(channel)
        notificationBuilder.setChannelId(channelId)
    }
}
