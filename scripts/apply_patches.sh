#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TMP_DIR="$(mktemp -d)"

# Define patches: name|git_url|base_commit|patch_file|target_dir
PATCHES=(
  "clam|https://github.com/seahorn/clam|9ce8172cb1658a62687d0420121e038f793ae4fc|src/patches/clam.patch|src/clam"
  "crab|https://github.com/seahorn/crab|146f5399c72ff508f176e6392e490647ac657ce7|src/patches/crab.patch|src/crab"
  "elina|https://github.com/eth-sri/ELINA|f524156d292ac3a6f3cd676e2d2e7db6629e2b6f|src/patches/elina.patch|src/elina"
)

# Cleanup temp directory on exit
cleanup() {
  rm -rf "$TMP_DIR"
}
trap cleanup EXIT

# Apply patches to target directories
for entry in "${PATCHES[@]}"; do
  IFS='|' read -r NAME URL BASE_COMMIT PATCH_REL TARGET_REL <<< "$entry"
  
  PATCH_FILE="$ROOT/$PATCH_REL"
  TARGET_DIR="$ROOT/$TARGET_REL"
  WORK_REPO="$TMP_DIR/${NAME}"
  
  # Skip if patch doesn't exist
  if [[ ! -f "$PATCH_FILE" ]]; then
    echo "⊘ Skipping $NAME: patch not found at $PATCH_FILE"
    continue
  fi
  
  # Display patch application info
  echo "Applying patch for: $NAME"
  echo "  Repo:    $URL"
  echo "  Commit:  $BASE_COMMIT"
  echo "  Patch:   $PATCH_FILE"
  echo "  Target:  $TARGET_DIR"
  
  # Clone repo at base commit and apply patch
  git clone "$URL" "$WORK_REPO" >/dev/null 2>&1
  pushd "$WORK_REPO" >/dev/null
  
  git checkout --detach "$BASE_COMMIT" >/dev/null 2>&1
  
  # Verify patch applies cleanly before proceeding
  if ! git apply --check "$PATCH_FILE" >/dev/null 2>&1; then
    echo "✗ Failed: patch does not apply cleanly" >&2
    popd >/dev/null
    exit 1
  fi
  
  # Apply patch to cloned repo
  git apply "$PATCH_FILE" >/dev/null 2>&1
  
  # Sync patched repo back to target directory, excluding .git
  rsync -a --delete --exclude ".git" --exclude ".git/" "$WORK_REPO"/ "$TARGET_DIR"/
  
  popd >/dev/null
  
  echo "✓ Done: patch applied and synced to $TARGET_DIR"
  echo
done

echo "All patches applied and synced successfully."
