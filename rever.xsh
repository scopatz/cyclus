import os

from rever.activity import activity

$ACTIVITIES = ['version_bump', 'pyne']

$VERSION_BUMP_PATTERNS = [
    ('cyclus/__init__.py', '__version__\s*=.*', "__version__ = '$VERSION'"),
    ]


@activity
def pyne():
    """Updates PyNE files."""
    # get pyne
    rm -rf $REVER_DIR/pyne
    cwd = os.getcwd()
    git clone --depth=1 https://github.com/pyne/pyne.git $REVER_DIR/pyne
    # amalgamate
    cd $REVER_DIR/pyne
    ./amalgamate.py -s pyne.cc -i pyne.h
    cd @(cwd)
    cp $REVER_DIR/pyne/pyne.* src/
    git add src/pyne.*
    git commit -am "Updated PyNE for $VERSION"
    # clean up
    rm -rf $REVER_DIR/pyne


