# Sourced by the AppImage's AppRun before Hop-tunes starts.
#
# A few libraries are normally provided by the host and deliberately not bundled as regular
# libraries (graphics dispatch, and small system libraries that every full desktop has).
# Minimal or unusual installs can still lack them, so each one is shipped in its own
# usr/lib/fallback/<soname>/ directory and added to the search path ONLY when the host is
# missing it. Hosts that have the library always use their own copy.
_ht_host_has() {
  for _d in /usr/lib/x86_64-linux-gnu /usr/lib64 /usr/lib /lib/x86_64-linux-gnu /lib64; do
    [ -e "$_d/$1" ] && return 0
  done
  { ldconfig -p 2>/dev/null || /sbin/ldconfig -p 2>/dev/null; } | grep -q "$1"
}
for _ht_dir in "$APPDIR"/usr/lib/fallback/*/; do
  [ -d "$_ht_dir" ] || continue
  _ht_lib=$(basename "$_ht_dir")
  if ! _ht_host_has "$_ht_lib"; then
    export LD_LIBRARY_PATH="${_ht_dir%/}${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
  fi
done
unset _ht_dir _ht_lib _d
