#!/usr/bin/env bash
# Usage: bash scripts/release.sh 0.2.0      (or 0.2.0-rc.1)
# Bumps VERSION, commits, creates an annotated tag vX.Y.Z. Does NOT push.
set -euo pipefail

V="${1:-}"
[[ "$V" =~ ^[0-9]+\.[0-9]+\.[0-9]+(-[0-9A-Za-z.]+)?$ ]] || { echo "usage: $0 X.Y.Z[-pre]"; exit 1; }
cd "$(dirname "$0")/.."

[[ -z "$(git status --porcelain)" ]] || { echo "error: working tree not clean"; exit 1; }
git rev-parse "v$V" >/dev/null 2>&1 && { echo "error: tag v$V already exists"; exit 1; }
grep -q "^## \[$V\]" CHANGELOG.md || { echo "error: add a '## [$V] - YYYY-MM-DD' section to CHANGELOG.md first"; exit 1; }

echo "$V" > VERSION
git add VERSION
git commit -m "Release v$V"
git tag -a "v$V" -m "AppleBoot v$V"

echo "Created tag v$V. Publish with:"
echo "  git push origin HEAD && git push origin v$V"
