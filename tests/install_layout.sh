#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
VERSION=0.1.0-stoned-dev
tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT

mkdir -p "$tmpdir/home" "$tmpdir/state" "$tmpdir/config/mise" "$tmpdir/project"

make -C "$ROOT_DIR" mise-install MISE_DATA_DIR="$tmpdir/mise" MISE_VERSION="$VERSION" >/dev/null

printf '%s\n' \
  'require "rbconfig"' \
  'puts RbConfig::CONFIG["ruby_version"]' \
  'puts RbConfig.ruby.end_with?("/ruby")' \
  'puts File.exist?(RbConfig.ruby)' \
  >"$tmpdir/project/check.rb"

env \
  HOME="$tmpdir/home" \
  XDG_CONFIG_HOME="$tmpdir/config" \
  XDG_STATE_HOME="$tmpdir/state" \
  MISE_DATA_DIR="$tmpdir/mise" \
  mise use --cd "$tmpdir/project" --path "$tmpdir/project/mise.toml" "ruby@$VERSION" >/dev/null

env \
  HOME="$tmpdir/home" \
  XDG_CONFIG_HOME="$tmpdir/config" \
  XDG_STATE_HOME="$tmpdir/state" \
  MISE_DATA_DIR="$tmpdir/mise" \
  mise exec --cd "$tmpdir/project" -- ruby "$tmpdir/project/check.rb" >"$tmpdir/out"

printf '4.0.0\ntrue\ntrue\n' >"$tmpdir/expected"
cmp -s "$tmpdir/expected" "$tmpdir/out"
