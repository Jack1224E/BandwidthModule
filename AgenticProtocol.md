the deep-layer agentic protocol (v2.0)

pre-requisite: do not touch project files. use temp_workspace/ for everything until step 10.
step 1: the atomic decomposition

break the user's request into a "dependency tree." do not write code. list every single logic block required. save to 01_blueprint.md.

    [pause] wait for user to confirm the logic tree is complete.

step 2: search-tree mapping

for the current task, create a list of 6-8 specific technical questions u need to answer. map out the libraries (libtorrent, aria2, etc.) and the specific class methods u need to understand. save to 02_search_map.txt.
step 3: deep-dive research (5 levels deep)

perform a multi-threaded search. for every top-level result, u must dig 4-5 levels deep into the sub-links (documentation, github issues, stack overflow threads).

    rule: do not summarize. extract raw, detailed implementation notes.

    output: a comprehensive 03_research_deep_dive.log.

step 4: the "code manifest"

before looking for actual code snippets, write a detailed list of every single helper function, variable, and library include u will need. u are not allowed to stop until this list is 100% exhaustive.
step 5: logic extraction & extraction

now find the code snippets based on the manifest. line-by-line, explain how the external logic (e.g., libtorrent's session management) maps to our specific qt6 implementation. save logic flow to 05_logic_flow.cpp.tmp.
step 6: atomic implementation (layer 0)

write only the core logic in a isolated temp file. no headers, no includes, just the raw algorithmic changes.

    [pause] wait for user to review the raw logic.

step 7: the dependency bridge

create the "bridge" code—headers and interfaces that connect this atomic block to the rest of the project. save to 07_bridge.h.tmp.
step 8: temp-file verification

merge the logic and the bridge into a standalone sandbox_module.cpp. analyze it for adhd-hallucinations or syntax errors.
step 9: the "final boss" check

compare the sandbox_module.cpp against the 03_research_deep_dive.log. if a single detail from the research is missing, rewrite it.

    [pause] wait for user's explicit "go-ahead" to merge with the main codebase.

step 10: layer merge & local build

finally, move the code into the project. run the build. if a ghost error or exit-code hang happens, stop immediately, revert the change, and return to step 1.