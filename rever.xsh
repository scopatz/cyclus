import os

import pyrax

from xonsh.tools import print_color

from rever.activity import activity


$ACTIVITIES = ['version_bump', 'pyne', 'nuc_data_make']

$VERSION_BUMP_PATTERNS = [
    ('cyclus/__init__.py', '__version__\s*=.*', "__version__ = '$VERSION'"),
    ]


def ensure_pyne():
    """Makes sure that the pyne dir is present and up-to-date for rever"""
    pyne_dir = os.path.join($REVER_DIR, 'pyne')
    if os.path.isdir(pyne_dir):
        cwd = $PWD
        cd @(pyne_dir)
        git checkout -- .
        git pull
        cd @(cwd)
    else:
        git clone --depth=1 https://github.com/pyne/pyne.git $REVER_DIR/pyne


@activity
def pyne():
    """Updates PyNE files."""
    # get pyne
    ensure_pyne()
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

