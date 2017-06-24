import os

import pyrax

from xonsh.tools import print_color

from rever.activity import activity


$ACTIVITIES = ['version_bump', 'pyne', 'nuc_data_make', 'gtest', 'tag']

$VERSION_BUMP_PATTERNS = [
    ('cyclus/__init__.py', '__version__\s*=.*', "__version__ = '$VERSION'"),
    ]
$TAG_REMOTE = 'git@github.com:cyclus/cyclus.git'
$TAG_PUSH = False

def ensure_repo(url, targ):
    """Makes sure that a repo dir is present and up-to-date for rever"""
    targ_dir = os.path.join($REVER_DIR, targ)
    if os.path.isdir(targ_dir):
        cwd = $PWD
        cd @(targ_dir)
        git checkout -- .
        git pull
        cd @(cwd)
    else:
        git clone --depth=1 @(url) @(targ_dir)
    return targ_dir


@activity
def pyne():
    """Updates PyNE files."""
    # get pyne
    ensure_repo('https://github.com/pyne/pyne.git', 'pyne')
    cwd = $PWD
    # amalgamate
    cd $REVER_DIR/pyne
    ./amalgamate.py -s pyne.cc -i pyne.h
    cd @(cwd)
    cp $REVER_DIR/pyne/pyne.* src/
    git add src/pyne.*
    git commit -am "Updated PyNE for $VERSION"


#
# Nuc data make
#
def push_rackspace(fname, cred_file='rs.cred'):
    pyrax.set_credential_file(cred_file)
    cf = pyrax.cloudfiles
    with open(fname, 'rb') as f:
        fdata = f.read()
    cont = cf.get_container("cyclus-data")
    obj = cf.store_object("cyclus-data", fname, fdata)


@activity
def nuc_data_make():
    """Makes nuclear data for cyclus"""
    if not os.path.isfile('rs.cred'):
        raise RuntimeError('No rackspace creditial file "rs.cred". Please place this file '
                           'in the root directory of this repository. Never commit this file!')
    nuc_data_make -o $REVER_DIR/cyclus_nuc_data.h5 \
        -m atomic_mass,scattering_lengths,decay,simple_xs,materials,eaf,wimsd_fpy,nds_fpy
    # setup pyrax
    pyrax.set_setting("identity_type", "rackspace")
    pyrax.set_setting('region', 'ORD')
    pyrax.set_credential_file('rs.cred')
    # upload file
    print("list_containers: {}".format(cf.list_containers()))
    print("get_all_containers: {}".format(cf.get_all_containers()))
    push_rackspace($REVER_DIR + '/cyclus_nuc_data.h5')


@nuc_data_make.undoer
def nuc_data_make():
    print_color('{RED}Cannot un-upload cyclus_nuc_data.h5{NO_COLOR}')


#
# GTEST Update
#
@activity
def gtest():
    """Updates google test files."""
    # get pyne
    repo = ensure_repo('https://github.com/google/googletest.git', 'googletest')
    cwd = $PWD
    # amalgamate
    cd @(repo + 'googletest/scripts')
    ./fuse_gtest_files.py @(cwd + 'tests/GoogleTest')
    cd @(cwd)
    git add tests/GoogleTest
    git commit -am "Updated google tests for $VERSION"
