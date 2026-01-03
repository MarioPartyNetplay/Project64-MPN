#!/bin/bash

# Script to automatically update the git hash in Version.h
# Usage: ./update_git_hash.sh

set -e

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

VERSION_FILE="$PROJECT_ROOT/Source/Project64-core/Version.h"

# Get the current git hash (7 characters)
GIT_HASH=$(git rev-parse --short=7 HEAD)

echo "Updating git hash in $VERSION_FILE to: $GIT_HASH"

# Create a temporary file
TEMP_FILE=$(mktemp)

# Replace GIT_HASH_PLACEHOLDER with the actual git hash
sed "s/GIT_HASH_PLACEHOLDER/$GIT_HASH/g" "$VERSION_FILE" > "$TEMP_FILE"

# Move the temporary file back to the original location
mv "$TEMP_FILE" "$VERSION_FILE"

echo "Git hash updated successfully!"