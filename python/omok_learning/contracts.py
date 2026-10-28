from omok_env import PROFILE

CONTRACT = dict(observation="renju15-observation-v1", action="row-major225-v1",
    mask="evaluate-legal-win-draw-v1", value="current-actor-wdl-v1",
    profile=PROFILE, profile_version=1, dtype="float32", layout="NCHW")
SAMPLE_SCHEMA = "omok-samples-v1"
PREPROCESS = "replay-board-outcome-d4-v1"


def transform_action(action, symmetry):
    if type(action) is not int or not 0 <= action < 225 or type(symmetry) is not int or not 0 <= symmetry < 8:
        raise ValueError("invalid action/symmetry")
    r,c=divmod(action,15)
    if symmetry >= 4:
        c=14-c
    for _ in range(symmetry%4):
        r,c=c,14-r
    return r*15+c


def spatial(values, symmetry):
    if len(values)%225:
        raise ValueError("spatial plane size")
    output=[0]*len(values)
    for offset in range(0,len(values),225):
        for a in range(225):
            output[offset+transform_action(a,symmetry)]=values[offset+a]
    return output


def augment(sample, symmetry):
    result=dict(sample)
    result["observation"]=spatial(sample["observation"],symmetry)
    result["mask"]=spatial(sample["mask"],symmetry)
    target=sample["policy"]
    if target["kind"] == "action":
        result["policy"]=dict(kind="action",action=transform_action(target["action"],symmetry))
    elif target["kind"] == "distribution":
        result["policy"]=dict(kind="distribution",probabilities=spatial(target["probabilities"],symmetry))
    else:
        raise ValueError("unknown policy target")
    return result
