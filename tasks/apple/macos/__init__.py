from invoke import Collection

from . import bindings
from . import host
from . import xp
ns = Collection()
ns.add_collection(bindings)
ns.add_collection(host)
ns.add_collection(xp)
