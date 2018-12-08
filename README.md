# DiscoPoP

https://www.informatik.tu-darmstadt.de/parallel/research_3/discopop.en.jsp
https://llvm.org/ProjectsWithLLVM/#discopop

DiscoPoP is a tool that helps software developers parallelize their programs with threads. It discovers potential parallelism in a sequential program and makes recommendations on how to exploit it.

The ability of compilers to automatically translate sequential programs into efficient parallel code is quite limited. Although auto-parallelization has been successfully applied in some cases such as loops that satisfy certain properties, no compiler exists yet that can effectively parallelize an arbitrarily structured program. Because a compiler does not know the precise value of pointers and array indices that are computed at runtime, it may assume parallelism-preventing data dependences in places where they would never occur in practice. As a result, parallelization becomes too conservative. With our parallelism discovery tool DiscoPoP, we aim to circumvent this problem. We abandon the idea of fully automatic parallelization and instead point the programmer to likely parallelization opportunities that we identify via dynamic dependence analysis. In this way, we consider only data dependences that actually occur. From these dynamic dependences we derive possible parallel design patterns, which we propose to the programmer to parallelize their programs.

Publications:
https://dl.acm.org/citation.cfm?doid=2723772.2723777
https://link.springer.com/chapter/10.1007%2F978-3-319-27140-8_39
