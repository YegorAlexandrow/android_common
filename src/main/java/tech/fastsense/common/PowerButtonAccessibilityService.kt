package tech.fastsense.common

import android.accessibilityservice.AccessibilityService
import android.content.Intent
import android.view.accessibility.AccessibilityEvent

class PowerButtonAccessibilityService : AccessibilityService() {
    override fun onAccessibilityEvent(event: AccessibilityEvent) {}
    override fun onInterrupt() {}
    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        performGlobalAction(GLOBAL_ACTION_POWER_DIALOG)
        return super.onStartCommand(intent, flags, startId)
    }
}