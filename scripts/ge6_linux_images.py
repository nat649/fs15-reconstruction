"""The two existing private GE6 inputs accepted by the Linux instruction loader."""
import hashlib
from native_gar import EXPECTED_IMAGE

LOCAL_IMAGE = 'e8a9f3b4397fb9aca31fdcb7471728f4f6ef7940a85911f1f727dbe09377a955'
IMAGE_KINDS = {EXPECTED_IMAGE: 'reference', LOCAL_IMAGE: 'local-platform'}


def image_kind(data):
    digest = hashlib.sha256(data).hexdigest()
    if digest not in IMAGE_KINDS:
        raise ValueError('GE6 image hash differs from the pinned reference/local-platform inputs')
    return IMAGE_KINDS[digest]
