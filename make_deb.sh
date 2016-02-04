#!/bin/bash

TOOLCHAINDIR="./toolchain64"
#PREFIX="arm-linux-androideabi-"
PREFIX="aarch64-linux-android-"
ARM_STRIP="$TOOLCHAINDIR/bin/${PREFIX}strip"
INTEL_STRIP="strip"

HELP2MAN="help2man"
GZIP="gzip"

PYTHON="/usr/bin/python2"
PYTHON_BIN_VERSION=$($PYTHON -V 2>&1 | cut -d ' ' -f 2)

# Ubuntu sucks
pythonpkg=$(dpkg -S "$PYTHON" | cut -d ':' -f 1)
PYTHON_PKG_VERSION=$(dpkg -l | awk '{print $2, $3}' | grep "^$pythonpkg" | cut -d ' ' -f 2 | cut -d '-' -f 1)

if [[ "$PYTHON_BIN_VERSION" != "$PYTHON_PKG_VERSION" ]]; then
	echo "Python binary version is $PYTHON_BIN_VERSION and python package ($pythonpkg) version is $PYTHON_PKG_VERSION"
	echo "$(lsb_release -i -s) sucks!"
fi

PYTHON_VERSION="$PYTHON_PKG_VERSION"
base=$(echo "$PYTHON_VERSION" | cut -d '.' -f -2)
last=$(echo "$PYTHON_VERSION" | cut -d '.' -f 3)
PYTHON_NEXT_VERSION="${base}.$(($last+1))"

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
pkgver="$2"
maintainer="$3"
arch="amd64"

tmpdir="/tmp/$pkgname-deb-$$"
pkgdir="${tmpdir}/${pkgname}_${pkgver}_${arch}"
srcdir="."

# python binaries

single_binaries=("adbiclient/adbi3"
				 "idk/adbipp"
				 "idk/autoadbi"
				 "idk/mkinj"
				 "idk/readinj"
				 "idk/inj")

# copy *.pyo and *.so from dirs
cp_dirs=("adbiclient" "idk")

DEBIAN_CONTROL="Package: %s
Version: %s
Section: devel
Priority: optional
Architecture: $arch
Depends: python (>= ${PYTHON_VERSION}), python (<< ${PYTHON_NEXT_VERSION}), python-pip
Recommends: $pkgname-toolchain
Installed-Size: %s
Maintainer: %s
Homepage: http://adbi.sprc.samsung.pl
Description: Android Dynamic Binary Instrumentation
 Android Dynamic Binary Instumentation tool.
"

DEBIAN_POSTINST="umask 022
tar -xf \"/usr/share/$pkgname/adbiclient.tar\" -C /usr/share/$pkgname/
tar -xf \"/usr/share/$pkgname/idk.tar\" -C /usr/share/$pkgname/
tar -xf \"/usr/share/$pkgname/target.tar\" -C /usr/share/$pkgname/
pip install pyelftools
pip install capstone
"

DEBIAN_POSTRM="rm -rf \"/usr/share/$pkgname/adbiclient\"
rm -rf \"/usr/share/$pkgname/idk\"
rm -rf \"/usr/share/$pkgname/target\"
"

USR_BIN_TEMPLATE="#!/bin/sh
PYTHONPATH=\"%s:\$PYTHONPATH\" python2 -O \"%s\" \"\$@\""


echo "Building..."

make

for dir in ${cp_dirs[@]}; do
	$PYTHON -O -m compileall "$srcdir/$dir"
done
for file in ${single_binaries[@]}; do
	$PYTHON -O -m py_compile "$srcdir/$file"
done


echo "Packaging..."

install -m 755 -d "$pkgdir/usr/bin/"                                    \
                  "$pkgdir/usr/share/$pkgname/adbiclient/"              \
                  "$pkgdir/usr/share/$pkgname/adbiclient/adbi"          \
                  "$pkgdir/usr/share/$pkgname/adbiclient/powercmd"      \
                  "$pkgdir/usr/share/$pkgname/idk/"                     \
                  "$pkgdir/usr/share/$pkgname/idk/cachebuilder"         \
                  "$pkgdir/usr/share/$pkgname/idk/cachereader"          \
                  "$pkgdir/usr/share/$pkgname/idk/cachereader/datatype" \
                  "$pkgdir/usr/share/$pkgname/idk/common"               \
                  "$pkgdir/usr/share/$pkgname/idk/dwexpr"               \
                  "$pkgdir/usr/share/$pkgname/idk/include"              \
                  "$pkgdir/usr/share/$pkgname/idk/libasdd"              \
                  "$pkgdir/usr/share/$pkgname/target"                   \
                  "$pkgdir/usr/share/man/man7"

