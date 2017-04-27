# KAsync #

KAsync helps writing composable asynchronous code using a continuation based approach.

## Releases ##
KAsync is released as required and does not follow a specific release schedule.
Releases *always* happen directly from a release tag.

A tarball can be created like so:
    git archive --format=tar.xz --prefix=kasync-0.1.1/ v0.1.1 > kasync-0.1.1.tar.xz

You may need to add the following to your ~/.gitconfig for the xz compression to be available:
    [tar "tar.xz"]
        command = xz -c

Tarballs should be uploaded to unstable/kasync/$VERSION/src/kasync-$version.tar.xz

See ftp://upload.kde.org/README

For more information see techbase.kde.org/ReleasingExtragearSoftware
