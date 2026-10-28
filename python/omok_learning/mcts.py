"""Sequential PUCT reference search. All game transitions use C++ clones.

No move pruning, tree reuse, transpositions, noise, batching or RL updates.
Evaluator boundary: evaluate(env, mask) -> {priors: probability225, value: [-1,1]}.
"""
from dataclasses import asdict,dataclass
import math
import random
import time


@dataclass(frozen=True)
class SearchConfig:
    simulations:int=16
    c_puct:float=1.5
    seed:int=23
    tie_break:str="index"

    def __post_init__(self):
        if type(self.simulations) is not int or self.simulations<1:
            raise ValueError("simulations must be a positive integer")
        if type(self.seed) is not int or type(self.c_puct) not in (int,float) or not math.isfinite(self.c_puct) or self.c_puct<0:
            raise ValueError("invalid seed/c_puct")
        if self.tie_break not in ("index","seeded"):
            raise ValueError("tie_break must be index or seeded")


@dataclass
class Edge:
    prior:float
    visits:int=0
    total:float=0.0
    child:object=None

    @property
    def q(self):
        return self.total/self.visits if self.visits else 0.0


@dataclass
class Node:
    actor:int|None
    visits:int=0  # Completed outgoing traversals, exactly sum(edge.visits).
    edges:dict|None=None


class SearchError(ValueError):
    def __init__(self,message,details):
        super().__init__(message)
        self.details=details


def terminal_value(snapshot,actor):
    """Known result from a preserved actor's perspective; terminal has no next actor."""
    if type(actor) is not int or actor not in (0,1) or not snapshot["terminated"]:
        raise ValueError("terminal value requires a known result and actor")
    if snapshot["result"]=="draw":
        return 0.0
    if snapshot["result"]=="won" and snapshot["winner"] in (0,1):
        return 1.0 if snapshot["winner"]==actor else -1.0
    raise ValueError("aborted/unknown results cannot be backed up")


def backup(path,value,perspective):
    # Compare explicit actors rather than guessing from prepared/sample ply parity.
    for node,edge in reversed(path):
        edge.visits+=1
        edge.total+=value if node.actor==perspective else -value
        node.visits+=1


def _choose(candidates,rng,mode):
    return min(candidates) if mode=="index" else rng.choice(sorted(candidates))


def select_edge(node,config,rng):
    scores={a:edge.q+config.c_puct*edge.prior*math.sqrt(node.visits)/(1+edge.visits)
            for a,edge in node.edges.items()}
    maximum=max(scores.values())
    tied=[a for a,score in scores.items() if score==maximum]
    # At N_parent=0 all Q+U are zero; choose the largest prior explicitly.
    prior=max(node.edges[a].prior for a in tied)
    return _choose([a for a in tied if node.edges[a].prior==prior],rng,config.tie_break)


def _expand(node,env,mask,evaluator,stats):
    if len(mask)!=225 or any(type(x) is not bool for x in mask) or not any(mask):
        raise ValueError("BlockedNoLegalAction or malformed mask")
    before=env.snapshot()
    stats["evaluation_attempts"]+=1
    result=evaluator.evaluate(env,mask)
    if env.snapshot()!=before:
        raise ValueError("evaluator mutated its environment")
    if "token" in result and result["token"]!=before["token"]:
        raise ValueError("stale evaluator result")
    priors=result["priors"];value=result["value"]
    if len(priors)!=225 or any(not math.isfinite(p) or p<0 for p in priors):
        raise ValueError("invalid policy probabilities")
    if any(p!=0 and not legal for p,legal in zip(priors,mask)):
        raise ValueError("policy has illegal probability mass")
    total=math.fsum(priors)
    if abs(total-1.0)>1e-5 or not math.isfinite(value) or not -1<=value<=1:
        raise ValueError("unnormalized policy or invalid value")
    node.edges={a:Edge(float(priors[a]/total)) for a,legal in enumerate(mask) if legal}
    stats["network_evaluations"]+=1
    stats["expanded_nodes"]+=1
    stats["network_ms"]+=float(result.get("forward_ms",0.0))
    return float(value)


