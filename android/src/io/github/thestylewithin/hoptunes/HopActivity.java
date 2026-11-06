// SPDX-License-Identifier: GPL-3.0-or-later
package io.github.thestylewithin.hoptunes;

import android.hardware.input.InputManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;

import org.qtproject.qt.android.bindings.QtActivity;

// The app's activity: Qt's own, plus game controllers. Android delivers a controller's buttons as key events and its
// sticks, d-pad hat and analog triggers as motion events; here they become the same plain action names the desktop
// Gamepad class uses ("left" "right" "up" "down" "a" "b" "x" "y" "lb" "rb" "lt" "rt" "start" "back") and are handed
// to C++ (Gamepad::fromJava). The events are only taken away from the rest of the app while a game is listening, so
// a controller still moves around the normal screens. A keyboard is never touched: its keys go to Qt as before.
public class HopActivity extends QtActivity {
    private static volatile boolean sListening = false;
    private static volatile HopActivity sInstance = null;
    private InputManager mInputManager;
    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private String mLastReported = null;
    private boolean mResumed = false;
    // Android takes a moment to finish recognising a controller (it can announce it before its buttons are listed), and a
    // clip-on one such as the Red Magic Gravity X more than most. So the activity also looks every 300 ms while it is in
    // front, and tells the app the moment the list changes, instead of waiting only for Android's announcement.
    private final Runnable mPoll = new Runnable() {
        @Override public void run() {
            if (!mResumed) return;
            report();
            mHandler.postDelayed(this, 300);
        }
    };
    private InputManager.InputDeviceListener mListener;

    // the analog axes seen as pressed (edge detection), so a trigger or the d-pad hat makes one press, not a stream
    private boolean mLeft, mRight, mUp, mDown, mLt, mRt;

    private static native void nativeAction(String action, boolean down);   // Gamepad::fromJava
    private static native void nativeRaw(String text);                      // for the diagnostics page
    private static native void nativeDevice(String name);                   // "" = none connected

    // called from C++ (Gamepad::setListening)
    public static void setListening(boolean on) { sListening = on; }

    // called from C++: the name of the first game controller that is connected, or ""
    public static String controllerName() {
        for (int id : InputDevice.getDeviceIds()) {
            InputDevice d = InputDevice.getDevice(id);
            if (d != null && isController(d)) return d.getName();
        }
        return "";
    }

