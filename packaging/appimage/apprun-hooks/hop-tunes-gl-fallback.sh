# Sourced by the AppImage's AppRun before Hop-tunes starts.
# Qt 6 links the GLVND OpenGL dispatcher (libOpenGL.so.0 + libGLdispatch.so.0). Graphics
# libraries normally must come from the host, so they are not bundled as regular libraries;
# but many distros don't install libOpenGL.so.0 by default. Use the bundled copies only for
# the pieces the host is missing.
_ht_host_has() {
  for _d in /usr/lib/x86_64-linux-gnu /usr/lib64 /usr/lib /lib/x86_64-linux-gnu /lib64; do
    [ -e "$_d/$1" ] && return 0
  done
  { ldconfig -p 2>/dev/null || /sbin/ldconfig -p 2>/dev/null; } | grep -q "$1"
}
for _ht_lib in libOpenGL.so.0 libGLdispatch.so.0; do
  if ! _ht_host_has "$_ht_lib"; then
    export LD_LIBRARY_PATH="$APPDIR/usr/lib/gl-fallback/${_ht_lib%%.so*}${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
  fi
done
unset _ht_lib _d
