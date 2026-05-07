#!/bin/zsh
set -e

echo "Installing xcodegen..."
brew install xcodegen

echo "Generating Xcode project..."
cd "$CI_PRIMARY_REPOSITORY_PATH/ios"
xcodegen generate --spec project.yml

echo "Xcode project generated."