def search(env,mask,evaluator,config=SearchConfig()):
    """Root expansion + exactly config.simulations completed root traversals.

    Every simulation starts from the same C++ snapshot. A nonterminal leaf's v is
    in its actor's perspective; terminal leaves use the last parent's actor.
    Each ancestor stores W/Q in that ancestor's actor perspective.
    """
    started=time.perf_counter()
    before=env.snapshot()
    stats=dict(simulations=0,network_evaluations=0,evaluation_attempts=0,expanded_nodes=0,
        terminal_hits=0,transitions=0,clones=0,max_depth=0,network_ms=0.0)
    rng=random.Random(config.seed)
    try:
        if before["phase"]!="playing" or before["next_actor"] not in (0,1):
            raise ValueError("cannot search a terminal/closed/not-ready environment")
        if tuple(mask)!=env.mask():
            raise ValueError("mask is not current environment mask")
        with env.clone() as base:
            stats["clones"]+=1
            root=Node(before["next_actor"])
            root_value=_expand(root,base,tuple(mask),evaluator,stats)
            for _ in range(config.simulations):
                with base.clone() as branch:
                    stats["clones"]+=1
                    node=root;path=[]
                    while True:
                        snapshot=branch.snapshot()
                        if snapshot["terminated"]:
                            perspective=path[-1][0].actor
                            value=terminal_value(snapshot,perspective)
                            stats["terminal_hits"]+=1
                            break
                        if snapshot["phase"]!="playing" or snapshot["next_actor"]!=node.actor:
                            raise ValueError("search state/actor mismatch or interrupted branch")
                        if node.edges is None:
                            value=_expand(node,branch,branch.mask(),evaluator,stats)
                            perspective=node.actor
                            break
                        action=select_edge(node,config,rng)
                        edge=node.edges[action]
                        decision=branch.step(action,actor=node.actor,expected=snapshot["token"])
                        if decision["verdict"] not in ("legal","win","draw"):
                            raise ValueError("masked search action was rejected/adjudicated")
                        stats["transitions"]+=1
                        path.append((node,edge))
                        if edge.child is None:
                            edge.child=Node(branch.snapshot()["next_actor"])
                        node=edge.child
                    backup(path,value,perspective)
                    stats["simulations"]+=1
                    stats["max_depth"]=max(stats["max_depth"],len(path))
        visits=[root.edges[a].visits if a in root.edges else 0 for a in range(225)]
        maximum=max(visits)
        tied=[a for a in root.edges if visits[a]==maximum]
        prior=max(root.edges[a].prior for a in tied)
        action=_choose([a for a in tied if root.edges[a].prior==prior],rng,config.tie_break)
        if root.visits!=config.simulations or sum(visits)!=config.simulations:
            raise RuntimeError("root budget accounting mismatch")
        if stats["network_evaluations"]+stats["terminal_hits"]!=config.simulations+1:
            raise RuntimeError("leaf evaluation accounting mismatch")
        return dict(action=action,value=root_value,root_q=root.edges[action].q,
            root_visits=visits,visit_distribution=[n/config.simulations for n in visits],
            root_priors=[root.edges[a].prior if a in root.edges else 0.0 for a in range(225)],
            root_edge_q=[root.edges[a].q if a in root.edges else 0.0 for a in range(225)],
            root_legal_count=len(root.edges),search=stats,search_config=asdict(config),
            milliseconds=(time.perf_counter()-started)*1000,token=before["token"])
    except Exception as exc:
        raise SearchError(str(exc),dict(search=stats,search_config=asdict(config),
            milliseconds=(time.perf_counter()-started)*1000)) from exc
    finally:
        if env.snapshot()!=before:
            raise RuntimeError("live environment changed during search")


class PUCTPlayer:
    def __init__(self,evaluator,config=SearchConfig()):
        self.evaluator=evaluator
        self.config=config
        self.descriptor=dict(kind="puct-v1",evaluator=evaluator.descriptor,config=asdict(config),
            selection="max-visits-then-prior-then-"+config.tie_break,
            root_noise=False,tree_reuse=False,candidate_pruning=False,value_usage="leaf-backup")

    def prepare(self):
        return self.evaluator.prepare()

    def select(self,env,mask):
        # Production evaluator preparation is separate from measured tree search.
        start=time.perf_counter()
        needs_load=getattr(self.evaluator,"model",None) is None
        load_ms=self.prepare()
        preparation_ms=(time.perf_counter()-start)*1000
        result=search(env,mask,self.evaluator,self.config)
        result.update(model_sha256=self.evaluator.descriptor.get("sha256"),
            preparation_ms=preparation_ms,model_load_ms=load_ms if needs_load else 0.0)
        return result
