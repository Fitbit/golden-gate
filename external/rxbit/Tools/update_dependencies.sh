xcode_version=$(xcodebuild -version | python3 -c "import sys, re; print(re.findall(r'Xcode ([0-9.]+)', sys.stdin.read())[0])")
xcode_version_major=$(python3 -c "print(int('$xcode_version'.split('.')[0]))")
echo "Xcode version major: $xcode_version_major"

echo "Applying Carthage build workaround to exclude Apple Silicon binaries. See https://github.com/Carthage/Carthage/issues/3019 for more details"

xcconfig=$(mktemp /tmp/static.xcconfig.XXXXXX)
trap 'rm -f "$xcconfig"' INT TERM HUP EXIT

# For Xcode 12 (beta 3+) make sure EXCLUDED_ARCHS is set to arm architectures otherwise
# the build will fail on lipo due to duplicate architectures.
echo "EXCLUDED_ARCHS__EFFECTIVE_PLATFORM_SUFFIX_simulator__NATIVE_ARCH_64_BIT_x86_64__XCODE_${xcode_version_major}00 = arm64 arm64e armv7 armv7s armv6 armv8" >> $xcconfig
echo 'EXCLUDED_ARCHS = $(inherited) $(EXCLUDED_ARCHS__EFFECTIVE_PLATFORM_SUFFIX_$(EFFECTIVE_PLATFORM_SUFFIX)__NATIVE_ARCH_64_BIT_$(NATIVE_ARCH_64_BIT)__XCODE_$(XCODE_VERSION_MAJOR))' >> $xcconfig

export XCODE_XCCONFIG_FILE="$xcconfig"

carthage checkout
carthage build --cache-builds --platform iOS,macOS
