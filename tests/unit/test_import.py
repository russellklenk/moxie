import moxie
import moxie._moxie_core as _mcore

def test_invalid_job_id():
    assert _mcore.JOB_ID_INVALID == 0
