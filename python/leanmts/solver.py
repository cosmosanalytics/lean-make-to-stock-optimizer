"""Port of LeanMtsSolver.h: strategy interface for card-allocation solvers."""

from abc import ABC, abstractmethod

from .problem import LeanMtsProblem, LeanMtsSolution


class LeanMtsSolver(ABC):
    """Strategy interface so callers can swap heuristic/exact/production
    backends without caring which one they're driving."""

    @abstractmethod
    def solve(self, problem: LeanMtsProblem) -> LeanMtsSolution:
        raise NotImplementedError

    @abstractmethod
    def name(self) -> str:
        raise NotImplementedError
