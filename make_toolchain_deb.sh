#!/bin/bash

set -e

function usage {
cat << EOF
 usage: $0 package_name version maintainer
EOF
}

if [[ "$#" -ne 3 ]]; then
	usage
	exit 0
fi

pkgname="$1"
subpkgname="toolchain"
pkgver="$2"
maintainer="$3"
arch="amd64"

tmpdir="/tmp/$pkgname-$subpkgname-deb-$$"
pkgdir="${tmpdir}/${pkgname}-${subpkgname}_${pkgver}_${arch}"
srcdir="."

DEBIAN_CONTROL="Package: %s
Version: %s
Section: devel
Priority: optional
Architecture: $arch
Depends: adbi (>= 3.0)
Installed-Size: %s
Maintainer: %s
Homepage: http://adbi.sprc.samsung.pl
Description: Sample toolchain for ADBI tool
 Sample toolchain for Android Dynamic Binary Instumentation tool.
"

DEBIAN_POSTINST="7z x -bd \"/usr/share/$pkgname/toolchain.7z\" -o\"/usr/share/$pkgname/\""
DEBIAN_POSTRM="rm -rf /usr/share/$pkgname/toolchain"

DEBIAN_CHANGELOG="$pkgname$pkgsubname (${pkgver%-*}) UNRELEASED; urgency=medium

  * Initial release.

 -- ASDD Team <sprc.asdd@partner.samsung.com>  Thu, 25 Sep 2014 14:20:02 +0200"

echo "Packaging..."

install -m 644 -D "$srcdir/COPYRIGHT" "$pkgdir/usr/share/doc/$pkgname-$subpkgname/copyright"
echo "$DEBIAN_CHANGELOG" > "$pkgdir/usr/share/doc/$pkgname-$subpkgname/changelog.Debian"
gzip --best "$pkgdir/usr/share/doc/$pkgname-$subpkgname/changelog.Debian"
chmod 644 "$pkgdir/usr/share/doc/$pkgname-$subpkgname/changelog.Debian.gz"

install -m 755 -d "$pkgdir/usr/share/$pkgname"
install -m 644 "$srcdir/toolchain.7z" "$pkgdir/usr/share/$pkgname/toolchain.7z"


size=$(du -s "$pkgdir" | cut -f 1)
install -m 755 -d "$pkgdir/DEBIAN"
printf "$DEBIAN_CONTROL" "$pkgname-$subpkgname" "$pkgver" "$size" "$maintainer" > "$pkgdir/DEBIAN/control"
echo "$DEBIAN_POSTINST" > "$pkgdir/DEBIAN/postinst"
echo "$DEBIAN_POSTRM"   > "$pkgdir/DEBIAN/postrm"

chmod 0755 "$pkgdir/DEBIAN/postinst"
chmod 0755 "$pkgdir/DEBIAN/postrm"

fakeroot dpkg-deb -Z gzip --build "$pkgdir"

lintian -i "${pkgdir}.deb"

#dpkg-sig -k 0B233A0C --sign builder "${pkgdir}.deb"

mv "${pkgdir}.deb" .

rm -rf $pkgdir
rmdir "$tmpdir"
