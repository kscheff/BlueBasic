# Assumes that you tag versions with the version number (e.g., "1.1") and then the build number is
# that plus the number of commits since the tag (e.g., "1.1.17")

echo "Updating version/build number from git..."
plist=${INFOPLIST_FILE}

# increment the build number (ie 115 to 116)
versionnum=`git describe | awk '{split($0,a,"-"); print a[1]}'`
buildnum=`git describe | awk '{split($0,a,"-"); print a[1] "." a[2]}'`

if [[ "${versionnum}" == "" ]]; then
echo "No version number from git"
exit 2
fi

if [[ "${buildnum}" == "" ]]; then
echo "No build number from git"
exit 2
fi

/usr/libexec/Plistbuddy -c "Set CFBundleShortVersionString $buildnum" "${plist}"
echo "Updated version number to $buildnum"
/usr/libexec/Plistbuddy -c "Set CFBundleVersion $buildnum" "${plist}"
echo "Updated build number to $buildnum"
echo ${PRODUCT_NAME} $buildnum  >version.txt
