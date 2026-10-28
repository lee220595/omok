"""One shared policy/value class for supervised learning and future RL."""
from dataclasses import asdict,dataclass
import torch
from torch import nn


@dataclass(frozen=True)
class ModelConfig:
    channels:int=8
    layers:int=2
    value_hidden:int=16

    def __post_init__(self):
        if any(type(x) is not int or x<1 or x>128 for x in (self.channels,self.layers,self.value_hidden)) or self.layers>8:
            raise ValueError("invalid small CNN configuration")


class PolicyValueNet(nn.Module):
    def __init__(self,config=ModelConfig()):
        super().__init__()
        self.config=config
        layers=[]
        for i in range(config.layers):
            layers.extend((nn.Conv2d(4 if i==0 else config.channels,config.channels,3,padding=1),nn.ReLU()))
        self.trunk=nn.Sequential(*layers)
        self.policy=nn.Conv2d(config.channels,1,1)
        self.value=nn.Sequential(nn.AdaptiveAvgPool2d(1),nn.Flatten(),nn.Linear(config.channels,config.value_hidden),nn.ReLU(),nn.Linear(config.value_hidden,1),nn.Tanh())

    def forward(self,x):
        if x.dtype != torch.float32 or x.ndim!=4 or tuple(x.shape[1:])!=(4,15,15) or not torch.isfinite(x).all():
            raise ValueError("expected finite float32 NCHW [N,4,15,15]")
        h=self.trunk(x)
        return self.policy(h).flatten(1),self.value(h)


def checked_outputs(logits,values,batch):
    if tuple(logits.shape)!=(batch,225) or tuple(values.shape)!=(batch,1):
        raise ValueError("model output shape mismatch")
    if not torch.isfinite(logits).all() or not torch.isfinite(values).all() or (values.abs()>1).any():
        raise ValueError("nonfinite/out-of-range model output")


def masked_logits(logits,mask):
    if mask.dtype != torch.bool or mask.shape!=logits.shape or not mask.any(dim=1).all():
        raise ValueError("mask mismatch or BlockedNoLegalAction")
    if not torch.isfinite(logits).all():
        raise ValueError("nonfinite policy logits")
    return logits.masked_fill(~mask,float("-inf"))


def probabilities(logits,mask):
    return torch.softmax(masked_logits(logits,mask),dim=1)
