// SPDX-License-Identifier: GPL-3.0-or-later
package io.github.thestylewithin.hoptunes;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.os.Build;
import android.os.IBinder;

// Keeps Hop-tunes alive while sound is playing with the app out of sight (music, or a stream whose picture was closed or left): Android stops or
// freezes a background app that is not a foreground service. It does nothing else: no sound goes through it, it only holds the app up and shows
// one small notification that opens the app. HopActivity starts it when playback starts and stops it when playback ends.
public class HopPlaybackService extends Service {
    private static final String CHANNEL = "playback";
    private static final int ID = 1;

    @Override public IBinder onBind(Intent intent) { return null; }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        try {
            final NotificationManager nm = (NotificationManager) getSystemService(NOTIFICATION_SERVICE);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O && nm != null)
                nm.createNotificationChannel(new NotificationChannel(CHANNEL, "Playing", NotificationManager.IMPORTANCE_LOW));
            final Intent open = getPackageManager().getLaunchIntentForPackage(getPackageName());
            final PendingIntent tap = open == null ? null
                    : PendingIntent.getActivity(this, 0, open, PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);
            final Notification.Builder b = Build.VERSION.SDK_INT >= Build.VERSION_CODES.O ? new Notification.Builder(this, CHANNEL) : new Notification.Builder(this);
            b.setContentTitle("Hop-tunes").setContentText("playing").setSmallIcon(getApplicationInfo().icon).setOngoing(true);
            if (tap != null) b.setContentIntent(tap);
            final Notification n = b.build();
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) startForeground(ID, n, ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PLAYBACK);
            else startForeground(ID, n);
        } catch (Throwable t) {
            stopSelf();      // (Android refused, e.g. no notification permission on some versions: playback itself is not affected)
        }
        return START_NOT_STICKY;
    }

    @Override
    public void onDestroy() {
        try { stopForeground(true); } catch (Throwable ignored) { }
        super.onDestroy();
    }
}