    private static boolean isController(InputDevice d) {
        final int s = d.getSources();
        return !d.isVirtual() && ((s & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD
                || (s & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK);
    }

    private static boolean fromController(int source) {
        return (source & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD
                || (source & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK
                || (source & InputDevice.SOURCE_DPAD) == InputDevice.SOURCE_DPAD;
    }

    private static String actionFor(int keyCode) {
        switch (keyCode) {
            case KeyEvent.KEYCODE_DPAD_LEFT:  return "left";
            case KeyEvent.KEYCODE_DPAD_RIGHT: return "right";
            case KeyEvent.KEYCODE_DPAD_UP:    return "up";
            case KeyEvent.KEYCODE_DPAD_DOWN:  return "down";
            case KeyEvent.KEYCODE_BUTTON_A:   return "a";
            case KeyEvent.KEYCODE_BUTTON_B:   return "b";
            case KeyEvent.KEYCODE_BUTTON_X:   return "x";
            case KeyEvent.KEYCODE_BUTTON_Y:   return "y";
            case KeyEvent.KEYCODE_BUTTON_L1:  return "lb";
            case KeyEvent.KEYCODE_BUTTON_R1:  return "rb";
            case KeyEvent.KEYCODE_BUTTON_L2:  return "lt";
            case KeyEvent.KEYCODE_BUTTON_R2:  return "rt";
            case KeyEvent.KEYCODE_BUTTON_START:  return "start";
            case KeyEvent.KEYCODE_BUTTON_SELECT: return "back";
            default: return null;
        }
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        sInstance = this;
        mInputManager = (InputManager) getSystemService(INPUT_SERVICE);
        if (mInputManager != null) {
            mListener = new InputManager.InputDeviceListener() {
                @Override public void onInputDeviceAdded(int id) { report(); }
                @Override public void onInputDeviceRemoved(int id) { report(); }
                @Override public void onInputDeviceChanged(int id) { report(); }
            };
            mInputManager.registerInputDeviceListener(mListener, null);
        }
        report();
    }

    @Override
    public void onDestroy() {
        if (mInputManager != null && mListener != null) mInputManager.unregisterInputDeviceListener(mListener);
        if (sInstance == this) sInstance = null;
        super.onDestroy();
    }

    private void report() {
        final String name = controllerName();
        if (name.equals(mLastReported)) return;          // nothing new
        mLastReported = name;
        try { nativeDevice(name); } catch (UnsatisfiedLinkError e) { mLastReported = null; /* native side not ready yet: try again */ }
    }

    @Override
    protected void onResume() {
        super.onResume();
        mResumed = true;
        report();
        mHandler.removeCallbacks(mPoll);
        mHandler.postDelayed(mPoll, 300);
    }

    @Override
    protected void onPause() {
        mResumed = false;
        mHandler.removeCallbacks(mPoll);
        super.onPause();
    }

    private static void send(String action, boolean down, String raw) {
        try { nativeRaw(raw); nativeAction(action, down); } catch (UnsatisfiedLinkError e) { /* not ready */ }
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent e) {
        if (fromController(e.getSource())) {
            final String action = actionFor(e.getKeyCode());
            if (action != null) {
                final boolean down = e.getAction() == KeyEvent.ACTION_DOWN;
                if (!(down && e.getRepeatCount() > 0)) {   // the C++ side makes its own auto-repeat
                    send(action, down, (down ? "down " : "up ") + KeyEvent.keyCodeToString(e.getKeyCode()) + " -> " + action);
                }
                if (sListening) return true;                // a game is listening: the rest of the app does not see it
            }
        }
        return super.dispatchKeyEvent(e);
    }

    @Override
    public boolean dispatchGenericMotionEvent(MotionEvent e) {
        if (fromController(e.getSource()) && e.getAction() == MotionEvent.ACTION_MOVE) {
            final float hx = e.getAxisValue(MotionEvent.AXIS_HAT_X), hy = e.getAxisValue(MotionEvent.AXIS_HAT_Y);
            final float sx = e.getAxisValue(MotionEvent.AXIS_X), sy = e.getAxisValue(MotionEvent.AXIS_Y);
            final float lt = Math.max(e.getAxisValue(MotionEvent.AXIS_LTRIGGER), e.getAxisValue(MotionEvent.AXIS_BRAKE));
            final float rt = Math.max(e.getAxisValue(MotionEvent.AXIS_RTRIGGER), e.getAxisValue(MotionEvent.AXIS_GAS));
            final boolean left = hx < -0.5f || sx < -0.6f, right = hx > 0.5f || sx > 0.6f;
            final boolean up = hy < -0.5f || sy < -0.6f, down = hy > 0.5f || sy > 0.6f;
            final boolean ltd = lt > 0.5f, rtd = rt > 0.5f;
            if (left != mLeft)   { mLeft = left;   send("left", left, "axis left " + (left ? "on" : "off")); }
            if (right != mRight) { mRight = right; send("right", right, "axis right " + (right ? "on" : "off")); }
            if (up != mUp)       { mUp = up;       send("up", up, "axis up " + (up ? "on" : "off")); }
            if (down != mDown)   { mDown = down;   send("down", down, "axis down " + (down ? "on" : "off")); }
            if (ltd != mLt)      { mLt = ltd;      send("lt", ltd, "axis L2 " + (ltd ? "on" : "off")); }
            if (rtd != mRt)      { mRt = rtd;      send("rt", rtd, "axis R2 " + (rtd ? "on" : "off")); }
            if (sListening) return true;
        }
        return super.dispatchGenericMotionEvent(e);
    }
}
