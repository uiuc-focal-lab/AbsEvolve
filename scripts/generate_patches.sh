#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TMP_DIR="$(mktemp -d)"

# Define repos: name|url|base_commit|snapshot|patch
REPOS=(
  "clam|https://github.com/seahorn/clam|9ce8172cb1658a62687d0420121e038f793ae4fc|src/clam|src/patches/clam.patch"
  "crab|https://github.com/seahorn/crab|146f5399c72ff508f176e6392e490647ac657ce7|src/crab|src/patches/crab.patch"
  "elina|https://github.com/eth-sri/ELINA|f524156d292ac3a6f3cd676e2d2e7db6629e2b6f|src/elina|src/patches/elina.patch"

)

# Cleanup temp directory on exit
cleanup() {
  rm -rf "$TMP_DIR"
}
trap cleanup EXIT

# Create patches directory
mkdir -p "$ROOT/src/patches"

# Generate patches for each repository
for entry in "${REPOS[@]}"; do
  IFS='|' read -r NAME URL BASE_COMMIT SNAPSHOT_REL PATCH_REL <<< "$entry"
  
  SNAPSHOT_DIR="$ROOT/$SNAPSHOT_REL"
  PATCH_FILE="$ROOT/$PATCH_REL"
  WORK_REPO="$TMP_DIR/${NAME}"
  
  # Validate snapshot exists before proceeding
  if [[ ! -d "$SNAPSHOT_DIR" ]]; then
    echo "Error: snapshot directory not found: $SNAPSHOT_DIR" >&2
    exit 1
  fi
  
  # Display patch generation info
  echo "Generating patch for: $NAME"
  echo "  URL:      $URL"
  echo "  Commit:   $BASE_COMMIT"
  echo "  Snapshot: $SNAPSHOT_DIR"
  echo "  Output:   $PATCH_FILE"
  
  # Remove old patch file
  rm -f "$PATCH_FILE"
  
  # Clone repo, checkout base commit, overlay snapshot, and generate patch
  git clone "$URL" "$WORK_REPO" >/dev/null 2>&1
  pushd "$WORK_REPO" >/dev/null
  
  git checkout --detach "$BASE_COMMIT" >/dev/null 2>&1
  
  # Overlay snapshot files, excluding .git entries that may be submodule pointers
  rsync -a --delete --exclude ".git" --exclude ".git/" "$SNAPSHOT_DIR"/ "$WORK_REPO"/
  
  # Clean up any gitignored files from snapshot
  git clean -fdX >/dev/null 2>&1 || true
  
  # Stage all changes (new files, modifications, deletions) for diff
  git add -A >/dev/null 2>&1
  
  # Generate binary patch with full-index for reproducibility
  git diff --cached --binary --full-index "$BASE_COMMIT" > "$PATCH_FILE"
  
  popd >/dev/null
  
  # Report completion status
  if [[ -s "$PATCH_FILE" ]]; then
    echo "Done: $PATCH_FILE"
  else
    echo "Done (empty patch): $PATCH_FILE"
  fi
  echo
done

echo "All patches generated successfully."
