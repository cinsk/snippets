#!/bin/bash

#
# Usage: cat jdk-from-centos.sh | ssh root@CENTOS-HOST | tar -xzf -
#

exec 9>&1
exec 1>&2

JDK_PACKAGE=java-1.8.0-openjdk

yum install -y yum-utils
yum install -y rpm-build

pushd .

# trap "rm -f /tmp/BUILD.$$" EXIT

mkdir -p /tmp/BUILD.$$
cd /tmp/BUILD.$$
yumdownloader --source "$JDK_PACKAGE"

for f in *.rpm; do
    rpm -i "$f"
done


cd ~/rpmbuild

base=$(pwd)

cd $base/SPECS/
# java-1.8.0-openjdk.spec

rpmbuild -bp --nodeps "${JDK_PACKAGE}.spec"

cd $base/BUILD/

exec 1>&9 9>&-
tar -czf - ${JDK_PACKAGE}*