for dir in ${cp_dirs[@]}; do
	for file in $(find "$srcdir/$dir" -iname '*\.pyo' -or -iname '*\.pyc'); do
		install -m 644 "$file" "${pkgdir}/usr/share/$pkgname/${file#$srcdir}"
		rm "$file"
	done
	for file in $(find "$srcdir/$dir" -iname '*\.so'); do
		install -s --strip-program="$INTEL_STRIP" -m 644 "$file" "${pkgdir}/usr/share/$pkgname/${file#$srcdir}"
	done
done

install -m 644 "$srcdir/idk/cachebuilder/schema.sql" "$pkgdir/usr/share/$pkgname/idk/cachebuilder/schema.sql"
install -m 644 "$srcdir/idk/inject.x" "$pkgdir/usr/share/$pkgname/idk/inject.x"

for file in $(ls "$srcdir/idk/include"); do
	install -m 644 "$srcdir/idk/include/$file" "$pkgdir/usr/share/$pkgname/idk/include/$file"
done

install -s --strip-program="$ARM_STRIP" -m 755 "$srcdir/adbiserver" "$pkgdir/usr/share/$pkgname/target/adbiserver"
install -s --strip-program="$ARM_STRIP" -m 755 "$srcdir/adbilog/adbilog" "$pkgdir/usr/share/$pkgname/target/adbilog"

# /usr/bin scripts and binaries
for file in ${single_binaries[@]}; do
	install -m 644 "$srcdir/${file}o" "$pkgdir/usr/share/$pkgname/${file}"
	rm "$srcdir/${file}o"
	printf "$USR_BIN_TEMPLATE" "/usr/share/$pkgname/${file%/*}" "/usr/share/$pkgname/${file}" > "$pkgdir/usr/bin/${file##*/}"
	chmod 755 "$pkgdir/usr/bin/${file##*/}"

	# manpage
	${HELP2MAN} -n "ADBI 3.0 ${file##*/}" -s 7 --no-discard-stderr "$srcdir/${file}" | $GZIP -9 > "$pkgdir/usr/share/man/man7/${file##*/}.7.gz" 	
	chmod 644 "$pkgdir/usr/share/man/man7/${file##*/}.7.gz"
done


# tar files for lintian
for file in $(ls "$pkgdir/usr/share/$pkgname/"); do
	tar --remove-files -C "$PWD" -cf "$pkgdir/usr/share/$pkgname/${file}.tar" -C "$pkgdir/usr/share/$pkgname" "$file"
	chmod 644 "$pkgdir/usr/share/$pkgname/${file}.tar"
done


# debi(l)anization
install -m 644 -D "$srcdir/COPYRIGHT" "$pkgdir/usr/share/doc/$pkgname/copyright"
install "$srcdir/CHANGELOG" "$pkgdir/usr/share/doc/$pkgname/changelog.Debian"
gzip --best "$pkgdir/usr/share/doc/$pkgname/changelog.Debian"
chmod 644 "$pkgdir/usr/share/doc/$pkgname/changelog.Debian.gz"

size=$(du -s "$pkgdir" | cut -f 1)

install -m 755 -d "$pkgdir/DEBIAN"
printf "$DEBIAN_CONTROL" "$pkgname" "$pkgver" "$size" "$maintainer" > "$pkgdir/DEBIAN/control"
echo "$DEBIAN_POSTINST" > "$pkgdir/DEBIAN/postinst"
echo "$DEBIAN_POSTRM"   > "$pkgdir/DEBIAN/postrm"

chmod 0755 "$pkgdir/DEBIAN/postinst"
chmod 0755 "$pkgdir/DEBIAN/postrm"

fakeroot dpkg-deb -Z gzip --build "$pkgdir"

lintian -i "${pkgdir}.deb"

#dpkg-sig -k 0B233A0C --sign builder "${pkgdir}.deb"

mv "${pkgdir}.deb" .

# clean
rm -rf "$pkgdir"
rmdir "$tmpdir"
