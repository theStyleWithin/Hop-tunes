#!/bin/sh
# Runs a command; on failure, re-emits the tail of its output as a GitHub error annotation,
# so failures are readable through the API without downloading the raw job log.
# Usage: logged.sh <title> <command...>
title=$1; shift
log=$(mktemp)
"$@" > "$log" 2>&1
code=$?
cat "$log"
if [ $code -ne 0 ]; then
  # coreutils + sed only (minimal images may lack awk)
  msg=$(tail -n 45 "$log" | sed -e 's/%/%25/g' -e 's/\r//g' | tr '\n' '\001' | sed 's/\x01/%0A/g')
  echo "::error title=$title (exit $code)::$msg"
fi
rm -f "$log"
exit $code
